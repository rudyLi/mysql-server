/* Copyright (C) 2000, 2005 MySQL AB

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

/*
  Code for generell handling of priority Queues.
  Implemention of queues from "Algoritms in C" by Robert Sedgewick.
  An optimisation of _downheap suggested in Exercise 7.51 in "Data
  Structures & Algorithms in C++" by Mark Allen Weiss, Second Edition
  was implemented by Mikael Ronstr�m 2005. Also the O(N) algorithm
  of queue_fix was implemented.
*/

#include "mysys_priv.h"
#include "mysys_err.h"
#include <queues.h>


/*
  Init queue

  SYNOPSIS
    init_queue()
    queue		Queue to initialise
    max_elements	Max elements that will be put in queue
    offset_to_key	Offset to key in element stored in queue
			Used when sending pointers to compare function
    max_at_top		Set to 1 if you want biggest element on top.
    compare		Compare function for elements, takes 3 arguments.
    first_cmp_arg	First argument to compare function

  NOTES
    Will allocate max_element pointers for queue array

  RETURN
    0	ok
    1	Could not allocate memory
*/

int init_queue(QUEUE *queue, uint max_elements, uint offset_to_key,
	       pbool max_at_top, int (*compare) (void *, byte *, byte *),
	       void *first_cmp_arg)
{
  DBUG_ENTER("init_queue");
  if ((queue->root= (byte **) my_malloc((max_elements+1)*sizeof(void*),
					 MYF(MY_WME))) == 0)
    DBUG_RETURN(1);
  queue->elements=0;
  queue->compare=compare;
  queue->first_cmp_arg=first_cmp_arg;
  queue->max_elements=max_elements;
  queue->offset_to_key=offset_to_key;
  queue->max_at_top= max_at_top ? (-1 ^ 1) : 0;
  DBUG_RETURN(0);
}


/*
  Reinitialize queue for other usage

  SYNOPSIS
    reinit_queue()
    queue		Queue to initialise
    max_elements	Max elements that will be put in queue
    offset_to_key	Offset to key in element stored in queue
			Used when sending pointers to compare function
    max_at_top		Set to 1 if you want biggest element on top.
    compare		Compare function for elements, takes 3 arguments.
    first_cmp_arg	First argument to compare function

  NOTES
    This will delete all elements from the queue.  If you don't want this,
    use resize_queue() instead.

  RETURN
    0			ok
    EE_OUTOFMEMORY	Wrong max_elements
*/

int reinit_queue(QUEUE *queue, uint max_elements, uint offset_to_key,
		 pbool max_at_top, int (*compare) (void *, byte *, byte *),
		 void *first_cmp_arg)
{
  DBUG_ENTER("reinit_queue");
  queue->elements=0;
  queue->compare=compare;
  queue->first_cmp_arg=first_cmp_arg;
  queue->offset_to_key=offset_to_key;
  queue->max_at_top= max_at_top ? (-1 ^ 1) : 0;
  resize_queue(queue, max_elements);
  DBUG_RETURN(0);
}


/*
  Resize queue

  SYNOPSIS
    resize_queue()
    queue			Queue
    max_elements		New max size for queue

  NOTES
    If you resize queue to be less than the elements you have in it,
    the extra elements will be deleted

  RETURN
    0	ok
    1	Error.  In this case the queue is unchanged
*/

int resize_queue(QUEUE *queue, uint max_elements)
{
  byte **new_root;
  DBUG_ENTER("resize_queue");
  if (queue->max_elements == max_elements)
    DBUG_RETURN(0);
  if ((new_root= (byte **) my_realloc((void *)queue->root,
				      (max_elements+1)*sizeof(void*),
				      MYF(MY_WME))) == 0)
    DBUG_RETURN(1);
  set_if_smaller(queue->elements, max_elements);
  queue->max_elements= max_elements;
  queue->root= new_root;
  DBUG_RETURN(0);
}


/*
  Delete queue

  SYNOPSIS
   delete_queue()
   queue		Queue to delete

  IMPLEMENTATION
    Just free allocated memory.

  NOTES
    Can be called safely multiple times
*/

void delete_queue(QUEUE *queue)
{
  DBUG_ENTER("delete_queue");
  if (queue->root)
  {
    my_free((gptr) queue->root,MYF(0));
    queue->root=0;
  }
  DBUG_VOID_RETURN;
}


	/* Code for insert, search and delete of elements */

void queue_insert(register QUEUE *queue, byte *element)
{
  reg2 uint idx,next;
  int cmp;

#ifndef DBUG_OFF
  if (queue->elements < queue->max_elements)
#endif
  {
    queue->root[0]=element;
    idx= ++queue->elements;

    /* max_at_top swaps the comparison if we want to order by desc */
    while ((cmp=queue->compare(queue->first_cmp_arg,
			       element+queue->offset_to_key,
			       queue->root[(next=idx >> 1)] +
			       queue->offset_to_key)) &&
	   (cmp ^ queue->max_at_top) < 0)
    {
      queue->root[idx]=queue->root[next];
      idx=next;
    }
    queue->root[idx]=element;
  }
}

	/* Remove item from queue */
	/* Returns pointer to removed element */

byte *queue_remove(register QUEUE *queue, uint idx)
{
#ifndef DBUG_OFF
  if (idx >= queue->max_elements)
    return 0;
#endif
  {
    byte *element=queue->root[++idx];	/* Intern index starts from 1 */
    queue->root[idx]=queue->root[queue->elements--];
    _downheap(queue,idx);
    return element;
  }
}

	/* Fix when element on top has been replaced */

#ifndef queue_replaced
void queue_replaced(QUEUE *queue)
{
  _downheap(queue,1);
}
#endif

#ifndef OLD_VERSION

void _downheap(register QUEUE *queue, uint idx)
{
  byte *element;
  uint elements,half_queue,offset_to_key, next_index;
  bool first= TRUE;
  uint start_idx= idx;

  offset_to_key=queue->offset_to_key;
  element=queue->root[idx];
  half_queue=(elements=queue->elements) >> 1;

  while (idx <= half_queue)
  {
    int cmp;
    next_index=idx+idx;
    if (next_index < elements &&
	(queue->compare(queue->first_cmp_arg,
			queue->root[next_index]+offset_to_key,
			queue->root[next_index+1]+offset_to_key) ^
	 queue->max_at_top) > 0)
      next_index++;
    if (first && 
        (((cmp=queue->compare(queue->first_cmp_arg,
                              queue->root[next_index]+offset_to_key,
                              element+offset_to_key)) == 0) ||
	 ((cmp ^ queue->max_at_top) > 0)))
    {
      queue->root[idx]= element;
      return;
    }
    queue->root[idx]=queue->root[next_index];
    idx=next_index;
    first= FALSE;
  }

  next_index= idx >> 1;
  while (next_index > start_idx)
  {
    if ((queue->compare(queue->first_cmp_arg,
                       queue->root[next_index]+offset_to_key,
                       element+offset_to_key) ^
         queue->max_at_top) < 0)
      break;
    queue->root[idx]=queue->root[next_index];
    idx=next_index;
    next_index= idx >> 1;
  }
  queue->root[idx]=element;
}

#else
  /*
    The old _downheap version is kept for comparisons with the benchmark
    suit or new benchmarks anyone wants to run for comparisons.
  */
	/* Fix heap when index have changed */
void _downheap(register QUEUE *queue, uint idx)
{
  byte *element;
  uint elements,half_queue,next_index,offset_to_key;
  int cmp;

  offset_to_key=queue->offset_to_key;
  element=queue->root[idx];
  half_queue=(elements=queue->elements) >> 1;

  while (idx <= half_queue)
  {
    next_index=idx+idx;
    if (next_index < elements &&
	(queue->compare(queue->first_cmp_arg,
			queue->root[next_index]+offset_to_key,
			queue->root[next_index+1]+offset_to_key) ^
	 queue->max_at_top) > 0)
      next_index++;
    if ((cmp=queue->compare(queue->first_cmp_arg,
			    queue->root[next_index]+offset_to_key,
			    element+offset_to_key)) == 0 ||
	(cmp ^ queue->max_at_top) > 0)
      break;
    queue->root[idx]=queue->root[next_index];
    idx=next_index;
  }
  queue->root[idx]=element;
}


#endif

/*
  Fix heap when every element was changed.
*/

void queue_fix(QUEUE *queue)
{
  uint i;
  for (i= queue->elements >> 1; i > 0; i--)
    _downheap(queue, i);
}

#ifdef MAIN
 /*
   A test program for the priority queue implementation.
   It can also be used to benchmark changes of the implementation
   Build by doing the following in the directory mysys
   make test_priority_queue
   ./test_priority_queue

   Written by Mikael Ronstr�m, 2005
 */

static uint num_array[1025];
static uint tot_no_parts= 0;
static uint tot_no_loops= 0;
static uint expected_part= 0;
static uint expected_num= 0;
static bool max_ind= 0;
static bool fix_used= 0;
static ulonglong start_time= 0;

static bool is_divisible_by(uint num, uint divisor)
{
  uint quotient= num / divisor;
  if (quotient * divisor == num)
    return TRUE;
  return FALSE;
}

void calculate_next()
{
  uint part= expected_part, num= expected_num;
  uint no_parts= tot_no_parts;
  if (max_ind)
  {
    do
    {
      while (++part <= no_parts)
      {
        if (is_divisible_by(num, part) &&
            (num <= ((1 << 21) + part)))
        {
          expected_part= part;
          expected_num= num;
          return;
        }
      }
      part= 0;
    } while (--num);
  }
  else
  {
    do
    {
      while (--part > 0)
      {
        if (is_divisible_by(num, part))
        {
          expected_part= part;
          expected_num= num;
          return;
        }
      }
      part= no_parts + 1;
    } while (++num);
  }
}

void calculate_end_next(uint part)
{
  uint no_parts= tot_no_parts, num;
  num_array[part]= 0;
  if (max_ind)
  {
    expected_num= 0;
    for (part= no_parts; part > 0 ; part--)
    {
      if (num_array[part])
      {
        num= num_array[part] & 0x3FFFFF;
        if (num >= expected_num)
        {
          expected_num= num;
          expected_part= part;
        }
      }
    }
    if (expected_num == 0)
      expected_part= 0;
  }
  else
  {
    expected_num= 0xFFFFFFFF;
    for (part= 1; part <= no_parts; part++)
    {
      if (num_array[part])
      {
        num= num_array[part] & 0x3FFFFF;
        if (num <= expected_num)
        {
          expected_num= num;
          expected_part= part;
        }
      }
    }
    if (expected_num == 0xFFFFFFFF)
      expected_part= 0;
  }
  return;
}
static int test_compare(void *null_arg, byte *a, byte *b)
{
  uint a_num= (*(uint*)a) & 0x3FFFFF;
  uint b_num= (*(uint*)b) & 0x3FFFFF;
  uint a_part, b_part;
  if (a_num > b_num)
    return +1;
  if (a_num < b_num)
    return -1;
  a_part= (*(uint*)a) >> 22;
  b_part= (*(uint*)b) >> 22;
  if (a_part < b_part)
    return +1;
  if (a_part > b_part)
    return -1;
  return 0;
}

bool check_num(uint num_part)
{
  uint part= num_part >> 22;
  uint num= num_part & 0x3FFFFF;
  if (part == expected_part)
    if (num == expected_num)
      return FALSE;
  printf("Expect part %u Expect num 0x%x got part %u num 0x%x max_ind %u fix_used %u \n",
          expected_part, expected_num, part, num, max_ind, fix_used);
  return TRUE;
}


void perform_insert(QUEUE *queue)
{
  uint i= 1, no_parts= tot_no_parts;
  uint backward_start= 0;

  expected_part= 1;
  expected_num= 1;
 
  if (max_ind)
    backward_start= 1 << 21;

  do
  {
    uint num= (i + backward_start);
    if (max_ind)
    {
      while (!is_divisible_by(num, i))
        num--;
      if (max_ind && (num > expected_num ||
                      (num == expected_num && i < expected_part)))
      {
        expected_num= num;
        expected_part= i;
      }
    }
    num_array[i]= num + (i << 22);
    if (fix_used)
      queue_element(queue, i-1)= (byte*)&num_array[i];
    else
      queue_insert(queue, (byte*)&num_array[i]);
  } while (++i <= no_parts);
  if (fix_used)
  {
    queue->elements= no_parts;
    queue_fix(queue);
  }
}

bool perform_ins_del(QUEUE *queue, bool max_ind)
{
  uint i= 0, no_loops= tot_no_loops, j= tot_no_parts;
  do
  {
    uint num_part= *(uint*)queue_top(queue);
    uint part= num_part >> 22;
    if (check_num(num_part))
      return TRUE;
    if (j++ >= no_loops)
    {
      calculate_end_next(part);
      queue_remove(queue, (uint) 0);
    }
    else
    {
      calculate_next();
      if (max_ind)
        num_array[part]-= part;
      else
        num_array[part]+= part;
      queue_top(queue)= (byte*)&num_array[part];
      queue_replaced(queue);
    }
  } while (++i < no_loops);
  return FALSE;
}

bool do_test(uint no_parts, uint l_max_ind, bool l_fix_used)
{
  QUEUE queue;
  bool result;
  max_ind= l_max_ind;
  fix_used= l_fix_used;
  init_queue(&queue, no_parts, 0, max_ind, test_compare, NULL);
  tot_no_parts= no_parts;
  tot_no_loops= 1024;
  perform_insert(&queue);
  if ((result= perform_ins_del(&queue, max_ind)))
  delete_queue(&queue);
  if (result)
  {
    printf("Error\n");
    return TRUE;
  }
  return FALSE;
}

static void start_measurement()
{
  start_time= my_getsystime();
}

static void stop_measurement()
{
  ulonglong stop_time= my_getsystime();
  uint time_in_micros;
  stop_time-= start_time;
  stop_time/= 10; /* Convert to microseconds */
  time_in_micros= (uint)stop_time;
  printf("Time expired is %u microseconds \n", time_in_micros);
}

static void benchmark_test()
{
  QUEUE queue_real;
  QUEUE *queue= &queue_real;
  uint i, add;
  fix_used= TRUE;
  max_ind= FALSE;
  tot_no_parts= 1024;
  init_queue(queue, tot_no_parts, 0, max_ind, test_compare, NULL);
  /*
    First benchmark whether queue_fix is faster than using queue_insert
    for sizes of 16 partitions.
  */
  for (tot_no_parts= 2, add=2; tot_no_parts < 128;
       tot_no_parts+= add, add++)
  {
    printf("Start benchmark queue_fix, tot_no_parts= %u \n", tot_no_parts);
    start_measurement();
    for (i= 0; i < 128; i++)
    {
      perform_insert(queue);
      queue_remove_all(queue);
    }
    stop_measurement();

    fix_used= FALSE;
    printf("Start benchmark queue_insert\n");
    start_measurement();
    for (i= 0; i < 128; i++)
    {
      perform_insert(queue);
      queue_remove_all(queue);
    }
    stop_measurement();
  }
  /*
    Now benchmark insertion and deletion of 16400 elements.
    Used in consecutive runs this shows whether the optimised _downheap
    is faster than the standard implementation.
  */
  printf("Start benchmarking _downheap \n");
  start_measurement();
  perform_insert(queue);
  for (i= 0; i < 65536; i++)
  {
    uint num, part;
    num= *(uint*)queue_top(queue);
    num+= 16;
    part= num >> 22;
    num_array[part]= num;
    queue_top(queue)= (byte*)&num_array[part];
    queue_replaced(queue);
  }
  for (i= 0; i < 16; i++)
    queue_remove(queue, (uint) 0);
  queue_remove_all(queue);
  stop_measurement();
}

int main()
{
  int i, add= 1;
  for (i= 1; i < 1024; i+=add, add++)
  {
    printf("Start test for priority queue of size %u\n", i);
    if (do_test(i, 0, 1))
      return -1;
    if (do_test(i, 1, 1))
      return -1;
    if (do_test(i, 0, 0))
      return -1;
    if (do_test(i, 1, 0))
      return -1;
  }
  benchmark_test();
  printf("OK\n");
  return 0;
}
#endif
