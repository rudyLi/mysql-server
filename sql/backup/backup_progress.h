#ifndef _BACKUP_PROGRESS_H
#define _BACKUP_PROGRESS_H

/* Copyright (C) 2004-2007 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/**
   @file backup_progress.h

   Data Dictionary related operations of Online Backup.
*/

#include "my_global.h"
#include "my_time.h"
#include "mysql_priv.h"

/**
   List of fields for online backup table.
  */
enum enum_backup_history_table_field
{
  ET_OBH_FIELD_BACKUP_ID = 0, /* start from 0 to correspond with field array */
  ET_OBH_FIELD_PROCESS_ID,
  ET_OBH_FIELD_BINLOG_POS,
  ET_OBH_FIELD_BINLOG_FILE,
  ET_OBH_FIELD_BACKUP_STATE,
  ET_OBH_FIELD_OPER,
  ET_OBH_FIELD_ERROR_NUM,
  ET_OBH_FIELD_NUM_OBJ,
  ET_OBH_FIELD_TOTAL_BYTES,
  ET_OBH_FIELD_VP,
  ET_OBH_FIELD_START_TIME,
  ET_OBH_FIELD_STOP_TIME,
  ET_OBH_FIELD_HOST_OR_SERVER,
  ET_OBH_FIELD_USERNAME,
  ET_OBH_FIELD_BACKUP_FILE,
  ET_OBH_FIELD_COMMENT,
  ET_OBH_FIELD_COMMAND,
  ET_OBH_FIELD_ENGINES,
  ET_OBH_FIELD_COUNT /* a cool trick to count the number of fields :) */
};

/**
   List of fields for online backup progress table.
  */
enum enum_backup_progress_table_field
{
  ET_OBP_FIELD_BACKUP_ID_FK = 0, /* start from 0 to correspond with field array */
  ET_OBP_FIELD_PROG_OBJECT,
  ET_OBP_FIELD_PROG_START_TIME,
  ET_OBP_FIELD_PROG_STOP_TIME,
  ET_OBP_FIELD_PROG_SIZE,
  ET_OBP_FIELD_PROGRESS,
  ET_OBP_FIELD_PROG_ERROR_NUM,
  ET_OBP_FIELD_PROG_NOTES,
  ET_OBP_FIELD_PROG_COUNT /* a cool trick to count the number of fields :) */
};

/**
   List of states for online backup table.
  */
enum enum_backup_state
{
  BUP_UNKNOWN = 0,
  BUP_COMPLETE,
  BUP_STARTING,
  BUP_VALIDITY_POINT,
  BUP_RUNNING,
  BUP_ERRORS,
  BUP_CANCEL
};

/**
   List of operations for online backup table.
  */
enum enum_backup_operation
{
  OP_BACKUP = 1,
  OP_RESTORE,
  OP_SHOW,
  OP_OTHER
};

/*
  This method attempts to open the online backup progress tables. It returns
  an error if either table is not present or cannot be opened.
*/
my_bool check_ob_progress_tables(THD *thd);


/*
  This method creates a new row in the backup history log with the
  information provided.
*/
bool backup_history_log_write(THD *thd, 
                              ulonglong *backup_id,
                              int process_id,
                              enum_backup_state state,
                              enum_backup_operation operation,
                              int error_num,
                              const char *user_comment,
                              const char *backup_file,
                              const char *command);

/*
   This method locates the row in the online backup table that matches the
   backup_id passed.
*/
bool find_backup_history_row(TABLE *table, ulonglong backup_id);

/*
  This method updates a row in the backup history log using one
  of four data types as determined by the field (see fld).
*/
bool backup_history_log_update(THD *thd, 
                               ulonglong backup_id,
                               enum_backup_history_table_field fld,
                               ulonglong val_long,
                               time_t val_time,
                               const char *val_str,
                               int val_state);

/*
  This method creates a new row in the backup progress log with the
  information provided.
*/
bool backup_progress_log_write(THD *thd,
                               ulonglong backup_id,
                               const char *object,
                               time_t start,
                               time_t stop,
                               longlong size,
                               longlong progress,
                               int error_num,
                               const char *notes);

/*
  This method inserts a new row in the online_backup table populating it with
  the initial values passed. It returns the backup_id of the new row.
*/
ulonglong report_ob_init(THD *thd,
                         int process_id,
                         enum_backup_state state,
                         enum_backup_operation operation,
                         int error_num,
                         const char *user_comment,
                         const char *backup_file,
                         const char *command);

/*
  This method updates the binary log information for the backup operation
  identified by backup_id.
*/
int report_ob_binlog_info(THD *thd,
                          ulonglong backup_id,
                          int binlog_pos,
                          const char *binlog_file);

/*
  This method updates the error number for the backup operation identified by
  backup_id.
*/
int report_ob_error(THD *thd,
                    ulonglong backup_id,
                    int error_num);

/*
  This method updates the state for the backup operation identified by
  backup_id.
*/
int report_ob_state(THD *thd,
                    ulonglong backup_id,
                    enum_backup_state state);

/*
  This method updates the number of objects for the backup operation identified by
  backup_id.
*/
int report_ob_num_objects(THD *thd,
                          ulonglong backup_id,
                          int num_objects);

/*
  This method updates the size for the backup operation identified by
  backup_id.
*/
int report_ob_size(THD *thd,
                   ulonglong backup_id,
                   longlong size);

/*
  This method updates the start/stop time for the backup operation identified by
  backup_id.

  Note: This method ignores values for start or stop that are NULL (only saves 
  values provided so that it can be called once for start and once again later 
  for stop).
*/
int report_ob_time(THD *thd,
                   ulonglong backup_id,
                   time_t start,
                   time_t stop);

/*
  This method updates the validity point time for the backup operation
  identified by backup_id.
*/
int report_ob_vp_time(THD *thd,
                      ulonglong backup_id,
                      time_t vp_time);

/*
  This method updates the engines information for the backup operation
  identified by backup_id. This method appends to the those listed in the
  table for the backup_id.
*/
int report_ob_engines(THD *thd,
                      ulonglong backup_id,
                      const char *engine_name);

/*
  This method inserts a new row in the progress table.
*/
int report_ob_progress(THD *thd,
                       ulonglong backup_id,
                       const char *object,
                       time_t start,
                       time_t stop,
                       longlong size,
                       longlong progress,
                       int error_num,
                       const char *notes);

#endif

