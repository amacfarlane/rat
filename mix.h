/*
 * FILE:    mix.h
 * PROGRAM: RAT
 * AUTHOR:  Isidor Kouvelas + Colin Perkins
 *
 * Copyright (c) 1995-2001 University College London
 * All rights reserved.
 *
 * $Id: mix.h 3642 2004-01-12 17:14:56Z ucacoxh $
 */

#ifndef _mix_h_
#define _mix_h_

#include "codec_types.h"
#include "ts.h"

typedef struct s_mixer mixer_t;

typedef struct {
        uint32_t sample_rate;
        uint16_t channels;
        uint32_t buffer_length;
} mixer_info_t;

struct s_source;
struct s_rtcp_dbentry;

int  mix_create    (mixer_t            **ms,
                    const mixer_info_t  *mi,
		    timestamp_t                 now);

const mixer_info_t*
     mix_query     (const mixer_t      *ms);

void mix_destroy   (mixer_t           **ms);

int  mix_put_audio (mixer_t            *ms,
                    pdb_entry_t        *pdbe,
                    coded_unit         *raw_frame,
                    timestamp_t                now);

int  mix_get_audio (mixer_t            *ms,
                    int                 amount,
                    sample            **bufp);

void mix_new_cushion (mixer_t           *ms,
                      int                last_cushion_size,
                      int                new_cushion_size,
                      int                dry_time,
                      sample           **bufp);

uint16_t mix_get_energy (mixer_t           *ms,
                         uint16_t           samples);

int  mix_active      (mixer_t           *ms);

#endif /* _mix_h_ */
