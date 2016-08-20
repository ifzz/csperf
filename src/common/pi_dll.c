/*
*    Copyright (C) 2015 Nikhil AP 
*
*    This program is free software: you can redistribute it and/or modify
*    it under the terms of the GNU General Public License as published by
*    the Free Software Foundation, either version 3 of the License, or
*    (at your option) any later version.
*
*    This program is distributed in the hope that it will be useful,
*    but WITHOUT ANY WARRANTY; without even the implied warranty of
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*    GNU General Public License for more details.
*
*    You should have received a copy of the GNU General Public License
*    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>

#include "log.h"
#include "pi_dll.h"


void    
pi_dll_init(pi_dll_t *list)
{
    list->dll_head = list;
    list->dll_tail = list;
}


/* Next two functions are required to prevent the compiler from
   optimizing away or reordering certain list consistency checks. */

pi_dll_t 
*pi_dll_next(pi_dll_t *entry)
{
    return(entry->dll_next);
}


pi_dll_t 
*pi_dll_prev(pi_dll_t *entry)
{
    return(entry->dll_prev);
}


int
pi_dll_idle(pi_dll_t *entry)
{
    return(entry->dll_next == entry && entry->dll_prev == entry);
}


int
pi_dll_queued(pi_dll_t *entry)
{
    return(entry->dll_next != entry || entry->dll_prev != entry);
}


void
pi_dll_insert_head(pi_dll_t *list, pi_dll_t *entry)
{
    if(!pi_dll_idle(entry)) {
        zlog_error(log_get_cat(), "%s: entry %p is not idle.\n", __FUNCTION__, entry);
        *((char*)5) = 0;
    }

    entry->dll_next = list->dll_head;
    entry->dll_prev = list;
    entry->dll_next->dll_prev = entry;
    list->dll_head = entry;
}


void
pi_dll_insert_tail(pi_dll_t *list, pi_dll_t *entry)
{
    if(!pi_dll_idle(entry)) {
        zlog_error(log_get_cat(), "%s: entry %p is not idle.\n", __FUNCTION__, entry);
        *((char*)5) = 0;
    }

    entry->dll_next = list;
    entry->dll_prev = list->dll_tail;
    entry->dll_prev->dll_next = entry;
    list->dll_tail = entry;
}


void
pi_dll_insert_following(pi_dll_t *ref, pi_dll_t *entry)
{
    if(!pi_dll_idle(entry)) {
        zlog_error(log_get_cat(), "%s: entry %p is not idle.\n", __FUNCTION__, entry);
        *((char*)5) = 0;
    }

    /* same as pi_dll_insert_head() */
    entry->dll_next = ref->dll_next;
    entry->dll_prev = ref;
    entry->dll_next->dll_prev = entry;
    ref->dll_next = entry;
}


void
pi_dll_insert_preceeding(pi_dll_t *ref, pi_dll_t *entry)
{
    if(!pi_dll_idle(entry)) {
        zlog_error(log_get_cat(), "%s: entry %p is not idle.\n",__FUNCTION__, entry);
        *((char*)5) = 0;
    }

    /* same as pi_dll_insert_tail() */
    entry->dll_next = ref;
    entry->dll_prev = ref->dll_prev;
    entry->dll_prev->dll_next = entry;
    ref->dll_prev = entry;
}


void
pi_dll_move_list(pi_dll_t *dst, pi_dll_t *src)
{
    if(!pi_dll_idle(dst)) {
        zlog_error(log_get_cat(), "%s: entry %p is not idle.\n",__FUNCTION__, dst);
        *((char*)5) = 0;
    }

    dst->dll_next = src->dll_next;
    dst->dll_next->dll_prev = dst;
    dst->dll_prev = src->dll_prev;
    dst->dll_prev->dll_next = dst;

    src->dll_next = src;
    src->dll_prev = src;
}


void
pi_dll_unlink_ex(pi_dll_t *entry, char *caller)
{

    if(pi_dll_idle(entry)) {
        zlog_error(log_get_cat(), "%s: entry %p is idle.\n", caller, entry);
        return;
    }
    if(entry->dll_next == NULL && entry->dll_prev == NULL) {
        zlog_error(log_get_cat(), "%s: entry %p is all NULL.\n", caller, entry);
        return;
    }
    if(entry->dll_next == NULL || entry->dll_prev == NULL) {
        zlog_error(log_get_cat(), "%s: entry %p is partially NULL.\n", caller, entry);
        return;
    }

    entry->dll_next->dll_prev = entry->dll_prev;
    entry->dll_prev->dll_next = entry->dll_next;
    entry->dll_next = entry->dll_prev = entry;
}


void
pi_dll_unlink(pi_dll_t *entry)
{
    pi_dll_unlink_ex(entry, "pi_dll_unlink");
}

pi_dll_t*
pi_dll_dequeue_tail(pi_dll_t *list)
{
    pi_dll_t *entry = list->dll_tail;

    if(entry == list) {
        entry = NULL;
    }

    if(entry) {
        pi_dll_unlink_ex(entry, "pi_dll_dequeue_tail");
    }

    return(entry);
}

pi_dll_t*
pi_dll_dequeue_head(pi_dll_t *list)
{
    pi_dll_t *entry = list->dll_head;

    if(entry == list) {
        entry = NULL;
    }

    if(entry) {
        pi_dll_unlink_ex(entry, "pi_dll_dequeue_head");
    }

    return(entry);
}
