/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */


/* Delete of records */

#include "mysql_priv.h"
#include "ha_innobase.h"

/*
  Optimize delete of all rows by doing a full generate of the table
  This will work even if the .ISM and .ISD tables are destroyed
*/

int generate_table(THD *thd, TABLE_LIST *table_list, TABLE *locked_table)
{
  char path[FN_REFLEN];
  int error;
  TABLE **table_ptr;
  DBUG_ENTER("generate_table");

  thd->proc_info="generate_table";

  if (global_read_lock)
  {
    if(thd->global_read_lock)
    {
      my_error(ER_TABLE_NOT_LOCKED_FOR_WRITE,MYF(0),
	       table_list->real_name);
      DBUG_RETURN(-1);
    }
    pthread_mutex_lock(&LOCK_open);
    while (global_read_lock && ! thd->killed ||
	   thd->version != refresh_version)
    {
      (void) pthread_cond_wait(&COND_refresh,&LOCK_open);
    }
    pthread_mutex_unlock(&LOCK_open);
  }

  
    /* If it is a temporary table, close and regenerate it */
  if ((table_ptr=find_temporary_table(thd,table_list->db,
				      table_list->real_name)))
  {
    TABLE *table= *table_ptr;
    HA_CREATE_INFO create_info;
    table->file->info(HA_STATUS_AUTO | HA_STATUS_NO_LOCK);
    bzero((char*) &create_info,sizeof(create_info));
    create_info.auto_increment_value= table->file->auto_increment_value;
    db_type table_type=table->db_type;

    strmov(path,table->path);
    *table_ptr= table->next;		// Unlink table from list
    close_temporary(table,0);
    *fn_ext(path)=0;				// Remove the .frm extension
    ha_create_table(path, &create_info,1);
    if ((error= (int) !(open_temporary_table(thd, path, table_list->db,
					     table_list->real_name, 1))))
    {
      (void) rm_temporary_table(table_type, path);
    }
  }
  else
  {
    (void) sprintf(path,"%s/%s/%s%s",mysql_data_home,table_list->db,
		   table_list->real_name,reg_ext);
    fn_format(path,path,"","",4);
    VOID(pthread_mutex_lock(&LOCK_open));
    if (locked_table)
      mysql_lock_abort(thd,locked_table);	 // end threads waiting on lock
    // close all copies in use
    if (remove_table_from_cache(thd,table_list->db,table_list->real_name))
    {
      if (!locked_table)
      {
	VOID(pthread_mutex_unlock(&LOCK_open));
	DBUG_RETURN(1);				// We must get a lock on table
      }
    }
    if (locked_table)
      locked_table->file->extra(HA_EXTRA_FORCE_REOPEN);
    if (thd->locked_tables)
      close_data_tables(thd,table_list->db,table_list->real_name);
    else
      close_thread_tables(thd,1);
    HA_CREATE_INFO create_info;
    bzero((char*) &create_info,sizeof(create_info));
    *fn_ext(path)=0;				// Remove the .frm extension
    error= ha_create_table(path,&create_info,1) ? -1 : 0;
    if (thd->locked_tables && reopen_tables(thd,1,0))
      error= -1;
    VOID(pthread_mutex_unlock(&LOCK_open));
  }
  if (!error)
  {
    mysql_update_log.write(thd,thd->query,thd->query_length);
    if (mysql_bin_log.is_open())
    {
      Query_log_event qinfo(thd, thd->query);
      mysql_bin_log.write(&qinfo);
    }
    send_ok(&thd->net);		// This should return record count
  }
  DBUG_RETURN(error ? -1 : 0);
}


int mysql_delete(THD *thd,
                 TABLE_LIST *table_list,
                 COND *conds,
                 ORDER *order,
                 ha_rows limit,
		 thr_lock_type lock_type,
                 ulong options)
{
  int		error;
  TABLE		*table;
  SQL_SELECT	*select;
  READ_RECORD	info;
  bool 		using_limit=limit != HA_POS_ERROR;
  bool	        use_generate_table,using_transactions;
  DBUG_ENTER("mysql_delete");

  if (!table_list->db)
    table_list->db=thd->db;
  if ((thd->options & OPTION_SAFE_UPDATES) && !conds)
  {
    send_error(&thd->net,ER_UPDATE_WITHOUT_KEY_IN_SAFE_MODE);
    DBUG_RETURN(1);
  }

  use_generate_table= (!using_limit && !conds &&
		       !(specialflag &
			 (SPECIAL_NO_NEW_FUNC | SPECIAL_SAFE_MODE)) &&
		       !(thd->options &
			 (OPTION_NOT_AUTO_COMMIT | OPTION_BEGIN)));
#ifdef HAVE_INNOBASE_DB
  /* We need to add code to not generate table based on the table type */
  if (!innobase_skip)
    use_generate_table=0;		// Innobase can't use re-generate table
#endif
  if (use_generate_table && ! thd->open_tables)
  {
    error=generate_table(thd,table_list,(TABLE*) 0);
    if (error <= 0)
      DBUG_RETURN(error);			// Error or ok
  }
  if (!(table = open_ltable(thd,table_list,
			    limit != HA_POS_ERROR ? TL_WRITE_LOW_PRIORITY :
			    lock_type)))
    DBUG_RETURN(-1);
  table->file->info(HA_STATUS_VARIABLE | HA_STATUS_NO_LOCK);
  thd->proc_info="init";
  if (use_generate_table)
    DBUG_RETURN(generate_table(thd,table_list,table));
  table->map=1;
  if (setup_conds(thd,table_list,&conds))
    DBUG_RETURN(-1);

  table->used_keys=table->quick_keys=0;		// Can't use 'only index'
  select=make_select(table,0,0,conds,&error);
  if (error)
    DBUG_RETURN(-1);
  if (select && select->check_quick(test(thd->options & SQL_SAFE_UPDATES),
				    limit))
  {
    delete select;
    send_ok(&thd->net,0L);
    DBUG_RETURN(0);
  }

  /* If running in safe sql mode, don't allow updates without keys */
  if (!table->quick_keys)
  {
    thd->lex.select_lex.options|=QUERY_NO_INDEX_USED;
    if ((thd->options & OPTION_SAFE_UPDATES) && limit == HA_POS_ERROR)
    {
      delete select;
      send_error(&thd->net,ER_UPDATE_WITHOUT_KEY_IN_SAFE_MODE);
      DBUG_RETURN(1);
    }
  }
  (void) table->file->extra(HA_EXTRA_NO_READCHECK);
  if (options & OPTION_QUICK)
    (void) table->file->extra(HA_EXTRA_QUICK);

  if (order)
  {
    uint         length;
    SORT_FIELD  *sortorder;
    TABLE_LIST   tables;
    List<Item>   fields;
    List<Item>   all_fields;
    ha_rows examined_rows;

    bzero((char*) &tables,sizeof(tables));
    tables.table = table;

    table->io_cache = (IO_CACHE *) my_malloc(sizeof(IO_CACHE),
                                             MYF(MY_FAE | MY_ZEROFILL));
    if (setup_order(thd, &tables, fields, all_fields, order) ||
        !(sortorder=make_unireg_sortorder(order, &length)) ||
        (table->found_records = filesort(&table, sortorder, length,
                                        (SQL_SELECT *) 0, 0L, HA_POS_ERROR,
					 &examined_rows))
        == HA_POS_ERROR)
    {
      delete select;
      DBUG_RETURN(-1);		// This will force out message
    }
  }

  init_read_record(&info,thd,table,select,1,1);
  ulong deleted=0L;
  thd->proc_info="updating";
  while (!(error=info.read_record(&info)) && !thd->killed)
  {
    if (!(select && select->skipp_record()))
    {
      if (!(error=table->file->delete_row(table->record[0])))
      {
	deleted++;
	if (!--limit && using_limit)
	{
	  error= -1;
	  break;
	}
      }
      else
      {
	table->file->print_error(error,MYF(0));
	error=0;
	break;
      }
    }
  }
  thd->proc_info="end";
  end_read_record(&info);
  /* if (order) free_io_cache(table); */ /* QQ Should not be needed */
  (void) table->file->extra(HA_EXTRA_READCHECK);
  if (options & OPTION_QUICK)
    (void) table->file->extra(HA_EXTRA_NORMAL);
  using_transactions=table->file->has_transactions();
  if (deleted && (error <= 0 || !using_transactions))
  {
    mysql_update_log.write(thd,thd->query, thd->query_length);
    if (mysql_bin_log.is_open())
    {
      Query_log_event qinfo(thd, thd->query, using_transactions);
      if (mysql_bin_log.write(&qinfo) && using_transactions)
	error=1;
    }
    if (!using_transactions)
      thd->options|=OPTION_STATUS_NO_TRANS_UPDATE;
  }
  if (using_transactions && ha_autocommit_or_rollback(thd,error >= 0))
    error=1;
  if (thd->lock)
  {
    mysql_unlock_tables(thd, thd->lock);
    thd->lock=0;
  }
  delete select;
  if (error >= 0)				// Fatal error
    send_error(&thd->net,thd->killed ? ER_SERVER_SHUTDOWN: 0);
  else
  {
    send_ok(&thd->net,deleted);
    DBUG_PRINT("info",("%d records deleted",deleted));
  }
  DBUG_RETURN(0);
}




/***************************************************************************
** delete multiple tables from join 
***************************************************************************/


#define MEM_STRIP_BUF_SIZE 2048

int
multi_delete::prepare(List<Item> &values)
{
  DBUG_ENTER("multi_delete::prepare");
  tempfiles = (IO_CACHE **) sql_calloc(sizeof(IO_CACHE *)*(num_of_tables));
  memory_lane = (byte *)sql_alloc(MAX_REFLENGTH*MEM_STRIP_BUF_SIZE); uint counter = 0;
  dup_checking = (byte *) sql_calloc(MAX_REFLENGTH * (num_of_tables + 1));
  memset(dup_checking,'\xFF', MAX_REFLENGTH * (num_of_tables + 1)); do_delete = true;
  for (table_being_deleted=delete_tables; table_being_deleted; table_being_deleted=table_being_deleted->next, counter++)
  {
    TABLE *table=table_being_deleted->table;
    if ((thd->options & OPTION_SAFE_UPDATES) && !table->quick_keys)
    {
      my_error(ER_UPDATE_WITHOUT_KEY_IN_SAFE_MODE,MYF(0));
      DBUG_RETURN(1);
    }
    (void) table->file->extra(HA_EXTRA_NO_READCHECK);
    if (counter < num_of_tables)
    {
      tempfiles[counter]=(IO_CACHE *)sql_alloc(sizeof(IO_CACHE));
      if (open_cached_file(tempfiles[counter], mysql_tmpdir,TEMP_PREFIX, DISK_BUFFER_SIZE, MYF(MY_WME)))
      {
	my_error(ER_CANT_OPEN_FILE,MYF(0),(tempfiles[counter])->file_name,errno);
	DBUG_RETURN(1);
      }
    }
  }
  thd->proc_info="updating";
  DBUG_RETURN(0);
}

multi_delete::~multi_delete()
{
  for (uint counter = 0; counter < num_of_tables; counter++)
    if (tempfiles[counter])
    end_io_cache(tempfiles[counter]);
// Here it crashes ...
}

bool multi_delete::send_data(List<Item> &values)
{
  int secure_counter = -1;
  for (table_being_deleted=delete_tables ; table_being_deleted ; table_being_deleted=table_being_deleted->next, secure_counter++)
  {
    TABLE *table=table_being_deleted->table;
    table->file->position(table->record[0]); int rl = table->file->ref_length;
    byte *dup_check = dup_checking + (secure_counter + 1)*MAX_REFLENGTH;
    if (!table->null_row && memcmp(dup_check,table->file->ref, rl) && memcmp(table->file->ref,wrong_record,rl))
    {
      memcpy(dup_check,table->file->ref,rl);
      if (secure_counter == -1)
      {
	if (!(error=table->file->delete_row(table->record[0])))
	  deleted++;
	else
	{
	  send_error(error,"An error occured in deleting rows");
	  return 1;
	}
      }
      else
      {
	if (my_b_write(tempfiles[secure_counter],table->file->ref,rl))
	{
	  error=-1;
	  return 1;
	}
      } 
    }
  }
  return 0;
}

#ifdef SINISAS_STRIP
static inline int COMP (byte *ml,uint len,unsigned int left, unsigned int right)
{
  return memcmp(ml + left*len,ml + right*len,len);
}

#define EX(ML,LEN,LLLEFT,RRRIGHT) \
ptr1 = ML + LLLEFT*LEN;\
ptr2 = ML + RRRIGHT*LEN;\
memcpy(tmp,ptr1,LEN);\
memcpy(ptr1,ptr2,LEN);\
memcpy(ptr2,tmp,LEN);\



static void  qsort_mem_pieces(byte *ml, uint length,  unsigned short pivotP, unsigned int nElem) 
{
  unsigned int     leftP, rightP, pivotEnd, pivotTemp, leftTemp;
  unsigned  int  lNum; byte tmp [MAX_REFLENGTH], *ptr1, *ptr2;
  int       retval;
tailRecursion:
  if (nElem <= 1) return;
  if (nElem == 2) 
  {
    if (COMP(ml,length,pivotP, rightP = pivotP + 1) > 0)
    {
      EX(ml,length,pivotP, rightP);
    }
    return;
  }

  rightP = (nElem - 1) + pivotP;
  leftP  = (nElem >> 1) + pivotP;

/*  sort the pivot, left, and right elements for "median of 3" */

  if (COMP (ml,length,leftP, rightP) > 0)
  {	
    EX (ml,length,leftP, rightP);
  }
  if (COMP (ml,length,leftP, pivotP) > 0)
  {
    EX (ml,length,leftP, pivotP);
  }
  else if (COMP (ml,length, pivotP, rightP) > 0)
  {
    EX (ml,length,pivotP, rightP);
  }
	
  if (nElem == 3) {
    EX (ml,length,pivotP, leftP);
    return;
  }

/*  now for the classic Hoare algorithm */
  
  leftP = pivotEnd = pivotP + 1;

  do {
    while ((retval = COMP (ml,length, leftP, pivotP)) <= 0) 
    {
      if (retval == 0) {
	EX(ml,length,leftP, pivotEnd);
	pivotEnd++;
      }
      if (leftP < rightP)
	leftP++;
      else
	goto qBreak;
    }
    while (leftP < rightP) {
      if ((retval = COMP(ml,length,pivotP, rightP)) < 0)
	rightP--;
      else 
      {
	EX (ml,length,leftP, rightP);
	if (retval != 0) {
	  leftP++;
	  rightP--;
	}
	break;
      }
    }
  }   while (leftP < rightP);

qBreak:

  if (COMP(ml,length,leftP, pivotP) <= 0)
    leftP++;

  leftTemp = leftP - 1;    pivotTemp = pivotP;

  while ((pivotTemp < pivotEnd) && (leftTemp >= pivotEnd)) 
  {
    EX(ml,length,pivotTemp, leftTemp);
    pivotTemp++;  leftTemp--;
  }

  lNum = leftP - pivotEnd;   nElem = (nElem + pivotP) - leftP;

  /* Sort smaller partition first to reduce stack usage */
  if (nElem < lNum) 
  {
    qsort_mem_pieces(ml,length,leftP, nElem); nElem = lNum;
  }
  else 
  {
    qsort_mem_pieces(ml,length,pivotP, lNum);
    pivotP = leftP;
  }
  goto tailRecursion;
}

static byte * btree_search(byte *lane, byte *key,register int last, uint length)
{
  register int first = 0;
  if (last == first)
  {
    if (!memcmp(lane,key,length)) return lane;
    return (byte *)0;
  }
Recursion_is_too_slow:
  if (last - first < 3)
  {
    if (!memcmp(lane + first*length,key,length)) return lane + first * length;
    if (last == first + 1) return (byte *)0;
    if (!memcmp(lane +  last*length,key,length)) return lane + last * length;
    return (byte *)0;
  }
  else 
  {
    int half = first + (last - first)/2;
    int result = memcmp(lane + half*length,key,length);
    if (!result) return lane + half*length;
    if (result < 0)
    {
      first = half + 1; goto Recursion_is_too_slow;
    }
    else
    {	
      last = half + 1; goto Recursion_is_too_slow;
    }
  }
}

struct written_block {
  byte first[MAX_REFLENGTH], last[MAX_REFLENGTH];
  my_off_t offset;
  uint how_many;
};

static IO_CACHE *strip_duplicates_from_temp (byte *memory_lane, IO_CACHE *ptr, uint ref_length,  int *written)
{
  byte *mem_ptr; my_off_t off = 0;
  int read_error, write_error, how_many_to_read, total_to_read = *written, pieces_in_memory = 0, mem_count,written_rows;
  int offset = written_rows=*written=0;
  int  mem_pool_size = MEM_STRIP_BUF_SIZE * MAX_REFLENGTH / ref_length;
  IO_CACHE *tempptr = (IO_CACHE *) sql_alloc(sizeof(IO_CACHE));
  byte dup_record[MAX_REFLENGTH]; memset(dup_record,'\xFF',MAX_REFLENGTH);
  if (reinit_io_cache(ptr,READ_CACHE,0L,0,0))
    return ptr;
  if (open_cached_file(tempptr, mysql_tmpdir,TEMP_PREFIX, DISK_BUFFER_SIZE, MYF(MY_WME)))
    return ptr;
  DYNAMIC_ARRAY written_blocks;
  VOID(init_dynamic_array(&written_blocks,sizeof(struct written_block),20,50));
  for (;pieces_in_memory < total_to_read;)
  {
    how_many_to_read = total_to_read - pieces_in_memory;  read_error=write_error=0;
    if (how_many_to_read > mem_pool_size) 
      how_many_to_read = mem_pool_size;
    if (my_b_read(ptr, memory_lane, (uint) how_many_to_read * ref_length))
    {
      read_error = 1;
      break;
    }
    pieces_in_memory += how_many_to_read;
    qsort_mem_pieces(memory_lane,0, how_many_to_read, ref_length);
    byte *checking = dup_record,  *cursor=NULL, *mem_end = memory_lane + how_many_to_read * ref_length;
    int opt_unique_pieces, unique_pieces_in_memory=0; write_error=0;
    for (mem_ptr=memory_lane; mem_ptr < mem_end ; mem_ptr += ref_length)
    {
      if (memcmp(mem_ptr,checking, ref_length))
      {
	if (cursor)
	{
	  memmove(cursor,mem_ptr,mem_end - mem_ptr);
	  mem_end -= mem_ptr - cursor;
	  mem_ptr = cursor; cursor = NULL;
	}
	unique_pieces_in_memory++;
	checking = mem_ptr;
      }
      else if (!cursor) cursor = mem_ptr;
    }
    opt_unique_pieces=unique_pieces_in_memory;
    if (written_rows) 
    {
      if (reinit_io_cache(tempptr,READ_CACHE,0L,0,0))  {write_error = -1; break;}
      for (uint i=0 ; i < written_blocks.elements ; i++)
      {
	struct written_block *wbp=dynamic_element(&written_blocks,i,struct written_block*);
	if ((memcmp(memory_lane,wbp->last,ref_length) == 1) || (memcmp(memory_lane + (unique_pieces_in_memory - 1) * ref_length, wbp->first, ref_length) == -1))
	  continue;
	else
	{
	  if (wbp->how_many < 3)
	  {
	    if ((mem_ptr=btree_search(memory_lane,wbp->first,unique_pieces_in_memory-1, ref_length)))
	    {
	      if (!--opt_unique_pieces) goto skip_writting; // nice little optimization
	      memcpy(mem_ptr,dup_record,ref_length);
	    }
	    if (wbp->how_many == 2 && (mem_ptr=btree_search(memory_lane,wbp->last,unique_pieces_in_memory-1, ref_length)))
	    {
	      if (!--opt_unique_pieces) goto skip_writting; // nice little optimization
	      memcpy(mem_ptr,dup_record,ref_length);
	    }
	  }
	  else
	  {
	    byte block[MAX_REFLENGTH * MEM_STRIP_BUF_SIZE]; // 16 K maximum and only temporary !!
	    if (my_b_read(tempptr, block, (uint) wbp->how_many * ref_length))
	    {
	      read_error = 1; goto skip_writting;
	    }
	    if (unique_pieces_in_memory < 3)
	    {
	      if ((mem_ptr=btree_search(block,memory_lane,wbp->how_many - 1, ref_length)))
	      {
		if (!--opt_unique_pieces) goto skip_writting; // nice little optimization
		memcpy(memory_lane,dup_record,ref_length);
	      }
	      if (unique_pieces_in_memory == 2 && (mem_ptr=btree_search(block,memory_lane + ref_length,wbp->how_many - 1, ref_length)))
	      {
		if (!--opt_unique_pieces) goto skip_writting; // nice little optimization
		memcpy(mem_ptr,dup_record,ref_length);
	      }
	    }
	    else
	    {
	      byte *cursor; bool do_check_past;
	      if (unique_pieces_in_memory < wbp->how_many)
	      {
		do_check_past = (memcmp(memory_lane + (unique_pieces_in_memory - 1)*ref_length,wbp->last,ref_length) == 1);
		for (cursor=memory_lane; cursor < memory_lane + unique_pieces_in_memory*ref_length; cursor += ref_length)
		{
		  if ((mem_ptr=btree_search(block,cursor,wbp->how_many - 1, ref_length)))
		  {
		    if (!--opt_unique_pieces) goto skip_writting; // nice little optimization
		    memcpy(cursor,dup_record,ref_length);
		  }
		  else if (do_check_past && (memcmp(cursor,wbp->last,ref_length) == 1)) break;
		}
	      }
	      else
	      {
		do_check_past = (memcmp(memory_lane + (unique_pieces_in_memory - 1)*ref_length,wbp->last,ref_length) == -1);
		for (cursor=block; cursor < block + wbp->how_many*ref_length;cursor += ref_length)
		{
		  if ((mem_ptr=btree_search(memory_lane,cursor,unique_pieces_in_memory-1, ref_length)))
		  {
		    if (!--opt_unique_pieces) goto skip_writting; // nice little optimization
		    memcpy(mem_ptr,dup_record,ref_length);
		  }
		  else if (do_check_past && (memcmp(cursor,memory_lane + (unique_pieces_in_memory - 1)*ref_length,ref_length) == 1)) break;
		}
	      }
	    }
	  }
	}
      }
    }
    reinit_io_cache(tempptr, WRITE_CACHE,off,0,0);
    struct written_block wb; wb.offset = off; wb.how_many=opt_unique_pieces; byte *last;
    if (opt_unique_pieces < unique_pieces_in_memory)
    {
      for (mem_count=0, mem_ptr=memory_lane; mem_count<unique_pieces_in_memory;mem_count++, mem_ptr += ref_length)
      {
	if (memcmp(mem_ptr,dup_record,ref_length)) 
	{
	  if (my_b_write(tempptr,mem_ptr,ref_length))
	  {
	    if (write_error == 9 || write_error == -1) write_error = 0;
	    if (write_error) break;
	  }
	  if (!mem_count) memcpy(wb.first,mem_ptr,ref_length);
	  last = mem_ptr;
	  written_rows++;
	}
      }
      memcpy(wb.last,last,ref_length);
    }
    else
    {
      memcpy(wb.first,memory_lane,ref_length); memcpy(wb.last,memory_lane + (unique_pieces_in_memory -1)*ref_length,ref_length);
      if (my_b_write(tempptr, memory_lane,unique_pieces_in_memory * ref_length))
      {
	write_error = 1; break;
      }
      written_rows += unique_pieces_in_memory;
    }
    off = my_b_tell(tempptr);
    VOID(push_dynamic(&written_blocks,(gptr) &wb));
  skip_writting:
    if (write_error || read_error) break;
  }
  delete_dynamic(&written_blocks);
  if (read_error || write_error)
  {
    close_cached_file(tempptr); end_io_cache(tempptr);
    return ptr;
  }
  else
  {
    close_cached_file(ptr); *written=written_rows; end_io_cache(ptr);
    reinit_io_cache(tempptr,READ_CACHE,0L,0,0); 
    return tempptr;
  }
}
#else
int refposcmp2(void* arg, const void *a,const void *b)
{
  return memcmp(a,b,(int)arg);
}
#endif

static bool some_table_is_not_transaction_safe (TABLE_LIST *tl)
{
  TABLE_LIST *deleting = tl;
  for (deleting=deleting->next; deleting ; deleting=deleting->next)
  { 
    if (!(deleting->table->file->has_transactions()))
      return true;
  }
  return false;
}


void multi_delete::send_error(uint errcode,const char *err)
{
  ::send_error(&thd->net,errcode,err);
  if (!deleted) return;
  if (!table_being_deleted) table_being_deleted=delete_tables;
  if ((table_being_deleted->table->file->has_transactions() && table_being_deleted == delete_tables) || !some_table_is_not_transaction_safe(delete_tables))
    ha_rollback(current_thd);
  else if (do_delete)
    VOID(do_deletes(true));
}

int  multi_delete::do_deletes (bool from_send_error)
{
  TABLE *table;
  int error = 0, counter = 0, count;
  if (from_send_error)
  {
    for (TABLE_LIST *aux=delete_tables; aux != table_being_deleted; aux=aux->next)
      counter++;
  }
  else
    table_being_deleted = delete_tables;
  do_delete = false;
  for (table_being_deleted=table_being_deleted->next; table_being_deleted ; counter++, table_being_deleted=table_being_deleted->next)
  { 
    table = table_being_deleted->table; int rl = table->file->ref_length;
    int num_of_positions =  (int)my_b_tell(tempfiles[counter])/rl;
    if (!num_of_positions) continue;
#ifdef SINISAS_STRIP
    tempfiles[counter] = strip_duplicates_from_temp(memory_lane, tempfiles[counter],rl,&num_of_positions);
    if (!num_of_positions)
    {
      error=1; break;
    }
#else
    Unique strip_it(refposcmp2,(void *)rl,rl,MEM_STRIP_BUF_SIZE);
    if (reinit_io_cache(tempfiles[counter],READ_CACHE,0L,0,0))
    {
      error=1; break;
    }
    for (count = 0; count < num_of_positions; count++)
    {
      byte tmp [MAX_REFLENGTH];
      if (my_b_read(tempfiles[counter], tmp, rl) || strip_it.unique_add(tmp))
      {
	error = 1;
	break;
      }
    }
    strip_it.get(table);
#endif
#if 0
    if (num_of_positions == table->file->records) // nice little optimization ....
    {                                             // but Monty has to fix generate_table...
      TABLE_LIST table_list;
      bzero((char*) &table_list,sizeof(table_list));
      table_list.name=table->table_name; table_list.real_name=table_being_deleted->real_name;
      table_list.table=table;
      table_list.grant=table->grant;
      table_list.db = table_being_deleted->db;
      error=generate_table(thd,&table_list,(TABLE *)0);
      if (error <= 0) {error = 1; break;}
      deleted += num_of_positions;
    }
    else
    {
#endif				
      SQL_SELECT	*select= new SQL_SELECT;
      READ_RECORD	info;
      select->head=table;
      select->file=*tempfiles[counter];
      init_read_record(&info,thd,table,select,0,0); error=0;
      while (!(error=info.read_record(&info)) && (!thd->killed ||  from_send_error))
      {
	error=table->file->delete_row(table->record[0]);
	if (error)
	{
	  table->file->print_error(error,MYF(0));
	  break;
	}
	else
	  deleted++;
      }
      end_read_record(&info);	 delete select;
      if (error = -1) error = 0; // Monty, that is what read_record returns on end of the file !!
#if 0
    }
#endif
  }
  return error;
}

bool multi_delete::send_eof()
{
  int error = 0;
  error = do_deletes(false);
  thd->proc_info="end";
  if (error && error != -1)
  {
    ::send_error(&thd->net);
    return 1;
  }
  for (table_being_deleted=delete_tables ; table_being_deleted ; table_being_deleted=table_being_deleted->next)
  {
    TABLE *table=table_being_deleted->table;
    if (table)
      VOID(table->file->extra(HA_EXTRA_READCHECK));
  }
  if (deleted && (error <= 0 || some_table_is_not_transaction_safe(delete_tables)))
  {
    mysql_update_log.write(thd,thd->query,thd->query_length);
    Query_log_event qinfo(thd, thd->query);
    if (mysql_bin_log.write(&qinfo) && !some_table_is_not_transaction_safe(delete_tables))
      error=1;
    VOID(ha_autocommit_or_rollback(thd,error >= 0));
  }
  if (thd->lock)
  {
    mysql_unlock_tables(thd, thd->lock);
    thd->lock=0;
  }
  ::send_ok(&thd->net,deleted);
  return 0;

}
