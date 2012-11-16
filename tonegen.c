/*
 * FILE:    tonegen.c
 * PROGRAM: RAT
 * AUTHORS: Orion Hodson
 *
 * Copyright (c) 1999-2001 University College London
 * All rights reserved.
 */

#ifndef HIDE_SOURCE_STRINGS
static const char cvsid[] =
	"$Id: tonegen.c 3642 2004-01-12 17:14:56Z ucacoxh $";
#endif /* HIDE_SOURCE_STRINGS */

#include "config_unix.h"
#include "config_win32.h"
#include "debug.h"
#include "memory.h"
#include "util.h"

#include "ts.h"
#include "audio_types.h"
#include "pdb.h"
#include "mix.h"
#include "sndfile.h"
#include "codec_types.h"
#include "codec.h"
#include "converter_types.h"
#include "converter.h"

#include "tonegen.h"

#define TONEGEN_MAGIC 0xdeebfeed

struct s_tonegen {
        struct s_mixer     *ms;
        pdb_t              *pdb;
        pdb_entry_t        *pdbe;       /* spoof participant                 */
        timestamp_t                write_end;  /* last time played                  */
        uint8_t             played;     /* initialized indicator             */
        uint16_t            tonefreq;
        uint16_t            toneamp;
        uint32_t            magic;
};

static const uint32_t TONEGEN_SSRC_ID = 0xefff0fff; /* 1 in 4 billion */

int
tonegen_create  (tonegen_t          **ppt,
                 struct s_mixer     *ms,
                 struct s_pdb       *pdb,
                 uint16_t            tonefreq,
                 uint16_t            toneamp)
{
        tonegen_t           *pt;
        pdb_entry_t         *pdbe;

        if (pdb_item_create(pdb, 8000, TONEGEN_SSRC_ID) == FALSE ||
            pdb_item_get(pdb, TONEGEN_SSRC_ID, &pdbe) == FALSE) {
                debug_msg("tonegen could not create spoof participant\n");
                return FALSE;
        }

        pt  = (tonegen_t*)xmalloc(sizeof(tonegen_t));
        if (pt == NULL) {
                debug_msg("Could not allocate tonegen\n");
                return FALSE;
        }

        *ppt          = pt;
        pt->ms        = ms;
        pt->pdb       = pdb;
        pt->pdbe      = pdbe;
        pt->played    = 0;
        pt->magic     = TONEGEN_MAGIC;
        pt->toneamp   = toneamp;
        pt->tonefreq  = tonefreq;

        return TRUE;
}

void
tonegen_destroy(tonegen_t **ppt)
{
        tonegen_t *pt;

        pt = *ppt;
        assert(pt->magic == TONEGEN_MAGIC);

        pdb_item_destroy(pt->pdb, TONEGEN_SSRC_ID);

        xfree(pt);
        *ppt = NULL;
}

int
tonegen_play(tonegen_t *pt, timestamp_t start, timestamp_t end)
{
        timestamp_t                delta, duration;
        uint32_t            samples, phase, i;
        coded_unit          src;
        const mixer_info_t *mi;
        sample             *buf;

        assert(ts_gt(start, end) == FALSE);

        if (pt->played == 0) {
                pt->write_end = end;
                pt->played    = 1;
        }

        delta = ts_sub(pt->write_end, end);
        duration = ts_map32(8000, 1280);
        if (!ts_gt(duration, delta)) {
                /* No audio needs putting into mixer but return TRUE to */
                /* indicate still active thought.                       */
                return TRUE;
        }

        mi = mix_query(pt->ms);

        duration       = ts_convert(mi->sample_rate, duration);
        samples        = duration.ticks * mi->channels;

        /* Initialize src for reading chunk of sound file */
        src.id        = codec_get_native_coding(mi->sample_rate, mi->channels);
        src.state     = NULL;
        src.state_len = 0;
        src.data_len  = sizeof(sample) * samples;
        src.data      = (u_char*)block_alloc(src.data_len);

        buf = (sample*)src.data;
        phase = pt->write_end.ticks;
        if (mi->channels == 1) {
                for(i = 0; i < samples; i++) {
                        double t = pt->toneamp * sin(2 * M_PI * (phase + i) * pt->tonefreq / mi->sample_rate);
                        buf[i]   = (sample)t;
                }
        } else {
                for(i = 0; i < samples; i++) {
                        uint32_t j = i / mi->channels;
                        double t   = pt->toneamp * sin(2 * M_PI * (phase + j) * pt->tonefreq / mi->sample_rate);
                        buf[i]     = (sample)t;
                }
        }

        xmemchk();
        mix_put_audio(pt->ms, pt->pdbe, &src, pt->write_end);
        codec_clear_coded_unit(&src);
        pt->write_end = ts_add(pt->write_end, duration);

        return TRUE;
}

