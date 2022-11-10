/*
 *  Copyright (c) 2016. Anyfi Inc.
 *
 *  All Rights Reserved.
 *
 *  NOTICE:  All information contained herein is, and remains the property of
 *  Anyfi Inc. and its suppliers, if any.  The intellectual and technical concepts
 *  contained herein are proprietary to Anyfi Inc. and its suppliers and may be
 *  covered by U.S. and Foreign Patents, patents in process, and are protected
 *  by trade secret or copyright law.
 *
 *  Dissemination of this information or reproduction of this material is
 *  strictly forbidden unless prior written permission is obtained from Anyfi Inc.
 */

 /**
 @file list.c
 @brief ENet linked list functions
*/
#define ENET_BUILDING_LIB 1
#include "enet.h"

/** 
    @defgroup list ENet linked list utility functions
    @ingroup private
    @{
*/
void
enet_list_clear (ENetList * list)
{
   list -> sentinel.next = & list -> sentinel;
   list -> sentinel.previous = & list -> sentinel;
}

ENetListIterator
enet_list_insert (ENetListIterator position, void * data)
{
   ENetListIterator result = (ENetListIterator) data;

   result -> previous = position -> previous;
   result -> next = position;

   result -> previous -> next = result;
   position -> previous = result;

   return result;
}

void *
enet_list_remove (ENetListIterator position)
{
   position -> previous -> next = position -> next;
   position -> next -> previous = position -> previous;

   return position;
}

ENetListIterator
enet_list_move (ENetListIterator position, void * dataFirst, void * dataLast)
{
   ENetListIterator first = (ENetListIterator) dataFirst,
                    last = (ENetListIterator) dataLast;

   first -> previous -> next = last -> next;
   last -> next -> previous = first -> previous;

   first -> previous = position -> previous;
   last -> next = position;

   first -> previous -> next = first;
   position -> previous = last;
    
   return first;
}

size_t
enet_list_size (ENetList * list)
{
   size_t size = 0;
   ENetListIterator position;

   for (position = enet_list_begin (list);
        position != enet_list_end (list);
        position = enet_list_next (position))
     ++ size;
   
   return size;
}

/** @} */
