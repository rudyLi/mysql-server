/***************************************************************************
                           client_test.c  -  description
                             -------------------------
    begin                : Sun Feb 3 2002
    copyright            : (C) MySQL AB 1995-2003, www.mysql.com
    author               : venu ( venu@mysql.com )
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *  This is a test sample to test the new features in MySQL client-server  * 
 *  protocol                                                               *
 *                                                                         *
 ***************************************************************************/

#include <my_global.h>

#if defined(__WIN__) || defined(_WIN32) || defined(_WIN64)
#include  <windows.h>
#endif

/* standrad headers */
#include <stdio.h>
#include <string.h>

/* mysql client headers */
#include <my_sys.h>
#include <mysql.h>
#include <my_getopt.h>

#include <assert.h>

#ifndef true
#define true 1
#endif

#ifndef false
#define false 0
#endif

#ifndef bzero
#define bzero(A,B) memset(A,0,B)
#endif

/* set default options */
static char *opt_db=0;
static char *opt_user=0;
static char *opt_password=0;
static char *opt_host=0;
static char *opt_unix_socket=0;
static unsigned int  opt_port;
static my_bool tty_password=0;

static MYSQL *mysql=0;
static char query[255]; 
static char current_db[]= "client_test_db";

#define myheader(str) { fprintf(stdout,"\n\n#######################\n"); \
                        fprintf(stdout,"%s",str); \
                        fprintf(stdout,"\n#######################\n"); \
                      }

#define init_bind(x) (bzero(x,sizeof(x)))

#ifndef mysql_param_result
#define mysql_param_result mysql_prepare_result
#endif

static void print_error(const char *msg)
{  
  if (mysql)
  {
    if (mysql->server_version)
      fprintf(stderr,"\n [MySQL-%s]",mysql->server_version);
    else
      fprintf(stderr,"\n [MySQL]");
    fprintf(stderr,"[%d] %s\n",mysql_errno(mysql),mysql_error(mysql));
  }
  else if(msg) fprintf(stderr, " [MySQL] %s\n", msg);
}

static void print_st_error(MYSQL_STMT *stmt, const char *msg)
{  
  if (stmt)
  {
    if (stmt->mysql && stmt->mysql->server_version)
      fprintf(stderr,"\n [MySQL-%s]",stmt->mysql->server_version);
    else
      fprintf(stderr,"\n [MySQL]");

    fprintf(stderr,"[%d] %s\n",mysql_stmt_errno(stmt),
                     mysql_stmt_error(stmt));
  }
	else if(msg) fprintf(stderr, " [MySQL] %s\n", msg);
}
static void client_disconnect();

#define myerror(msg) print_error(msg)
#define mysterror(stmt, msg) print_st_error(stmt, msg)

#define myassert(exp) assert(exp)
#define myassert_r(exp) assert(!(exp))

#define myquery(r) \
{ \
if( r || r == -1) \
  myerror(NULL); \
 myassert(r == 0); \
}

#define myquery_r(r) \
{ \
if( r || r == -1) \
  myerror(NULL); \
myassert_r(r == 0); \
}

#define mystmt(stmt,r) \
{ \
if( r || r == -1) \
  mysterror(stmt,NULL); \
myassert(r == 0);\
}

#define mystmt_r(stmt,r) \
{ \
if( r || r == -1) \
  mysterror(stmt,NULL); \
myassert_r(r == 0);\
}

#define mystmt_init(stmt) \
{ \
if( stmt == 0) \
  myerror(NULL); \
myassert(stmt != 0); \
}

#define mystmt_init_r(stmt) \
{ \
myassert(stmt == 0);\
} 

#define mytest(x) if(!x) {myerror(NULL);myassert(true);}
#define mytest_r(x) if(x) {myerror(NULL);myassert(true);}

#define PREPARE(A,B) mysql_prepare(A,B,strlen(B))

/********************************************************
* connect to the server                                 *
*********************************************************/
static void client_connect()
{
  char buff[255];
  myheader("client_connect");  

  if(!(mysql = mysql_init(NULL)))
  { 
	  myerror("mysql_init() failed");
    exit(0);
  }
  if (!(mysql_real_connect(mysql,opt_host,opt_user,
			   opt_password, opt_db ? opt_db:"test", opt_port,
			   opt_unix_socket, 0)))
  {
    myerror("connection failed");    
    mysql_close(mysql);
    exit(0);
  }    

  /* set AUTOCOMMIT to ON*/
  mysql_autocommit(mysql, true);
  sprintf(buff,"CREATE DATABASE IF NOT EXISTS %s", current_db);
  mysql_query(mysql, buff);
  sprintf(buff,"USE %s", current_db);
  mysql_query(mysql, buff);
}

/********************************************************
* close the connection                                  *
*********************************************************/
static void client_disconnect()
{
  if (mysql)
  {
    char buff[255];
    sprintf(buff,"DROP DATABASE IF EXISTS %s", current_db);
    mysql_query(mysql, buff);
    mysql_close(mysql);
  }
}

/********************************************************
* query processing                                      *
*********************************************************/
static void client_query()
{
  int rc;

  myheader("client_query");

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS myclient_test");
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE myclient_test(id int primary key auto_increment,\
                                              name varchar(20))");
  myquery(rc);
  
  rc = mysql_query(mysql,"CREATE TABLE myclient_test(id int, name varchar(20))");
  myquery_r(rc);
  
  rc = mysql_query(mysql,"INSERT INTO myclient_test(name) VALUES('mysql')");
  myquery(rc);
  
  rc = mysql_query(mysql,"INSERT INTO myclient_test(name) VALUES('monty')");
  myquery(rc);

  rc = mysql_query(mysql,"INSERT INTO myclient_test(name) VALUES('venu')");
  myquery(rc);

  rc = mysql_query(mysql,"INSERT INTO myclient_test(name) VALUES('deleted')");
  myquery(rc);

  rc = mysql_query(mysql,"INSERT INTO myclient_test(name) VALUES('deleted')");
  myquery(rc);

  rc = mysql_query(mysql,"UPDATE myclient_test SET name='updated' WHERE name='deleted'");
  myquery(rc);

  rc = mysql_query(mysql,"UPDATE myclient_test SET id=3 WHERE name='updated'");
  myquery_r(rc);
}

/********************************************************
* print dashes                                          *
*********************************************************/
static void my_print_dashes(MYSQL_RES *result)
{
  MYSQL_FIELD  *field;
  unsigned int i,j;

  mysql_field_seek(result,0);
  fputc('\t',stdout);
  fputc('+', stdout);

  for(i=0; i< mysql_num_fields(result); i++)
  {
    field = mysql_fetch_field(result);
    for(j=0; j < field->max_length+2; j++)
      fputc('-',stdout);
    fputc('+',stdout);
  }
  fputc('\n',stdout);
}

/********************************************************
* print resultset metadata information                  *
*********************************************************/
static void my_print_result_metadata(MYSQL_RES *result)
{
  MYSQL_FIELD  *field;
  unsigned int i,j;
  unsigned int field_count;

  mysql_field_seek(result,0);
  fputc('\n', stdout);

  field_count = mysql_num_fields(result);
  for(i=0; i< field_count; i++)
  {
    field = mysql_fetch_field(result);
    j = strlen(field->name);
    if(j < field->max_length)
      j = field->max_length;
    if(j < 4 && !IS_NOT_NULL(field->flags))
      j = 4;
    field->max_length = j;
  }
  my_print_dashes(result);
  fputc('\t',stdout);
  fputc('|', stdout);

  mysql_field_seek(result,0);
  for(i=0; i< field_count; i++)
  {
    field = mysql_fetch_field(result);
    fprintf(stdout, " %-*s |",(int) field->max_length, field->name);
  }
  fputc('\n', stdout);
  my_print_dashes(result);
}

/********************************************************
* process the result set                                *
*********************************************************/
int my_process_result_set(MYSQL_RES *result)
{
  MYSQL_ROW    row;
  MYSQL_FIELD  *field;
  unsigned int i;
  unsigned int row_count=0;
  
  if (!result)
    return 0;

  my_print_result_metadata(result);

  while((row = mysql_fetch_row(result)) != NULL)
  {
    mysql_field_seek(result,0);
    fputc('\t',stdout);
    fputc('|',stdout);

    for(i=0; i< mysql_num_fields(result); i++)
    {
      field = mysql_fetch_field(result);
      if(row[i] == NULL)
        fprintf(stdout, " %-*s |", (int) field->max_length, "NULL");
      else if (IS_NUM(field->type))
        fprintf(stdout, " %*s |", (int) field->max_length, row[i]);
      else
        fprintf(stdout, " %-*s |", (int) field->max_length, row[i]);
    }
    fputc('\t',stdout);
    fputc('\n',stdout);
    row_count++;
  }
  my_print_dashes(result);

  if (mysql_errno(mysql) != 0)
    fprintf(stderr, "\n\tmysql_fetch_row() failed\n");
  else
    fprintf(stdout,"\n\t%d %s returned\n", row_count, 
                   row_count == 1 ? "row" : "rows");
  return row_count;
}

/********************************************************
* process the stmt result set                           *
*********************************************************/
uint my_process_stmt_result(MYSQL_STMT *stmt)
{
  int         field_count;
  uint        row_count= 0;
  MYSQL_BIND  buffer[50];
  MYSQL_FIELD *field;
  MYSQL_RES   *result;
  char        data[50][255];
  long        length[50];
  int         rc, i;

  if (!(result= mysql_prepare_result(stmt)))
  {
    while (!mysql_fetch(stmt));
    return 0;
  }
  
  field_count= mysql_num_fields(result);
  for(i=0; i < field_count; i++)
  {
    buffer[i].buffer_type= MYSQL_TYPE_STRING;
    buffer[i].buffer_length=50;
    buffer[i].length=(long *)&length[i];
    buffer[i].buffer=(gptr)data[i];
  }

  my_print_result_metadata(result);

  rc= mysql_bind_result(stmt,buffer);
  mystmt(stmt,rc);

  mysql_field_seek(result, 0);  
  while (mysql_fetch(stmt) == 0)
  {    
    fputc('\t',stdout);
    fputc('|',stdout);
    
    mysql_field_seek(result,0);
    for (i=0; i < field_count; i++)
    {
      field = mysql_fetch_field(result);
      if(length[i] == MYSQL_NULL_DATA)
        fprintf(stdout, " %-*s |", (int) field->max_length, "NULL");
      else if (length[i] == 0)
        data[i][0]='\0';  /* unmodified buffer */
      else if (IS_NUM(field->type))
        fprintf(stdout, " %*s |", (int) field->max_length, data[i]);
      else
        fprintf(stdout, " %-*s |", (int) field->max_length, data[i]);
    }
    fputc('\t',stdout);
    fputc('\n',stdout);
    row_count++;
  }
  my_print_dashes(result);
  fprintf(stdout,"\n\t%d %s returned\n", row_count, 
                 row_count == 1 ? "row" : "rows");
  mysql_free_result(result);
  return row_count;
}

/********************************************************
* process the stmt result set                           *
*********************************************************/
uint my_stmt_result(const char *query, unsigned long length)
{
  MYSQL_STMT *stmt;
  uint       row_count;
  int        rc;

  stmt= mysql_prepare(mysql,query,length);
  mystmt_init(stmt);

  rc = mysql_execute(stmt);
  mystmt(stmt,rc);

  row_count= my_process_stmt_result(stmt);
  mysql_stmt_close(stmt);
    
  return row_count;
}

/*
  Utility function to verify a particular column data
*/
static void verify_col_data(const char *table, const char *col, 
                            const char *exp_data)
{
  MYSQL_RES *result;
  MYSQL_ROW row;
  char      query[255]; 
  int       rc, field= 1;
 
  if (table && col)
  {
    sprintf(query, "SELECT %s FROM %s LIMIT 1", col, table);

    fprintf(stdout,"\n %s", query);
    rc = mysql_query(mysql, query);
    myquery(rc);

    field= 0;
  }

  result = mysql_use_result(mysql);
  mytest(result);

  if (!(row= mysql_fetch_row(result)) || !row[field])
  {
    fprintf(stdout,"\n *** ERROR: FAILED TO GET THE RESULT ***");
    exit(1);
  }
  fprintf(stdout,"\n obtained: `%s` (expected: `%s`)", 
    row[field], exp_data);
  myassert(strcmp(row[field],exp_data) == 0);
  mysql_free_result(result);
}

/*
  Utility function to verify the field members
*/

static void verify_prepare_field(MYSQL_RES *result, 
      unsigned int no,const char *name, const char *org_name, 
      enum enum_field_types type, const char *table, 
      const char *org_table, const char *db)
{
  MYSQL_FIELD *field;

  if (!(field= mysql_fetch_field_direct(result,no)))
  {
    fprintf(stdout,"\n *** ERROR: FAILED TO GET THE RESULT ***");
    exit(1);
  }
  fprintf(stdout,"\n field[%d]:", no);
  fprintf(stdout,"\n    name     :`%s`\t(expected: `%s`)", field->name, name);
  fprintf(stdout,"\n    org_name :`%s`\t(expected: `%s`)", field->org_name, org_name);
  fprintf(stdout,"\n    type     :`%d`\t(expected: `%d`)", field->type, type);
  fprintf(stdout,"\n    table    :`%s`\t(expected: `%s`)", field->table, table);
  fprintf(stdout,"\n    org_table:`%s`\t(expected: `%s`)", field->org_table, org_table);
  fprintf(stdout,"\n    database :`%s`\t(expected: `%s`)", field->db, db);
  fprintf(stdout,"\n");
  myassert(strcmp(field->name,name) == 0);
  myassert(strcmp(field->org_name,org_name) == 0);
  myassert(field->type == type);
  myassert(strcmp(field->table,table) == 0);
  myassert(strcmp(field->org_table,org_table) == 0);
  myassert(strcmp(field->db,db) == 0);
}

/*
  Utility function to verify the parameter count
*/
static void verify_param_count(MYSQL_STMT *stmt, long exp_count)
{
  long param_count= mysql_param_count(stmt);
  fprintf(stdout,"\n total parameters in stmt: %ld (expected: %ld)",
                  param_count, exp_count);
  myassert(param_count == exp_count);
}


/********************************************************
* store result processing                               *
*********************************************************/
static void client_store_result()
{
  MYSQL_RES *result;
  int       rc;

  myheader("client_store_result");

  rc = mysql_query(mysql, "SELECT * FROM myclient_test");
  myquery(rc);

  /* get the result */
  result = mysql_store_result(mysql);
  mytest(result);

  my_process_result_set(result);
  mysql_free_result(result);
}

/********************************************************
* fetch the results
*********************************************************/
static void client_use_result()
{
  MYSQL_RES *result;
  int       rc;
  myheader("client_use_result");

  rc = mysql_query(mysql, "SELECT * FROM myclient_test");
  myquery(rc);

  /* get the result */
  result = mysql_use_result(mysql);
  mytest(result);

  my_process_result_set(result);
  mysql_free_result(result);
}


/********************************************************
* query processing                                      *
*********************************************************/
static void test_debug_example()
{
  int rc;
  MYSQL_RES *result;

  myheader("test_debug_example");

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_debug_example");
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_debug_example(id int primary key auto_increment,\
                                              name varchar(20),xxx int)");
  myquery(rc);

  rc = mysql_query(mysql,"INSERT INTO test_debug_example(name) VALUES('mysql')");
  myquery(rc);

  rc = mysql_query(mysql,"UPDATE test_debug_example SET name='updated' WHERE name='deleted'");
  myquery(rc);

  rc = mysql_query(mysql,"SELECT * FROM test_debug_example where name='mysql'");
  myquery(rc);

  result = mysql_use_result(mysql);
  mytest(result);

  my_process_result_set(result);
  mysql_free_result(result);

  rc = mysql_query(mysql,"DROP TABLE test_debug_example");
  myquery(rc);
}

/********************************************************
* to test autocommit feature                            *
*********************************************************/
static void test_tran_bdb()
{
  MYSQL_RES *result;
  MYSQL_ROW row;
  int       rc;

  myheader("test_tran_bdb");

  /* set AUTOCOMMIT to OFF */
  rc = mysql_autocommit(mysql, false);
  myquery(rc);

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS my_demo_transaction");
  myquery(rc);


  rc = mysql_commit(mysql);
  myquery(rc);

  /* create the table 'mytran_demo' of type BDB' or 'InnoDB' */
  rc = mysql_query(mysql,"CREATE TABLE my_demo_transaction(col1 int ,col2 varchar(30)) TYPE = BDB");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  /* insert a row and commit the transaction */
  rc = mysql_query(mysql,"INSERT INTO my_demo_transaction VALUES(10,'venu')");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

    /* now insert the second row, and rollback the transaction */
  rc = mysql_query(mysql,"INSERT INTO my_demo_transaction VALUES(20,'mysql')");
  myquery(rc);

  rc = mysql_rollback(mysql);
  myquery(rc);

  /* delete first row, and rollback it */
  rc = mysql_query(mysql,"DELETE FROM my_demo_transaction WHERE col1 = 10");
  myquery(rc);

  rc = mysql_rollback(mysql);
  myquery(rc);

  /* test the results now, only one row should exists */
  rc = mysql_query(mysql,"SELECT * FROM my_demo_transaction");
  myquery(rc);

  /* get the result */
  result = mysql_store_result(mysql);
  mytest(result);

  my_process_result_set(result);

  /* test the results now, only one row should exists */
  rc = mysql_query(mysql,"SELECT * FROM my_demo_transaction");
  myquery(rc);

  /* get the result */
  result = mysql_use_result(mysql);
  mytest(result);

  row = mysql_fetch_row(result);
  mytest(row);

  row = mysql_fetch_row(result);
  mytest_r(row);

  mysql_free_result(result);
  mysql_autocommit(mysql,true);
}

/********************************************************
* to test autocommit feature                            *
*********************************************************/
static void test_tran_innodb()
{
  MYSQL_RES *result;
  MYSQL_ROW row;
  int       rc;

  myheader("test_tran_innodb");

  /* set AUTOCOMMIT to OFF */
  rc = mysql_autocommit(mysql, false);
  myquery(rc);

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS my_demo_transaction");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  /* create the table 'mytran_demo' of type BDB' or 'InnoDB' */
  rc = mysql_query(mysql,"CREATE TABLE my_demo_transaction(col1 int ,col2 varchar(30)) TYPE = InnoDB");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  /* insert a row and commit the transaction */
  rc = mysql_query(mysql,"INSERT INTO my_demo_transaction VALUES(10,'venu')");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

    /* now insert the second row, and rollback the transaction */
  rc = mysql_query(mysql,"INSERT INTO my_demo_transaction VALUES(20,'mysql')");
  myquery(rc);

  rc = mysql_rollback(mysql);
  myquery(rc);

  /* delete first row, and rollback it */
  rc = mysql_query(mysql,"DELETE FROM my_demo_transaction WHERE col1 = 10");
  myquery(rc);

  rc = mysql_rollback(mysql);
  myquery(rc);

  /* test the results now, only one row should exists */
  rc = mysql_query(mysql,"SELECT * FROM my_demo_transaction");
  myquery(rc);

  /* get the result */
  result = mysql_store_result(mysql);
  mytest(result);

  my_process_result_set(result);

  /* test the results now, only one row should exists */
  rc = mysql_query(mysql,"SELECT * FROM my_demo_transaction");
  myquery(rc);

  /* get the result */
  result = mysql_use_result(mysql);
  mytest(result);

  row = mysql_fetch_row(result);
  mytest(row);

  row = mysql_fetch_row(result);
  mytest_r(row);

  mysql_free_result(result);
  mysql_autocommit(mysql,true);
}


/********************************************************
 To test simple prepares of all DML statements
*********************************************************/

static void test_prepare_simple()
{
  MYSQL_STMT *stmt;
  int        rc;

  myheader("test_prepare_simple");

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_prepare_simple");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_prepare_simple(id int, name varchar(50))");
  myquery(rc);

  /* alter table */
  strcpy(query,"ALTER TABLE test_prepare_simple ADD new char(20)");
  stmt = mysql_prepare(mysql, query, strlen(query));
  mystmt_init(stmt);

  verify_param_count(stmt,0);
  mysql_stmt_close(stmt);

  /* insert */
  strcpy(query,"INSERT INTO test_prepare_simple VALUES(?,?)");
  stmt = mysql_prepare(mysql, query, strlen(query));
  mystmt_init(stmt);

  verify_param_count(stmt,2);
  mysql_stmt_close(stmt);

  /* update */
  strcpy(query,"UPDATE test_prepare_simple SET id=? WHERE id=? AND name= ?");
  stmt = mysql_prepare(mysql, query, strlen(query));
  mystmt_init(stmt);

  verify_param_count(stmt,3);
  mysql_stmt_close(stmt);

  /* delete */
  strcpy(query,"DELETE FROM test_prepare_simple WHERE id=10");
  stmt = mysql_prepare(mysql, query, strlen(query));
  mystmt_init(stmt);

  verify_param_count(stmt,0);

  rc = mysql_execute(stmt);
  mystmt(stmt, rc);
  mysql_stmt_close(stmt);

  /* delete */
  strcpy(query,"DELETE FROM test_prepare_simple WHERE id=?");
  stmt = mysql_prepare(mysql, query, strlen(query));
  mystmt_init(stmt);

  verify_param_count(stmt,1);

  rc = mysql_execute(stmt);
  mystmt_r(stmt, rc);
  mysql_stmt_close(stmt);

  /* select */
  strcpy(query,"SELECT * FROM test_prepare_simple WHERE id=? AND name= ?");
  stmt = mysql_prepare(mysql, query, strlen(query));
  mystmt_init(stmt);

  verify_param_count(stmt,2);

  mysql_stmt_close(stmt);

  /* now fetch the results ..*/
  rc = mysql_commit(mysql);
  myquery(rc);
}


/********************************************************
* to test simple prepare field results                  *
*********************************************************/
static void test_prepare_field_result()
{
  MYSQL_STMT *stmt;
  MYSQL_RES  *result;
  int        rc,param_count;

  myheader("test_prepare_field_result");

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_prepare_field_result");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_prepare_field_result(int_c int, \
                          var_c varchar(50), ts_c timestamp(14),\
                          char_c char(3), date_c date,extra tinyint)");
  myquery(rc); 

  /* insert */
  strcpy(query,"SELECT int_c,var_c,date_c as date,ts_c,char_c FROM \
                  test_prepare_field_result as t1 WHERE int_c=?");
  stmt = mysql_prepare(mysql, query, strlen(query));
  mystmt_init(stmt);

  verify_param_count(stmt,1);

  result = mysql_prepare_result(stmt);
  mytest(result);
  
  my_print_result_metadata(result);

  fprintf(stdout,"\n\n field attributes:\n");
  verify_prepare_field(result,0,"int_c","int_c",MYSQL_TYPE_LONG,
                       "t1","test_prepare_field_result",current_db);
  verify_prepare_field(result,1,"var_c","var_c",MYSQL_TYPE_VAR_STRING,
                       "t1","test_prepare_field_result",current_db);
  verify_prepare_field(result,2,"date","date_c",MYSQL_TYPE_DATE,
                       "t1","test_prepare_field_result",current_db);
  verify_prepare_field(result,3,"ts_c","ts_c",MYSQL_TYPE_TIMESTAMP,
                       "t1","test_prepare_field_result",current_db);
  verify_prepare_field(result,4,"char_c","char_c",MYSQL_TYPE_STRING,
                       "t1","test_prepare_field_result",current_db);

  param_count= mysql_num_fields(result);
  fprintf(stdout,"\n\n total fields: `%d` (expected: `5`)", param_count);
  myassert(param_count == 5);
  mysql_stmt_close(stmt);
}


/********************************************************
* to test simple prepare field results                  *
*********************************************************/
static void test_prepare_syntax()
{
  MYSQL_STMT *stmt;
  int        rc;

  myheader("test_prepare_syntax");

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_prepare_syntax");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_prepare_syntax(id int, name varchar(50), extra int)");
  myquery(rc);

  strcpy(query,"INSERT INTO test_prepare_syntax VALUES(?");
  stmt = mysql_prepare(mysql, query, strlen(query));
  mystmt_init_r(stmt);

  strcpy(query,"SELECT id,name FROM test_prepare_syntax WHERE id=? AND WHERE");
  stmt = mysql_prepare(mysql, query, strlen(query));
  mystmt_init_r(stmt);

  /* now fetch the results ..*/
  rc = mysql_commit(mysql);
  myquery(rc);
}


/********************************************************
* to test simple prepare                                *
*********************************************************/
static void test_prepare()
{
  MYSQL_STMT *stmt;
  int        rc;
  char       query[200];
  int        int_data;
  char       str_data[50];
  char       tiny_data;
  short      small_data;
  longlong   big_data;
  double     real_data;
  double     double_data;
  MYSQL_RES  *result;
  MYSQL_BIND bind[8];

  myheader("test_prepare");

  init_bind(bind);
  rc = mysql_autocommit(mysql, true);
  myquery(rc);

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS my_prepare");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE my_prepare(col1 tinyint,\
                                col2 varchar(15), col3 int,\
                                col4 smallint, col5 bigint, \
                                col6 float, col7 double )");
  myquery(rc);

  /* insert by prepare */
  strcpy(query,"INSERT INTO my_prepare VALUES(?,?,?,?,?,?,?)");
  stmt = mysql_prepare(mysql, query, strlen(query));
  mystmt_init(stmt);

  verify_param_count(stmt,7);

  /* tinyint */
  bind[0].buffer_type=FIELD_TYPE_TINY;
  bind[0].buffer=(gptr)&tiny_data;
  /* string */
  bind[1].buffer_type=FIELD_TYPE_STRING;
  bind[1].buffer=str_data;
  bind[1].buffer_length=sizeof(str_data);
  /* integer */
  bind[2].buffer_type=FIELD_TYPE_LONG;
  bind[2].buffer= (gptr)&int_data;
  /* short */
  bind[3].buffer_type=FIELD_TYPE_SHORT;
  bind[3].buffer= (gptr)&small_data;
  /* bigint */
  bind[4].buffer_type=FIELD_TYPE_LONGLONG;
  bind[4].buffer= (gptr)&big_data;
  /* float */
  bind[5].buffer_type=FIELD_TYPE_DOUBLE;
  bind[5].buffer= (gptr)&real_data;
  /* double */
  bind[6].buffer_type=FIELD_TYPE_DOUBLE;
  bind[6].buffer= (gptr)&double_data;

  rc = mysql_bind_param(stmt,bind);
  mystmt(stmt, rc);

  int_data = 320;
  small_data = 1867;
  big_data   = 1000;
  real_data = 2;
  double_data = 6578.001;

  /* now, execute the prepared statement to insert 10 records.. */
  for (tiny_data=0; tiny_data < 100; tiny_data++)
  {
    bind[1].buffer_length = sprintf(str_data,"MySQL%d",int_data);
    rc = mysql_execute(stmt);
    mystmt(stmt, rc);
    int_data += 25;
    small_data += 10;
    big_data += 100;
    real_data += 1;
    double_data += 10.09;
  }

  mysql_stmt_close(stmt);

  /* now fetch the results ..*/
  rc = mysql_commit(mysql);
  myquery(rc);

  /* test the results now, only one row should exists */
  rc = mysql_query(mysql,"SELECT * FROM my_prepare");
  myquery(rc);

  /* get the result */
  result = mysql_store_result(mysql);
  mytest(result);

  myassert((int)tiny_data == my_process_result_set(result));
  mysql_free_result(result);

}

/********************************************************
* to test double comparision                            *
*********************************************************/
static void test_double_compare()
{
  MYSQL_STMT *stmt;
  int        rc;
  char       query[200],real_data[10], tiny_data;
  double     double_data;
  MYSQL_RES  *result;
  MYSQL_BIND bind[3];

  myheader("test_double_compare");

  init_bind(bind);
  rc = mysql_autocommit(mysql, true);
  myquery(rc);

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_double_compare");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_double_compare(col1 tinyint,\
                                col2 float, col3 double )");
  myquery(rc);

  rc = mysql_query(mysql,"INSERT INTO test_double_compare VALUES(1,10.2,34.5)");
  myquery(rc);

  strcpy(query, "UPDATE test_double_compare SET col1=100 WHERE col1 = ? AND col2 = ? AND COL3 = ?");
  stmt = mysql_prepare(mysql,query, strlen(query));
  mystmt_init(stmt);

  verify_param_count(stmt,3);

  /* tinyint */
  bind[0].buffer_type=FIELD_TYPE_TINY;
  bind[0].buffer=(gptr)&tiny_data;
  /* string->float */
  bind[1].buffer_type=FIELD_TYPE_STRING;
  bind[1].buffer= (gptr)&real_data;
  bind[1].buffer_length=10;  
  /* double */
  bind[2].buffer_type=FIELD_TYPE_DOUBLE;
  bind[2].buffer= (gptr)&double_data;

  tiny_data = 1;
  strcpy(real_data,"10.2");
  double_data = 34.5;
  rc = mysql_bind_param(stmt,bind);
  mystmt(stmt, rc);

  rc = mysql_execute(stmt);
  mystmt(stmt, rc);

  rc = (int)mysql_affected_rows(mysql);
  fprintf(stdout,"\n total affected rows:%d",rc);

  mysql_stmt_close(stmt);

  /* now fetch the results ..*/
  rc = mysql_commit(mysql);
  myquery(rc);

  /* test the results now, only one row should exists */
  rc = mysql_query(mysql,"SELECT * FROM test_double_compare");
  myquery(rc);

  /* get the result */
  result = mysql_store_result(mysql);
  mytest(result);

  myassert((int)tiny_data == my_process_result_set(result));
  mysql_free_result(result);
}

/********************************************************
* to test simple null                                   *
*********************************************************/
static void test_null()
{
  MYSQL_STMT *stmt;
  int        rc;
  int        nData=1;
  MYSQL_RES  *result;
  MYSQL_BIND bind[2];

  myheader("test_null");

  init_bind(bind);
  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_null");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_null(col1 int,col2 varchar(50))");
  myquery(rc);

  /* insert by prepare, wrong column name */
  strcpy(query,"INSERT INTO test_null(col3,col2) VALUES(?,?)");
  stmt = mysql_prepare(mysql, query, strlen(query));
  mystmt_init_r(stmt);

  strcpy(query,"INSERT INTO test_null(col1,col2) VALUES(?,?)");
  stmt = mysql_prepare(mysql, query, strlen(query));
  mystmt_init(stmt);

  verify_param_count(stmt,2);

  bind[0].is_null=1;
  bind[0].buffer_type=MYSQL_TYPE_NULL;
  bind[1]=bind[0];		/* string data */

  rc = mysql_bind_param(stmt,bind);
  mystmt(stmt, rc);

  /* now, execute the prepared statement to insert 10 records.. */
  for (nData=0; nData<9; nData++)
  {
    rc = mysql_execute(stmt);
    mystmt(stmt, rc);
  }
  mysql_stmt_close(stmt);

  /* now fetch the results ..*/
  rc = mysql_commit(mysql);
  myquery(rc);

  /* test the results now, only one row should exists */
  rc = mysql_query(mysql,"SELECT * FROM test_null");
  myquery(rc);

  /* get the result */
  result = mysql_store_result(mysql);
  mytest(result);

  myassert(nData == my_process_result_set(result));
  mysql_free_result(result);
}


/********************************************************
* to test fetch null                                    *
*********************************************************/
static void test_fetch_null()
{
  MYSQL_STMT *stmt;
  int        rc;
  const char query[100];
  int        length[11], i, nData;
  MYSQL_BIND bind[11];

  myheader("test_fetch_null");

  init_bind(bind);
  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_fetch_null");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_fetch_null(col1 tinyint, col2 smallint, \
                                                       col3 int, col4 bigint, \
                                                       col5 float, col6 double, \
                                                       col7 date, col8 time, \
                                                       col9 varbinary(10), \
                                                       col10 varchar(50),\
                                                       col11 char(20))");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"INSERT INTO test_fetch_null(col11) VALUES(1000),(88),(389789)");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  /* fetch */
  for (i=0; i <= 10; i++)
  {
    bind[i].buffer_type=FIELD_TYPE_LONG;
    length[i]=99;
    bind[i].length= (long *)&length[i];
  }
  bind[i-1].buffer=(gptr)&nData;

  strcpy((char *)query , "SELECT * FROM test_fetch_null");

  myassert(3 == my_stmt_result(query,50));

  stmt = mysql_prepare(mysql, query, 50);
  mystmt_init(stmt);

  rc = mysql_bind_result(stmt,bind);
  mystmt(stmt, rc);

  rc = mysql_execute(stmt);
  mystmt(stmt, rc);

  rc= 0;
  while (mysql_fetch(stmt) != MYSQL_NO_DATA)
  {
    rc++;
    for (i=0; i < 10; i++)
    {
      fprintf(stdout, "\n data[%d] : %s", i, 
              length[i] == MYSQL_NULL_DATA ? "NULL" : "NOT NULL");
      myassert(length[i] == MYSQL_NULL_DATA);
    }
    fprintf(stdout, "\n data[%d]: %d", i, nData);
    myassert(nData == 1000 || nData == 88 || nData == 389789);
    myassert(length[i] == 4);
  }
  myassert(rc == 3);
  mysql_stmt_close(stmt);
}

/********************************************************
* to test simple select                                 *
*********************************************************/
static void test_select_version()
{
  MYSQL_STMT *stmt;
  int        rc;
  const char query[100];

  myheader("test_select_version");

  strcpy((char *)query , "SELECT @@version");
  stmt = PREPARE(mysql, query);
  mystmt_init(stmt);

  verify_param_count(stmt,0);

  rc = mysql_execute(stmt);
  mystmt(stmt, rc);

  my_process_stmt_result(stmt);
  mysql_stmt_close(stmt);
}

/********************************************************
* to test simple select                                 *
*********************************************************/
static void test_select_simple()
{
  MYSQL_STMT *stmt;
  int        rc;
  const char query[100];

  myheader("test_select_simple");

  /* insert by prepare */
  strcpy((char *)query, "SHOW TABLES FROM mysql");
  stmt = mysql_prepare(mysql, query, strlen(query));
  mystmt_init(stmt);

  verify_param_count(stmt,0);

  rc = mysql_execute(stmt);
  mystmt(stmt, rc);

  my_process_stmt_result(stmt);
  mysql_stmt_close(stmt);
}

/********************************************************
* to test simple select to debug                        *
*********************************************************/
static void test_select_direct()
{
  int        rc;
  MYSQL_RES  *result;

  myheader("test_select_direct");
  
  rc = mysql_autocommit(mysql,true);
  myquery(rc);

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_select");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_select(id int, id1 tinyint, \
                                                   id2 float, \
                                                   id3 double, \
                                                   name varchar(50))");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  /* insert a row and commit the transaction */
  rc = mysql_query(mysql,"INSERT INTO test_select VALUES(10,5,2.3,4.5,'venu')");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"SELECT * FROM test_select");
  myquery(rc);

  /* get the result */
  result = mysql_store_result(mysql);
  mytest(result);

  my_process_result_set(result);
  mysql_free_result(result);
}

/********************************************************
* to test simple select with prepare                    *
*********************************************************/
static void test_select_prepare()
{
  int        rc, count;
  MYSQL_STMT *stmt;

  myheader("test_select_prepare");
  
  rc = mysql_autocommit(mysql,true);
  myquery(rc);

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_select");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_select(id int, name varchar(50))");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  /* insert a row and commit the transaction */
  rc = mysql_query(mysql,"INSERT INTO test_select VALUES(10,'venu')");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  stmt = mysql_prepare(mysql,"SELECT * FROM test_select",50);
  mystmt_init(stmt);
  
  rc = mysql_execute(stmt);
  mystmt(stmt,rc);

  count= my_process_stmt_result(stmt);

  rc = mysql_query(mysql,"DROP TABLE test_select");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_select(id tinyint, id1 int, \
                                                   id2 float, id3 float, \
                                                   name varchar(50))");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  /* insert a row and commit the transaction */
  rc = mysql_query(mysql,"INSERT INTO test_select(id,id1,id2,name) VALUES(10,5,2.3,'venu')");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  stmt = mysql_prepare(mysql,"SELECT * FROM test_select",25);
  mystmt_init(stmt);

  rc = mysql_execute(stmt);
  mystmt(stmt,rc);

  my_process_stmt_result(stmt);
}

/********************************************************
* to test simple select                                 *
*********************************************************/
static void test_select()
{
  MYSQL_STMT *stmt;
  int        rc;
  char       szData[25];
  int        nData=1;
  MYSQL_BIND bind[2];

  myheader("test_select");

  init_bind(bind);
  rc = mysql_autocommit(mysql,true);
  myquery(rc);

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_select");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_select(id int,name varchar(50))");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  /* insert a row and commit the transaction */
  rc = mysql_query(mysql,"INSERT INTO test_select VALUES(10,'venu')");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

    /* now insert the second row, and rollback the transaction */
  rc = mysql_query(mysql,"INSERT INTO test_select VALUES(20,'mysql')");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  strcpy(query,"SELECT * FROM test_select WHERE id=? AND name=?");
  stmt = mysql_prepare(mysql, query, strlen(query));
  mystmt_init(stmt);

  verify_param_count(stmt,2);

  /* string data */
  nData=10;
  strcpy(szData,(char *)"venu");
  bind[1].buffer_type=FIELD_TYPE_STRING;
  bind[1].buffer=szData;
  bind[1].buffer_length=4;
  bind[0].buffer=(gptr)&nData;
  bind[0].buffer_type=FIELD_TYPE_LONG;

  rc = mysql_bind_param(stmt,bind);
  mystmt(stmt, rc);

  rc = mysql_execute(stmt);
  mystmt(stmt, rc);

  myassert( 1 == my_process_stmt_result(stmt));

  mysql_stmt_close(stmt);
}

/********************************************************
* to test simple select show                            *
*********************************************************/
static void test_select_show()
{
  MYSQL_STMT *stmt;
  int        rc;
  MYSQL_RES  *result;

  myheader("test_select_show");

  mysql_autocommit(mysql,true);

  strcpy(query,"SELECT * FROM mysql.host");
  stmt = mysql_prepare(mysql, query, strlen(query));
  mystmt_init(stmt);

  verify_param_count(stmt,0);

  rc = mysql_execute(stmt);
  mystmt(stmt, rc);

  /* get the result */
  result = mysql_store_result(mysql);
  mytest(result);

  my_process_result_set(result);
  mysql_free_result(result);

  mysql_stmt_close(stmt);
}

/********************************************************
* to test simple update                                *
*********************************************************/
static void test_simple_update()
{
  MYSQL_STMT *stmt;
  int        rc;
  char       szData[25];
  int        nData=1;
  MYSQL_RES  *result;
  MYSQL_BIND bind[2];   

  myheader("test_simple_update");

  init_bind(bind);
  rc = mysql_autocommit(mysql,true);
  myquery(rc);

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_update");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_update(col1 int,\
                                col2 varchar(50), col3 int )");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"INSERT INTO test_update VALUES(1,'MySQL',100)");
  myquery(rc);

  myassert(1 == mysql_affected_rows(mysql));

  rc = mysql_commit(mysql);
  myquery(rc);

  /* insert by prepare */
  strcpy(query,"UPDATE test_update SET col2=? WHERE col1=?");
  stmt = mysql_prepare(mysql, query, strlen(query));
  mystmt_init(stmt);

  verify_param_count(stmt,2);

  nData=1;
  bind[0].buffer_type=FIELD_TYPE_STRING;
  bind[0].buffer=szData;		/* string data */
  bind[0].buffer_length=sprintf(szData,"updated-data");
  bind[1].buffer=(gptr)&nData;
  bind[1].buffer_type=FIELD_TYPE_LONG;

  rc = mysql_bind_param(stmt,bind);
  mystmt(stmt, rc);

  rc = mysql_execute(stmt);
  mystmt(stmt, rc);
  myassert(1 == mysql_affected_rows(mysql));

  mysql_stmt_close(stmt);

  /* now fetch the results ..*/
  rc = mysql_commit(mysql);
  myquery(rc);

  /* test the results now, only one row should exists */
  rc = mysql_query(mysql,"SELECT * FROM test_update");
  myquery(rc);

  /* get the result */
  result = mysql_store_result(mysql);
  mytest(result);

  myassert(1 == my_process_result_set(result));
  mysql_free_result(result);
}


/********************************************************
* to test simple long data handling                     *
*********************************************************/
static void test_long_data()
{
  MYSQL_STMT *stmt;
  int        rc, int_data;
  char       *data=NullS;
  long       length;
  MYSQL_RES  *result;
  MYSQL_BIND bind[3];


  myheader("test_long_data");

  init_bind(bind);
  rc = mysql_autocommit(mysql,true);
  myquery(rc);

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_long_data");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_long_data(col1 int,\
                                col2 long varchar, col3 long varbinary)");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  strcpy(query,"INSERT INTO test_long_data(col1,col2) VALUES(?)");
  stmt = mysql_prepare(mysql, query, strlen(query));
  mystmt_init_r(stmt);

  strcpy(query,"INSERT INTO test_long_data(col1,col2,col3) VALUES(?,?,?)");
  stmt = mysql_prepare(mysql, query, strlen(query));
  mystmt_init(stmt);

  verify_param_count(stmt,3);

  bind[0].buffer=(char *)&int_data;
  bind[0].buffer_type=FIELD_TYPE_LONG;

  /* Non string or binary type, error */
  bind[1].buffer_type=FIELD_TYPE_LONG;
  bind[1].length=&length;
  length= MYSQL_LONG_DATA;   /* specify long data suppy during run-time */
  rc = mysql_bind_param(stmt,bind);
  fprintf(stdout," mysql_bind_param() returned: %d\n",rc);
  mystmt_r(stmt, rc);

  bind[1].buffer_type=FIELD_TYPE_STRING;
  bind[2]=bind[1];
  rc = mysql_bind_param(stmt,bind);
  mystmt(stmt, rc);

  int_data= 999;
  rc = mysql_execute(stmt);
  fprintf(stdout," mysql_execute() returned %d\n",rc);
  myassert(rc == MYSQL_NEED_DATA);

  data = (char *)"Micheal";

  /* supply data in pieces */
  rc = mysql_send_long_data(stmt,1,data,7,0);
  mystmt(stmt, rc);

  /* try to execute mysql_execute() now, it should return
     MYSQL_NEED_DATA as the long data supply is not yet over
  */
  rc = mysql_execute(stmt);
  fprintf(stdout," mysql_execute() returned %d\n",rc);
  myassert(rc == MYSQL_NEED_DATA);

  /* append data again ..*/

  /* Indicate end of data */
  data = (char *)" 'monty' Widenius";
  rc = mysql_send_long_data(stmt,1,data,17,1);
  mystmt(stmt, rc);

  rc = mysql_send_long_data(stmt,2,"Venu (venu@mysql.com)",4,1);
  mystmt(stmt, rc);

  /* execute */
  rc = mysql_execute(stmt);
  fprintf(stdout," mysql_execute() returned %d\n",rc);
  mystmt(stmt,rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  /* now fetch the results ..*/
  rc = mysql_query(mysql,"SELECT * FROM test_long_data");
  myquery(rc);

  /* get the result */
  result = mysql_store_result(mysql);
  mytest(result);

  myassert(1 == my_process_result_set(result));
  mysql_free_result(result);

  verify_col_data("test_long_data","col1","999");
  verify_col_data("test_long_data","col2","Micheal 'monty' Widenius");
  verify_col_data("test_long_data","col3","Venu");
}

/********************************************************
* to test long data (string) handling                   *
*********************************************************/
static void test_long_data_str()
{
  MYSQL_STMT *stmt;
  int        rc, i;
  char       data[255];
  long       length, length1;
  MYSQL_RES  *result;
  MYSQL_BIND bind[2];


  myheader("test_long_data_str");

  init_bind(bind);
  rc = mysql_autocommit(mysql,true);
  myquery(rc);

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_long_data_str");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_long_data_str(id int, longstr long varchar)");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  strcpy(query,"INSERT INTO test_long_data_str VALUES(?,?)");
  stmt = mysql_prepare(mysql, query, strlen(query));
  mystmt_init(stmt);

  verify_param_count(stmt,2);

  bind[0].buffer = (gptr)&length;
  bind[0].buffer_type = FIELD_TYPE_LONG;

  bind[1].buffer=data;		  /* string data */
  bind[1].buffer_type=FIELD_TYPE_STRING;
  bind[1].length= &length1;
  length1= MYSQL_LONG_DATA;
  rc = mysql_bind_param(stmt,bind);
  mystmt(stmt, rc);

  length = 10;
  rc = mysql_execute(stmt);
  fprintf(stdout," mysql_execute() returned %d\n",rc);
  myassert(rc == MYSQL_NEED_DATA);

  length = 40;
  sprintf(data,"MySQL AB");

  /* supply data in pieces */
  for(i=0; i < 4; i++)
  {
    rc = mysql_send_long_data(stmt,1,(char *)data,5,0);
    mystmt(stmt, rc);
  }
  /* try to execute mysql_execute() now, it should return
     MYSQL_NEED_DATA as the long data supply is not yet over
   */
  rc = mysql_execute(stmt);
  fprintf(stdout," mysql_execute() returned %d\n",rc);
  myassert(rc == MYSQL_NEED_DATA); 

  /* Indiate end of data supply */
  rc = mysql_send_long_data(stmt,1,0,0,1);
  mystmt(stmt, rc);

  /* execute */
  rc = mysql_execute(stmt);
  fprintf(stdout," mysql_execute() returned %d\n",rc);
  mystmt(stmt,rc);

  mysql_stmt_close(stmt);

  rc = mysql_commit(mysql);
  myquery(rc);

  /* now fetch the results ..*/
  rc = mysql_query(mysql,"SELECT LENGTH(longstr), longstr FROM test_long_data_str");
  myquery(rc);

  /* get the result */
  result = mysql_store_result(mysql);
  mytest(result);

  myassert(1 == my_process_result_set(result));
  mysql_free_result(result);

  sprintf(data,"%d", i*5);
  verify_col_data("test_long_data_str","LENGTH(longstr)", data);
  data[0]='\0';
  while (i--)
   sprintf(data,"%s%s", data,"MySQL");
  verify_col_data("test_long_data_str","longstr", data);
}


/********************************************************
* to test long data (string) handling                   *
*********************************************************/
static void test_long_data_str1()
{
  MYSQL_STMT *stmt;
  int        rc, i;
  char       data[255];
  long       length, length1;
  MYSQL_RES  *result;
  MYSQL_BIND bind[2];


  myheader("test_long_data_str1");

  init_bind(bind);
  rc = mysql_autocommit(mysql,true);
  myquery(rc);

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_long_data_str");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_long_data_str(longstr long varchar,blb long varbinary)");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  strcpy(query,"INSERT INTO test_long_data_str VALUES(?,?)");
  stmt = mysql_prepare(mysql, query, strlen(query));
  mystmt_init(stmt);

  verify_param_count(stmt,2);

  bind[0].buffer=data;		  /* string data */
  bind[0].length= &length1;
  bind[0].buffer_type=FIELD_TYPE_STRING;
  length1= MYSQL_LONG_DATA;

  bind[1] = bind[0];
  bind[1].buffer_type=FIELD_TYPE_BLOB;

  rc = mysql_bind_param(stmt,bind);
  mystmt(stmt, rc);

  length = 10;
  rc = mysql_execute(stmt);
  fprintf(stdout," mysql_execute() returned %d\n",rc);
  myassert(rc == MYSQL_NEED_DATA);

  length = sprintf(data,"MySQL AB");

  /* supply data in pieces */
  for(i=0; i < 3; i++)
  {
    rc = mysql_send_long_data(stmt,0,data,length,0);
    mystmt(stmt, rc);

    rc = mysql_send_long_data(stmt,1,data,2,0);
    mystmt(stmt, rc);
  }
  /* try to execute mysql_execute() now, it should return
     MYSQL_NEED_DATA as the long data supply is not yet over
  */
  rc = mysql_execute(stmt);
  fprintf(stdout," mysql_execute() returned %d\n",rc);
  myassert(rc == MYSQL_NEED_DATA);
 
  /* Indiate end of data supply */
  rc = mysql_send_long_data(stmt,1,0,0,1);
  mystmt(stmt, rc);

  rc = mysql_execute(stmt);
  fprintf(stdout," mysql_execute() returned %d\n",rc);
  myassert(rc == MYSQL_NEED_DATA);

  rc = mysql_send_long_data(stmt,0,0,0,1);
  mystmt(stmt, rc);

  /* execute */
  rc = mysql_execute(stmt);
  fprintf(stdout," mysql_execute() returned %d\n",rc);
  mystmt(stmt,rc);

  mysql_stmt_close(stmt);

  rc = mysql_commit(mysql);
  myquery(rc);

  /* now fetch the results ..*/
  rc = mysql_query(mysql,"SELECT LENGTH(longstr),longstr,LENGTH(blb),blb FROM test_long_data_str");
  myquery(rc);

  /* get the result */
  result = mysql_store_result(mysql);
  mytest(result);

  myassert(1 == my_process_result_set(result));
  mysql_free_result(result);

  sprintf(data,"%ld",(long)i*length);
  verify_col_data("test_long_data_str","length(longstr)",data);

  sprintf(data,"%d",i*2);
  verify_col_data("test_long_data_str","length(blb)",data);
}


/********************************************************
* to test long data (binary) handling                   *
*********************************************************/
static void test_long_data_bin()
{
  MYSQL_STMT *stmt;
  int        rc;
  char       data[255];
  long       length, length1;
  MYSQL_RES  *result;
  MYSQL_BIND bind[2];


  myheader("test_long_data_bin");

  init_bind(bind);
  rc = mysql_autocommit(mysql,true);
  myquery(rc);

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_long_data_bin");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_long_data_bin(id int, longbin long varbinary)");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  strcpy(query,"INSERT INTO test_long_data_bin VALUES(?,?)");
  stmt = mysql_prepare(mysql, query, strlen(query));
  mystmt_init(stmt);

  verify_param_count(stmt,2);

  bind[0].buffer = (gptr)&length;
  bind[0].buffer_type = FIELD_TYPE_LONG;

  bind[1].buffer=data;		  /* string data */
  bind[1].buffer_type=FIELD_TYPE_LONG_BLOB;
  bind[1].length= &length1;
  length1= MYSQL_LONG_DATA;
  rc = mysql_bind_param(stmt,bind);
  mystmt(stmt, rc);

  length = 10;
  rc = mysql_execute(stmt);
  fprintf(stdout," mysql_execute() returned %d\n",rc);
  myassert(rc == MYSQL_NEED_DATA);

  sprintf(data,"MySQL AB");

  /* supply data in pieces */
  {
    int i;
    for(i=0; i < 100; i++)
    {
      rc = mysql_send_long_data(stmt,1,(char *)data,4,0);
      mystmt(stmt, rc);
    }

    /* try to execute mysql_execute() now, it should return
       MYSQL_NEED_DATA as the long data supply is not yet over
    */
    rc = mysql_execute(stmt);
    fprintf(stdout," mysql_execute() returned %d\n",rc);
    myassert(rc == MYSQL_NEED_DATA);
  }

  /* Indiate end of data supply */
  rc = mysql_send_long_data(stmt,1,0,0,1);
  mystmt(stmt, rc);

  /* execute */
  rc = mysql_execute(stmt);
  fprintf(stdout," mysql_execute() returned %d\n",rc);
  mystmt(stmt,rc);

  mysql_stmt_close(stmt);

  rc = mysql_commit(mysql);
  myquery(rc);

  /* now fetch the results ..*/
  rc = mysql_query(mysql,"SELECT LENGTH(longbin), longbin FROM test_long_data_bin");
  myquery(rc);

  /* get the result */
  result = mysql_store_result(mysql);
  mytest(result);

  myassert(1 == my_process_result_set(result));
  mysql_free_result(result);
}


/********************************************************
* to test simple delete                                 *
*********************************************************/
static void test_simple_delete()
{
  MYSQL_STMT *stmt;
  int        rc;
  char       szData[30]={0};
  int        nData=1;
  MYSQL_RES  *result;
  MYSQL_BIND bind[2];


  myheader("test_simple_delete");

  init_bind(bind);
  rc = mysql_autocommit(mysql,true);
  myquery(rc);

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_simple_delete");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_simple_delete(col1 int,\
                                col2 varchar(50), col3 int )");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"INSERT INTO test_simple_delete VALUES(1,'MySQL',100)");
  myquery(rc);

  myassert(1 == mysql_affected_rows(mysql));

  rc = mysql_commit(mysql);
  myquery(rc);

  /* insert by prepare */
  strcpy(query,"DELETE FROM test_simple_delete WHERE col1=? AND col2=? AND col3=100");
  stmt = mysql_prepare(mysql, query, strlen(query));
  mystmt_init(stmt);

  verify_param_count(stmt,2);

  nData=1;
  strcpy(szData,"MySQL");
  bind[1].buffer_length = 5;
  bind[1].buffer_type=FIELD_TYPE_STRING;
  bind[1].buffer=szData;		/* string data */
  bind[0].buffer=(gptr)&nData;
  bind[0].buffer_type=FIELD_TYPE_LONG;

  rc = mysql_bind_param(stmt,bind);
  mystmt(stmt, rc);

  rc = mysql_execute(stmt);
  mystmt(stmt, rc);
  myassert(1 == mysql_affected_rows(mysql));

  mysql_stmt_close(stmt);

  /* now fetch the results ..*/
  rc = mysql_commit(mysql);
  myquery(rc);

  /* test the results now, only one row should exists */
  rc = mysql_query(mysql,"SELECT * FROM test_simple_delete");
  myquery(rc);

  /* get the result */
  result = mysql_store_result(mysql);
  mytest(result);

  myassert(0 == my_process_result_set(result));
  mysql_free_result(result);
}



/********************************************************
* to test simple update                                 *
*********************************************************/
static void test_update()
{
  MYSQL_STMT *stmt;
  int        rc;
  char       szData[25];
  int        nData=1;
  MYSQL_RES  *result;
  MYSQL_BIND bind[2];


  myheader("test_update");

  init_bind(bind);
  rc = mysql_autocommit(mysql,true);
  myquery(rc);

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_update");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_update(col1 int primary key auto_increment,\
                                col2 varchar(50), col3 int )");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  strcpy(query,"INSERT INTO test_update(col2,col3) VALUES(?,?)");
  stmt = mysql_prepare(mysql, query, strlen(query));
  mystmt_init(stmt);

  verify_param_count(stmt,2);

  /* string data */
  bind[0].buffer_type=FIELD_TYPE_STRING;
  bind[0].buffer=szData;
  bind[0].buffer_length=sprintf(szData,"inserted-data");
  bind[1].buffer=(gptr)&nData;
  bind[1].buffer_type=FIELD_TYPE_LONG;

  rc = mysql_bind_param(stmt,bind);
  mystmt(stmt, rc);

  nData=100;
  rc = mysql_execute(stmt);
  mystmt(stmt, rc);

  myassert(1 == mysql_affected_rows(mysql));
  mysql_stmt_close(stmt);

  strcpy(query,"UPDATE test_update SET col2=? WHERE col3=?");
  stmt = mysql_prepare(mysql, query, strlen(query));
  mystmt_init(stmt);

  verify_param_count(stmt,2);
  nData=100;


  bind[0].buffer_type=FIELD_TYPE_STRING;
  bind[0].buffer=szData;
  bind[0].buffer_length=sprintf(szData,"updated-data");
  bind[1].buffer=(gptr)&nData;
  bind[1].buffer_type=FIELD_TYPE_LONG;

  rc = mysql_bind_param(stmt,bind);
  mystmt(stmt, rc);

  rc = mysql_execute(stmt);
  mystmt(stmt, rc);
  myassert(1 == mysql_affected_rows(mysql));

  mysql_stmt_close(stmt);

  /* now fetch the results ..*/
  rc = mysql_commit(mysql);
  myquery(rc);

  /* test the results now, only one row should exists */
  rc = mysql_query(mysql,"SELECT * FROM test_update");
  myquery(rc);

  /* get the result */
  result = mysql_store_result(mysql);
  mytest(result);

  myassert(1 == my_process_result_set(result));
  mysql_free_result(result);
}


/********************************************************
* to test simple prepare                                *
*********************************************************/
static void test_prepare_noparam()
{
  MYSQL_STMT *stmt;
  int        rc;
  MYSQL_RES  *result;

  myheader("test_prepare_noparam");

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS my_prepare");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE my_prepare(col1 int ,col2 varchar(50))");
  myquery(rc);


  /* insert by prepare */
  strcpy(query,"INSERT INTO my_prepare VALUES(10,'venu')");
  stmt = mysql_prepare(mysql, query, strlen(query));
  mystmt_init(stmt);

  verify_param_count(stmt,0);

  rc = mysql_execute(stmt);
  mystmt(stmt, rc);

  mysql_stmt_close(stmt);

  /* now fetch the results ..*/
  rc = mysql_commit(mysql);
  myquery(rc);

  /* test the results now, only one row should exists */
  rc = mysql_query(mysql,"SELECT * FROM my_prepare");
  myquery(rc);

  /* get the result */
  result = mysql_store_result(mysql);
  mytest(result);

  myassert(1 == my_process_result_set(result));
  mysql_free_result(result);
}


/********************************************************
* to test simple bind result                            *
*********************************************************/
static void test_bind_result()
{
  MYSQL_STMT *stmt;
  int        rc;
  const char query[100];
  int        nData, length, length1;
  char       szData[100];
  MYSQL_BIND bind[2];

  myheader("test_bind_result");

  init_bind(bind);
  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_bind_result");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_bind_result(col1 int ,col2 varchar(50))");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"INSERT INTO test_bind_result VALUES(10,'venu')");
  myquery(rc);

  rc = mysql_query(mysql,"INSERT INTO test_bind_result VALUES(20,'MySQL')");
  myquery(rc);

  rc = mysql_query(mysql,"INSERT INTO test_bind_result(col2) VALUES('monty')");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  /* fetch */

  bind[0].buffer_type=FIELD_TYPE_LONG;
  bind[0].buffer= (gptr) &nData;	/* integer data */
  bind[0].length= (long *)&length;
  bind[1].buffer_type=FIELD_TYPE_STRING;
  bind[1].buffer=szData;		/* string data */
  bind[1].buffer_length=sizeof(szData);
  bind[1].length=(long *)&length1;

  strcpy((char *)query , "SELECT * FROM test_bind_result");
  stmt = mysql_prepare(mysql, query, strlen(query));
  mystmt_init(stmt);

  rc = mysql_bind_result(stmt,bind);
  mystmt(stmt, rc);

  rc = mysql_execute(stmt);
  mystmt(stmt, rc);

  rc = mysql_fetch(stmt);
  mystmt(stmt,rc);

  fprintf(stdout,"\n row 1: %d,%s(%d)",nData, szData, length1);
  myassert(nData == 10);
  myassert(strcmp(szData,"venu")==0);
  myassert(length1 == 4);

  rc = mysql_fetch(stmt);
  mystmt(stmt,rc);

  fprintf(stdout,"\n row 2: %d,%s(%d)",nData, szData, length1);
  myassert(nData == 20);
  myassert(strcmp(szData,"MySQL")==0);
  myassert(length1 == 5);

  length=99;
  rc = mysql_fetch(stmt);
  mystmt(stmt,rc);
 
  if (length == MYSQL_NULL_DATA)
    fprintf(stdout,"\n row 3: NULL,%s(%d)", szData, length1);
  else
    fprintf(stdout,"\n row 3: %d,%s(%d)", nData, szData, length1);
  myassert(length == MYSQL_NULL_DATA);
  myassert(strcmp(szData,"monty")==0);
  myassert(length1 == 5);

  rc = mysql_fetch(stmt);
  myassert(rc == MYSQL_NO_DATA);

  mysql_stmt_close(stmt);
}


/********************************************************
* to test ext bind result                               *
*********************************************************/
static void test_bind_result_ext()
{
  MYSQL_STMT *stmt;
  int        rc;
  const char query[100];
  uchar      t_data;
  short      s_data;
  int        i_data;
  longlong   b_data;
  float      f_data;
  double     d_data;
  char       szData[20], bData[20];
  int        szLength, bLength;
  MYSQL_BIND bind[8];

  myheader("test_bind_result_ext");

  init_bind(bind);
  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_bind_result");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_bind_result(c1 tinyint, c2 smallint, \
                                                        c3 int, c4 bigint, \
                                                        c5 float, c6 double, \
                                                        c7 varbinary(10), \
                                                        c8 varchar(50))");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"INSERT INTO test_bind_result VALUES(19,2999,3999,4999999,\
                                                              2345.6,5678.89563,\
                                                              'venu','mysql')");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  bind[0].buffer_type=MYSQL_TYPE_TINY;
  bind[0].buffer=(gptr)&t_data;

  bind[1].buffer_type=MYSQL_TYPE_SHORT;
  bind[2].buffer_type=MYSQL_TYPE_LONG;
  
  bind[3].buffer_type=MYSQL_TYPE_LONGLONG;
  bind[1].buffer=(gptr)&s_data;
  
  bind[2].buffer=(gptr)&i_data;
  bind[3].buffer=(gptr)&b_data;

  bind[4].buffer_type=MYSQL_TYPE_FLOAT;
  bind[4].buffer=(gptr)&f_data;

  bind[5].buffer_type=MYSQL_TYPE_DOUBLE;
  bind[5].buffer=(gptr)&d_data;

  bind[6].buffer_type=MYSQL_TYPE_STRING;
  bind[6].buffer=(gptr)&szData;
  bind[6].length=(long *)&szLength;

  bind[7].buffer_type=MYSQL_TYPE_TINY_BLOB;
  bind[7].buffer=(gptr)&bData;
  bind[7].length=(long *)&bLength;

  strcpy((char *)query , "SELECT * FROM test_bind_result");
  stmt = mysql_prepare(mysql, query, strlen(query));
  mystmt_init(stmt);

  rc = mysql_bind_result(stmt,bind);
  mystmt(stmt, rc);

  rc = mysql_execute(stmt);
  mystmt(stmt, rc);

  rc = mysql_fetch(stmt);
  mystmt(stmt,rc);

  fprintf(stdout, "\n data (tiny)   : %d", t_data);
  fprintf(stdout, "\n data (short)  : %d", s_data);
  fprintf(stdout, "\n data (int)    : %d", i_data);
  fprintf(stdout, "\n data (big)    : %lld", b_data);

  fprintf(stdout, "\n data (float)  : %f", f_data);
  fprintf(stdout, "\n data (double) : %f", d_data);

  fprintf(stdout, "\n data (str)    : %s(%d)", szData, szLength);
  fprintf(stdout, "\n data (bin)    : %s(%d)", bData, bLength);


  myassert(t_data == 19);
  myassert(s_data == 2999);
  myassert(i_data == 3999);
  myassert(b_data == 4999999);
  /*myassert(f_data == 2345.60);*/
  /*myassert(d_data == 5678.89563);*/
  myassert(strcmp(szData,"venu")==0);
  myassert(strcmp(bData,"mysql")==0);
  myassert(szLength == 4);
  myassert(bLength == 5);

  rc = mysql_fetch(stmt);
  myassert(rc == MYSQL_NO_DATA);

  mysql_stmt_close(stmt);
}


/********************************************************
* to test ext bind result                               *
*********************************************************/
static void test_bind_result_ext1()
{
  MYSQL_STMT *stmt;
  int        rc;
  const char query[100];
  char       t_data[20];
  float      s_data;
  short      i_data;
  short      b_data;
  int        f_data;
  long       bData;
  long       length[11];
  char       d_data[20];
  double     szData;
  MYSQL_BIND bind[8];

  myheader("test_bind_result_ext1");

  init_bind(bind);
  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_bind_result");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_bind_result(c1 tinyint, c2 smallint, \
                                                        c3 int, c4 bigint, \
                                                        c5 float, c6 double, \
                                                        c7 varbinary(10), \
                                                        c8 varchar(10))");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"INSERT INTO test_bind_result VALUES(120,2999,3999,54,\
                                                              2.6,58.89,\
                                                              '206','6.7')");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  bind[0].buffer_type=MYSQL_TYPE_STRING;
  bind[0].buffer=(gptr)t_data;

  for (rc=0; rc <= 7; rc++)
    bind[rc].length= &length[rc];

  bind[1].buffer_type=MYSQL_TYPE_FLOAT;
  bind[1].buffer=(gptr)&s_data;

  bind[2].buffer_type=MYSQL_TYPE_SHORT;
  bind[2].buffer=(gptr)&i_data;

  bind[3].buffer_type=MYSQL_TYPE_TINY;
  bind[3].buffer=(gptr)&b_data;

  bind[4].buffer_type=MYSQL_TYPE_LONG;
  bind[4].buffer=(gptr)&f_data;

  bind[5].buffer_type=MYSQL_TYPE_STRING;
  bind[5].buffer=(gptr)d_data;

  bind[6].buffer_type=MYSQL_TYPE_LONG;
  bind[6].buffer=(gptr)&bData;

  bind[7].buffer_type=MYSQL_TYPE_DOUBLE;
  bind[7].buffer=(gptr)&szData;

  strcpy((char *)query , "SELECT * FROM test_bind_result");
  stmt = mysql_prepare(mysql, query, strlen(query));
  mystmt_init(stmt);

  rc = mysql_bind_result(stmt,bind);
  mystmt(stmt, rc);

  rc = mysql_execute(stmt);
  mystmt(stmt, rc);

  rc = mysql_fetch(stmt);
  mystmt(stmt,rc);

  fprintf(stdout, "\n data (tiny)   : %s(%ld)", t_data, length[0]);
  fprintf(stdout, "\n data (short)  : %f(%ld)", s_data, length[1]);
  fprintf(stdout, "\n data (int)    : %d(%ld)", i_data, length[2]);
  fprintf(stdout, "\n data (big)    : %d(%ld)", b_data, length[3]);

  fprintf(stdout, "\n data (float)  : %d(%ld)", f_data, length[4]);
  fprintf(stdout, "\n data (double) : %s(%ld)", d_data, length[5]);

  fprintf(stdout, "\n data (bin)    : %ld(%ld)", bData, length[6]);
  fprintf(stdout, "\n data (str)    : %g(%ld)", szData, length[7]);

  myassert(strcmp(t_data,"120")==0);
  myassert(i_data == 3999);
  myassert(f_data == 2);
  myassert(strcmp(d_data,"58.89")==0);

  myassert(length[0] == 3);
  myassert(length[1] == 4);
  myassert(length[2] == 2);
  myassert(length[3] == 1);
  myassert(length[4] == 4);
  myassert(length[5] == 5);
  myassert(length[6] == 4);
  myassert(length[7] == 8);

  rc = mysql_fetch(stmt);
  myassert(rc == MYSQL_NO_DATA);

  mysql_stmt_close(stmt);
}

/********************************************************
* to test fetching of date, time and ts                 *
*********************************************************/
static void test_fetch_date()
{
  MYSQL_STMT *stmt;
  int        rc, year;
  char       date[25], time[25], ts[25], ts_4[15], ts_6[20], dt[20];
  int        d_length, t_length, ts_length, ts4_length, ts6_length, 
             dt_length, y_length;

  MYSQL_BIND bind[3];

  myheader("test_fetch_date");

  init_bind(bind);
  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_bind_result");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_bind_result(c1 date, c2 time, \
                                                        c3 timestamp(14), \
                                                        c4 year, \
                                                        c5 datetime, \
                                                        c6 timestamp(4), \
                                                        c7 timestamp(6))");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"INSERT INTO test_bind_result VALUES('2002-01-02',\
                                                              '12:49:00',\
                                                              '2002-01-02 17:46:59', \
                                                              2010,\
                                                              '2010-07-10', \
                                                              '2020','1999-12-29')");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  bind[0].buffer_type=MYSQL_TYPE_STRING;
  bind[1]=bind[2]=bind[0];

  bind[0].buffer=(gptr)&date;
  bind[0].length=(long *)&d_length;

  bind[1].buffer=(gptr)&time;
  bind[1].length=(long *)&t_length;

  bind[2].buffer=(gptr)&ts;
  bind[2].length=(long *)&ts_length;

  bind[3].buffer_type=MYSQL_TYPE_LONG;
  bind[3].buffer=(gptr)&year;
  bind[3].length=(long *)&y_length;

  bind[4].buffer_type=MYSQL_TYPE_STRING;
  bind[4].buffer=(gptr)&dt;
  bind[4].length=(long *)&dt_length;

  bind[5].buffer_type=MYSQL_TYPE_STRING;
  bind[5].buffer=(gptr)&ts_4;
  bind[5].length=(long *)&ts4_length;

  bind[6].buffer_type=MYSQL_TYPE_STRING;
  bind[6].buffer=(gptr)&ts_6;
  bind[6].length=(long *)&ts6_length;

  myassert(1 == my_stmt_result("SELECT * FROM test_bind_result",50));

  stmt = mysql_prepare(mysql, "SELECT * FROM test_bind_result", 50);
  mystmt_init(stmt);

  rc = mysql_bind_result(stmt,bind);
  mystmt(stmt, rc);

  rc = mysql_execute(stmt);
  mystmt(stmt, rc);
  
  ts_4[0]='\0';
  rc = mysql_fetch(stmt);
  mystmt(stmt,rc);

  fprintf(stdout, "\n date   : %s(%d)", date, d_length);
  fprintf(stdout, "\n time   : %s(%d)", time, t_length);
  fprintf(stdout, "\n ts     : %s(%d)", ts, ts_length);
  fprintf(stdout, "\n year   : %d(%d)", year, y_length);
  fprintf(stdout, "\n dt     : %s(%d)", dt,  dt_length);
  fprintf(stdout, "\n ts(4)  : %s(%d)", ts_4, ts4_length);
  fprintf(stdout, "\n ts(6)  : %s(%d)", ts_6, ts6_length);

  myassert(strcmp(date,"2002-01-02")==0);
  myassert(d_length == 10);

  myassert(strcmp(time,"12:49:00")==0);
  myassert(t_length == 8);

  myassert(strcmp(ts,"2002-01-02 17:46:59")==0);
  myassert(ts_length == 19);

  myassert(year == 2010);
  myassert(y_length == 4);
  
  myassert(strcmp(dt,"2010-07-10")==0);
  myassert(dt_length == 10);

  myassert(ts_4[0] == '\0');
  myassert(ts4_length == 0);

  myassert(strcmp(ts_6,"1999-12-29")==0);
  myassert(ts6_length == 10);

  rc = mysql_fetch(stmt);
  myassert(rc == MYSQL_NO_DATA);

  mysql_stmt_close(stmt);
}

/********************************************************
* to test fetching of str to all types                  *
*********************************************************/
static void test_fetch_str()
{
  MYSQL_STMT   *stmt;
  int          rc, i, round, bit;
  long         data[10], length[10];
  float        f_data;
  double       d_data;
  char         s_data[10];
  MYSQL_BIND   bind[7];

  myheader("test_fetch_str");

  init_bind(bind);
  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_bind_str");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_bind_str(c1 char(10),\
                                                     c2 char(10),\
                                                     c3 char(20),\
                                                     c4 char(20),\
                                                     c5 char(30),\
                                                     c6 char(40),\
                                                     c7 char(20))");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  stmt = mysql_prepare(mysql,"INSERT INTO test_bind_str VALUES(?,?,?,?,?,?,?)",100);
  myquery(rc);

  verify_param_count(stmt, 7);

  round= 0;
  for (i=0; i < 7; i++)
  {  
    bind[i].buffer_type= MYSQL_TYPE_LONG;
    bind[i].buffer= (void *)&data[i];
    data[i]= round+i+1;
    round= (round +10)*10;
  }   
  rc = mysql_bind_param(stmt, bind);
  mystmt(stmt,rc);
  
  rc = mysql_execute(stmt);
  mystmt(stmt, rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  mysql_stmt_close(stmt);

  myassert(1 == my_stmt_result("SELECT * FROM test_bind_str",50));

  stmt = mysql_prepare(mysql,"SELECT * FROM test_bind_str",50);
  myquery(rc);

  for (i=0; i < 7; i++)
  {
    bind[i].buffer= (void *)&data[i];
    bind[i].length= (long *)&length[i];
  }
  bind[0].buffer_type= MYSQL_TYPE_TINY;
  bind[1].buffer_type= MYSQL_TYPE_SHORT;
  bind[2].buffer_type= MYSQL_TYPE_LONG;
  bind[3].buffer_type= MYSQL_TYPE_LONGLONG;

  bind[4].buffer_type= MYSQL_TYPE_FLOAT;
  bind[4].buffer= (void *)&f_data;

  bind[5].buffer_type= MYSQL_TYPE_DOUBLE;
  bind[5].buffer= (void *)&d_data;

  bind[6].buffer_type= MYSQL_TYPE_STRING;
  bind[6].buffer= (void *)&s_data;

  rc = mysql_bind_result(stmt, bind);
  mystmt(stmt, rc);

  rc = mysql_execute(stmt);
  mystmt(stmt, rc);

  rc = mysql_fetch(stmt);
  mystmt(stmt,rc);

  fprintf(stdout, "\n tiny     : %ld(%ld)", data[0], length[0]);
  fprintf(stdout, "\n short    : %ld(%ld)", data[1], length[1]);
  fprintf(stdout, "\n int      : %ld(%ld)", data[2], length[2]);
  fprintf(stdout, "\n longlong : %ld(%ld)", data[3], length[3]);
  fprintf(stdout, "\n float    : %f(%ld)",  f_data,  length[4]);
  fprintf(stdout, "\n double   : %g(%ld)",  d_data,  length[5]);
  fprintf(stdout, "\n char     : %s(%ld)",  s_data,  length[6]);

  round= 0;
  bit= 1;
  
  for (i=0; i < 4; i++)
  {
    myassert(data[i] == round+i+1);
    myassert(length[i] == bit);
    round= (round+10)*10;
    bit<<= 1;
  }

  /* FLOAT */
  myassert((int)f_data == round+1+i);
  myassert(length[4] == 4);

  /* DOUBLE */
  round= (round+10)*10;
  myassert((int)d_data == round+2+i);
  myassert(length[5] == 8);

  /* CHAR */
  round= (round+10)*10;
  {
    char buff[20];
    int len= sprintf(buff,"%d", round+3+i);
    myassert(strcmp(s_data,buff)==0);
    myassert(length[6] == len);
  }

  rc = mysql_fetch(stmt);
  myassert(rc == MYSQL_NO_DATA);

  mysql_stmt_close(stmt);
}

/********************************************************
* to test fetching of long to all types                 *
*********************************************************/
static void test_fetch_long()
{
  MYSQL_STMT   *stmt;
  int          rc, i, round, bit;
  long         data[10], length[10];
  float        f_data;
  double       d_data;
  char         s_data[10];
  MYSQL_BIND   bind[7];

  myheader("test_fetch_long");

  init_bind(bind);
  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_bind_long");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_bind_long(c1 int unsigned,\
                                                     c2 int unsigned,\
                                                     c3 int,\
                                                     c4 int,\
                                                     c5 int,\
                                                     c6 int unsigned,\
                                                     c7 int)");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  stmt = mysql_prepare(mysql,"INSERT INTO test_bind_long VALUES(?,?,?,?,?,?,?)",100);
  myquery(rc);

  verify_param_count(stmt, 7);

  round= 0;
  for (i=0; i < 7; i++)
  {  
    bind[i].buffer_type= MYSQL_TYPE_LONG;
    bind[i].buffer= (void *)&data[i];
    data[i]= round+i+1;
    round= (round +10)*10;
  }   
  rc = mysql_bind_param(stmt, bind);
  mystmt(stmt,rc);
  
  rc = mysql_execute(stmt);
  mystmt(stmt, rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  mysql_stmt_close(stmt);

  myassert(1 == my_stmt_result("SELECT * FROM test_bind_long",50));

  stmt = mysql_prepare(mysql,"SELECT * FROM test_bind_long",50);
  myquery(rc);

  for (i=0; i < 7; i++)
  {
    bind[i].buffer= (void *)&data[i];
    bind[i].length= (long *)&length[i];
  }
  bind[0].buffer_type= MYSQL_TYPE_TINY;
  bind[1].buffer_type= MYSQL_TYPE_SHORT;
  bind[2].buffer_type= MYSQL_TYPE_LONG;
  bind[3].buffer_type= MYSQL_TYPE_LONGLONG;

  bind[4].buffer_type= MYSQL_TYPE_FLOAT;
  bind[4].buffer= (void *)&f_data;

  bind[5].buffer_type= MYSQL_TYPE_DOUBLE;
  bind[5].buffer= (void *)&d_data;

  bind[6].buffer_type= MYSQL_TYPE_STRING;
  bind[6].buffer= (void *)&s_data;

  rc = mysql_bind_result(stmt, bind);
  mystmt(stmt, rc);

  rc = mysql_execute(stmt);
  mystmt(stmt, rc);

  rc = mysql_fetch(stmt);
  mystmt(stmt,rc);

  fprintf(stdout, "\n tiny     : %ld(%ld)", data[0], length[0]);
  fprintf(stdout, "\n short    : %ld(%ld)", data[1], length[1]);
  fprintf(stdout, "\n int      : %ld(%ld)", data[2], length[2]);
  fprintf(stdout, "\n longlong : %ld(%ld)", data[3], length[3]);
  fprintf(stdout, "\n float    : %f(%ld)",  f_data,  length[4]);
  fprintf(stdout, "\n double   : %g(%ld)",  d_data,  length[5]);
  fprintf(stdout, "\n char     : %s(%ld)",  s_data,  length[6]);

  round= 0;
  bit= 1;
  
  for (i=0; i < 4; i++)
  {
    myassert(data[i] == round+i+1);
    myassert(length[i] == bit);
    round= (round+10)*10;
    bit<<= 1;
  }

  /* FLOAT */
  myassert((int)f_data == round+1+i);
  myassert(length[4] == 4);

  /* DOUBLE */
  round= (round+10)*10;
  myassert((int)d_data == round+2+i);
  myassert(length[5] == 8);

  /* CHAR */
  round= (round+10)*10;
  {
    char buff[20];
    int len= sprintf(buff,"%d", round+3+i);
    myassert(strcmp(s_data,buff)==0);
    myassert(length[6] == len);
  }

  rc = mysql_fetch(stmt);
  myassert(rc == MYSQL_NO_DATA);

  mysql_stmt_close(stmt);
}

/********************************************************
* to test fetching of short to all types                 *
*********************************************************/
static void test_fetch_short()
{
  MYSQL_STMT   *stmt;
  int          rc, i, round, bit;
  long         data[10], length[10];
  float        f_data;
  double       d_data;
  char         s_data[10];
  MYSQL_BIND   bind[7];

  myheader("test_fetch_short");

  init_bind(bind);
  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_bind_long");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_bind_long(c1 smallint unsigned,\
                                                     c2 smallint,\
                                                     c3 smallint unsigned,\
                                                     c4 smallint,\
                                                     c5 smallint,\
                                                     c6 smallint,\
                                                     c7 smallint unsigned)");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  stmt = mysql_prepare(mysql,"INSERT INTO test_bind_long VALUES(?,?,?,?,?,?,?)",100);
  myquery(rc);

  verify_param_count(stmt, 7);

  round= 0;
  for (i=0; i < 7; i++)
  {  
    bind[i].buffer_type= MYSQL_TYPE_LONG;
    bind[i].buffer= (void *)&data[i];
    data[i]= round+i+1;
    round= (round +10)*2;
  }   
  rc = mysql_bind_param(stmt, bind);
  mystmt(stmt,rc);
  
  rc = mysql_execute(stmt);
  mystmt(stmt, rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  mysql_stmt_close(stmt);

  myassert(1 == my_stmt_result("SELECT * FROM test_bind_long",50));

  stmt = mysql_prepare(mysql,"SELECT * FROM test_bind_long",50);
  myquery(rc);

  for (i=0; i < 7; i++)
  {
    bind[i].buffer= (void *)&data[i];
    bind[i].length= (long *)&length[i];
  }
  bind[0].buffer_type= MYSQL_TYPE_TINY;
  bind[1].buffer_type= MYSQL_TYPE_SHORT;
  bind[2].buffer_type= MYSQL_TYPE_LONG;
  bind[3].buffer_type= MYSQL_TYPE_LONGLONG;

  bind[4].buffer_type= MYSQL_TYPE_FLOAT;
  bind[4].buffer= (void *)&f_data;

  bind[5].buffer_type= MYSQL_TYPE_DOUBLE;
  bind[5].buffer= (void *)&d_data;

  bind[6].buffer_type= MYSQL_TYPE_STRING;
  bind[6].buffer= (void *)&s_data;

  rc = mysql_bind_result(stmt, bind);
  mystmt(stmt, rc);

  rc = mysql_execute(stmt);
  mystmt(stmt, rc);

  rc = mysql_fetch(stmt);
  mystmt(stmt,rc);

  fprintf(stdout, "\n tiny     : %ld(%ld)", data[0], length[0]);
  fprintf(stdout, "\n short    : %ld(%ld)", data[1], length[1]);
  fprintf(stdout, "\n int      : %ld(%ld)", data[2], length[2]);
  fprintf(stdout, "\n longlong : %ld(%ld)", data[3], length[3]);
  fprintf(stdout, "\n float    : %f(%ld)",  f_data,  length[4]);
  fprintf(stdout, "\n double   : %g(%ld)",  d_data,  length[5]);
  fprintf(stdout, "\n char     : %s(%ld)",  s_data,  length[6]);

  round= 0;
  bit= 1;
  
  for (i=0; i < 4; i++)
  {
    myassert(data[i] == round+i+1);
    myassert(length[i] == bit);
    round= (round+10)*2;
    bit<<= 1;
  }

  /* FLOAT */
  myassert((int)f_data == round+1+i);
  myassert(length[4] == 4);

  /* DOUBLE */
  round= (round+10)*2;
  myassert((int)d_data == round+2+i);
  myassert(length[5] == 8);

  /* CHAR */
  round= (round+10)*2;
  {
    char buff[20];
    int len= sprintf(buff,"%d", round+3+i);
    myassert(strcmp(s_data,buff)==0);
    myassert(length[6] == len);
  }

  rc = mysql_fetch(stmt);
  myassert(rc == MYSQL_NO_DATA);

  mysql_stmt_close(stmt);
}

/********************************************************
* to test fetching of tiny to all types                 *
*********************************************************/
static void test_fetch_tiny()
{
  MYSQL_STMT   *stmt;
  int          rc, i, bit;
  long         data[10], length[10];
  float        f_data;
  double       d_data;
  char         s_data[10];
  MYSQL_BIND   bind[7];

  myheader("test_fetch_tiny");

  init_bind(bind);
  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_bind_long");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_bind_long(c1 tinyint unsigned,\
                                                     c2 tinyint,\
                                                     c3 tinyint unsigned,\
                                                     c4 tinyint,\
                                                     c5 tinyint,\
                                                     c6 tinyint,\
                                                     c7 tinyint unsigned)");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  stmt = mysql_prepare(mysql,"INSERT INTO test_bind_long VALUES(?,?,?,?,?,?,?)",100);
  myquery(rc);

  verify_param_count(stmt, 7);
 
  rc= 10;
  for (i=0; i < 7; i++)
  {  
    bind[i].buffer_type= MYSQL_TYPE_LONG;
    bind[i].buffer= (void *)&data[i];
    data[i]= rc+i;
    rc+= 10;
  }   
  rc = mysql_bind_param(stmt, bind);
  mystmt(stmt,rc);
  
  rc = mysql_execute(stmt);
  mystmt(stmt, rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  mysql_stmt_close(stmt);

  myassert(1 == my_stmt_result("SELECT * FROM test_bind_long",50));

  stmt = mysql_prepare(mysql,"SELECT * FROM test_bind_long",50);
  myquery(rc);

  for (i=0; i < 7; i++)
  {
    bind[i].buffer= (void *)&data[i];
    bind[i].length= (long *)&length[i];
  }
  bind[0].buffer_type= MYSQL_TYPE_TINY;
  bind[1].buffer_type= MYSQL_TYPE_SHORT;
  bind[2].buffer_type= MYSQL_TYPE_LONG;
  bind[3].buffer_type= MYSQL_TYPE_LONGLONG;

  bind[4].buffer_type= MYSQL_TYPE_FLOAT;
  bind[4].buffer= (void *)&f_data;

  bind[5].buffer_type= MYSQL_TYPE_DOUBLE;
  bind[5].buffer= (void *)&d_data;

  bind[6].buffer_type= MYSQL_TYPE_STRING;
  bind[6].buffer= (void *)&s_data;

  rc = mysql_bind_result(stmt, bind);
  mystmt(stmt, rc);

  rc = mysql_execute(stmt);
  mystmt(stmt, rc);

  rc = mysql_fetch(stmt);
  mystmt(stmt,rc);

  fprintf(stdout, "\n tiny     : %ld(%ld)", data[0], length[0]);
  fprintf(stdout, "\n short    : %ld(%ld)", data[1], length[1]);
  fprintf(stdout, "\n int      : %ld(%ld)", data[2], length[2]);
  fprintf(stdout, "\n longlong : %ld(%ld)", data[3], length[3]);
  fprintf(stdout, "\n float    : %f(%ld)",  f_data,  length[4]);
  fprintf(stdout, "\n double   : %g(%ld)",  d_data,  length[5]);
  fprintf(stdout, "\n char     : %s(%ld)",  s_data,  length[6]);

  bit= 1;
  rc= 10;
  for (i=0; i < 4; i++)
  {
    myassert(data[i] == rc+i);
    myassert(length[i] == bit);
    bit<<= 1;
    rc+= 10;
  }

  /* FLOAT */
  rc+= i;
  myassert((int)f_data == rc);
  myassert(length[4] == 4);

  /* DOUBLE */
  rc+= 11;
  myassert((int)d_data == rc);
  myassert(length[5] == 8);

  /* CHAR */
  rc+= 11;
  {
    char buff[20];
    int len= sprintf(buff,"%d", rc);
    myassert(strcmp(s_data,buff)==0);
    myassert(length[6] == len);
  }

  rc = mysql_fetch(stmt);
  myassert(rc == MYSQL_NO_DATA);

  mysql_stmt_close(stmt);
}

/********************************************************
* to test fetching of longlong to all types             *
*********************************************************/
static void test_fetch_bigint()
{
  MYSQL_STMT   *stmt;
  int          rc, i, round, bit;
  long         data[10], length[10];
  float        f_data;
  double       d_data;
  char         s_data[10];
  MYSQL_BIND   bind[7];

  myheader("test_fetch_bigint");

  init_bind(bind);
  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_bind_long");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_bind_long(c1 bigint,\
                                                     c2 bigint,\
                                                     c3 bigint unsigned,\
                                                     c4 bigint unsigned,\
                                                     c5 bigint unsigned,\
                                                     c6 bigint unsigned,\
                                                     c7 bigint unsigned)");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  stmt = mysql_prepare(mysql,"INSERT INTO test_bind_long VALUES(?,?,?,?,?,?,?)",100);
  myquery(rc);

  verify_param_count(stmt, 7);

  round= 0;
  for (i=0; i < 7; i++)
  {  
    bind[i].buffer_type= MYSQL_TYPE_LONG;
    bind[i].buffer= (void *)&data[i];
    data[i]= round+i+1;
    round= (round +10)*10;
  }   
  rc = mysql_bind_param(stmt, bind);
  mystmt(stmt,rc);
  
  rc = mysql_execute(stmt);
  mystmt(stmt, rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  mysql_stmt_close(stmt);

  myassert(1 == my_stmt_result("SELECT * FROM test_bind_long",50));

  stmt = mysql_prepare(mysql,"SELECT * FROM test_bind_long",50);
  myquery(rc);

  for (i=0; i < 7; i++)
  {
    bind[i].buffer= (void *)&data[i];
    bind[i].length= (long *)&length[i];
  }
  bind[0].buffer_type= MYSQL_TYPE_TINY;
  bind[1].buffer_type= MYSQL_TYPE_SHORT;
  bind[2].buffer_type= MYSQL_TYPE_LONG;
  bind[3].buffer_type= MYSQL_TYPE_LONGLONG;

  bind[4].buffer_type= MYSQL_TYPE_FLOAT;
  bind[4].buffer= (void *)&f_data;

  bind[5].buffer_type= MYSQL_TYPE_DOUBLE;
  bind[5].buffer= (void *)&d_data;

  bind[6].buffer_type= MYSQL_TYPE_STRING;
  bind[6].buffer= (void *)&s_data;

  rc = mysql_bind_result(stmt, bind);
  mystmt(stmt, rc);

  rc = mysql_execute(stmt);
  mystmt(stmt, rc);

  rc = mysql_fetch(stmt);
  mystmt(stmt,rc);

  fprintf(stdout, "\n tiny     : %ld(%ld)", data[0], length[0]);
  fprintf(stdout, "\n short    : %ld(%ld)", data[1], length[1]);
  fprintf(stdout, "\n int      : %ld(%ld)", data[2], length[2]);
  fprintf(stdout, "\n longlong : %ld(%ld)", data[3], length[3]);
  fprintf(stdout, "\n float    : %f(%ld)",  f_data,  length[4]);
  fprintf(stdout, "\n double   : %g(%ld)",  d_data,  length[5]);
  fprintf(stdout, "\n char     : %s(%ld)",  s_data,  length[6]);

  round= 0;
  bit= 1;
  
  for (i=0; i < 4; i++)
  {
    myassert(data[i] == round+i+1);
    myassert(length[i] == bit);
    round= (round+10)*10;
    bit<<= 1;
  }

  /* FLOAT */
  myassert((int)f_data == round+1+i);
  myassert(length[4] == 4);

  /* DOUBLE */
  round= (round+10)*10;
  myassert((int)d_data == round+2+i);
  myassert(length[5] == 8);

  /* CHAR */
  round= (round+10)*10;
  {
    char buff[20];
    int len= sprintf(buff,"%d", round+3+i);
    myassert(strcmp(s_data,buff)==0);
    myassert(length[6] == len);
  }

  rc = mysql_fetch(stmt);
  myassert(rc == MYSQL_NO_DATA);

  mysql_stmt_close(stmt);
}

/********************************************************
* to test fetching of float to all types                 *
*********************************************************/
static void test_fetch_float()
{
  MYSQL_STMT   *stmt;
  int          rc, i, round, bit;
  long         data[10], length[10];
  float        f_data;
  double       d_data;
  char         s_data[10];
  MYSQL_BIND   bind[7];

  myheader("test_fetch_float");

  init_bind(bind);
  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_bind_long");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_bind_long(c1 float(3),\
                                                     c2 float,\
                                                     c3 float unsigned,\
                                                     c4 float,\
                                                     c5 float,\
                                                     c6 float,\
                                                     c7 float(10) unsigned)");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  stmt = mysql_prepare(mysql,"INSERT INTO test_bind_long VALUES(?,?,?,?,?,?,?)",100);
  myquery(rc);

  verify_param_count(stmt, 7);

  round= 0;
  for (i=0; i < 7; i++)
  {  
    bind[i].buffer_type= MYSQL_TYPE_LONG;
    bind[i].buffer= (void *)&data[i];
    data[i]= round+i+1;
    round= (round +10)*2;
  }   
  rc = mysql_bind_param(stmt, bind);
  mystmt(stmt,rc);
  
  rc = mysql_execute(stmt);
  mystmt(stmt, rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  mysql_stmt_close(stmt);

  myassert(1 == my_stmt_result("SELECT * FROM test_bind_long",50));

  stmt = mysql_prepare(mysql,"SELECT * FROM test_bind_long",50);
  myquery(rc);

  for (i=0; i < 7; i++)
  {
    bind[i].buffer= (void *)&data[i];
    bind[i].length= (long *)&length[i];
  }
  bind[0].buffer_type= MYSQL_TYPE_TINY;
  bind[1].buffer_type= MYSQL_TYPE_SHORT;
  bind[2].buffer_type= MYSQL_TYPE_LONG;
  bind[3].buffer_type= MYSQL_TYPE_LONGLONG;

  bind[4].buffer_type= MYSQL_TYPE_FLOAT;
  bind[4].buffer= (void *)&f_data;

  bind[5].buffer_type= MYSQL_TYPE_DOUBLE;
  bind[5].buffer= (void *)&d_data;

  bind[6].buffer_type= MYSQL_TYPE_STRING;
  bind[6].buffer= (void *)&s_data;

  rc = mysql_bind_result(stmt, bind);
  mystmt(stmt, rc);

  rc = mysql_execute(stmt);
  mystmt(stmt, rc);

  rc = mysql_fetch(stmt);
  mystmt(stmt,rc);

  fprintf(stdout, "\n tiny     : %ld(%ld)", data[0], length[0]);
  fprintf(stdout, "\n short    : %ld(%ld)", data[1], length[1]);
  fprintf(stdout, "\n int      : %ld(%ld)", data[2], length[2]);
  fprintf(stdout, "\n longlong : %ld(%ld)", data[3], length[3]);
  fprintf(stdout, "\n float    : %f(%ld)",  f_data,  length[4]);
  fprintf(stdout, "\n double   : %g(%ld)",  d_data,  length[5]);
  fprintf(stdout, "\n char     : %s(%ld)",  s_data,  length[6]);

  round= 0;
  bit= 1;
  
  for (i=0; i < 4; i++)
  {
    myassert(data[i] == round+i+1);
    myassert(length[i] == bit);
    round= (round+10)*2;
    bit<<= 1;
  }

  /* FLOAT */
  myassert((int)f_data == round+1+i);
  myassert(length[4] == 4);

  /* DOUBLE */
  round= (round+10)*2;
  myassert((int)d_data == round+2+i);
  myassert(length[5] == 8);

  /* CHAR */
  round= (round+10)*2;
  {
    char buff[20];
    int len= sprintf(buff,"%d", round+3+i);
    myassert(strcmp(s_data,buff)==0);
    myassert(length[6] == len);
  }

  rc = mysql_fetch(stmt);
  myassert(rc == MYSQL_NO_DATA);

  mysql_stmt_close(stmt);
}

/********************************************************
* to test fetching of double to all types               *
*********************************************************/
static void test_fetch_double()
{
  MYSQL_STMT   *stmt;
  int          rc, i, round, bit;
  long         data[10], length[10];
  float        f_data;
  double       d_data;
  char         s_data[10];
  MYSQL_BIND   bind[7];

  myheader("test_fetch_double");

  init_bind(bind);
  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_bind_long");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_bind_long(c1 double(5,2),\
                                                     c2 double unsigned,\
                                                     c3 double unsigned,\
                                                     c4 double unsigned,\
                                                     c5 double unsigned,\
                                                     c6 double unsigned,\
                                                     c7 double unsigned)");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  stmt = mysql_prepare(mysql,"INSERT INTO test_bind_long VALUES(?,?,?,?,?,?,?)",100);
  myquery(rc);

  verify_param_count(stmt, 7);

  round= 0;
  for (i=0; i < 7; i++)
  {  
    bind[i].buffer_type= MYSQL_TYPE_LONG;
    bind[i].buffer= (void *)&data[i];
    data[i]= round+i+1;
    round= (round +10)*10;
  }   
  rc = mysql_bind_param(stmt, bind);
  mystmt(stmt,rc);
  
  rc = mysql_execute(stmt);
  mystmt(stmt, rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  mysql_stmt_close(stmt);

  myassert(1 == my_stmt_result("SELECT * FROM test_bind_long",50));

  stmt = mysql_prepare(mysql,"SELECT * FROM test_bind_long",50);
  myquery(rc);

  for (i=0; i < 7; i++)
  {
    bind[i].buffer= (void *)&data[i];
    bind[i].length= (long *)&length[i];
  }
  bind[0].buffer_type= MYSQL_TYPE_TINY;
  bind[1].buffer_type= MYSQL_TYPE_SHORT;
  bind[2].buffer_type= MYSQL_TYPE_LONG;
  bind[3].buffer_type= MYSQL_TYPE_LONGLONG;
  
  bind[4].buffer_type= MYSQL_TYPE_STRING;
  bind[4].buffer= (void *)&s_data;

  bind[5].buffer_type= MYSQL_TYPE_FLOAT;
  bind[5].buffer= (void *)&f_data;

  bind[6].buffer_type= MYSQL_TYPE_DOUBLE;
  bind[6].buffer= (void *)&d_data;

  rc = mysql_bind_result(stmt, bind);
  mystmt(stmt, rc);

  rc = mysql_execute(stmt);
  mystmt(stmt, rc);

  rc = mysql_fetch(stmt);
  mystmt(stmt,rc);

  fprintf(stdout, "\n tiny     : %ld(%ld)", data[0], length[0]);
  fprintf(stdout, "\n short    : %ld(%ld)", data[1], length[1]);
  fprintf(stdout, "\n int      : %ld(%ld)", data[2], length[2]);
  fprintf(stdout, "\n longlong : %ld(%ld)", data[3], length[3]);
  fprintf(stdout, "\n float    : %f(%ld)",  f_data,  length[5]);
  fprintf(stdout, "\n double   : %g(%ld)",  d_data,  length[6]);
  fprintf(stdout, "\n char     : %s(%ld)",  s_data,  length[4]);

  round= 0;
  bit= 1;
  
  for (i=0; i < 4; i++)
  {
    myassert(data[i] == round+i+1);
    myassert(length[i] == bit);
    round= (round+10)*10;
    bit<<= 1;
  }
  /* CHAR */
  {
    char buff[20];
    int len= sprintf(buff,"%d", round+1+i);
    myassert(strcmp(s_data,buff)==0);
    myassert(length[4] == len);
  }

  /* FLOAT */
  round= (round+10)*10;
  myassert((int)f_data == round+2+i);
  myassert(length[5] == 4);

  /* DOUBLE */
  round= (round+10)*10;
  myassert((int)d_data == round+3+i);
  myassert(length[6] == 8);

  rc = mysql_fetch(stmt);
  myassert(rc == MYSQL_NO_DATA);

  mysql_stmt_close(stmt);
}

/********************************************************
* to test simple prepare with all possible types        *
*********************************************************/
static void test_prepare_ext()
{
  MYSQL_STMT *stmt;
  int        rc;
  char       *sql;
  int        nData=1;
  MYSQL_RES  *result;
  char       tData=1;
  short      sData=10;
  longlong   bData=20;
  MYSQL_BIND bind_int[6];

  myheader("test_prepare_ext");

  init_bind(bind_int);

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_prepare_ext");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  sql = (char *)"CREATE TABLE test_prepare_ext\
			(\
			c1  tinyint,\
			c2  smallint,\
			c3  mediumint,\
			c4  int,\
			c5  integer,\
			c6  bigint,\
			c7  float,\
			c8  double,\
			c9  double precision,\
			c10 real,\
			c11 decimal(7,4),\
      c12 numeric(8,4),\
			c13 date,\
			c14 datetime,\
			c15 timestamp(14),\
			c16 time,\
			c17 year,\
			c18 bit,\
      c19 bool,\
			c20 char,\
			c21 char(10),\
			c22 varchar(30),\
			c23 tinyblob,\
			c24 tinytext,\
			c25 blob,\
			c26 text,\
			c27 mediumblob,\
			c28 mediumtext,\
			c29 longblob,\
			c30 longtext,\
			c31 enum('one','two','three'),\
			c32 set('monday','tuesday','wednesday'))";

  rc = mysql_query(mysql,sql);
  myquery(rc);

  /* insert by prepare - all integers */
  strcpy(query,(char *)"INSERT INTO test_prepare_ext(c1,c2,c3,c4,c5,c6) VALUES(?,?,?,?,?,?)");
  stmt = mysql_prepare(mysql,query, strlen(query));
  myquery(rc);

  verify_param_count(stmt,6);

  /*tinyint*/
  bind_int[0].buffer_type=FIELD_TYPE_TINY;
  bind_int[0].buffer= (void *)&tData;

  /*smallint*/
  bind_int[1].buffer_type=FIELD_TYPE_SHORT;
  bind_int[1].buffer= (void *)&sData;

  /*mediumint*/
  bind_int[2].buffer_type=FIELD_TYPE_LONG;
  bind_int[2].buffer= (void *)&nData;

  /*int*/
  bind_int[3].buffer_type=FIELD_TYPE_LONG;
  bind_int[3].buffer= (void *)&nData;

  /*integer*/
  bind_int[4].buffer_type=FIELD_TYPE_LONG;
  bind_int[4].buffer= (void *)&nData;

  /*bigint*/
  bind_int[5].buffer_type=FIELD_TYPE_LONGLONG;
  bind_int[5].buffer= (void *)&bData;

  rc = mysql_bind_param(stmt,bind_int);
  mystmt(stmt, rc);

  /*
  *  integer to integer
  */
  for (nData=0; nData<10; nData++, tData++, sData++,bData++)
  {
    rc = mysql_execute(stmt);
    mystmt(stmt, rc);
  }
  mysql_stmt_close(stmt);

  /* now fetch the results ..*/
  rc = mysql_commit(mysql);
  myquery(rc);

  /* test the results now, only one row should exists */
  rc = mysql_query(mysql,"SELECT c1,c2,c3,c4,c5,c6 FROM test_prepare_ext");
  myquery(rc);

  /* get the result */
  result = mysql_store_result(mysql);
  mytest(result);

  myassert(nData == my_process_result_set(result));
  mysql_free_result(result);
}




/********************************************************
* to test real and alias names                          *
*********************************************************/
static void test_field_names()
{
  int        rc;
  MYSQL_RES  *result;

  myheader("test_field_names");

  fprintf(stdout,"\n %d,%d,%d",MYSQL_TYPE_DECIMAL,MYSQL_TYPE_NEWDATE,MYSQL_TYPE_ENUM);
  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_field_names1");
  myquery(rc);

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_field_names2");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_field_names1(id int,name varchar(50))");
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_field_names2(id int,name varchar(50))");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  /* with table name included with true column name */
  rc = mysql_query(mysql,"SELECT id as 'id-alias' FROM test_field_names1");
  myquery(rc);

  result = mysql_use_result(mysql);
  mytest(result);

  myassert(0 == my_process_result_set(result));
  mysql_free_result(result);

  /* with table name included with true column name */
  rc = mysql_query(mysql,"SELECT t1.id as 'id-alias',test_field_names2.name FROM test_field_names1 t1,test_field_names2");
  myquery(rc);

  result = mysql_use_result(mysql);
  mytest(result);

  myassert(0 == my_process_result_set(result));
  mysql_free_result(result);
}

/********************************************************
* to test warnings                                      *
*********************************************************/
static void test_warnings()
{
  int        rc;
  MYSQL_RES  *result;

  myheader("test_warnings");

  rc = mysql_query(mysql,"SHOW WARNINGS");
  myquery(rc);

  result = mysql_use_result(mysql);
  mytest(result);

  my_process_result_set(result);
  mysql_free_result(result);
}

/********************************************************
* to test errors                                        *
*********************************************************/
static void test_errors()
{
  int        rc;
  MYSQL_RES  *result;

  myheader("test_errors");

  rc = mysql_query(mysql,"SHOW ERRORS");
  myquery(rc);

  result = mysql_use_result(mysql);
  mytest(result);

  my_process_result_set(result);
  mysql_free_result(result);
}



/********************************************************
* to test simple prepare-insert                         *
*********************************************************/
static void test_insert()
{
  MYSQL_STMT *stmt;
  int        rc, length;
  char       query[200];
  char       str_data[50];
  char       tiny_data;
  MYSQL_RES  *result;
  MYSQL_BIND bind[2];

  myheader("test_insert");

  rc = mysql_autocommit(mysql, true);
  myquery(rc);

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_prep_insert");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_prep_insert(col1 tinyint,\
                                col2 varchar(50))");
  myquery(rc);

  /* insert by prepare */
  bzero(bind, sizeof(bind));
  strcpy(query,"INSERT INTO test_prep_insert VALUES(?,?)");
  stmt = mysql_prepare(mysql, query, strlen(query));
  mystmt_init(stmt);

  verify_param_count(stmt,2);

  /* tinyint */
  bind[0].buffer_type=FIELD_TYPE_TINY;
  bind[0].buffer=(gptr)&tiny_data;
  /* string */
  bind[1].buffer_type=FIELD_TYPE_STRING;
  bind[1].buffer=str_data;
  bind[1].length=(long *)&length;

  rc = mysql_bind_param(stmt,bind);
  mystmt(stmt, rc);

  /* now, execute the prepared statement to insert 10 records.. */
  for (tiny_data=0; tiny_data < 3; tiny_data++)
  {
    length = sprintf(str_data,"MySQL%d",tiny_data);
    rc = mysql_execute(stmt);
    mystmt(stmt, rc);
  }

  mysql_stmt_close(stmt);

  /* now fetch the results ..*/
  rc = mysql_commit(mysql);
  myquery(rc);

  /* test the results now, only one row should exists */
  rc = mysql_query(mysql,"SELECT * FROM test_prep_insert");
  myquery(rc);

  /* get the result */
  result = mysql_store_result(mysql);
  mytest(result);

  myassert((int)tiny_data == my_process_result_set(result));
  mysql_free_result(result);

}

/********************************************************
* to test simple prepare-resultset info                 *
*********************************************************/
static void test_prepare_resultset()
{
  MYSQL_STMT *stmt;
  int        rc;
  char       query[200];
  MYSQL_RES  *result;

  myheader("test_prepare_resultset");

  rc = mysql_autocommit(mysql, true);
  myquery(rc);

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_prepare_resultset");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_prepare_resultset(id int,\
                                name varchar(50),extra double)");
  myquery(rc);

  strcpy(query,"SELECT * FROM test_prepare_resultset");
  stmt = PREPARE(mysql, query);
  mystmt_init(stmt);

  verify_param_count(stmt,0);

  result = mysql_prepare_result(stmt);
  mytest(result);
  my_print_result_metadata(result);
  mysql_stmt_close(stmt);
}

/********************************************************
* to test field flags (verify .NET provider)            *
*********************************************************/

static void test_field_flags()
{
  int          rc;
  MYSQL_RES    *result;
  MYSQL_FIELD  *field;
  unsigned int i;


  myheader("test_field_flags");

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_field_flags");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_field_flags(id int NOT NULL AUTO_INCREMENT PRIMARY KEY,\
                                                        id1 int NOT NULL,\
                                                        id2 int UNIQUE,\
                                                        id3 int,\
                                                        id4 int NOT NULL,\
                                                        id5 int,\
                                                        KEY(id3,id4))");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  /* with table name included with true column name */
  rc = mysql_query(mysql,"SELECT * FROM test_field_flags");
  myquery(rc);

  result = mysql_use_result(mysql);
  mytest(result);

  mysql_field_seek(result,0);
  fputc('\n', stdout);  

  for(i=0; i< mysql_num_fields(result); i++)
  {
    field = mysql_fetch_field(result);
    fprintf(stdout,"\n field:%d",i);
    if(field->flags & NOT_NULL_FLAG)
      fprintf(stdout,"\n  NOT_NULL_FLAG");
    if(field->flags & PRI_KEY_FLAG)
      fprintf(stdout,"\n  PRI_KEY_FLAG");
    if(field->flags & UNIQUE_KEY_FLAG)
      fprintf(stdout,"\n  UNIQUE_KEY_FLAG");
    if(field->flags & MULTIPLE_KEY_FLAG)
      fprintf(stdout,"\n  MULTIPLE_KEY_FLAG");
    if(field->flags & AUTO_INCREMENT_FLAG)
      fprintf(stdout,"\n  AUTO_INCREMENT_FLAG");

  }
  mysql_free_result(result);
}

/**************************************************************
 * Test mysql_stmt_close for open stmts                       *
**************************************************************/
static void test_stmt_close()
{
  MYSQL *lmysql;
  MYSQL_STMT *stmt1, *stmt2, *stmt3, *stmt_x;
  MYSQL_BIND  param[1];
  MYSQL_RES   *result;
  char  query[100];
  unsigned int  count;
  int   rc;
  
  myheader("test_stmt_close");  

  init_bind(param);
  if(!(lmysql = mysql_init(NULL)))
  { 
	  myerror("mysql_init() failed");
    exit(0);
  }
  if (!(mysql_real_connect(lmysql,opt_host,opt_user,
			   opt_password, opt_db ? opt_db:"inter_client_test_db", opt_port,
			   opt_unix_socket, 0)))
  {
    myerror("connection failed");
    exit(0);
  }   
  if (opt_db)
    strcpy(current_db,opt_db);

  /* set AUTOCOMMIT to ON*/
  mysql_autocommit(lmysql, true);
  mysql_query(lmysql,"DROP TABLE IF EXISTS test_stmt_close");
  mysql_query(lmysql,"CREATE TABLE test_stmt_close(id int)");

  strcpy(query,"ALTER TABLE test_stmt_close ADD name varchar(20)");
  stmt1= PREPARE(lmysql, query);
  mystmt_init(stmt1);
  count= mysql_param_count(stmt1);
  fprintf(stdout,"\n total params in alter: %d", count);
  myassert(count == 0);
  strcpy(query,"INSERT INTO test_stmt_close(id) VALUES(?)");
  stmt_x= PREPARE(mysql, query);
  mystmt_init(stmt_x);
  count= mysql_param_count(stmt_x);
  fprintf(stdout,"\n total params in insert: %d", count);
  myassert(count == 1);
  strcpy(query,"UPDATE test_stmt_close SET id=? WHERE id=?");
  stmt3= PREPARE(lmysql, query);
  mystmt_init(stmt3);
  count= mysql_param_count(stmt3);
  fprintf(stdout,"\n total params in update: %d", count);
  myassert(count == 2);
  strcpy(query,"SELECT * FROM test_stmt_close WHERE id=?");
  stmt2= PREPARE(lmysql, query);
  mystmt_init(stmt2);
  count= mysql_param_count(stmt2);
  fprintf(stdout,"\n total params in select: %d", count);
  myassert(count == 1);

  rc= mysql_stmt_close(stmt1);
  fprintf(stdout,"\n mysql_close_stmt(1) returned: %d", rc);
  myassert(rc == 0);
  mysql_close(lmysql); /* it should free all stmts */
#if NOT_VALID 
  rc= mysql_stmt_close(stmt3);
  fprintf(stdout,"\n mysql_close_stmt(3) returned: %d", rc);
  myassert( rc == 1);
  rc= mysql_stmt_close(stmt2);
  fprintf(stdout,"\n mysql_close_stmt(2) returned: %d", rc);
  myassert( rc == 1);
#endif

  count= 100;
  param[0].buffer=(gptr)&count;
  param[0].buffer_type=MYSQL_TYPE_LONG;
  rc = mysql_bind_param(stmt_x, param);
  mystmt(stmt_x, rc);
  rc = mysql_execute(stmt_x);
  mystmt(stmt_x, rc);

  rc= (ulong)mysql_affected_rows(stmt_x->mysql);
  fprintf(stdout,"\n total rows affected: %d", rc);
  myassert (rc == 1);

  rc= mysql_stmt_close(stmt_x);
  fprintf(stdout,"\n mysql_close_stmt(x) returned: %d", rc);
  myassert( rc == 0);

  /*verify_col_data("test_stmt_close", "id", "100");*/
  rc = mysql_query(mysql,"SELECT id FROM test_stmt_close");
  myquery(rc);

  result = mysql_store_result(mysql);
  mytest(result);

  myassert(1 == my_process_result_set(result));
  mysql_free_result(result);
}

/********************************************************
 * To test simple set-variable prepare                   *
*********************************************************/
static void test_set_variable()
{
  MYSQL_STMT *stmt;
  int        rc, select_limit=88;
  char       query[200];
  MYSQL_BIND bind[1];
  MYSQL_RES  *result;


  myheader("test_set_variable");

  rc = mysql_autocommit(mysql, true);
  myquery(rc);

  strcpy(query,"SET GLOBAL delayed_insert_limit=?");
  stmt = mysql_prepare(mysql, query, strlen(query));
  mystmt_init(stmt);

  verify_param_count(stmt,1);

  result= mysql_param_result(stmt);
  mytest_r(result);

  init_bind(bind);

  bind[0].buffer_type= MYSQL_TYPE_LONG;
  bind[0].buffer=(gptr)&select_limit;

  rc = mysql_bind_param(stmt, bind);
  mystmt(stmt,rc);

  rc= mysql_execute(stmt);
  mystmt(stmt,rc);

  mysql_store_result(mysql);

  strcpy(query,"show variables like 'delayed_insert_limit'");
  rc = mysql_query(mysql,query);
  myquery(rc);

  verify_col_data(NullS, NullS, "88");

#if TO_BE_FIXED

  select_limit= 100;/* reset to default */
  rc= mysql_execute(stmt);
  mystmt(stmt,rc);

  mysql_store_result(mysql);
  mysql_stmt_close(stmt);

  rc = mysql_query(mysql,query);
  myquery(rc);
  
  verify_col_data(NullS, NullS, "100");
#endif
  mysql_stmt_close(stmt);
}
#if NOT_USED
/* Insert meta info .. */
static void test_insert_meta()
{
  MYSQL_STMT *stmt;
  int        rc;
  char       query[200];
  MYSQL_RES  *result;
  MYSQL_FIELD *field;

  myheader("test_insert_meta");

  rc = mysql_autocommit(mysql, true);
  myquery(rc);

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_prep_insert");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_prep_insert(col1 tinyint,\
                                col2 varchar(50), col3 varchar(30))");
  myquery(rc);

  strcpy(query,"INSERT INTO test_prep_insert VALUES(10,'venu1','test')");
  stmt = mysql_prepare(mysql, query, strlen(query));
  mystmt_init(stmt);

  verify_param_count(stmt,0);

  result= mysql_param_result(stmt);
  mytest_r(result);

  strcpy(query,"INSERT INTO test_prep_insert VALUES(?,'venu',?)");
  stmt = mysql_prepare(mysql, query, strlen(query));
  mystmt_init(stmt);

  verify_param_count(stmt,2);

  result= mysql_param_result(stmt);
  mytest(result);

  my_print_result_metadata(result);

  mysql_field_seek(result, 0);
  field= mysql_fetch_field(result);
  mytest(field);
  fprintf(stdout, "\n obtained: `%s` (expected: `%s`)", field->name, "col1");
  myassert(strcmp(field->name,"col1")==0);

  field= mysql_fetch_field(result);
  mytest(field);
  fprintf(stdout, "\n obtained: `%s` (expected: `%s`)", field->name, "col3");
  myassert(strcmp(field->name,"col3")==0);

  field= mysql_fetch_field(result);
  mytest_r(field);

  mysql_free_result(result);
  mysql_stmt_close(stmt);
}

/* Update meta info .. */
static void test_update_meta()
{
  MYSQL_STMT *stmt;
  int        rc;
  char       query[200];
  MYSQL_RES  *result;
  MYSQL_FIELD *field;

  myheader("test_update_meta");

  rc = mysql_autocommit(mysql, true);
  myquery(rc);

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_prep_update");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_prep_update(col1 tinyint,\
                                col2 varchar(50), col3 varchar(30))");
  myquery(rc);

  strcpy(query,"UPDATE test_prep_update SET col1=10, col2='venu1' WHERE col3='test'");
  stmt = mysql_prepare(mysql, query, strlen(query));
  mystmt_init(stmt);

  verify_param_count(stmt,0);

  result= mysql_param_result(stmt);
  mytest_r(result);

  strcpy(query,"UPDATE test_prep_update SET col1=?, col2='venu' WHERE col3=?");
  stmt = mysql_prepare(mysql, query, strlen(query));
  mystmt_init(stmt);

  verify_param_count(stmt,2);

  result= mysql_param_result(stmt);
  mytest(result);

  my_print_result_metadata(result);

  mysql_field_seek(result, 0);
  field= mysql_fetch_field(result);
  mytest(field);
  fprintf(stdout, "\n col obtained: `%s` (expected: `%s`)", field->name, "col1");
  fprintf(stdout, "\n tab obtained: `%s` (expected: `%s`)", field->table, "test_prep_update");
  myassert(strcmp(field->name,"col1")==0);
  myassert(strcmp(field->table,"test_prep_update")==0);

  field= mysql_fetch_field(result);
  mytest(field);
  fprintf(stdout, "\n col obtained: `%s` (expected: `%s`)", field->name, "col3");
  fprintf(stdout, "\n tab obtained: `%s` (expected: `%s`)", field->table, "test_prep_update");
  myassert(strcmp(field->name,"col3")==0);
  myassert(strcmp(field->table,"test_prep_update")==0);

  field= mysql_fetch_field(result);
  mytest_r(field);

  mysql_free_result(result);
  mysql_stmt_close(stmt);
}

/* Select meta info .. */
static void test_select_meta()
{
  MYSQL_STMT *stmt;
  int        rc;
  char       query[200];
  MYSQL_RES  *result;
  MYSQL_FIELD *field;

  myheader("test_select_meta");

  rc = mysql_autocommit(mysql, true);
  myquery(rc);

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_prep_select");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_prep_select(col1 tinyint,\
                                col2 varchar(50), col3 varchar(30))");
  myquery(rc);

  strcpy(query,"SELECT * FROM test_prep_select WHERE col1=10");
  stmt = mysql_prepare(mysql, query, strlen(query));
  mystmt_init(stmt);

  verify_param_count(stmt,0);

  result= mysql_param_result(stmt);
  mytest_r(result);

  strcpy(query,"SELECT col1, col3 from test_prep_select WHERE col1=? AND col3='test' AND col2= ?");
  stmt = mysql_prepare(mysql, query, strlen(query));
  mystmt_init(stmt);

  verify_param_count(stmt,2);

  result= mysql_param_result(stmt);
  mytest(result);

  my_print_result_metadata(result);

  mysql_field_seek(result, 0);
  field= mysql_fetch_field(result);
  mytest(field);
  fprintf(stdout, "\n col obtained: `%s` (expected: `%s`)", field->name, "col1");
  fprintf(stdout, "\n tab obtained: `%s` (expected: `%s`)", field->table, "test_prep_select");
  myassert(strcmp(field->name,"col1")==0);
  myassert(strcmp(field->table,"test_prep_select")==0);

  field= mysql_fetch_field(result);
  mytest(field);
  fprintf(stdout, "\n col obtained: `%s` (expected: `%s`)", field->name, "col2");
  fprintf(stdout, "\n tab obtained: `%s` (expected: `%s`)", field->table, "test_prep_select");
  myassert(strcmp(field->name,"col2")==0);
  myassert(strcmp(field->table,"test_prep_select")==0);

  field= mysql_fetch_field(result);
  mytest_r(field);

  mysql_free_result(result);
  mysql_stmt_close(stmt);
}
#endif

/* Test FUNCTION field info / DATE_FORMAT() table_name . */
static void test_func_fields()
{
  int        rc;
  MYSQL_RES  *result;
  MYSQL_FIELD *field;

  myheader("test_func_fields");

  rc = mysql_autocommit(mysql, true);
  myquery(rc);

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_dateformat");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_dateformat(id int, \
                                                       ts timestamp)");
  myquery(rc);

  rc = mysql_query(mysql, "INSERT INTO test_dateformat(id) values(10)");
  myquery(rc);

  rc = mysql_query(mysql, "SELECT ts FROM test_dateformat");
  myquery(rc);

  result = mysql_store_result(mysql);
  mytest(result);

  field = mysql_fetch_field(result);
  mytest(field);
  fprintf(stdout,"\n table name: `%s` (expected: `%s`)", field->table,
                  "test_dateformat");
  myassert(strcmp(field->table, "test_dateformat")==0);

  field = mysql_fetch_field(result);
  mytest_r(field); /* no more fields */

  mysql_free_result(result);

  /* DATE_FORMAT */
  rc = mysql_query(mysql, "SELECT DATE_FORMAT(ts,'%Y') AS 'venu' FROM test_dateformat");
  myquery(rc);

  result = mysql_store_result(mysql);
  mytest(result);

  field = mysql_fetch_field(result);
  mytest(field);
  fprintf(stdout,"\n table name: `%s` (expected: `%s`)", field->table, "");
  myassert(field->table[0] == '\0');

  field = mysql_fetch_field(result);
  mytest_r(field); /* no more fields */

  mysql_free_result(result);

  /* FIELD ALIAS TEST */
  rc = mysql_query(mysql, "SELECT DATE_FORMAT(ts,'%Y')  AS 'YEAR' FROM test_dateformat");
  myquery(rc);

  result = mysql_store_result(mysql);
  mytest(result);

  field = mysql_fetch_field(result);
  mytest(field);
  fprintf(stdout,"\n field name: `%s` (expected: `%s`)", field->name, "YEAR");
  fprintf(stdout,"\n field org name: `%s` (expected: `%s`)",field->org_name,"");
  myassert(strcmp(field->name, "YEAR")==0);
  myassert(field->org_name[0] == '\0');

  field = mysql_fetch_field(result);
  mytest_r(field); /* no more fields */

  mysql_free_result(result);
}

/* Multiple stmts .. */
static void test_multi_stmt()
{
}

/********************************************************
* to test simple sample - manual                        *
*********************************************************/
static void test_manual_sample()
{
  unsigned int param_count;
  MYSQL_BIND   bind[3];
  MYSQL_STMT   *stmt;
  short        small_data;
  int          int_data;
  char         str_data[50], query[255];
  long         length;
  ulonglong    affected_rows;

  myheader("test_manual_sample");

  /*
    Sample which is incorporated directly in the manual under Prepared 
    statements section (Example from mysql_execute()
  */

  mysql_autocommit(mysql, 1);
  if (mysql_query(mysql,"DROP TABLE IF EXISTS test_table"))
  {
    fprintf(stderr, "\n drop table failed");
    fprintf(stderr, "\n %s", mysql_error(mysql));
    exit(0);
  }
  if (mysql_query(mysql,"CREATE TABLE test_table(col1 int, col2 varchar(50), \
                                                 col3 smallint,\
                                                 col4 timestamp(14))"))
  {
    fprintf(stderr, "\n create table failed");
    fprintf(stderr, "\n %s", mysql_error(mysql));
    exit(0);
  }
  
  /* Prepare a insert query with 3 parameters */
  strcpy(query, "INSERT INTO test_table(col1,col2,col3) values(?,?,?)");
  if(!(stmt = mysql_prepare(mysql,query,strlen(query))))
  {
    fprintf(stderr, "\n prepare, insert failed");
    fprintf(stderr, "\n %s", mysql_error(mysql));
    exit(0);
  }
  fprintf(stdout, "\n prepare, insert successful");

  /* Get the parameter count from the statement */
  param_count= mysql_param_count(stmt);

  fprintf(stdout, "\n total parameters in insert: %d", param_count);
  if (param_count != 3) /* validate parameter count */
  {
    fprintf(stderr, "\n invalid parameter count returned by MySQL");
    exit(0);
  }

  /* Bind the data for the parameters */

  /* INTEGER PART */
  memset(bind,0,sizeof(bind));
  bind[0].buffer_type= MYSQL_TYPE_LONG;
  bind[0].buffer= (void *)&int_data;
 
  /* STRING PART */
  bind[1].buffer_type= MYSQL_TYPE_VAR_STRING;
  bind[1].buffer= (void *)str_data;
  bind[1].buffer_length= sizeof(str_data);
 
  /* SMALLINT PART */
  bind[2].buffer_type= MYSQL_TYPE_SHORT;
  bind[2].buffer= (void *)&small_data;       
  bind[2].length= (long *)&length;

  /* Bind the buffers */
  if (mysql_bind_param(stmt, bind))
  {
    fprintf(stderr, "\n param bind failed");
    fprintf(stderr, "\n %s", mysql_stmt_error(stmt));
    exit(0);
  }

  /* Specify the data */
  int_data= 10;             /* integer */
  strcpy(str_data,"MySQL"); /* string  */
  /* INSERT SMALLINT data as NULL */
  length= MYSQL_NULL_DATA;

  /* Execute the insert statement - 1*/
  if (mysql_execute(stmt))
  {
    fprintf(stderr, "\n execute 1 failed");
    fprintf(stderr, "\n %s", mysql_stmt_error(stmt));
    exit(0);
  }
    
  /* Get the total rows affected */   
  affected_rows= mysql_stmt_affected_rows(stmt);

  fprintf(stdout, "\n total affected rows: %lld", affected_rows);
  if (affected_rows != 1) /* validate affected rows */
  {
    fprintf(stderr, "\n invalid affected rows by MySQL");
    exit(0);
  }

  /* Re-execute the insert, by changing the values */
  int_data= 1000;             
  strcpy(str_data,"The most popular open source database"); 
  small_data= 1000;         /* smallint */
  length= 0;

  /* Execute the insert statement - 2*/
  if (mysql_execute(stmt))
  {
    fprintf(stderr, "\n execute 2 failed");
    fprintf(stderr, "\n %s", mysql_stmt_error(stmt));
    exit(0);
  }
    
  /* Get the total rows affected */   
  affected_rows= mysql_stmt_affected_rows(stmt);

  fprintf(stdout, "\n total affected rows: %lld", affected_rows);
  if (affected_rows != 1) /* validate affected rows */
  {
    fprintf(stderr, "\n invalid affected rows by MySQL");
    exit(0);
  }

  /* Close the statement */
  if (mysql_stmt_close(stmt))
  {
    fprintf(stderr, "\n failed while closing the statement");
    fprintf(stderr, "\n %s", mysql_stmt_error(stmt));
    exit(0);
  }
  myassert(2 == my_stmt_result("SELECT * FROM test_table",50));

  /* DROP THE TABLE */
  if (mysql_query(mysql,"DROP TABLE test_table"))
  {
    fprintf(stderr, "\n drop table failed");
    fprintf(stderr, "\n %s", mysql_error(mysql));
    exit(0);
  }
  fprintf(stdout, "Success !!!");
}


static struct my_option myctest_long_options[] =
{
  {"help", '?', "Display this help and exit", 0, 0, 0, GET_NO_ARG, NO_ARG, 0,
   0, 0, 0, 0, 0},
  {"database", 'D', "Database to use", (gptr*) &opt_db, (gptr*) &opt_db,
   0, GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"host", 'h', "Connect to host", (gptr*) &opt_host, (gptr*) &opt_host, 0, GET_STR_ALLOC,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"password", 'p',
   "Password to use when connecting to server. If password is not given it's asked from the tty.",
   0, 0, 0, GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},
#ifndef DONT_ALLOW_USER_CHANGE
  {"user", 'u', "User for login if not current user", (gptr*) &opt_user,
   (gptr*) &opt_user, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#endif
  {"port", 'P', "Port number to use for connection", (gptr*) &opt_port,
   (gptr*) &opt_port, 0, GET_UINT, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"socket", 'S', "Socket file to use for connection", (gptr*) &opt_unix_socket,
   (gptr*) &opt_unix_socket, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  { 0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};

static void usage(void)
{
  /*
   *  show the usage string when the user asks for this
  */    
  puts("***********************************************************************\n");
  puts("                Test for client-server protocol 4.1");
  puts("                        By Monty & Venu \n");
  puts("This software comes with ABSOLUTELY NO WARRANTY. This is free software,");
  puts("and you are welcome to modify and redistribute it under the GPL license\n");
  puts("                 Copyright (C) 1995-2002 MySQL AB ");
  puts("-----------------------------------------------------------------------\n");
  fprintf(stdout,"usage: %s [OPTIONS]\n\n", my_progname);  
  fprintf(stdout,"\
  -?, --help		Display this help message and exit.\n\
  -D  --database=...    Database name to be used for test.\n\
  -h, --host=...	Connect to host.\n\
  -p, --password[=...]	Password to use when connecting to server.\n");
#ifdef __WIN__
  fprintf(stdout,"\
  -W, --pipe	        Use named pipes to connect to server.\n");
#endif
  fprintf(stdout,"\
  -P, --port=...	Port number to use for connection.\n\
  -S, --socket=...	Socket file to use for connection.\n");
#ifndef DONT_ALLOW_USER_CHANGE
  fprintf(stdout,"\
  -u, --user=#		User for login if not current user.\n");
#endif  
  fprintf(stdout,"*********************************************************************\n");
}

static my_bool
get_one_option(int optid, const struct my_option *opt __attribute__((unused)),
	       char *argument)
{
  switch (optid) {
  case 'p':
    if (argument)
    {
      char *start=argument;
      my_free(opt_password, MYF(MY_ALLOW_ZERO_PTR));
      opt_password= my_strdup(argument, MYF(MY_FAE));
      while (*argument) *argument++= 'x';		/* Destroy argument */
      if (*start)
        start[1]=0;
    }
    else
      tty_password= 1;
    break;
  case '?':
  case 'I':					/* Info */
    usage();
    exit(0);
    break;
  }
  return 0;
}

static const char *load_default_groups[]= { "client",0 };

static void get_options(int argc, char **argv)
{
  int ho_error;

  load_defaults("my",load_default_groups,&argc,&argv);

  if ((ho_error=handle_options(&argc,&argv, myctest_long_options, 
                               get_one_option)))
    exit(ho_error);

  /*free_defaults(argv);*/
  if (tty_password)
    opt_password=get_tty_password(NullS);
  return;
}

/********************************************************
* main routine                                          *
*********************************************************/
int main(int argc, char **argv)
{  
  MY_INIT(argv[0]);
  get_options(argc,argv);
    
  client_connect();       /* connect to server */
  client_query();         /* simple client query test */
  test_manual_sample();   /* sample in the manual */
  test_bind_result();     /* result bind test */  
  test_fetch_null();      /* to fetch null data */
  test_fetch_date();      /* to fetch date,time and timestamp */
  test_fetch_str();       /* to fetch string to all types */
  test_fetch_long();      /* to fetch long to all types */
  test_fetch_short();     /* to fetch short to all types */
  test_fetch_tiny();      /* to fetch tiny to all types */
  test_fetch_bigint();    /* to fetch bigint to all types */
  test_fetch_float();     /* to fetch float to all types */
  test_fetch_double();    /* to fetch double to all types */
  test_bind_result_ext(); /* result bind test - extension */
  test_bind_result_ext1(); /* result bind test - extension */
  test_select_direct();  /* direct select - protocol_simple debug */
  test_select_prepare();  /* prepare select - protocol_prep debug */
  test_select_direct();   /* direct select - protocol_simple debug */
  test_select();          /* simple select test */
  test_select_version();  /* select with variables */
  test_set_variable();  /* set variable prepare */
#if NOT_USED
  test_select_meta();   /* select param meta information */
  test_update_meta();   /* update param meta information */  
  test_insert_meta();   /* insert param meta information */
#endif
  test_simple_update(); /* simple update test */
  test_func_fields();   /* test for new 4.1 MYSQL_FIELD members */
  test_long_data();     /* test for sending text data in chunks */
  test_insert();        /* simple insert test - prepare */
  test_set_variable();  /* prepare with set variables */
  test_tran_innodb();   /* test for mysql_commit(), rollback() and autocommit() */
  test_select_show();   /* prepare - show test */
  test_null();          /* test null data handling */
  test_simple_update(); /* simple prepare - update */
  test_prepare_noparam();/* prepare without parameters */
  test_select();        /* simple prepare-select */
  test_insert();        /* prepare with insert */
  test_bind_result();   /* result bind test */   
  test_long_data();     /* long data handling in pieces */
  test_prepare_simple();/* simple prepare */ 
  test_prepare();       /* prepare test */
  test_null();          /* test null data handling */
  test_debug_example(); /* some debugging case */
  test_update();        /* prepare-update test */
  test_simple_update(); /* simple prepare with update */
  test_long_data();     /* long data handling in pieces */
  test_simple_delete(); /* prepare with delete */
  test_field_names();   /* test for field names */
  test_double_compare();/* float comparision */ 
  client_query();       /* simple client query test */
  client_store_result();/* usage of mysql_store_result() */
  client_use_result();  /* usage of mysql_use_result() */  
  test_tran_bdb();      /* transaction test on BDB table type */
  test_tran_innodb();   /* transaction test on InnoDB table type */ 
  test_prepare_ext();   /* test prepare with all types conversion -- TODO */
  test_prepare_syntax();/* syntax check for prepares */
  test_prepare_field_result(); /* prepare meta info */
  test_prepare_resultset(); /* prepare meta info test */
  test_field_names();   /* test for field names */
  test_field_flags();   /* test to help .NET provider team */
  test_long_data_str(); /* long data handling */
  test_long_data_str1();/* yet another long data handling */
  test_long_data_bin(); /* long binary insertion */
  test_warnings();      /* show warnings test */
  test_errors();        /* show errors test */
  test_select_simple(); /* simple select prepare */
  test_prepare_resultset();/* prepare meta info test */
  test_func_fields();   /* FUNCTION field info */
  /*test_stmt_close(); */    /* mysql_stmt_close() test -- hangs */
  test_prepare_field_result(); /* prepare meta info */
  test_multi_stmt();    /* multi stmt test */
  client_disconnect();  /* disconnect from server */

  fprintf(stdout,"\n\nSUCCESS !!!\n");
  return(0);
}
