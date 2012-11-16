/*
 * FILE:     playout.c
 * AUTHORS:  Orion Hodson
 * MODIFIED: Colin Perkins
 *
 * Copyright (c) 1999-2001 University College London
 * All rights reserved.
 */

#ifndef HIDE_SOURCE_STRINGS
static const char cvsid[] =
	"$Id: playout.c 3642 2004-01-12 17:14:56Z ucacoxh $";
#endif /* HIDE_SOURCE_STRINGS */

#include "config_unix.h"
#include "config_win32.h"
#include "debug.h"
#include "memory.h"
#include "util.h"
#include "playout.h"
#include "ts.h"

/* Playout buffer types ******************************************************/

typedef struct s_pb_node {
        struct s_pb_node* prev;
        struct s_pb_node* next;
        timestamp_t              playout;    /* playout  timestamp */
        u_char           *data;
        uint32_t          data_len;
	uint32_t	  magic;
} pb_node_t;

typedef struct s_pb_iterator {
        struct s_pb *buffer;
        pb_node_t   *node;
} pb_iterator_t;

#define PB_MAX_ITERATORS 5

typedef struct s_pb {
        pb_node_t      	 sentinel;	/* Stores head and tail */
        pb_node_t     	*psentinel;	/* pointer to sentinel - saves address calc in link ops.  */
        /* Free proc for data added, so we don't leak when buffer destroyed and so we can audit the buffer */
        void          	(*freeproc)(u_char **data, uint32_t len);
        /* Iterators for the buffer */
        uint16_t       	n_iterators;
        pb_iterator_t 	iterators[PB_MAX_ITERATORS];
        /* Debugging info */
        uint32_t        n_nodes;
	uint32_t	magic;
} pb_t;

/* Validation/debugging functions * ******************************************/

#define PB_MAGIC	0x83838383
#define PB_NODE_MAGIC	0x12341234

static void
pb_validate(pb_t *pb)
{
#ifdef DEBUG
        pb_node_t	*stop, *curr;
        uint32_t	 cnt = 0;

        stop = pb->psentinel;
        curr = pb->psentinel->prev;
        while (curr != stop) {
		assert(curr->magic == PB_NODE_MAGIC);
		assert(curr->data  != NULL);
		assert(curr->data_len > 0);
		assert(curr->next->prev == curr);
		assert(curr->prev->next == curr);
		if (curr->next != stop) assert(ts_gt(curr->next->playout, curr->playout));
		if (curr->prev != stop) assert(ts_gt(curr->playout, curr->prev->playout));
                curr = curr->prev;
                cnt++;
        }
        assert(pb->n_nodes == cnt);

	assert(pb->n_iterators < PB_MAX_ITERATORS);
	for (cnt = 0; cnt < pb->n_iterators; cnt ++) {
		assert(pb->iterators[cnt].buffer == pb);
	}
#endif
	assert(pb->magic == PB_MAGIC);
}

/* Construction / Destruction functions **************************************/

int
pb_create(pb_t **ppb, void (*datafreeproc)(u_char **data, uint32_t data_len))
{
        pb_t *pb;

        assert(datafreeproc  != NULL);

        pb = (pb_t*)xmalloc(sizeof(pb_t));
        if (pb != NULL) {
                memset(pb, 0, sizeof(pb_t));
                *ppb = pb;
                pb->sentinel.next = pb->sentinel.prev = &pb->sentinel;
                pb->psentinel     = &pb->sentinel;
                pb->freeproc      = datafreeproc;
		pb->magic         = PB_MAGIC;
		pb_validate(pb);
                return TRUE;
        }
        return FALSE;
}

int
pb_destroy (pb_t **ppb)
{
        pb_t *pb = *ppb;
	pb_validate(pb);
        pb_flush(pb);

#ifdef DEBUG
        if (pb->n_iterators != 0) {
                debug_msg("Iterators dangling from buffer.  Release first.\n");
		abort();
        }
#endif

        assert(pb->n_iterators == 0);
        xfree(pb);
        *ppb = NULL;

        return TRUE;
}

void
pb_flush (pb_t *pb)
{
        pb_node_t *curr, *next, *stop;
        uint16_t i, n_iterators;

	pb_validate(pb);
        stop  = pb->psentinel;
        curr  = stop->next; /* next = head */

        while(curr != stop) {
                next = curr->next;
                curr->next->prev = curr->prev;
                curr->prev->next = curr->next;
                pb->freeproc(&curr->data, curr->data_len);
                pb->n_nodes--;
                assert(curr->data == NULL);
                block_free(curr, sizeof(pb_node_t));
                curr = next;
        }

        assert(stop->next  == stop);
        assert(stop->prev  == stop);
        assert(pb->n_nodes == 0);

        /* Reset all iterators */
        n_iterators = pb->n_iterators;
        for(i = 0; i < n_iterators; i++) {
                pb->iterators[i].node = stop;
        }
	pb_validate(pb);
}

int
pb_add (pb_t *pb, u_char *data, uint32_t data_len, timestamp_t playout)
{
        pb_node_t *curr, *stop, *made;

	assert(data != NULL);
	assert(data_len > 0);
	pb_validate(pb);

        /* Assuming that addition will be at the end of list (or near it) */
        stop = pb->psentinel;
        curr = stop->prev;    /* list tail */
        while (curr != stop && ts_gt(curr->playout, playout)) {
                curr = curr->prev;
        }

        /* Check if unit already exists */
        if (curr != stop && ts_eq(curr->playout, playout)) {
                debug_msg("Add failed - unit already exists\n");
                return FALSE;
        }

        made = (pb_node_t*)block_alloc(sizeof(pb_node_t));
        if (made) {
                made->playout  = playout;
                made->data     = data;
                made->data_len = data_len;
                made->prev = curr;
                made->next = curr->next;
                made->next->prev = made;
                made->prev->next = made;
		made->magic      = PB_NODE_MAGIC;
                pb->n_nodes++;
		pb_validate(pb);
                return TRUE;
        }
        debug_msg("Insufficient memory\n");
        return FALSE;
}

void
pb_shift_back(pb_t *pb, timestamp_t delta)
{
        pb_node_t *stop, *curr;

	pb_validate(pb);
        stop = pb->psentinel;
        curr = pb->psentinel->next;

        while(curr != stop) {
                curr->playout = ts_add(curr->playout, delta);
                curr = curr->next;
        }
	pb_validate(pb);
}

void
pb_shift_forward(pb_t *pb, timestamp_t delta)
{
        pb_node_t *stop, *curr;

	pb_validate(pb);
        stop = pb->psentinel;
        curr = pb->psentinel->next;

        while(curr != stop) {
                curr->playout = ts_sub(curr->playout, delta);
                curr = curr->next;
        }
	pb_validate(pb);
}

void
pb_shift_units_back_after(pb_t *pb, timestamp_t ref_ts, timestamp_t delta)
{
        pb_node_t *stop, *curr;

	pb_validate(pb);
        stop = pb->psentinel;
        curr = pb->psentinel->prev;
        while(curr != stop && ts_gt(curr->playout, ref_ts)) {
                curr->playout = ts_add(curr->playout, delta);
                curr = curr->prev;
        }
	pb_validate(pb);
}

int
pb_is_empty(pb_t *pb)
{
	pb_validate(pb);
        return (pb->psentinel->next == pb->psentinel);
}

uint32_t
pb_node_count(pb_t *pb)
{
	pb_validate(pb);
        return pb->n_nodes;
}

uint16_t
pb_iterator_count(pb_t *pb)
{
        return pb->n_iterators;
}

/* Iterator functions ********************************************************/

int
pb_iterator_create(pb_t           *pb,
                   pb_iterator_t **ppi)
{
        pb_iterator_t *pi;
        if (pb->n_iterators == PB_MAX_ITERATORS) {
                debug_msg("Too many iterators\n");
                *ppi = NULL;
                return FALSE;
        }
	pb_validate(pb);

        /* Link the playout buffer to the iterator */
        pi = &pb->iterators[pb->n_iterators];
        pb->n_iterators++;
        /* Set up node and buffer references */
        pi->node   = &pb->sentinel;
        pi->buffer = pb;
        *ppi = pi;

	pb_validate(pi->buffer);
        return TRUE;
}

void
pb_iterator_destroy(pb_t           *pb,
                    pb_iterator_t **ppi)
{
        uint16_t i, j;
        pb_iterator_t *pi;

	pb_validate(pb);
        pi = *ppi;
        assert(pi->buffer == pb);

        /* Remove iterator from buffer's list of iterators and
         * compress list in one pass */
        for(j = 0, i = 0; j < pb->n_iterators; i++,j++) {
                if (&pb->iterators[i] == pi) {
                        j++;
                }
                pb->iterators[i] = pb->iterators[j];
        }
        pb->n_iterators--;

        /* Empty pointer */
        *ppi = NULL;
}

int
pb_iterator_dup(pb_iterator_t **pb_dst,
                pb_iterator_t  *pb_src)
{
	pb_validate(pb_src->buffer);
        if (pb_iterator_create(pb_src->buffer, pb_dst)) {
                (*pb_dst)->node = pb_src->node;
                return TRUE;
        }
        return FALSE;
}

int
pb_iterator_get_at(pb_iterator_t *pi,
                   u_char       **data,
                   uint32_t       *data_len,
                   timestamp_t          *playout)
{
        pb_node_t *sentinel = pi->buffer->psentinel;
       /* This can be rewritten in fewer lines, but the obvious way is not as efficient. */
	pb_validate(pi->buffer);
        if (pi->node != sentinel) {
                *data     = pi->node->data;
                *data_len = pi->node->data_len;
                *playout  = pi->node->playout;
                return TRUE;
        } else {
                /* We are at the start of the list so maybe we haven't tried reading before */
                pi->node = sentinel->next;
                if (pi->node != sentinel) {
                        *data     = pi->node->data;
                        *data_len = pi->node->data_len;
                        *playout  = pi->node->playout;
                        return TRUE;
                }
        }
        /* There is data on the list */
        *data     = NULL;
        *data_len = 0;
        memset(playout, 0, sizeof(timestamp_t));
        return FALSE;
}

int
pb_iterator_detach_at (pb_iterator_t *pi,
                       u_char       **data,
                       uint32_t      *data_len,
                       timestamp_t          *playout)
{
        pb_iterator_t  *iterators;
        pb_node_t      *curr_node, *next_node;
        uint16_t         i, n_iterators;

	pb_validate(pi->buffer);
        if (pb_iterator_get_at(pi, data, data_len, playout) == FALSE) {
                /* There is no data to get */
                return FALSE;
        }

        /* Check we are not attempting to remove
         * data that another iterator is pointing to
         */
        iterators   = &pi->buffer->iterators[0];
        n_iterators = pi->buffer->n_iterators;
        for(i = 0; i < n_iterators; i++) {
                if (iterators[i].node == pi->node && iterators + i != pi) {
                        debug_msg("Eek removing node that another iterator is using...danger!\n");
                        iterators[i].node = iterators[i].node->prev;
                }
        }

        /* Relink list of nodes */
        curr_node = pi->node;
        next_node = curr_node->next;

        curr_node->next->prev = curr_node->prev;
        curr_node->prev->next = curr_node->next;

#ifdef DEBUG
	curr_node->next     = NULL;
	curr_node->prev     = NULL;
	curr_node->magic    = 0;
	curr_node->data     = NULL;
	curr_node->data_len = 0;
#endif
        block_free(curr_node, sizeof(pb_node_t));
        pi->node = next_node;
        pi->buffer->n_nodes--;
	pb_validate(pi->buffer);
        return TRUE;
}

int
pb_iterator_advance(pb_iterator_t *pi)
{
        pb_node_t *sentinel = pi->buffer->psentinel;

	pb_validate(pi->buffer);
        if (pi->node->next != sentinel) {
                pi->node = pi->node->next;
                return TRUE;
        }
        return FALSE;
}

int
pb_iterator_retreat(pb_iterator_t *pi)
{
        pb_node_t *sentinel = pi->buffer->psentinel;

	pb_validate(pi->buffer);
        if (pi->node->prev != sentinel) {
                pi->node = pi->node->prev;
                return TRUE;
        }
        return FALSE;
}

int
pb_iterator_ffwd(pb_iterator_t *pi)
{
        pb_node_t *sentinel = pi->buffer->psentinel;

	pb_validate(pi->buffer);
        /* Sentinel prev is tail of buffer */
        if (sentinel->prev != sentinel) {
                pi->node = sentinel->prev;
                return TRUE;
        }
        return FALSE;
}

int
pb_iterator_rwd(pb_iterator_t *pi)
{
        pb_node_t *sentinel = pi->buffer->psentinel;

	pb_validate(pi->buffer);
        if (sentinel->next != sentinel) {
                pi->node = sentinel->next;
                return TRUE;
        }
        return FALSE;
}


int
pb_iterators_equal(pb_iterator_t *pi1,
                   pb_iterator_t *pi2)
{
        assert(pi1 != NULL);
        assert(pi2 != NULL);

	pb_validate(pi1->buffer);
	pb_validate(pi2->buffer);
        /* Move nodes off of sentinel if necessary and possible */
        if (pi1->node == pi1->buffer->psentinel) {
                pi1->node = pi1->buffer->psentinel->next; /* yuk */
        }
        if (pi2->node == pi2->buffer->psentinel) {
                pi2->node = pi2->buffer->psentinel->next; /* yuk */
        }

        return (pi1->node == pi2->node);
}

/* Book-keeping functions ****************************************************/

int
pb_iterator_audit(pb_iterator_t *pi, timestamp_t history_len)
{
        timestamp_t cutoff;
        int  removed;
        pb_node_t *stop, *curr, *next;
        pb_t      *pb;

#ifdef DEBUG
        /* If we are debugging we check we are not deleting
         * nodes pointed to by other iterators since this *BAD*
         */
        uint16_t        i, n_iterators;
        pb_iterator_t *iterators = pi->buffer->iterators;
        n_iterators = pi->buffer->n_iterators;
#endif

	pb_validate(pi->buffer);

        pb   = pi->buffer;
        stop = pi->node;
        removed = 0;
        if (pi->node != pb->psentinel) {
                curr = pb->psentinel->next; /* head */;
                cutoff = ts_sub(pi->node->playout, history_len);
                while(ts_gt(cutoff, curr->playout)) {
                        /* About to erase a block an iterator is using! */
#ifdef DEBUG
                        for(i = 0; i < n_iterators; i++) {
                                assert(iterators[i].node != curr);
                        }
#endif
                        next = curr->next;
                        curr->next->prev = curr->prev;
                        curr->prev->next = curr->next;
                        pb->freeproc(&curr->data, curr->data_len);
                        pb->n_nodes--;
                        block_free(curr, sizeof(pb_node_t));
                        curr = next;
                        removed ++;
                }
        }
        return removed;
}

int
pb_relevant (struct s_pb *pb, timestamp_t now)
{
        pb_node_t *last;

        last = pb->psentinel->prev; /* tail */

        if (last == pb->psentinel || ts_gt(now, last->playout)) {
                return FALSE;
        }

        return TRUE;
}

/* Timestamp info functions **************************************************/

int
pb_get_start_ts(pb_t *pb, timestamp_t *ts)
{
        pb_validate(pb);
        assert(ts);

        if (pb->sentinel.next != pb->psentinel) {
                *ts = pb->sentinel.next->playout;
                return TRUE;
        }
        return FALSE;
}

int
pb_get_end_ts(pb_t *pb, timestamp_t *ts)
{
        pb_validate(pb);
        assert(ts);

        if (pb->sentinel.prev != pb->psentinel) {
                *ts = pb->sentinel.prev->playout;
                return TRUE;
        }
        return FALSE;
}

int
pb_iterator_get_ts(pb_iterator_t *pbi, timestamp_t *ts)
{
        pb_validate(pbi->buffer);
        assert(ts);
        if (pbi->node != pbi->buffer->psentinel) {
                *ts = pbi->node->playout;
                return TRUE;
        }
        return FALSE;
}

struct s_pb*
pb_iterator_get_playout_buffer(pb_iterator_t *pbi)
{
        return pbi->buffer;
}
