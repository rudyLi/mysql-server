/******************************************************
Simple SQL optimizer

(c) 1997 Innobase Oy

Created 12/21/1997 Heikki Tuuri
*******************************************************/

#ifndef pars0opt_h
#define pars0opt_h

#include "univ.i"
#include "que0types.h"
#include "usr0types.h"
#include "pars0sym.h"
#include "dict0types.h"
#include "row0sel.h"

/***********************************************************************
Optimizes a select. Decides which indexes to tables to use. The tables
are accessed in the order that they were written to the FROM part in the
select statement. */

void
opt_search_plan(
/*============*/
	sel_node_t*	sel_node);	/* in: parsed select node */
/***********************************************************************
Looks for occurrences of the columns of the table in the query subgraph and
adds them to the list of columns if an occurrence of the same column does not
already exist in the list. If the column is already in the list, puts a value
indirection to point to the occurrence in the column list, except if the
column occurrence we are looking at is in the column list, in which case
nothing is done. */

void
opt_find_all_cols(
/*==============*/
	ibool		copy_val,	/* in: if TRUE, new found columns are
					added as columns to copy */
	dict_index_t*	index,		/* in: index to use */
	sym_node_list_t* col_list,	/* in: base node of a list where
					to add new found columns */
	plan_t*		plan,		/* in: plan or NULL */
	que_node_t*	exp);		/* in: expression or condition */
/************************************************************************
Prints info of a query plan. */

void
opt_print_query_plan(
/*=================*/
	sel_node_t*	sel_node);	/* in: select node */

#ifndef UNIV_NONINL
#include "pars0opt.ic"
#endif

#endif 
