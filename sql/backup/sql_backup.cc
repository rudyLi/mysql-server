/**
  @file

  Implementation of the backup kernel API.
 */

/*
  TODO:

  - Handle SHOW BACKUP ARCHIVE command.
  - Add database list to the backup/restore summary.
  - Implement meta-data freeze during backup/restore.
  - Improve logic selecting backup driver for a table.
  - Handle other types of meta-data in Backup_info methods.
  - Handle item dependencies when adding new items.
  - Lock I_S tables when reading table list and similar (is it needed?)
  - When reading table list from I_S tables, use select conditions to
    limit amount of data read. (check prepare_select_* functions in sql_help.cc)
  - Handle other kinds of backup locations (far future).
  - Complete feedback given by BACKUP/RESTORE statements (send_summary function).
 */

#include "../mysql_priv.h"

#include "backup_aux.h"
#include "stream.h"
#include "backup_kernel.h"
#include "meta_backup.h"
#include "archive.h"
#include "debug.h"

namespace backup {

// Helper functions

static IStream* open_for_read(const Location&);
static OStream* open_for_write(const Location&);

/*
  Report errors. The main error code and optional arguments for its description
  are given plus a logger object which can contain stored errors.
 */
static int report_errors(THD*,int, Logger&,...);

/*
  Check if info object is valid. If not, report error to client.
 */
static int check_info(THD*,Backup_info&);
static int check_info(THD*,Restore_info&);

static bool send_summary(THD*,const Backup_info&);
static bool send_summary(THD*,const Restore_info&);

#ifdef DBUG_BACKUP
// Flag used for testing error reporting
bool test_error_flag= FALSE;
#endif
}

/**
  Call backup kernel API to execute backup related SQL statement.

  @param lex  results of parsing the statement.

  @note This function sends response to the client (ok, result set or error).
 */

int
execute_backup_command(THD *thd, LEX *lex)
{
  DBUG_ENTER("execute_backup_command");
  DBUG_ASSERT(thd && lex);

  BACKUP_SYNC("backup_command");

  backup::Location *loc= backup::Location::find(lex->backup_dir);

  if (!loc)
  {
    my_error(ER_BACKUP_INVALID_LOC,MYF(0),lex->backup_dir);
    DBUG_RETURN(ER_BACKUP_INVALID_LOC);
  }

  int res= 0;

  switch (lex->sql_command) {

  case SQLCOM_SHOW_ARCHIVE:
  case SQLCOM_RESTORE:
  {
    backup::IStream *stream= backup::open_for_read(*loc);

    if (!stream)
    {
      res= -1;
      my_error(ER_BACKUP_READ_LOC,MYF(0),loc->describe());
      goto finish_restore;
    }
    else
    {
      backup::Restore_info info(*stream);

      if (check_info(thd,info))
      {
        res= -2;
        goto finish_restore;
      }

      if (lex->sql_command == SQLCOM_SHOW_ARCHIVE)
      {
        my_error(ER_NOT_ALLOWED_COMMAND,MYF(0));
        /*
          Uncomment when mysql_show_archive() is implemented

          res= mysql_show_archive(thd,info);
         */
      }
      else
      {
        info.report_error(backup::log_level::INFO,ER_BACKUP_RESTORE_START);

        // TODO: freeze all DDL operations here

        info.save_errors();
        info.restore_all_dbs();

        if (check_info(thd,info))
        {
          res= -3;
          goto finish_restore;
        }

        info.clear_saved_errors();

        if (mysql_restore(thd,info,*stream))
        {
          res= -4;
          report_errors(thd,ER_BACKUP_RESTORE,info);
        }
        else
        {
          info.report_error(backup::log_level::INFO,ER_BACKUP_RESTORE_DONE);
          info.total_size += info.header_size;
          send_summary(thd,info);
        }

        // TODO: unfreeze DDL here
      }
    } // if (!stream)

   finish_restore:

    if (stream)
      stream->close();

    break;
  }

  case SQLCOM_BACKUP:
  {
    backup::OStream *stream= backup::open_for_write(*loc);

    if (!stream)
    {
      res= -5;
      my_error(ER_BACKUP_WRITE_LOC,MYF(0),loc->describe());
      goto finish_backup;
    }
    else
    {
      backup::Backup_info info(thd);

      if (check_info(thd,info))
      {
        res= -6;
        goto finish_backup;
      }

      info.report_error(backup::log_level::INFO,ER_BACKUP_BACKUP_START);

      // TODO: freeze all DDL operations here

      info.save_errors();

      if (lex->db_list.is_empty())
      {
        info.write_message(backup::log_level::INFO,"Backing up all databases");
        info.add_all_dbs(); // backup all databases
      }
      else
      {
        info.write_message(backup::log_level::INFO,"Backing up selected databases");
        info.add_dbs(lex->db_list); // backup databases specified by user
      }

      if (check_info(thd,info))
      {
        res= -7;
        goto finish_backup;
      }

      info.close();

      if (check_info(thd,info))
      {
        res= -8;
        goto finish_backup;
      }

      info.clear_saved_errors();

      if (mysql_backup(thd,info,*stream))
      {
        res= -9;
        report_errors(thd,ER_BACKUP_BACKUP,info);
      }
      else
      {
        info.report_error(backup::log_level::INFO,ER_BACKUP_BACKUP_DONE);
        send_summary(thd,info);
      }

      // TODO: unfreeze DDL here
    } // if (!stream)

   finish_backup:

    if (stream)
      stream->close();

#ifdef  DBUG_BACKUP
    backup::IStream *is= backup::open_for_read(*loc);
    if (is)
    {
      dump_stream(*is);
      is->close();
    }
#endif

    break;
  }

   default:
     /*
       execute_backup_command() should be called with correct command id
       from the parser. If not, we fail on this assertion.
      */
     DBUG_ASSERT(FALSE);
  }

  loc->free();
  DBUG_RETURN(res);
}


/*************************************************
 *
 *                 BACKUP
 *
 *************************************************/

// Declarations for functions used in backup operation

namespace backup {

// defined in meta_backup.cc
int write_meta_data(THD*, Backup_info&, OStream&);

// defined in data_backup.cc
int write_table_data(THD*, Backup_info&, OStream&);

} // backup namespace


/**
  Create backup archive.
*/

int mysql_backup(THD *thd,
                 backup::Backup_info &info,
                 backup::OStream &s)
{
  DBUG_ENTER("mysql_backup");

  // This function should not be called with invalid backup info.
  DBUG_ASSERT(info.is_valid());

  size_t start_bytes= s.bytes;
  int res= 0;

  BACKUP_SYNC("backup_meta");

  DBUG_PRINT("backup",("Writing image header"));

  if (info.save(s))
  {
    res= -1;
    goto finish;
  }

  DBUG_PRINT("backup",("Writing meta-data"));

  if (backup::write_meta_data(thd,info,s))
  {
    res= -2;
    goto finish;
  }

  DBUG_PRINT("backup",("Writing table data"));

  BACKUP_SYNC("backup_data");

  if (backup::write_table_data(thd,info,s))
  {
    res= -3;
    goto finish;
  }

  DBUG_PRINT("backup",("Backup done."));
  BACKUP_SYNC("backup_done");

  info.total_size= s.bytes - start_bytes;

 finish:

  DBUG_RETURN(res);
}

namespace backup {


class Backup_info::Table_ref:
 public backup::Table_ref
{
  String  m_db_name;
  String  m_name;
  const ::handlerton   *m_hton;

 public:

  Table_ref(TABLE*);
  const ::handlerton *hton() const
  { return m_hton; }
};

/**
  Find image to which given table can be added and add it.

  Creates new images as needed.

  @returns Position of the image found in @c images[] table or -1 on error.
 */

/*
   Logic for selecting image format for a given table location:

   1. If tables from that location have been already stored in one of the
      sub-images then choose that sub-image.

   2. If location has "native" backup format, put it in a new sub-image with
      that format.

   3. Otherwise check if one of the existing sub-images would accept table from
      this location.

   Note: 1 is not implemented yet and hence we start with 3.
 */

int Backup_info::find_image(const Backup_info::Table_ref &tbl)
{
   DBUG_ENTER("Backup_info::find_image");

   const ::handlerton *hton= tbl.hton();
   Image_info *img;

   // If table has no handlerton something is really bad - we crash here
   DBUG_ASSERT(hton);

   DBUG_PRINT("backup",("Adding table %s.%s (%s,%p) to archive.",
                        tbl.db().name().ptr(),
                        tbl.name().ptr(),
                        ::ha_resolve_storage_engine_name(hton),
                        hton->get_backup_engine) );

   // Point 3: try existing images but not the default one.

   for (uint no=0; no < img_count && no < MAX_IMAGES ; ++no)
   {
     if (default_image_no >= 0 && no == (uint)default_image_no)
       continue;

     img= images[no];

     // An image object decides if it can handle given table or not
     if( img && img->accept(tbl,hton) )
       DBUG_RETURN(no);
   }

   // Point 2: try native backup of table's storage engine.

   if (hton->get_backup_engine)
   {
     uint no= img_count;

     // We handle at most MAX_IMAGES many images
     DBUG_ASSERT(no<MAX_IMAGES);

     img= images[no]= new Native_image(*this,hton);

     if (!img)
     {
       report_error(ER_OUT_OF_RESOURCES);
       DBUG_RETURN(-1);
     }

     DBUG_PRINT("backup",("%s image added to archive",img->name()));
     img_count++;

     // native image should accept all tables from its own engine
     bool res= img->accept(tbl,hton);
     DBUG_ASSERT(res);

     DBUG_RETURN(no);
   }

   Table_ref::describe_buf buf;
   report_error(ER_BACKUP_NO_BACKUP_DRIVER,tbl.describe(buf,sizeof(buf)));
   DBUG_RETURN(-1);
}

} // backup namespace


/****************************

  Backup_info implementation

 ****************************/

namespace backup {

//  Returns tmp table containing records from a given I_S table
TABLE* get_schema_table(THD *thd, ST_SCHEMA_TABLE *st);

/**
  Create @c Backup_info structure and prepare it for populating with meta-data
  items.

  When adding a complete database to the archive, all its tables are added.
  These are found by reading INFORMATION_SCHEMA.TABLES table. The table is
  opened here so that it is ready for use in @c add_db_items() method. It is
  closed when the structure is closed with the @c close() method.
 */
Backup_info::Backup_info(THD *thd):
  default_image_no(-1), m_thd(thd), i_s_tables(NULL), m_state(INIT),
  m_items(NULL), m_last_item(NULL), m_last_db(NULL)
{
  i_s_tables= get_schema_table(m_thd, ::get_schema_table(SCH_TABLES));
  if (!i_s_tables)
  {
    report_error(ER_BACKUP_LIST_TABLES);
    m_state= ERROR;
  }
}

Backup_info::~Backup_info()
{
  close();
  //close_tables();
  m_state= DONE;

  Item_iterator it(*this);
  Item *i;

  while ((i=it++))
    delete i;

  m_items= NULL;
}

/**
  Close the structure after populating it with items.

  When meta-data is written, we need to open archive's tables to be able
  to read their definitions. Tables are opened here so that the
  structure is ready for creation of the archive.

  FIXME: opening all tables at once consumes too much resources when there
  is a lot of tables (opened file descriptors!). Find a better solution.
 */
bool Backup_info::close()
{
  bool ok= is_valid();

  if(i_s_tables)
    ::free_tmp_table(m_thd,i_s_tables);
  i_s_tables= NULL;

  if (m_state == INIT)
    m_state= READY;

  return ok;
}

int Backup_info::save(OStream &s)
{
  if (OK != Archive_info::save(s))
  {
    report_error(ER_BACKUP_WRITE_HEADER);
    return -1;
  }

  return 0;
}

/**
  Specialization of @c Db_ref which takes db name from @c LEX_STRING structure
  or reads it from INFORMATION_SCHEMA.SCHEMATA table.
 */

class Backup_info::Db_ref: public backup::Db_ref
{
  String m_name;

 public:

  Db_ref(const LEX_STRING &s):
    backup::Db_ref(m_name),
    m_name(s.str,s.length,::system_charset_info)
  {}

  Db_ref(TABLE *t): backup::Db_ref(m_name)
  {
    t->field[1]->val_str(&m_name);
  }
};


/**
  Add to archive all databases in the list.
 */
int Backup_info::add_dbs(List< ::LEX_STRING > &dbs)
{
  List_iterator< ::LEX_STRING > it(dbs);
  int res= 0;
  ::LEX_STRING *s;
  String unknown_dbs; // comma separated list of databases which don't exist

  while ((s= it++))
  {
    Db_ref db(*s);

    if (db_exists(db))
    {
      // In case of error, we just compose unknown_dbs list
      if (res)
        continue;

      Db_item *it= add_db(db);

      if (!it)
      {
        res= -1;
        goto finish;
      }

      if (add_db_items(*it))
      {
        res= -2;
        goto finish;
      }
    }
    else
    {
      if (res)
        unknown_dbs.append(",");
      else
        res= -3;
      unknown_dbs.append(db.name());
    }
  }

  if (res == -3)
    report_error(ER_BAD_DB_ERROR,unknown_dbs.c_ptr());

 finish:

  if (res)
    m_state= ERROR;

  return res;
}

static const LEX_STRING is_string = "information_schema";
static const LEX_STRING mysql_string = "mysql";

/**
  Add to archive all instance's databases (except the internal ones).
 */

int Backup_info::add_all_dbs()
{
  DBUG_PRINT("backup", ("Reading databases from I_S"));

  ::TABLE *db_table = get_schema_table(m_thd, ::get_schema_table(SCH_SCHEMATA));

  if (!db_table)
  {
    report_error(ER_BACKUP_LIST_DBS);
    return -1;
  }

  ::handler *ha = db_table->file;

  // TODO: locking

  const Db_ref is_db(is_string);
  const Db_ref mysql_db(mysql_string);
  int res= 0;

  if (ha->ha_rnd_init(TRUE))
  {
    res= -2;
    report_error(ER_BACKUP_LIST_DBS);
    goto finish;
  }

  while (!ha->rnd_next(db_table->record[0]))
  {
    Db_ref db(db_table);

    if (db==is_db || db==mysql_db)
    {
      DBUG_PRINT("backup",(" Skipping internal database %s",db.name().ptr()));
      continue;
    }

    DBUG_PRINT("backup", (" Found database %s", db.name().ptr()));

    Db_item *it= add_db(db);

    if (!it)
    {
      res= -3;
      goto finish;
    }

    if (add_db_items(*it))
    {
      res= -4;
      goto finish;
    }
  }

  DBUG_PRINT("backup", ("No more databases in I_S"));

  ha->ha_rnd_end();

 finish:

  if (ha)
    ha->close();

  if (res)
    m_state= ERROR;

  return res;
}

/**
  Insert a @c Db_item into the meta-data items list.
 */
Backup_info::Db_item*
Backup_info::add_db(const backup::Db_ref &db)
{
  StringPool::Key key= db_names.add(db.name());

  if (!key.is_valid())
  {
    report_error(ER_OUT_OF_RESOURCES);
    return NULL;
  }

  Db_item *di= new Db_item(*this,key);

  if (!di)
  {
    report_error(ER_OUT_OF_RESOURCES);
    return NULL;
  }

  // insert db item before all table items

  if (!m_last_db)
  {
    di->next= m_items;
    m_items= m_last_db= di;
    if (!m_last_item)
     m_last_item= m_items;
  }
  else
  {
    // since m_last_db is not NULL, m_items list can't be empty
    DBUG_ASSERT(m_items);

    di->next= m_last_db->next;
    if (m_last_item == m_last_db)
     m_last_item= di;
    m_last_db->next= di;
  }

  return di;
}

/**
  Add to archive all items belonging to a given database.
 */
int Backup_info::add_db_items(Db_item &db)
{
  DBUG_ASSERT(m_state == INIT);  // should be called before structure is closed
  DBUG_ASSERT(is_valid());       // should be valid
  DBUG_ASSERT(i_s_tables->file); // i_s_tables should be opened

  // add all tables from db.

  ::handler *ha= i_s_tables->file;

  /*
    If error debugging is switched on (see debug.h) then I_S.TABLES access
    error will be triggered when backing up database whose name starts with 'a'.
   */
  TEST_ERROR_IF(db.name().ptr()[0]=='a');

  if (ha->ha_rnd_init(TRUE) || TEST_ERROR)
  {
    report_error(ER_BACKUP_LIST_DB_TABLES,db.name().ptr());
    return -1;
  }

  int res= 0;

  while (!ha->rnd_next(i_s_tables->record[0]))
  {
    /*
      Check if it is table or view.
      
      FIXME: make it better when views are supported.
     */ 
    String type;
    
    i_s_tables->field[3]->val_str(&type);
    
    if (type == String("VIEW",&::my_charset_bin))
    {
      String name, db_name;
      
      i_s_tables->field[1]->val_str(&db_name);
      i_s_tables->field[1]->val_str(&name);
      report_error(log_level::WARNING,ER_BACKUP_SKIP_VIEW,
                   name.c_ptr(),db_name.c_ptr());
      continue;
    }
    
    const Backup_info::Table_ref t(i_s_tables);

    if (!t.hton())
    {
      Table_ref::describe_buf buf;
      report_error(ER_NO_STORAGE_ENGINE,t.describe(buf,sizeof(buf)));
      res= -2;
      break;
    }

    if (t.db() == db)
    {
      DBUG_PRINT("backup", ("Found table %s for database %s",
                             t.name().ptr(), t.db().name().ptr()));

      // add_table method selects/creates sub-image appropriate for storing given table
      Table_item *ti= add_table(db,t);
      if (!ti)
      {
        res= -3;
        break;
      }

      if (add_table_items(*ti))
      {
        res= -4;
        break;
      }
    }
  }

  ha->ha_rnd_end();

  return res;
}

/// Gets table identity from a row of I_S.TABLES table

Backup_info::Table_ref::Table_ref(TABLE *t):
  backup::Table_ref(m_db_name,m_name)
{
  String engine_name;

  t->field[1]->val_str(&m_db_name);
  t->field[2]->val_str(&m_name);
  t->field[4]->val_str(&engine_name);

  LEX_STRING name_lex;

  name_lex.str= const_cast<char*>(engine_name.ptr());
  name_lex.length= engine_name.length();

  m_hton= plugin_data(::ha_resolve_by_name(::current_thd,&name_lex),
                      handlerton*);
}

/**
  Add @c Table_item to archive's list of meta-data items.
 */

Backup_info::Table_item*
Backup_info::add_table(Db_item &db,
                       const Table_ref &t)
{
  int no= find_image(t); // Note: find_image reports errors

  /*
    If error debugging is switched on (see debug.h) then any table whose
    name starts with 'a' will trigger "no backup driver" error.
   */
  TEST_ERROR_IF(t.name().ptr()[0]=='a');

  if (no < 0 || TEST_ERROR)
    return NULL;

  /*
    If image was found then images[no] should point at the Image_info
    object
   */
  Image_info *img= images[no];
  DBUG_ASSERT(img);

  /*
    If error debugging is switched on (see debug.h) then any table whose
    name starts with 'b' will trigger error when added to backup image.
   */
  TEST_ERROR_IF(t.name().ptr()[0]=='b');

  int tno= img->tables.add(t);
  if (tno < 0 || TEST_ERROR)
  {
    Table_ref::describe_buf buf;
    report_error(ER_BACKUP_NOT_ACCEPTED,img->name(),t.describe(buf,sizeof(buf)));
    return NULL;
  }

  table_count++;

  Table_item *ti= new Table_item(*this,no,tno);

  if (!ti)
  {
    report_error(ER_OUT_OF_RESOURCES);
    return NULL;
  }

  // add to the end of items list

  ti->next= NULL;
  if (m_last_item)
   m_last_item->next= ti;
  m_last_item= ti;
  if (!m_items)
   m_items= ti;

  return ti;
}

/**
  Add to archive all items belonging to a given table.
 */
int Backup_info::add_table_items(Table_item&)
{
  // TODO: Implement when we handle per-table meta-data.
  return 0;
}

} // backup namespcae


/*************************************************
 *
 *                 RESTORE
 *
 *************************************************/

// Declarations of functions used in restore operation

namespace backup {

// defined in meta_backup.cc
bool restore_meta_data(THD*, Restore_info&, IStream&);

// defined in data_backup.cc
bool restore_table_data(THD*, Restore_info&, IStream&);

} // backup namespace


/**
  Restore items saved in backup archive.

  @pre archive info has been already read and the stream is positioned
        at archive's meta-data
*/
int mysql_restore(THD *thd, backup::Restore_info &info, backup::IStream &s)
{
  DBUG_ENTER("mysql_restore");

  size_t start_bytes= s.bytes;

  DBUG_PRINT("restore",("Restoring meta-data"));

  if (backup::restore_meta_data(thd, info, s))
    DBUG_RETURN(-1);

  DBUG_PRINT("restore",("Restoring table data"));

  if (backup::restore_table_data(thd,info,s))
    DBUG_RETURN(-2);

  DBUG_PRINT("restore",("Done."));

  info.total_size= s.bytes - start_bytes;

  DBUG_RETURN(0);
}

/*************************************************
 *
 *             BACKUP LOCATIONS
 *
 *************************************************/

namespace backup {

struct File_loc: public Location
{
  String path;

  enum_type type() const
  { return SERVER_FILE; }

  File_loc(const char *p)
  { path.append(p); }

  const char* describe()
  { return path.c_ptr(); }

};


template<class S>
S* open_stream(const Location &loc)
{
  switch (loc.type()) {

  case Location::SERVER_FILE:
  {
    const File_loc &f= static_cast<const File_loc&>(loc);
    S *s= new S(f.path);

    if (s && s->open())
      return s;

    delete s;
    return NULL;
  }

  default:
    return NULL;

  }
}

template IStream* open_stream<IStream>(const Location&);
template OStream* open_stream<OStream>(const Location&);

IStream* open_for_read(const Location &loc)
{
  return open_stream<IStream>(loc);
}


OStream* open_for_write(const Location &loc)
{
  return open_stream<OStream>(loc);
}

/**
  Find location described by a string.

  The string is taken from the "TO ..." clause of BACKUP/RESTORE commands.
  This function parses the string and creates instance of @c Location class
  describing the location or NULL if string doesn't describe any valid location.

  Currently the only supported type of location is a file on the server host.
  The string is supposed to contain a path to such file.

  @note No checks on the location are made at this stage. In particular the
  location might not physically exist. In the future methods performing such
  checks can be added to @Location class.
 */
Location*
Location::find(const LEX_STRING &where)
{
  return new File_loc(where.str);
}

} // backup namespace


/*************************************************

                 Helper functions

 *************************************************/

TABLE *create_schema_table(THD *thd, TABLE_LIST *table_list); // defined in sql_show.cc

namespace backup {

/**
  Report errors.

  Current implementation reports the last error saved in the logger if it exist.
  Otherwise it reports error given by @c error_code.
 */
int report_errors(THD *thd,int error_code, Logger &log, ...)
{
  MYSQL_ERROR *error= log.last_saved_error();

  if (error && !util::report_mysql_error(thd,error,error_code))
    if (error->code)
      error_code= error->code;
  else // there are no error information in the logger - report error_code
  {
    char buf[ERRMSGSIZE + 20];
    va_list args;
    va_start(args,log);

    my_vsnprintf(buf,sizeof(buf),ER_SAFE(error_code),args);
    my_printf_error(error_code,buf,MYF(0));

    va_end(args);
  }

  return error_code;
}


template<class Info>
static
int check_info(THD *thd, Info &info, int error_code)
{
  if (info.is_valid())
    return OK;

  return report_errors(thd,error_code,info);
}

template int check_info(THD*,Backup_info&,int);
template int check_info(THD*,Restore_info&,int);

inline
int check_info(THD *thd, Backup_info &info)
{ return check_info<Backup_info>(thd,info,ER_BACKUP_BACKUP_PREPARE); }

inline
int check_info(THD *thd, Restore_info &info)
{ return check_info<Restore_info>(thd,info,ER_BACKUP_RESTORE_PREPARE); }

/**
  Send a summary of the backup/restore operation to the client.

  The data about the operation is taken from filled @c Archive_info
  structure. Parameter @c backup determines if this was backup or
  restore operation.

  TODO: Add list of databases in the output.
*/

static
bool send_summary(THD *thd, const Archive_info &info, bool backup)
{
  Protocol   *protocol= ::current_thd->protocol; // client comms
  List<Item> field_list;                         // list of fields to send
  Item       *item;                              // field item
  char       buf[255];                           // buffer for data
  String     op_str;                             // operations string
  String     print_str;                          // string to print

  DBUG_ENTER("backup::send_summary");

  DBUG_PRINT(backup?"backup":"restore", ("sending summary"));

  op_str.length(0);
  if (backup)
    op_str.append("Backup Summary");
  else
    op_str.append("Restore Summary");


  field_list.push_back(item= new Item_empty_string(op_str.c_ptr(),255)); // TODO: use varchar field
  item->maybe_null= TRUE;
  protocol->send_fields(&field_list, Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF);

  /*
    Loop through the catalog and list the contents by
    database.

    Enable this code when functions for browsing archive contents are
    implemented.

    List<schema_ref> cat1= *catalog;
    List_iterator<schema_ref> cat(cat1);
    schema_ref *tmp;

    for (Archive_info::Db_iterator it(info); it; ++it)
    while ((tmp= cat++))
    {
      uint howmuch= 0;

      for (Archive_info::DItem_iterator dit(it); dit; ++dit)
        howmuch++;

      my_snprintf(buf,sizeof(buf),"%s %3u %s in database %s.",
                  backup? "Backed up" : "Restored",
                  howmuch, howmuch != 1 ? "tables" : " table",
                  it->name().c_ptr());

      protocol->prepare_for_resend();
      protocol->store(buf,system_charset_info);
      protocol->write();
    }

    my_snprintf(buf,sizeof(buf)," ");
    protocol->prepare_for_resend();
    protocol->store(buf,system_charset_info);
    protocol->write();
  */

  my_snprintf(buf,sizeof(buf)," header     = %-8lu bytes",(unsigned long)info.header_size);
  protocol->prepare_for_resend();
  protocol->store(buf,system_charset_info);
  protocol->write();

  my_snprintf(buf,sizeof(buf)," meta-data  = %-8lu bytes",(unsigned long)info.meta_size);
  protocol->prepare_for_resend();
  protocol->store(buf,system_charset_info);
  protocol->write();

  my_snprintf(buf,sizeof(buf)," data       = %-8lu bytes",(unsigned long)info.data_size);
  protocol->prepare_for_resend();
  protocol->store(buf,system_charset_info);
  protocol->write();

  my_snprintf(buf,sizeof(buf),"              --------------");
  protocol->prepare_for_resend();
  protocol->store(buf,system_charset_info);
  protocol->write();

  my_snprintf(buf,sizeof(buf)," total        %-8lu bytes", (unsigned long)info.total_size);
  protocol->prepare_for_resend();
  protocol->store(buf,system_charset_info);
  protocol->write();

  send_eof(thd);
  DBUG_RETURN(0);
}

inline
bool send_summary(THD *thd, const Backup_info &info)
{ return send_summary(thd,info,TRUE); }

inline
bool send_summary(THD *thd, const Restore_info &info)
{ return send_summary(thd,info,FALSE); }


/// Open given table in @c INFORMATION_SCHEMA database.
TABLE* get_schema_table(THD *thd, ST_SCHEMA_TABLE *st)
{
  TABLE *t;
  TABLE_LIST arg;

  bzero( &arg, sizeof(TABLE_LIST) );

  // set context for create_schema_table call
  arg.schema_table= st;
  arg.alias=        NULL;
  arg.select_lex=   NULL;

  t= ::create_schema_table(thd,&arg); // Note: callers must free t.

  if( !t ) return NULL; // error!

  /*
   Temporarily set thd->lex->wild to NULL to keep st->fill_table
   happy.
  */
  String *wild= thd->lex->wild;
  ::enum_sql_command command= thd->lex->sql_command;
  
  thd->lex->wild = NULL;
  thd->lex->sql_command = enum_sql_command(0);

  /* context for fill_table */
  arg.table= t;

  /*
    Question: is it correct to fill I_S table each time we use it or should it
    be filled only once?
   */
  st->fill_table(thd,&arg,NULL);  // NULL = no select condition

  // undo changes to thd->lex
  thd->lex->wild= wild;
  thd->lex->sql_command= command;

  return t;
}

/// Build linked @c TABLE_LIST list from a list stored in @c Table_list object.

/*
  FIXME: build list with the same order as in input
  Actually, should work fine with reversed list as long as we use the reversed
  list both in table writing and reading.
 */
TABLE_LIST *build_table_list(const Table_list &tables, thr_lock_type lock)
{
  TABLE_LIST *tl= NULL;

  for( uint tno=0; tno < tables.count() ; tno++ )
  {
    TABLE_LIST *ptr= (TABLE_LIST*)sql_alloc(sizeof(TABLE_LIST));
    DBUG_ASSERT(ptr);  // FIXME: report error instead
    bzero(ptr,sizeof(TABLE_LIST));

    Table_ref tbl= tables[tno];

    ptr->alias= ptr->table_name= const_cast<char*>(tbl.name().ptr());
    ptr->db= const_cast<char*>(tbl.db().name().ptr());
    ptr->lock_type= lock;

    // and add it to the list

    ptr->next_global= ptr->next_local=
      ptr->next_name_resolution_table= tl;
    tl= ptr;
    tl->table= ptr->table;
  }

  return tl;
}


} // backup namespace


/****************************

  Old code to be integrated into the current framework.

 ****************************/

#if FALSE

/*
  Get the metadata for the backup file specified and display
  it to the client interface.

  SYNOPSIS
    show_archive_catalog()
    THD *thd     - The current thread instance.
    Location loc - The name of the archive with path.

  DESCRIPTION
    This procedure opens the archive and extracts the metadata
    for the databases and tables stored in the archive. A simple
    list of the tables by database is sent to the client.

  RETURNS
    0  - no errors.
    -1 - database cannot be read or doesn't exist.
*/
int mysql_show_archive(THD *thd, const backup::Location &loc)
{
  Protocol   *protocol= thd->protocol;
  List<Item> field_list;
  Item       *item;

  DBUG_ENTER("backup::show_archive_catalog");
  field_list.push_back(item= new Item_empty_string("Database",255));
  field_list.push_back(item= new Item_empty_string("Table",255));
  field_list.push_back(item= new Item_empty_string("Metadata",255));
  item->maybe_null= TRUE;
  protocol->send_fields(&field_list, Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF);
  backup::Restore_info   info;

  backup::IStream *backup_str= backup::open_for_read(loc);

  if (!backup_str)
    DBUG_RETURN(-1);

  // for debug purposes -- grep for 'stream: ' in server trace
  backup::dump_stream(*backup_str);

  backup_str->rewind();

  bool res= backup::read_header(*backup_str,info);
  DBUG_ASSERT(res);

  res= backup::read_catalog(*backup_str,info);
  DBUG_ASSERT(res);

  backup_str->close();

  // send catalog to client

  /*
    Loop through the catalog and list the contents by
    database.
  */
  List<backup::schema_ref> cat1= info.catalog;
  List_iterator<backup::schema_ref> catalog(cat1);
  backup::schema_ref *tmp;
  while ((tmp= catalog++))
  {
    List_iterator<backup::table_info> tbls(tmp->tables);
    backup::table_info *t;
    while ((t= tbls++))
    {
      /*
        Send the following data:
          database name (from tmp->db_name)
          table name (from t->table_name)
          CREATE TABLE SQL for the table (first string in table_metadata)
     */
      protocol->prepare_for_resend();
      protocol->store(tmp->db_name.c_ptr(),system_charset_info);
      protocol->store(t->table_name.c_ptr(),system_charset_info);
      List_iterator<String> tbl_meta(t->table_metadata);
      String *tmp= tbl_meta++;  //just get the first string which is create
      protocol->store(tmp->c_ptr(),system_charset_info);
      protocol->write();
    }
  }
  send_eof(thd);
  DBUG_RETURN(backup::OK);
}

#endif

#if FALSE

bool collect_tables_to_backup(THD *,Backup_info &);
bool collect_databases_to_backup(THD *thd, Backup_info &info);


// Collect tables to be backed-up and fill Backup_info structure describing backup image.
// Currently we try to backup all tables in 'test' database (but only tables for which
// appropriate backup engine is found will be included in the image).

bool collect_tables_to_backup(THD *thd,Backup_info &info)
{
  LEX_STRING *tmp;
  schema_ref *cat;
  String     db_name;

  DBUG_ENTER("backup::collect_tables_to_backup");

  // open and scan I_S.TABLES table

  TABLE *i_s_tables = get_schema_table(thd, ::get_schema_table(SCH_TABLES) );

  if( !i_s_tables )
  {
    DBUG_PRINT("backup",("get_schema_table returned NULL!"));
    DBUG_ERROR(ERROR);
  }

  ::handler *ha = i_s_tables->file;

  // TODO: locking

  /*
    Build catalog for writing...
      For each database in the db_list,
        create a new catalog struct
        fill with tables and their metadata
        add catalog to list
  */
  List_iterator<LEX_STRING> databases(*info.db_list);

  table_info *ti;
  while ((tmp= databases++))
  {
    /*
      Save this database in the catalog along with metadata
    */
    cat= new schema_ref();          // create a new catalog struct
    //cat->db_name= new (current_thd->mem_root) String();
    cat->db_name.length(0);
    cat->db_name.append(tmp->str); // save the database name

    DBUG_PRINT("backup", ("Creating database metadata."));
    get_db_metadata(thd, cat->db_name.c_ptr(), &cat->db_metadata);
    DBUG_PRINT("backup", ("Database metadata created."));

    ha->ha_rnd_init(TRUE);

    // TODO: use conditions, do it properly
    while( ! ha->rnd_next(i_s_tables->record[0]) )
    {
      Table tbl(i_s_tables);
      db_name= tbl.db().name();
      if (tbl && !my_strcasecmp(system_charset_info,
         db_name.c_ptr(), tmp->str) &&
         my_strcasecmp(system_charset_info,
         db_name.c_ptr(), "information_schema") &&
         my_strcasecmp(system_charset_info,
         db_name.c_ptr(), "mysql"))
      {
        DBUG_PRINT("backup", ("Found table %s for database %s",
                               tbl.name().ptr(), tmp->str));
        // Backup_info::add method selects/creates sub-image appropriate for storing given table
        info.add(tbl,tbl.hton());
        /*
          Add the table to this database's table list
        */
        ti= new table_info();
        ti->table_name.length(0);
        ti->table_name.append(tbl.name());
        /*
          Get the metadata for the table
        */
        TABLE_LIST *ptr=
          build_table_list_str((char *)tbl.name().ptr(), db_name.c_ptr(), TL_READ);
        get_table_metadata(thd, ptr, &ti->table_metadata);
        /*
          Save the table_info to the tables list for this database
        */
        cat->tables.push_back(ti);
      }
    }
    info.catalog.push_back(cat); //add this catalog to the list of catalogs
  }

  DBUG_PRINT("backup", ("No more tables in I_S"));

  ha->ha_rnd_end();

  ::free_tmp_table(thd,i_s_tables);

  DBUG_RETURN(TRUE);
}

bool collect_databases_to_backup(THD *thd, Backup_info &info)
{
  LEX_STRING *tmp;
  String     db_name;

  DBUG_ENTER("backup::collect_databases_to_backup");

  // open and scan I_S.TABLES table

  TABLE *db_table = get_schema_table(thd, ::get_schema_table(SCH_SCHEMATA));

  if (!db_table)
  {
    DBUG_PRINT("backup",("get_schema_table returned NULL!"));
    DBUG_ERROR(ERROR);
  }

  ::handler *ha = db_table->file;

  // TODO: locking

  ha->ha_rnd_init(TRUE);

  // TODO: use conditions, do it properly
  while (!ha->rnd_next(db_table->record[0]))
  {
    db_name.length(0);
    db_table->field[1]->val_str(&db_name);
    if (my_strcasecmp(system_charset_info,
       db_name.c_ptr(), "information_schema") &&
       my_strcasecmp(system_charset_info,
       db_name.c_ptr(), "mysql"))
    {
      DBUG_PRINT("backup", ("Found database %s", db_name.ptr()));
      tmp= (LEX_STRING *)thd->alloc(sizeof(LEX_STRING));
      tmp->str= my_malloc(MAX_ALIAS_NAME, MYF(MY_WME));
      strcpy(tmp->str, db_name.c_ptr());
      info.db_list->push_back(tmp);
    }
  }

  DBUG_PRINT("backup", ("No more databases in I_S"));

  ha->ha_rnd_end();

  db_table->file->close();
  DBUG_RETURN(TRUE);
}

#endif
