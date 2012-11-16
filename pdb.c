/*
 * FILE:    pdb.c
 * PROGRAM: RAT
 * AUTHOR:  O.Hodson
 *
 * Copyright (c) 1999-2001 University College London
 * All rights reserved.
 *
 * These functions provide a means of maintaining persistent
 * information on conference participants that is not contained in the
 * RTCP database.  Entries are stored in a binary tree, identified
 * with a unique 32 bit unsigned identifer (probably the same as
 * SSRC).
 */

#ifndef HIDE_SOURCE_STRINGS
static const char cvsid[] =
	"$Id: pdb.c 3969 2007-03-02 19:18:26Z piers $";
#endif /* HIDE_SOURCE_STRINGS */

#include "config_unix.h"
#include "config_win32.h"
#include "debug.h"
#include "memory.h"
#include "btree.h"
#include "channel_types.h"
#include "audio_types.h"
#include "codec_types.h"
#include "session.h"
#include "pdb.h"

struct s_pdb {
        btree_t *db;
        uint32_t  nelem;
};

int
pdb_create(pdb_t **pp)
{
        pdb_t *p;

        p = (pdb_t*)xmalloc(sizeof(pdb_t));
        if (p == NULL) {
                *pp = NULL;
                return FALSE;
        }

        if (btree_create(&p->db) == FALSE) {
                xfree(p);
                *pp = NULL;
                return FALSE;
        }

        p->nelem = 0;
        *pp = p;
        return TRUE;
}

int
pdb_destroy(pdb_t **pp)
{
        pdb_t   *p = *pp;
        uint32_t id;

        while(pdb_get_first_id(p, &id)) {
                if (pdb_item_destroy(p, id) == FALSE) {
                        debug_msg("Failed to destroy item\n");
                        return FALSE;
                }
        }

        if (btree_destroy(&p->db) == FALSE) {
                debug_msg("Failed to destroy tree\n");
                return FALSE;
        }

        xfree(p);
        *pp = NULL;
        return TRUE;
}

uint32_t
pdb_item_count(pdb_t *p)
{
        return p->nelem;
}

int
pdb_get_first_id(pdb_t *p, uint32_t *id)
{
        return btree_get_min_key(p->db, id);
}

int
pdb_get_next_id(pdb_t *p, uint32_t cur, uint32_t *next)
{
        return btree_get_next_key(p->db, cur, next);
}

int
pdb_item_get(pdb_t *p, uint32_t id, pdb_entry_t **item)
{
        void *v;
        if (btree_find(p->db, id, &v) == FALSE) {
                *item = NULL;
                return FALSE;
        }
        assert(v != NULL);
        *item = (pdb_entry_t*)v;
	pdb_item_validate(*item);
        return TRUE;
}

int
pdb_item_create(pdb_t *p, uint16_t freq, uint32_t id)
{
        void        *item_get;
        pdb_entry_t *item;
        timestamp_t  zero_ts;

        if (btree_find(p->db, id, &item_get)) {
                debug_msg("Item already exists\n");
                return FALSE;
        }

        item = (pdb_entry_t*)xmalloc(sizeof(pdb_entry_t));
        if (item == NULL) {
                return FALSE;
        }

        /* Initialize elements of item here as necesary **********************/

	item->magic             = 0xc001babe;
        item->ssrc              = id;
        item->render_3D_data    = NULL;
        item->enc               = -1;
        item->enc_fmt_len       = 2 * (CODEC_LONG_NAME_LEN + 1);
        item->enc_fmt           = xmalloc(item->enc_fmt_len);
	item->sample_rate       = freq;
        item->gain              = 1.0;
        item->mute              = 0;
        zero_ts                 = ts_map32(8000, 0);
        item->last_ui_update    = zero_ts;
        item->first_mix         = TRUE;

        /* Initial jitter estimate (30ms)                                    */
        item->jitter            = ts_map32(8000, 240);
        item->transit           = zero_ts;
        item->last_transit      = zero_ts;
        item->last_last_transit = zero_ts;
        item->avg_transit       = zero_ts;
        item->playout           = zero_ts;
        item->last_arr          = zero_ts;
        item->last_rtt          = 0.0;
        item->avg_rtt           = 0.0;
        /* Packet stats initialization                                       */
        item->received          = 0;
        item->duplicates        = 0;
        item->misordered        = 0;
        item->jit_toged         = 0;
        item->spike_toged       = 0;
        item->spike_events      = 0;

	item->siteid          = NULL;

	pdb_item_validate(item);
        /*********************************************************************/

        if (btree_add(p->db, id, (void*)item) == FALSE) {
                debug_msg("failed to add item to persistent database!\n");
                return FALSE;
        }

        p->nelem++;
        return TRUE;
}

int
pdb_item_destroy(pdb_t *p, uint32_t id)
{
        void        *item_get;
        pdb_entry_t *item;

        if (btree_remove(p->db, id, &item_get) == FALSE) {
                debug_msg("Cannot delete item because it does not exist!\n");
                return FALSE;
        }
        item = (pdb_entry_t*)item_get;

	pdb_item_validate(item);
        assert(id == item->ssrc);

        /* clean up elements of item here ************************************/

        if (item->render_3D_data != NULL) {
                render_3D_free(&item->render_3D_data);
        }

        if (item->enc_fmt != NULL) {
                xfree(item->enc_fmt);
                item->enc_fmt = NULL;
        }

        /*********************************************************************/

        debug_msg("Removing persistent database entry for SSRC 0x%08lx\n",
                  item->ssrc);
        xfree(item);
        p->nelem--;
        return TRUE;
}

void
pdb_item_validate(pdb_entry_t *item)
{
	assert(item != NULL);
	assert(item->magic == 0xc001babe);
}

