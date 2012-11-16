/*
 * FILE:    voxlet.c
 * PROGRAM: RAT
 * AUTHORS: Orion Hodson / Colin Perkins
 *
 *
 * Copyright (c) 1999-2001 University College London
 * All rights reserved.
 */

#ifndef HIDE_SOURCE_STRINGS
static const char cvsid[] =
	"$Id: voxlet.c 3642 2004-01-12 17:14:56Z ucacoxh $";
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

#include "voxlet.h"

#define VOXLET_MAGIC 0xdeebfeed

struct s_voxlet {
        struct s_sndfile   *sound;      /* open sound file                   */
        struct s_mixer     *ms;
        pdb_t              *pdb;
        pdb_entry_t        *pdbe;       /* spoof participant                 */
        struct s_converter *converter;  /* sample rate and channel converter */
        timestamp_t                write_end;  /* last time played                  */
        uint16_t            played;
        uint32_t            magic;
};

static const uint32_t VOXLET_SSRC_ID = 0xffff0000; /* 1 in 4 billion */

int
voxlet_create  (voxlet_t          **ppv,
                struct s_mixer     *ms,
                struct s_pdb       *pdb,
                const char         *sndfile)
{
        const  mixer_info_t *mi;
        struct s_sndfile    *sound;
        sndfile_fmt_t        sfmt;
        voxlet_t            *pv;
        pdb_entry_t         *pdbe;
	char                *filename, *voxdir;

	voxdir = getenv("VOXLETDIR");
	if (voxdir == NULL) {
#ifdef WIN32
		voxdir = "\voxlets";
#else
		voxdir = "/usr/local/share/voxlets";
#endif
	}

	filename = (char *) xmalloc(strlen(voxdir) + strlen(sndfile) + 2);
	sprintf(filename, "%s/%s", voxdir, sndfile);

        sound = NULL; /* snd_read_open attempts to close otherwise (yuk!) */
        if (snd_read_open(&sound, (char*)filename, NULL) == 0 || snd_get_format(sound, &sfmt) == 0) {
		xfree(filename);
                return FALSE;
        }
	xfree(filename);

        if (pdb_item_create(pdb, 8000, VOXLET_SSRC_ID) == FALSE ||
            pdb_item_get(pdb, VOXLET_SSRC_ID, &pdbe) == FALSE) {
                debug_msg("voxlet could not create spoof participant\n");
                snd_read_close(&sound);
                return FALSE;
        }

        pv  = (voxlet_t*)xmalloc(sizeof(voxlet_t));
        if (pv == NULL) {
                debug_msg("Could not allocate voxlet\n");
                pdb_item_destroy(pdb, VOXLET_SSRC_ID);
                snd_read_close(&sound);
                return FALSE;
        }

        *ppv          = pv;
        pv->sound     = sound;
        pv->ms        = ms;
        pv->pdb       = pdb;
        pv->pdbe      = pdbe;
        pv->converter = NULL;
        pv->played    = 0;
        pv->magic     = VOXLET_MAGIC;

        mi = mix_query(ms);
        if (sfmt.sample_rate != mi->sample_rate ||
            sfmt.channels    != mi->channels) {
                const converter_details_t *cd = NULL;
                converter_fmt_t            cf;
                uint32_t                   i, n;
                /* Try to get best quality converter */
                n = converter_get_count();
                for (i = 0; i < n; i++) {
                        cd = converter_get_details(i);
                        if (strncmp(cd->name, "High", 4) == 0) {
                                break;
                        }
                }
                /* Safety in case someone changes converter names */
                if (i == n) {
                        debug_msg("Could not find hq converter\n");
                        cd = converter_get_details(0);
                }

                cf.src_freq     = (uint32_t)sfmt.sample_rate;
                cf.src_channels = (uint16_t)sfmt.channels;
                cf.dst_freq     = mi->sample_rate;
                cf.dst_channels = mi->channels;
                if (converter_create(cd->id, &cf, &pv->converter) == FALSE) {
                        debug_msg("Could not create converter\n");
                        voxlet_destroy(&pv);
                        return FALSE;
                }
        }

        return TRUE;
}

void
voxlet_destroy(voxlet_t **ppv)
{
        voxlet_t   *pv;

        pv = *ppv;
        assert(pv->magic == VOXLET_MAGIC);

        if (pv->converter != NULL) {
                converter_destroy(&pv->converter);
        }

        if (pv->sound != NULL) {
                snd_read_close(&pv->sound);
        }

        pdb_item_destroy(pv->pdb, VOXLET_SSRC_ID);

        xfree(pv);
        *ppv = NULL;
}

int
voxlet_play(voxlet_t *pv, timestamp_t start, timestamp_t end)
{
        timestamp_t          duration, safety;
        uint32_t      samples;
        coded_unit    src, dst;
        sndfile_fmt_t sfmt;

        assert(ts_gt(start, end) == FALSE);

        if (pv->played == 0) {
                pv->write_end = start;
                pv->played    = 1;
        }

        if (pv->sound == NULL) {
                /* Maybe file stopped playing last round */
                return FALSE;
        }

        if (ts_gt(end, pv->write_end) == FALSE) {
                /* No audio needs putting into mixer but return TRUE to */
                /* indicate still active thought.                       */
                return TRUE;
        }

        assert(ts_valid(start));
        assert(ts_valid(end));

        snd_get_format(pv->sound, &sfmt);

        /* Calculate duration in sound file time base */
        duration = ts_sub(end, pv->write_end);

        /* Convert duration to sampling rate of file */
        duration = ts_convert(sfmt.sample_rate, duration);
        duration.ticks = duration.ticks + (320 - duration.ticks % 320);
        samples  = duration.ticks * sfmt.channels;
        safety = ts_map32(8000, 320);

        /* Initialize src for reading chunk of sound file */
        src.id        = codec_get_native_coding((uint32_t)sfmt.sample_rate, (uint16_t)sfmt.channels);
        src.state     = NULL;
        src.state_len = 0;
        src.data_len  = sizeof(sample) * samples;
        src.data      = (u_char*)block_alloc(src.data_len);

        snd_read_audio(&pv->sound,
                       (sample*)src.data,
                       (uint16_t)samples);

        xmemchk();
        if (pv->converter) {
                /* Prepare destination */
                memset(&dst, 0, sizeof(dst));

                /* Convert buffer */
                if (converter_process(pv->converter, &src, &dst) == FALSE) {
                        debug_msg("Conversion failed\n");
                }
                mix_put_audio(pv->ms, pv->pdbe, &dst, pv->write_end);
                codec_clear_coded_unit(&dst);
        } else {
                mix_put_audio(pv->ms, pv->pdbe, &src, pv->write_end);
        }
        codec_clear_coded_unit(&src);
        pv->write_end = ts_add(pv->write_end, duration);

        return TRUE;
}

