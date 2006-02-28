/* Copyright (C) 2004-2005 MySQL AB

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

#include "event_priv.h"
#include "event.h"
#include "sp.h"



extern int yyparse(void *thd);

/*
  Init all member variables

  SYNOPSIS
    Event_timed::init()
*/

void
Event_timed::init()
{
  DBUG_ENTER("Event_timed::init");

  dbname.str= name.str= body.str= comment.str= 0;
  dbname.length= name.length= body.length= comment.length= 0;

  set_zero_time(&starts, MYSQL_TIMESTAMP_DATETIME);
  set_zero_time(&ends, MYSQL_TIMESTAMP_DATETIME);
  set_zero_time(&execute_at, MYSQL_TIMESTAMP_DATETIME);
  set_zero_time(&last_executed, MYSQL_TIMESTAMP_DATETIME);
  starts_null= ends_null= execute_at_null= TRUE;

  definer_user.str= definer_host.str= 0;
  definer_user.length= definer_host.length= 0;

  sql_mode= 0;

  DBUG_VOID_RETURN;
}


/*
  Set a name of the event

  SYNOPSIS
    Event_timed::init_name()
      thd   THD
      spn   the name extracted in the parser
*/

void
Event_timed::init_name(THD *thd, sp_name *spn)
{
  DBUG_ENTER("Event_timed::init_name");
  uint n;                                      /* Counter for nul trimming */
  /* During parsing, we must use thd->mem_root */
  MEM_ROOT *root= thd->mem_root;

  /* We have to copy strings to get them into the right memroot */
  if (spn)
  {
    dbname.length= spn->m_db.length;
    if (spn->m_db.length == 0)
      dbname.str= NULL;
    else
      dbname.str= strmake_root(root, spn->m_db.str, spn->m_db.length);
    name.length= spn->m_name.length;
    name.str= strmake_root(root, spn->m_name.str, spn->m_name.length);

    if (spn->m_qname.length == 0)
      spn->init_qname(thd);
  }
  else if (thd->db)
  {
    dbname.length= thd->db_length;
    dbname.str= strmake_root(root, thd->db, dbname.length);
  }

  DBUG_PRINT("dbname", ("len=%d db=%s",dbname.length, dbname.str));
  DBUG_PRINT("name", ("len=%d name=%s",name.length, name.str));

  DBUG_VOID_RETURN;
}


/*
  Set body of the event - what should be executed.

  SYNOPSIS
    Event_timed::init_body()
      thd   THD

  NOTE
    The body is extracted by copying all data between the
    start of the body set by another method and the current pointer in Lex.
*/

void
Event_timed::init_body(THD *thd)
{
  DBUG_ENTER("Event_timed::init_body");
  DBUG_PRINT("info", ("body=[%s] body_begin=0x%ld end=0x%ld", body_begin,
             body_begin, thd->lex->ptr));

  body.length= thd->lex->ptr - body_begin;
  /* Trim nuls at the end */
  while (body.length && body_begin[body.length-1] == '\0')
    body.length--;

  /* the first is always whitespace which I cannot skip in the parser */
  while (my_isspace(thd->variables.character_set_client, *body_begin))
  {
    ++body_begin;
    --body.length;
  }
  body.str= strmake_root(thd->mem_root, (char *)body_begin, body.length);

  DBUG_VOID_RETURN;
}


/*
  Set time for execution for one time events.

  SYNOPSIS
    Event_timed::init_execute_at()
      expr   when (datetime)

  RETURN VALUE
    0                  OK
    EVEX_PARSE_ERROR   fix_fields failed
    EVEX_BAD_PARAMS    datetime is in the past
    ER_WRONG_VALUE     wrong value for execute at
*/

int
Event_timed::init_execute_at(THD *thd, Item *expr)
{
  my_bool not_used;
  TIME ltime;
  my_time_t my_time_tmp;

  TIME time_tmp;
  DBUG_ENTER("Event_timed::init_execute_at");

  if (expr->fix_fields(thd, &expr))
    DBUG_RETURN(EVEX_PARSE_ERROR);
  
  /* no starts and/or ends in case of execute_at */
  DBUG_PRINT("info", ("starts_null && ends_null should be 1 is %d",
                      (starts_null && ends_null)));
  DBUG_ASSERT(starts_null && ends_null);
  
  /* let's check whether time is in the past */
  thd->variables.time_zone->gmt_sec_to_TIME(&time_tmp, 
                                            (my_time_t) thd->query_start());

  if ((not_used= expr->get_date(&ltime, TIME_NO_ZERO_DATE)))
    DBUG_RETURN(ER_WRONG_VALUE);

  if (TIME_to_ulonglong_datetime(&ltime) <
      TIME_to_ulonglong_datetime(&time_tmp))
    DBUG_RETURN(EVEX_BAD_PARAMS);


  /*
    This may result in a 1970-01-01 date if ltime is > 2037-xx-xx.
    CONVERT_TZ has similar problem.
  */
  my_tz_UTC->gmt_sec_to_TIME(&ltime, TIME_to_timestamp(thd,&ltime, &not_used));

  execute_at_null= FALSE;
  execute_at= ltime;
  DBUG_RETURN(0);
}


/*
  Set time for execution for transient events.

  SYNOPSIS
    Event_timed::init_interval()
      expr      how much?
      new_interval  what is the interval

  RETURNS
    0                  OK
    EVEX_PARSE_ERROR   fix_fields failed
    EVEX_BAD_PARAMS    Interval is not positive
    EVEX_MICROSECOND_UNSUP  Microseconds are not supported.
*/

int
Event_timed::init_interval(THD *thd, Item *expr, interval_type new_interval)
{
  longlong tmp;
  String value;
  INTERVAL interval;

  DBUG_ENTER("Event_timed::init_interval");

  if (expr->fix_fields(thd, &expr))
    DBUG_RETURN(EVEX_PARSE_ERROR);

  value.alloc(MAX_DATETIME_FULL_WIDTH*MY_CHARSET_BIN_MB_MAXLEN);
  if (get_interval_value(expr, new_interval, &value, &interval))
    DBUG_RETURN(EVEX_PARSE_ERROR);

  expression= 0;

  switch (new_interval) {
  case INTERVAL_YEAR:
    expression= interval.year;
    break;
  case INTERVAL_QUARTER:
  case INTERVAL_MONTH:
    expression= interval.month;
    break;
  case INTERVAL_WEEK:
  case INTERVAL_DAY:
    expression= interval.day;
    break;
  case INTERVAL_HOUR:
    expression= interval.hour;
    break;
  case INTERVAL_MINUTE:
    expression= interval.minute;
    break;
  case INTERVAL_SECOND:
    expression= interval.second;
    break;
  case INTERVAL_YEAR_MONTH:                     // Allow YEAR-MONTH YYYYYMM
    expression= interval.year* 12 + interval.month;
    break;
  case INTERVAL_DAY_HOUR:
    expression= interval.day* 24 + interval.hour;
    break;
  case INTERVAL_DAY_MINUTE:
    expression= (interval.day* 24 + interval.hour) * 60 + interval.minute;
    break;
  case INTERVAL_HOUR_SECOND: /* day is anyway 0 */
  case INTERVAL_DAY_SECOND:
    /* DAY_SECOND having problems because of leap seconds? */
    expression= ((interval.day* 24 + interval.hour) * 60 + interval.minute)*60
                 + interval.second;
    break;
  case INTERVAL_MINUTE_MICROSECOND: /* day and hour are 0 */
  case INTERVAL_HOUR_MICROSECOND:   /* day is anyway 0    */
  case INTERVAL_DAY_MICROSECOND:
    DBUG_RETURN(EVEX_MICROSECOND_UNSUP);
    expression= ((((interval.day*24) + interval.hour)*60+interval.minute)*60 +
                interval.second) * 1000000L + interval.second_part;
    break;
  case INTERVAL_HOUR_MINUTE:
    expression= interval.hour * 60 + interval.minute;
    break;
  case INTERVAL_MINUTE_SECOND:
    expression= interval.minute * 60 + interval.second;
    break;
  case INTERVAL_SECOND_MICROSECOND:
    DBUG_RETURN(EVEX_MICROSECOND_UNSUP);
    expression= interval.second * 1000000L + interval.second_part;
    break;
  case INTERVAL_MICROSECOND:
    DBUG_RETURN(EVEX_MICROSECOND_UNSUP);  
  }
  if (interval.neg || expression > EVEX_MAX_INTERVAL_VALUE)
    DBUG_RETURN(EVEX_BAD_PARAMS);

  this->interval= new_interval;
  DBUG_RETURN(0);
}


/*
  Set activation time.

  SYNOPSIS
    Event_timed::init_starts()
    expr      how much?
    interval  what is the interval

  NOTES
    Note that activation time is not execution time.
    EVERY 5 MINUTE STARTS "2004-12-12 10:00:00" means that
    the event will be executed every 5 minutes but this will
    start at the date shown above. Expressions are possible :
    DATE_ADD(NOW(), INTERVAL 1 DAY)  -- start tommorow at
    same time.

  RETURNS
    0                  OK
    EVEX_PARSE_ERROR   fix_fields failed
*/

int
Event_timed::init_starts(THD *thd, Item *new_starts)
{
  my_bool not_used;
  TIME ltime, time_tmp;

  DBUG_ENTER("Event_timed::init_starts");

  if (new_starts->fix_fields(thd, &new_starts))
    DBUG_RETURN(EVEX_PARSE_ERROR);

  if ((not_used= new_starts->get_date(&ltime, TIME_NO_ZERO_DATE)))
    DBUG_RETURN(EVEX_BAD_PARAMS);

  /* Let's check whether time is in the past */
  thd->variables.time_zone->gmt_sec_to_TIME(&time_tmp,
                                            (my_time_t) thd->query_start());

  DBUG_PRINT("info",("now   =%lld", TIME_to_ulonglong_datetime(&time_tmp)));
  DBUG_PRINT("info",("starts=%lld", TIME_to_ulonglong_datetime(&ltime)));
  if (TIME_to_ulonglong_datetime(&ltime) <
      TIME_to_ulonglong_datetime(&time_tmp))
    DBUG_RETURN(EVEX_BAD_PARAMS);

  /*
    This may result in a 1970-01-01 date if ltime is > 2037-xx-xx
    CONVERT_TZ has similar problem
  */
  my_tz_UTC->gmt_sec_to_TIME(&ltime, TIME_to_timestamp(thd, &ltime, &not_used));

  starts= ltime;
  starts_null= FALSE;
  DBUG_RETURN(0);
}


/*
  Set deactivation time.

  SYNOPSIS
    Event_timed::init_ends()
      thd       THD
      new_ends  when?

  NOTES
    Note that activation time is not execution time.
    EVERY 5 MINUTE ENDS "2004-12-12 10:00:00" means that
    the event will be executed every 5 minutes but this will
    end at the date shown above. Expressions are possible :
    DATE_ADD(NOW(), INTERVAL 1 DAY)  -- end tommorow at
    same time.

  RETURNS
    0                  OK
    EVEX_PARSE_ERROR   fix_fields failed
    EVEX_BAD_PARAMS    ENDS before STARTS
*/

int
Event_timed::init_ends(THD *thd, Item *new_ends)
{
  TIME ltime, ltime_now;
  my_bool not_used;

  DBUG_ENTER("Event_timed::init_ends");

  if (new_ends->fix_fields(thd, &new_ends))
    DBUG_RETURN(EVEX_PARSE_ERROR);

  DBUG_PRINT("info", ("convert to TIME"));
  if ((not_used= new_ends->get_date(&ltime, TIME_NO_ZERO_DATE)))
    DBUG_RETURN(EVEX_BAD_PARAMS);

  /*
    This may result in a 1970-01-01 date if ltime is > 2037-xx-xx ?
    CONVERT_TZ has similar problem ?
  */
  DBUG_PRINT("info", ("get the UTC time"));
  my_tz_UTC->gmt_sec_to_TIME(&ltime, TIME_to_timestamp(thd, &ltime, &not_used));

  /* Check whether ends is after starts */
  DBUG_PRINT("info", ("ENDS after STARTS?"));
  if (!starts_null && my_time_compare(&starts, &ltime) != -1)
    DBUG_RETURN(EVEX_BAD_PARAMS);

  /*
    The parser forces starts to be provided but one day STARTS could be
    set before NOW() and in this case the following check should be done.
    Check whether ENDS is not in the past.
  */
  DBUG_PRINT("info", ("ENDS after NOW?"));
  my_tz_UTC->gmt_sec_to_TIME(&ltime_now, thd->query_start());
  if (my_time_compare(&ltime_now, &ltime) == 1)
    DBUG_RETURN(EVEX_BAD_PARAMS);

  ends= ltime;
  ends_null= FALSE;
  DBUG_RETURN(0);
}


/*
  Sets comment.

  SYNOPSIS
    Event_timed::init_comment()
      thd      THD - used for memory allocation
      comment  the string.
*/

void
Event_timed::init_comment(THD *thd, LEX_STRING *set_comment)
{
  DBUG_ENTER("Event_timed::init_comment");

  comment.str= strmake_root(thd->mem_root, set_comment->str,
                            comment.length= set_comment->length);

  DBUG_VOID_RETURN;
}


/*
  Inits definer (definer_user and definer_host) during parsing.

  SYNOPSIS
    Event_timed::init_definer()
*/

int
Event_timed::init_definer(THD *thd)
{
  DBUG_ENTER("Event_timed::init_definer");

  DBUG_PRINT("info",("init definer_user thd->mem_root=0x%lx "
                     "thd->sec_ctx->priv_user=0x%lx", thd->mem_root,
                     thd->security_ctx->priv_user));
  definer_user.str= strdup_root(thd->mem_root, thd->security_ctx->priv_user);
  definer_user.length= strlen(thd->security_ctx->priv_user);

  DBUG_PRINT("info",("init definer_host thd->s_c->priv_host=0x%lx",
                     thd->security_ctx->priv_host));
  definer_host.str= strdup_root(thd->mem_root, thd->security_ctx->priv_host);
  definer_host.length= strlen(thd->security_ctx->priv_host);

  DBUG_PRINT("info",("init definer as whole"));
  definer.length= definer_user.length + definer_host.length + 1;
  definer.str= alloc_root(thd->mem_root, definer.length + 1);

  DBUG_PRINT("info",("copy the user"));
  memcpy(definer.str, definer_user.str, definer_user.length);
  definer.str[definer_user.length]= '@';

  DBUG_PRINT("info",("copy the host"));
  memcpy(definer.str + definer_user.length + 1, definer_host.str,
         definer_host.length);
  definer.str[definer.length]= '\0';
  DBUG_PRINT("info",("definer initted"));

  DBUG_RETURN(0);
}


/*
  Loads an event from a row from mysql.event

  SYNOPSIS
    Event_timed::load_from_row(MEM_ROOT *mem_root, TABLE *table)

  NOTES
    This method is silent on errors and should behave like that. Callers
    should handle throwing of error messages. The reason is that the class
    should not know about how to deal with communication.
*/

int
Event_timed::load_from_row(MEM_ROOT *mem_root, TABLE *table)
{
  longlong created;
  longlong modified;
  char *ptr;
  Event_timed *et;
  uint len;
  bool res1, res2;

  DBUG_ENTER("Event_timed::load_from_row");

  if (!table)
    goto error;

  et= this;

  if (table->s->fields != EVEX_FIELD_COUNT)
    goto error;

  if ((et->dbname.str= get_field(mem_root,
                          table->field[EVEX_FIELD_DB])) == NULL)
    goto error;

  et->dbname.length= strlen(et->dbname.str);

  if ((et->name.str= get_field(mem_root,
                          table->field[EVEX_FIELD_NAME])) == NULL)
    goto error;

  et->name.length= strlen(et->name.str);

  if ((et->body.str= get_field(mem_root,
                          table->field[EVEX_FIELD_BODY])) == NULL)
    goto error;

  et->body.length= strlen(et->body.str);

  if ((et->definer.str= get_field(mem_root,
                          table->field[EVEX_FIELD_DEFINER])) == NullS)
    goto error;
  et->definer.length= strlen(et->definer.str);

  ptr= strchr(et->definer.str, '@');

  if (! ptr)
    ptr= et->definer.str;

  len= ptr - et->definer.str;

  et->definer_user.str= strmake_root(mem_root, et->definer.str, len);
  et->definer_user.length= len;
  len= et->definer.length - len - 1;            //1 is because of @
  et->definer_host.str= strmake_root(mem_root, ptr + 1, len);/* 1:because of @*/
  et->definer_host.length= len;
  
  et->starts_null= table->field[EVEX_FIELD_STARTS]->is_null();
  res1= table->field[EVEX_FIELD_STARTS]->get_date(&et->starts,TIME_NO_ZERO_DATE);

  et->ends_null= table->field[EVEX_FIELD_ENDS]->is_null();
  res2= table->field[EVEX_FIELD_ENDS]->get_date(&et->ends, TIME_NO_ZERO_DATE);
  
  if (!table->field[EVEX_FIELD_INTERVAL_EXPR]->is_null())
    et->expression= table->field[EVEX_FIELD_INTERVAL_EXPR]->val_int();
  else
    et->expression= 0;
  /*
    If res1 and res2 are true then both fields are empty.
    Hence if EVEX_FIELD_EXECUTE_AT is empty there is an error.
  */
  et->execute_at_null= table->field[EVEX_FIELD_EXECUTE_AT]->is_null();
  DBUG_ASSERT(!(et->starts_null && et->ends_null && !et->expression &&
              et->execute_at_null));
  if (!et->expression &&
      table->field[EVEX_FIELD_EXECUTE_AT]->get_date(&et->execute_at,
                                                     TIME_NO_ZERO_DATE))
    goto error;

  /*
    In DB the values start from 1 but enum interval_type starts
    from 0
  */
  if (!table->field[EVEX_FIELD_TRANSIENT_INTERVAL]->is_null())
    et->interval= (interval_type)
       ((ulonglong) table->field[EVEX_FIELD_TRANSIENT_INTERVAL]->val_int() - 1);
  else
    et->interval= (interval_type) 0;

  et->modified= table->field[EVEX_FIELD_CREATED]->val_int();
  et->created= table->field[EVEX_FIELD_MODIFIED]->val_int();

  /*
    ToDo Andrey : Ask PeterG & Serg what to do in this case.
                  Whether on load last_executed_at should be loaded
                  or it must be 0ed. If last_executed_at is loaded
                  then an event can be scheduled for execution
                  instantly. Let's say an event has to be executed
                  every 15 mins. The server has been stopped for
                  more than this time and then started. If L_E_AT
                  is loaded from DB, execution at L_E_AT+15min
                  will be scheduled. However this time is in the past.
                  Hence immediate execution. Due to patch of
                  ::mark_last_executed() last_executed gets time_now
                  and not execute_at. If not like this a big
                  queue can be scheduled for times which are still in
                  the past (2, 3 and more executions which will be
                  consequent).
  */
  set_zero_time(&last_executed, MYSQL_TIMESTAMP_DATETIME);
#ifdef ANDREY_0
  table->field[EVEX_FIELD_LAST_EXECUTED]->
                     get_date(&et->last_executed, TIME_NO_ZERO_DATE);
#endif
  last_executed_changed= false;

  /* ToDo : Andrey . Find a way not to allocate ptr on event_mem_root */
  if ((ptr= get_field(mem_root, table->field[EVEX_FIELD_STATUS])) == NullS)
    goto error;

  DBUG_PRINT("load_from_row", ("Event [%s] is [%s]", et->name.str, ptr));
  et->status= (ptr[0]=='E'? MYSQL_EVENT_ENABLED:MYSQL_EVENT_DISABLED);

  /* ToDo : Andrey . Find a way not to allocate ptr on event_mem_root */
  if ((ptr= get_field(mem_root,
                  table->field[EVEX_FIELD_ON_COMPLETION])) == NullS)
    goto error;

  et->on_completion= (ptr[0]=='D'? MYSQL_EVENT_ON_COMPLETION_DROP:
                                   MYSQL_EVENT_ON_COMPLETION_PRESERVE);

  et->comment.str= get_field(mem_root, table->field[EVEX_FIELD_COMMENT]);
  if (et->comment.str != NullS)
    et->comment.length= strlen(et->comment.str);
  else
    et->comment.length= 0;
    

  et->sql_mode= (ulong) table->field[EVEX_FIELD_SQL_MODE]->val_int();

  DBUG_RETURN(0);
error:
  DBUG_RETURN(EVEX_GET_FIELD_FAILED);
}


/*
  Computes the sum of a timestamp plus interval

  SYNOPSIS
    get_next_time(TIME *start, int interval_value, interval_type interval)
      next          the sum
      start         add interval_value to this time
      i_value       quantity of time type interval to add
      i_type        type of interval to add (SECOND, MINUTE, HOUR, WEEK ...)
*/

static
bool get_next_time(TIME *next, TIME *start, int i_value, interval_type i_type)
{
  bool ret;
  INTERVAL interval;
  TIME tmp;

  bzero(&interval, sizeof(interval));

  switch (i_type) {
  case INTERVAL_YEAR:
    interval.year= (ulong) i_value;
    break;
  case INTERVAL_QUARTER:
    interval.month= (ulong)(i_value*3);
    break;
  case INTERVAL_YEAR_MONTH:
  case INTERVAL_MONTH:
    interval.month= (ulong) i_value;
    break;
  case INTERVAL_WEEK:
    interval.day= (ulong)(i_value*7);
    break;
  case INTERVAL_DAY:
    interval.day= (ulong) i_value;
    break;
  case INTERVAL_DAY_HOUR:
  case INTERVAL_HOUR:
    interval.hour= (ulong) i_value;
    break;
  case INTERVAL_DAY_MINUTE:
  case INTERVAL_HOUR_MINUTE:
  case INTERVAL_MINUTE:
    interval.minute=i_value;
    break;
  case INTERVAL_DAY_SECOND:
  case INTERVAL_HOUR_SECOND:
  case INTERVAL_MINUTE_SECOND:
  case INTERVAL_SECOND:
    interval.second=i_value;
    break;
  case INTERVAL_DAY_MICROSECOND:
  case INTERVAL_HOUR_MICROSECOND:
  case INTERVAL_MINUTE_MICROSECOND:
  case INTERVAL_SECOND_MICROSECOND:
  case INTERVAL_MICROSECOND:
    interval.second_part=i_value;
    break;
  }
  tmp= *start;
  if (!(ret= date_add_interval(&tmp, i_type, interval)))
    *next= tmp;

  return ret;
}


/*
  Computes next execution time.

  SYNOPSIS
    Event_timed::compute_next_execution_time()

  NOTES
    The time is set in execute_at, if no more executions the latter is set to
    0000-00-00.
*/

bool
Event_timed::compute_next_execution_time()
{
  TIME time_now;
  my_time_t now;
  int tmp;

  DBUG_ENTER("Event_timed::compute_next_execution_time");

  if (status == MYSQL_EVENT_DISABLED)
  {
    DBUG_PRINT("compute_next_execution_time",
                  ("Event %s is DISABLED", name.str));
    goto ret;
  }
  /* If one-time, no need to do computation */
  if (!expression)
  {
    /* Let's check whether it was executed */
    if (last_executed.year)
    {
      DBUG_PRINT("info",("One-time event %s.%s of was already executed",
                         dbname.str, name.str, definer.str));
      dropped= (on_completion == MYSQL_EVENT_ON_COMPLETION_DROP);
      DBUG_PRINT("info",("One-time event will be dropped=%d.", dropped));

      status= MYSQL_EVENT_DISABLED;
      status_changed= true;
    }
    goto ret;
  }
  time((time_t *)&now);
  my_tz_UTC->gmt_sec_to_TIME(&time_now, now);

#ifdef ANDREY_0
  sql_print_information("[%s.%s]", dbname.str, name.str);
  sql_print_information("time_now : [%d-%d-%d %d:%d:%d ]",
                         time_now.year, time_now.month, time_now.day,
                         time_now.hour, time_now.minute, time_now.second);
  sql_print_information("starts : [%d-%d-%d %d:%d:%d ]", starts.year,
                        starts.month, starts.day, starts.hour,
                        starts.minute, starts.second);
  sql_print_information("ends   : [%d-%d-%d %d:%d:%d ]", ends.year,
                        ends.month, ends.day, ends.hour,
                        ends.minute, ends.second);
  sql_print_information("m_last_ex: [%d-%d-%d %d:%d:%d ]", last_executed.year,
                        last_executed.month, last_executed.day,
                        last_executed.hour, last_executed.minute,
                        last_executed.second);
#endif

  /* if time_now is after ends don't execute anymore */
  if (!ends_null && (tmp= my_time_compare(&ends, &time_now)) == -1)
  {
    /* time_now is after ends. don't execute anymore */
    set_zero_time(&execute_at, MYSQL_TIMESTAMP_DATETIME);
    execute_at_null= TRUE;
    if (on_completion == MYSQL_EVENT_ON_COMPLETION_DROP)
      dropped= true;
    status= MYSQL_EVENT_DISABLED;
    status_changed= true;

    goto ret;
  }

  /*
    Here time_now is before or equals ends if the latter is set.
    Let's check whether time_now is before starts.
    If so schedule for starts.
  */
  if (!starts_null && (tmp= my_time_compare(&time_now, &starts)) < 1)
  {
    if (tmp == 0 && my_time_compare(&starts, &last_executed) == 0)
    {
      /*
        time_now = starts = last_executed
        do nothing or we will schedule for second time execution at starts.
      */
    }
    else
    {
      /*
        starts is in the future
        time_now before starts. Scheduling for starts
      */
      execute_at= starts;
      execute_at_null= FALSE;
      goto ret;
    }
  }

  if (!starts_null && !ends_null)
  {
    /*
      Both starts and m_ends are set and time_now is between them (incl.)
      If last_executed is set then increase with m_expression. The new TIME is
      after m_ends set execute_at to 0. And check for on_completion
      If not set then schedule for now.
    */
    if (!last_executed.year)
    {
      execute_at= time_now;
      execute_at_null= FALSE;
    }
    else
    {
      TIME next_exec;

      if (get_next_time(&next_exec, &last_executed, expression, interval))
        goto err;

      /* There was previous execution */
      if (my_time_compare(&ends, &next_exec) == -1)
      {
        /* Next execution after ends. No more executions */
        set_zero_time(&execute_at, MYSQL_TIMESTAMP_DATETIME);
        execute_at_null= TRUE;
        if (on_completion == MYSQL_EVENT_ON_COMPLETION_DROP)
          dropped= true;
      }
      else
      {
        execute_at= next_exec;
        execute_at_null= FALSE;
      }
    }
    goto ret;
  }
  else if (starts_null && ends_null)
  {
    /*
      Both starts and m_ends are not set, so we schedule for the next
      based on last_executed.
    */
    if (last_executed.year)
    {
      if (get_next_time(&execute_at, &last_executed, expression, interval))
        goto err;
    }
    else
    {
      /* last_executed not set. Schedule the event for now */
      execute_at= time_now;
    }
    execute_at_null= FALSE;
  }
  else
  {
    /* either starts or m_ends is set */
    if (!starts_null)
    {
      /*
        - starts is set.
        - starts is not in the future according to check made before
        Hence schedule for starts + m_expression in case last_executed
        is not set, otherwise to last_executed + m_expression
      */
      if (last_executed.year)
      {
        if (get_next_time(&execute_at, &last_executed, expression, interval))
          goto err;
      }
      else
        execute_at= starts;
      execute_at_null= FALSE;
    }
    else
    {
      /*
        - m_ends is set
        - m_ends is after time_now or is equal
        Hence check for m_last_execute and increment with m_expression.
        If last_executed is not set then schedule for now
      */

      if (!last_executed.year)
        execute_at= time_now;
      else
      {
        TIME next_exec;

        if (get_next_time(&next_exec, &last_executed, expression, interval))
          goto err;

        if (my_time_compare(&ends, &next_exec) == -1)
        {
          set_zero_time(&execute_at, MYSQL_TIMESTAMP_DATETIME);
          execute_at_null= TRUE;
          if (on_completion == MYSQL_EVENT_ON_COMPLETION_DROP)
            dropped= true;
        }
        else
        {
          execute_at= next_exec;
          execute_at_null= FALSE;
        }
      }
    }
    goto ret;
  }
ret:

  DBUG_RETURN(false);
err:
  DBUG_RETURN(true);
}


/*
  Set the internal last_executed TIME struct to now. NOW is the
  time according to thd->query_start(), so the THD's clock.

  SYNOPSIS
    Event_timed::drop()
      thd   thread context
*/

void
Event_timed::mark_last_executed(THD *thd)
{
  TIME time_now;

  thd->end_time();
  my_tz_UTC->gmt_sec_to_TIME(&time_now, (my_time_t) thd->query_start());

  last_executed= time_now; /* was execute_at */
#ifdef ANDREY_0
  last_executed= execute_at;
#endif
  last_executed_changed= true;
}


/*
  Drops the event

  SYNOPSIS
    Event_timed::drop()
      thd   thread context

  RETURN VALUE
    0       OK
   -1       Cannot open mysql.event
   -2       Cannot find the event in mysql.event (already deleted?)

   others   return code from SE in case deletion of the event row
            failed.
*/

int
Event_timed::drop(THD *thd)
{
  TABLE *table;
  uint tmp= 0;
  DBUG_ENTER("Event_timed::drop");

  DBUG_RETURN(db_drop_event(thd, this, false, &tmp));
}


/*
  Saves status and last_executed_at to the disk if changed.

  SYNOPSIS
    Event_timed::update_fields()
      thd - thread context

  RETURN VALUE
    0   OK
    SP_OPEN_TABLE_FAILED    Error while opening mysql.event for writing
    EVEX_WRITE_ROW_FAILED   On error to write to disk

   others                   return code from SE in case deletion of the event
                            row failed.
*/

bool
Event_timed::update_fields(THD *thd)
{
  TABLE *table;
  Open_tables_state backup;
  int ret= 0;
  bool opened;

  DBUG_ENTER("Event_timed::update_time_fields");

  DBUG_PRINT("enter", ("name: %*s", name.length, name.str));

  /* No need to update if nothing has changed */
  if (!(status_changed || last_executed_changed))
    goto done;

  thd->reset_n_backup_open_tables_state(&backup);

  if (evex_open_event_table(thd, TL_WRITE, &table))
  {
    ret= SP_OPEN_TABLE_FAILED;
    goto done;
  }


  if ((ret= evex_db_find_event_by_name(thd, dbname, name, definer, table)))
    goto done;

  store_record(table,record[1]);
  /* Don't update create on row update. */
  table->timestamp_field_type= TIMESTAMP_NO_AUTO_SET;

  if (last_executed_changed)
  {
    table->field[EVEX_FIELD_LAST_EXECUTED]->set_notnull();
    table->field[EVEX_FIELD_LAST_EXECUTED]->store_time(&last_executed,
                           MYSQL_TIMESTAMP_DATETIME);
    last_executed_changed= false;
  }
  if (status_changed)
  {
    table->field[EVEX_FIELD_STATUS]->set_notnull();
    table->field[EVEX_FIELD_STATUS]->store((longlong)status);
    status_changed= false;
  }

  if ((table->file->ha_update_row(table->record[1],table->record[0])))
    ret= EVEX_WRITE_ROW_FAILED;

done:
  close_thread_tables(thd);
  thd->restore_backup_open_tables_state(&backup);

  DBUG_RETURN(ret);
}

extern LEX_STRING interval_type_to_name[];

/*
  Get SHOW CREATE EVENT as string

  SYNOPSIS
    Event_timed::get_create_event(THD *thd, String *buf)
      thd    Thread
      buf    String*, should be already allocated. CREATE EVENT goes inside.

  RETURN VALUE
    0                       OK
    EVEX_MICROSECOND_UNSUP  Error (for now if mysql.event has been
                            tampered and MICROSECONDS interval or
                            derivative has been put there.
*/

int
Event_timed::get_create_event(THD *thd, String *buf)
{
  int multipl= 0;
  char tmp_buff[128];
  String expr_buf(tmp_buff, sizeof(tmp_buff), system_charset_info);
  expr_buf.length(0);

  DBUG_ENTER("get_create_event");
  DBUG_PRINT("ret_info",("body_len=[%d]body=[%s]", body.length, body.str));

  if (expression &&
      event_reconstruct_interval_expression(&expr_buf, interval, expression))
    DBUG_RETURN(EVEX_MICROSECOND_UNSUP);

  buf->append(STRING_WITH_LEN("CREATE EVENT "));
  append_identifier(thd, buf, dbname.str, dbname.length);
  buf->append(STRING_WITH_LEN("."));
  append_identifier(thd, buf, name.str, name.length);

  buf->append(STRING_WITH_LEN(" ON SCHEDULE "));
  if (expression)
  {
    buf->append(STRING_WITH_LEN("EVERY "));
    buf->append(expr_buf);
    buf->append(' ');
    LEX_STRING *ival= &interval_type_to_name[interval];
    buf->append(ival->str, ival->length);
  }
  else
  {
    char dtime_buff[20*2+32];/* +32 to make my_snprintf_{8bit|ucs2} happy */
    buf->append(STRING_WITH_LEN("AT '"));
    /*
      Pass the buffer and the second param tells fills the buffer and
      returns the number of chars to copy.
    */
    buf->append(dtime_buff, my_datetime_to_str(&execute_at, dtime_buff));
    buf->append(STRING_WITH_LEN("'"));
  }

  if (on_completion == MYSQL_EVENT_ON_COMPLETION_DROP)
    buf->append(STRING_WITH_LEN(" ON COMPLETION NOT PRESERVE "));
  else
    buf->append(STRING_WITH_LEN(" ON COMPLETION PRESERVE "));

  if (status == MYSQL_EVENT_ENABLED)
    buf->append(STRING_WITH_LEN("ENABLE"));
  else
    buf->append(STRING_WITH_LEN("DISABLE"));

  if (comment.length)
  {
    buf->append(STRING_WITH_LEN(" COMMENT "));
    append_unescaped(buf, comment.str, comment.length);
  }
  buf->append(STRING_WITH_LEN(" DO "));
  buf->append(body.str, body.length);

  DBUG_RETURN(0);
}


/*
  Executes the event (the underlying sp_head object);

  SYNOPSIS
    evex_fill_row()
      thd       THD
      mem_root  If != NULL use it to compile the event on it

  RETURNS
    0        success
    -99      No rights on this.dbname.str
    -100     event in execution (parallel execution is impossible)
    others   retcodes of sp_head::execute_procedure()
*/

int
Event_timed::execute(THD *thd, MEM_ROOT *mem_root)
{
  Security_context *save_ctx;
  /* this one is local and not needed after exec */
  Security_context security_ctx;
  int ret= 0;

  DBUG_ENTER("Event_timed::execute");
  DBUG_PRINT("info", ("    EVEX EXECUTING event %s.%s [EXPR:%d]",
               dbname.str, name.str, (int) expression));

  VOID(pthread_mutex_lock(&this->LOCK_running));
  if (running)
  {
    VOID(pthread_mutex_unlock(&this->LOCK_running));
    DBUG_RETURN(-100);
  }
  running= true;
  VOID(pthread_mutex_unlock(&this->LOCK_running));

  DBUG_PRINT("info", ("master_access=%d db_access=%d",
             thd->security_ctx->master_access, thd->security_ctx->db_access));
  change_security_context(thd, &security_ctx, &save_ctx);
  DBUG_PRINT("info", ("master_access=%d db_access=%d",
             thd->security_ctx->master_access, thd->security_ctx->db_access));

  if (!sphead && (ret= compile(thd, mem_root)))
    goto done;
  /* Now we are sure we have valid this->sphead so we can copy the context */
  sphead->m_security_ctx= security_ctx;
  thd->db= dbname.str;
  thd->db_length= dbname.length;
  if (!check_access(thd, EVENT_ACL,dbname.str, 0, 0, 0,is_schema_db(dbname.str)))
  {
    List<Item> empty_item_list;
    empty_item_list.empty();
    ret= sphead->execute_procedure(thd, &empty_item_list);
  }
  else
  {
    DBUG_PRINT("error", ("%s@%s has no rights on %s", definer_user.str,
               definer_host.str, dbname.str));
    ret= -99;
  }
  restore_security_context(thd, save_ctx);
  DBUG_PRINT("info", ("master_access=%d db_access=%d",
             thd->security_ctx->master_access, thd->security_ctx->db_access));
  thd->db= 0;

  VOID(pthread_mutex_lock(&this->LOCK_running));
  running= false;
  VOID(pthread_mutex_unlock(&this->LOCK_running));

done:
  /*
    1. Don't cache sphead if allocated on another mem_root
    2. Don't call security_ctx.destroy() because this will free our dbname.str
       name.str and definer.str
  */
  if (mem_root && sphead)
  {
    delete sphead;
    sphead= 0;
  }
  DBUG_PRINT("info", ("    EVEX EXECUTED event %s.%s  [EXPR:%d]. RetCode=%d",
                      dbname.str, name.str, (int) expression, ret));

  DBUG_RETURN(ret);
}


/*
  Switches the security context
  Synopsis
    Event_timed::change_security_context()
      thd    - thread
      backup - where to store the old context 
  
  RETURN
    0  - OK
    1  - Error (generates error too)
*/
bool
Event_timed::change_security_context(THD *thd, Security_context *s_ctx,
                                     Security_context **backup)
{
  DBUG_ENTER("Event_timed::change_security_context");
  DBUG_PRINT("info",("%s@%s@%s",definer_user.str,definer_host.str, dbname.str));
#ifndef NO_EMBEDDED_ACCESS_CHECKS
  s_ctx->init();
  *backup= 0;
  if (acl_getroot_no_password(s_ctx, definer_user.str, definer_host.str,
                             definer_host.str, dbname.str))
  {
    my_error(ER_NO_SUCH_USER, MYF(0), definer_user.str, definer_host.str);
    DBUG_RETURN(TRUE);
  }
  *backup= thd->security_ctx;
  thd->security_ctx= s_ctx;
#endif
  DBUG_RETURN(FALSE);
}


/*
  Restores the security context
  Synopsis
    Event_timed::restore_security_context()
      thd    - thread
      backup - switch to this context
 */

void
Event_timed::restore_security_context(THD *thd, Security_context *backup)
{
  DBUG_ENTER("Event_timed::change_security_context");
#ifndef NO_EMBEDDED_ACCESS_CHECKS
  if (backup)
    thd->security_ctx= backup;
#endif
  DBUG_VOID_RETURN;
}


/*
  Compiles an event before it's execution. Compiles the anonymous
  sp_head object held by the event

  SYNOPSIS
    Event_timed::compile()
      thd        thread context, used for memory allocation mostly
      mem_root   if != NULL then this memory root is used for allocs
                 instead of thd->mem_root

  RETURN VALUE
    0                       success
    EVEX_COMPILE_ERROR      error during compilation
    EVEX_MICROSECOND_UNSUP  mysql.event was tampered 
*/

int
Event_timed::compile(THD *thd, MEM_ROOT *mem_root)
{
  int ret= 0;
  MEM_ROOT *tmp_mem_root= 0;
  LEX *old_lex= thd->lex, lex;
  char *old_db;
  int old_db_length;
  Event_timed *ett;
  sp_name *spn;
  char *old_query;
  uint old_query_len;
  st_sp_chistics *p;
  ulong old_sql_mode= thd->variables.sql_mode;
  char create_buf[2048];
  String show_create(create_buf, sizeof(create_buf), system_charset_info);
  CHARSET_INFO *old_character_set_client,
               *old_collation_connection,
               *old_character_set_results;

  DBUG_ENTER("Event_timed::compile");

  show_create.length(0);

  switch (get_create_event(thd, &show_create)) {
  case EVEX_MICROSECOND_UNSUP:
    sql_print_error("Scheduler");
    DBUG_RETURN(EVEX_MICROSECOND_UNSUP);
  case 0:
    break;
  default:
    DBUG_ASSERT(0);
  }

  old_character_set_client= thd->variables.character_set_client;
  old_character_set_results= thd->variables.character_set_results;
  old_collation_connection= thd->variables.collation_connection;

  thd->variables.character_set_client=
    thd->variables.character_set_results=
      thd->variables.collation_connection=
           get_charset_by_csname("utf8", MY_CS_PRIMARY, MYF(MY_WME));

  thd->update_charset();

  DBUG_PRINT("info",("old_sql_mode=%d new_sql_mode=%d",old_sql_mode, sql_mode));
  thd->variables.sql_mode= this->sql_mode;
  /* Change the memory root for the execution time */
  if (mem_root)
  {
    tmp_mem_root= thd->mem_root;
    thd->mem_root= mem_root;
  }
  old_query_len= thd->query_length;
  old_query= thd->query;
  old_db= thd->db;
  old_db_length= thd->db_length;
  thd->db= dbname.str;
  thd->db_length= dbname.length;

  thd->query= show_create.c_ptr();
  thd->query_length= show_create.length();
  DBUG_PRINT("Event_timed::compile", ("query:%s",thd->query));

  thd->lex= &lex;
  lex_start(thd, (uchar*)thd->query, thd->query_length);
  lex.et_compile_phase= TRUE;
  if (yyparse((void *)thd) || thd->is_fatal_error)
  {
    DBUG_PRINT("error", ("error during compile or thd->is_fatal_error=%d",
                          thd->is_fatal_error));
    /*
      Free lex associated resources
      QQ: Do we really need all this stuff here?
    */
    sql_print_error("error during compile of %s.%s or thd->is_fatal_error=%d",
                    dbname.str, name.str, thd->is_fatal_error);
    if (lex.sphead)
    {
      if (&lex != thd->lex)
        thd->lex->sphead->restore_lex(thd);
      delete lex.sphead;
      lex.sphead= 0;
    }
    ret= EVEX_COMPILE_ERROR;
    goto done;
  }
  DBUG_PRINT("note", ("success compiling %s.%s", dbname.str, name.str));

  sphead= lex.et->sphead;
  sphead->m_db= dbname;

  sphead->set_definer(definer.str, definer.length);
  sphead->set_info(0, 0, &lex.sp_chistics, sql_mode);
  sphead->optimize();
  ret= 0;
done:
  lex.et->free_sphead_on_delete= false;
  delete lex.et;
  lex_end(&lex);
  DBUG_PRINT("note", ("return old data on its place. set back NAMES"));

  thd->lex= old_lex;
  thd->query= old_query;
  thd->query_length= old_query_len;
  thd->db= old_db;

  thd->variables.sql_mode= old_sql_mode;
  thd->variables.character_set_client= old_character_set_client;
  thd->variables.character_set_results= old_character_set_results;
  thd->variables.collation_connection= old_collation_connection;
  thd->update_charset();

  /* Change the memory root for the execution time. */
  if (mem_root)
    thd->mem_root= tmp_mem_root;

  DBUG_RETURN(ret);
}


/*
  Checks whether this thread can lock the object for modification ->
  preventing being spawned for execution, and locks if possible.
  use ::can_spawn_now() only for basic checking because a race
  condition may occur between the check and eventual modification (deletion)
  of the object.

  Returns
    true  - locked
    false - cannot lock
*/

my_bool
Event_timed::can_spawn_now_n_lock(THD *thd)
{
  my_bool ret= FALSE;
  VOID(pthread_mutex_lock(&this->LOCK_running));
  if (!in_spawned_thread)
  {
    in_spawned_thread= TRUE;
    ret= TRUE;
    locked_by_thread_id= thd->thread_id;
  }
  VOID(pthread_mutex_unlock(&this->LOCK_running));
  return ret;  
}


extern pthread_attr_t connection_attrib;

/*
  Checks whether is possible and forks a thread. Passes self as argument.

  Returns
  EVENT_EXEC_STARTED       - OK
  EVENT_EXEC_ALREADY_EXEC  - Thread not forked, already working
  EVENT_EXEC_CANT_FORK     - Unable to spawn thread (error)
*/

int
Event_timed::spawn_now(void * (*thread_func)(void*))
{  
  int ret= EVENT_EXEC_STARTED;
  static uint exec_num= 0;
  DBUG_ENTER("Event_timed::spawn_now");
  DBUG_PRINT("info", ("[%s.%s]", dbname.str, name.str));

  VOID(pthread_mutex_lock(&this->LOCK_running));
  if (!in_spawned_thread)
  {
    pthread_t th;
    in_spawned_thread= true;
    if (pthread_create(&th, &connection_attrib, thread_func, (void*)this))
    {
      DBUG_PRINT("info", ("problem while spawning thread"));
      ret= EVENT_EXEC_CANT_FORK;
      in_spawned_thread= false;
    }
#ifndef DBUG_OFF
    else
    {
      sql_print_information("SCHEDULER: Started thread %d", ++exec_num);
      DBUG_PRINT("info", ("thread spawned"));
    }
#endif
  }
  else
  {
    DBUG_PRINT("info", ("already in spawned thread. skipping"));
    ret= EVENT_EXEC_ALREADY_EXEC;
  }
  VOID(pthread_mutex_unlock(&this->LOCK_running));

  DBUG_RETURN(ret);  
}


void
Event_timed::spawn_thread_finish(THD *thd)
{
  DBUG_ENTER("Event_timed::spawn_thread_finish");
  VOID(pthread_mutex_lock(&this->LOCK_running));
  in_spawned_thread= false;
  if ((flags & EVENT_EXEC_NO_MORE) || status == MYSQL_EVENT_DISABLED)
  {
    DBUG_PRINT("info", ("%s exec no more. to drop=%d", name.str, dropped));
    if (dropped)
      drop(thd);
    VOID(pthread_mutex_unlock(&this->LOCK_running));
    delete this;
    DBUG_VOID_RETURN;
  }
  VOID(pthread_mutex_unlock(&this->LOCK_running));
  DBUG_VOID_RETURN;
}


/*
  Unlocks the object after it has been locked with ::can_spawn_now_n_lock()

  Returns
    0 - ok
    1 - not locked by this thread
*/
 

int
Event_timed::spawn_unlock(THD *thd)
{
  int ret= 0;
  VOID(pthread_mutex_lock(&this->LOCK_running));
  if (!in_spawned_thread)
  {
    if (locked_by_thread_id == thd->thread_id)
    {        
      in_spawned_thread= FALSE;
      locked_by_thread_id= 0;
    }
    else
    {
      sql_print_error("A thread tries to unlock when he hasn't locked. "
                      "thread_id=%ld locked by %ld",
                      thd->thread_id, locked_by_thread_id);
      DBUG_ASSERT(0);
      ret= 1;
    }
  }
  VOID(pthread_mutex_unlock(&this->LOCK_running));
  return ret;
}
