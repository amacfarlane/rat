/*
 * FILE:    pdb.h
 * PROGRAM: RAT
 * AUTHOR:  O.Hodson
 *
 * Copyright (c) 1999-2001 University College London
 * All rights reserved.
 *
 * These functions provide a means of maintaining persistent
 * information on conference participants that is not contained in the
 * RTCP database.  Entries are stored in a binary table, identified with
 * a unique 32 bit unsigned identifer (probably the same as SSRC).
 *
 * $Id: pdb.h 3969 2007-03-02 19:18:26Z piers $
 */

#ifndef __PERSIST_DB_H__
#define __PERSIST_DB_H__

/* RAT specific includes for entries in pdb_entry_t */
#include "channel_types.h"
#include "codec_types.h"
#include "ts.h"
#include "render_3D.h"

typedef struct s_pdb pdb_t;

typedef struct {
        uint32_t        ssrc;                        /* Unique Id */
        u_char          first_mix:1;
        struct s_render_3D_dbentry  *render_3D_data; /* Participant 3d state */
        double          gain;                        /* Participant gain */
	u_char	        mute:1;                      /* source muted */
	uint16_t        units_per_packet;
        uint16_t        inter_pkt_gap;               /* expected time between pkt arrivals */
        timestamp_t            frame_dur;
        u_char          enc;
        char*           enc_fmt;
        int             enc_fmt_len;
	uint32_t        sample_rate;
        uint32_t        last_ts;
        uint32_t        last_seq;
        timestamp_t            last_arr;                    /* timestamp_t representation of last_ts */

        /* Playout info */
        timestamp_t            jitter;
        timestamp_t            transit;
        timestamp_t            last_transit;
        timestamp_t            last_last_transit;
        timestamp_t            avg_transit;
        cc_id_t         channel_coder_id;            /* channel_coder of last received packet    */
	timestamp_t            next_mix;                    /* Used to check mixing                     */
	timestamp_t            playout;                     /* Playout delay for this talkspurt         */
        ts_sequencer    seq;                         /* Mapper from RTP time rep to rat time rep */
        uint32_t        spike_events;                /* Number of spike events                   */
        uint32_t        spike_toged;                 /* Number of packets dropped in spike mode  */
        double          last_rtt;
        double          avg_rtt;

        /* Display Info */
        timestamp_t            last_ui_update;              /* Used for periodic update of packet counts, etc */

        /* Packet info */
        uint32_t        received;
        uint32_t        duplicates;
        uint32_t        misordered;
        uint32_t        jit_toged;                   /* Packets discarded because late ("Thrown on ground") */

  /* SiteID from RTCP APP packet. */
        char           *siteid;

	uint32_t	magic;	/* For debugging */
} pdb_entry_t;

/* Functions for creating and destroying persistent database.  Return
 * TRUE on success and fill in p accordingly, FALSE on failure.  */

int pdb_create  (pdb_t **p);
int pdb_destroy (pdb_t **p);

/* pdb_get_{first,next}_id attempt to get keys from database.  Return
 * TRUE on succes and fill in id.  FALSE on failure.  */

int pdb_get_first_id (pdb_t *p, uint32_t *id);

int pdb_get_next_id  (pdb_t *p, uint32_t cur_id, uint32_t *next_id);

/* Functions for manipulating persistent database items. id is key in
 * database and must be unique. */

int     pdb_item_get     (pdb_t *p, uint32_t id, pdb_entry_t **item);

int     pdb_item_create  (pdb_t *p,
                          uint16_t freq,
                          uint32_t id);

int     pdb_item_destroy (pdb_t *p, uint32_t id);
void	pdb_item_validate(pdb_entry_t *item);

uint32_t pdb_item_count   (pdb_t *p);

#endif /* __PERSIST_DB_H__ */
