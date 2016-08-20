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

#ifndef __PI_DLL_H
#define __PI_DLL_H

/* doubly linked list */

typedef struct pi_dll_s pi_dll_t;

struct pi_dll_s {
    pi_dll_t        *dll_next;
    pi_dll_t        *dll_prev;
};

#define    dll_head dll_next
#define    dll_tail dll_prev

#define pi_dll_empty(_dll) ((_dll)->dll_next == (_dll))
#define pi_dll_queued_m(_dll) ((_dll)->dll_next != (_dll))

void pi_dll_init(pi_dll_t *list);
pi_dll_t *pi_dll_next(pi_dll_t *entry);
pi_dll_t *pi_dll_prev(pi_dll_t *entry);
void pi_dll_insert_head(pi_dll_t *list, pi_dll_t *entry);
void pi_dll_insert_tail(pi_dll_t *list, pi_dll_t *entry);
void pi_dll_insert_following(pi_dll_t *ref, pi_dll_t *entry);
void pi_dll_insert_preceeding(pi_dll_t *ref, pi_dll_t *entry);
void pi_dll_move_list(pi_dll_t *dst, pi_dll_t *src);
void pi_dll_unlink(pi_dll_t *entry);
int pi_dll_queued(pi_dll_t *entry);
pi_dll_t *pi_dll_dequeue_tail(pi_dll_t *list);
pi_dll_t *pi_dll_dequeue_head(pi_dll_t *list);

#define PI_IMH_BUF_SIZE         64
#define PI_IP_HDR_BUF_SIZE      64
#define PI_TCP_HDR_BUF_SIZE     64


/* generic linkage with back reference */

typedef struct pi_brlink_s {
    pi_dll_t        brl_dll;
    void            *brl_ref;
} pi_brlink_t;

#endif /* __PI_DLL_H */
