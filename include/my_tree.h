/* Copyright (C) 2000 MySQL AB

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

#ifndef _tree_h
#define _tree_h
#ifdef	__cplusplus
extern "C" {
#endif

#include "my_base.h"		/* get 'enum ha_rkey_function' */

#define MAX_TREE_HEIGHT	40	/* = max 1048576 leafs in tree */
#define ELEMENT_KEY(tree,element)\
(tree->offset_to_key ? (void*)((byte*) element+tree->offset_to_key) :\
			*((void**) (element+1)))

#define tree_set_pointer(element,ptr) *((byte **) (element+1))=((byte*) (ptr))

typedef enum { left_root_right, right_root_left } TREE_WALK;
typedef uint32 element_count;
typedef int (*tree_walk_action)(void *,element_count,void *);

typedef enum { free_init, free_free, free_end } TREE_FREE;
typedef void (*tree_element_free)(void*, TREE_FREE, void *);

#ifdef MSDOS
typedef struct st_tree_element {
  struct st_tree_element *left,*right;
  unsigned long count;
  uchar    colour;			/* black is marked as 1 */
} TREE_ELEMENT;
#else
typedef struct st_tree_element {
  struct st_tree_element *left,*right;
  uint32 count:31,
	 colour:1;			/* black is marked as 1 */
} TREE_ELEMENT;
#endif /* MSDOS */

#define ELEMENT_CHILD(element, offs) (*(TREE_ELEMENT**)((char*)element + offs))

typedef struct st_tree {
  TREE_ELEMENT *root,null_element;
  TREE_ELEMENT **parents[MAX_TREE_HEIGHT];
  uint offset_to_key,elements_in_tree,size_of_element,memory_limit,allocated;
  qsort_cmp2 compare;
  void* custom_arg;
  MEM_ROOT mem_root;
  my_bool with_delete;
  tree_element_free free;
} TREE;

	/* Functions on whole tree */
void init_tree(TREE *tree, uint default_alloc_size, uint memory_limit,
               int size, qsort_cmp2 compare, my_bool with_delete,
	       tree_element_free free_element, void *custom_arg);
void delete_tree(TREE*);
void reset_tree(TREE*);
  /* similar to delete tree, except we do not my_free() blocks in mem_root
   */
#define is_tree_inited(tree) ((tree)->root != 0)

	/* Functions on leafs */
TREE_ELEMENT *tree_insert(TREE *tree,void *key, uint key_size, 
                          void *custom_arg);
void *tree_search(TREE *tree, void *key, void *custom_arg);
int tree_walk(TREE *tree,tree_walk_action action,
	      void *argument, TREE_WALK visit);
int tree_delete(TREE *tree, void *key, void *custom_arg);

void *tree_search_key(TREE *tree, const void *key, 
                      TREE_ELEMENT **parents, TREE_ELEMENT ***last_pos,
                      enum ha_rkey_function flag, void *custom_arg);
void *tree_search_edge(TREE *tree, TREE_ELEMENT **parents, 
                        TREE_ELEMENT ***last_pos, int child_offs);
void *tree_search_next(TREE *tree, TREE_ELEMENT ***last_pos, int l_offs, 
                       int r_offs);
uint tree_record_pos(TREE *tree, const void *key, 
                     enum ha_rkey_function search_flag, void *custom_arg);
#ifdef	__cplusplus
}
#endif
#endif
