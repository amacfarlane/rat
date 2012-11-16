/*
 * FILE:    convert_extra.c
 * PROGRAM: RAT
 * AUTHOR:  O.Hodson <O.Hodson@cs.ucl.ac.uk>
 *
 * Copyright (c) 1998-2001 University College London
 * All rights reserved.
 */

#ifndef HIDE_SOURCE_STRINGS
static const char cvsid[] =
	"$Id: convert_extra.c 3642 2004-01-12 17:14:56Z ucacoxh $";
#endif /* HIDE_SOURCE_STRINGS */

#include "config_unix.h"
#include "config_win32.h"
#include "audio_types.h"
#include "converter_types.h"
#include "convert_extra.h"
#include "convert_util.h"
#include "util.h"
#include "memory.h"
#include "debug.h"

/* Extrusion *************************************************************
 * This is the cheap and nasty, for upsampling we drag out samples and
 * downsamping we just subsample and suffer aliasing effects (v. dumb).
 */

typedef void (*extra_cf)(int offset, int channels, sample *src, int src_len, sample *dst, int dst_len);

typedef struct {
        short scale;
        int   steps;
        sample *tmp_buf;
        short   tmp_len;
        extra_cf convert_f;
} extra_state_t;

static void
extra_upsample(int offset, int channels, sample *src, int src_len, sample *dst, int dst_len)
{
        register short *sp, *dp;
        register int dstep, loop;

        sp = src + offset;
        dp = dst + offset;
        dstep = channels * dst_len / src_len;

        loop = min(dst_len / dstep, src_len / channels);
#ifdef DEBUG_CONVERT
        debug_msg("loop %d choice (%d, %d)\n", loop, dst_len/dstep, src_len/channels);
#endif
        dstep /= channels;

        switch(dstep) {
        case 6:
                while (loop--) {
                        *dp = *sp; dp += channels;
                        *dp = *sp; dp += channels;
                        *dp = *sp; dp += channels;
                        *dp = *sp; dp += channels;
                        *dp = *sp; dp += channels;
                        *dp = *sp; dp += channels;
                        sp += channels;
                }
                break;
        case 5:
                while (loop--) {
                        *dp = *sp; dp += channels;
                        *dp = *sp; dp += channels;
                        *dp = *sp; dp += channels;
                        *dp = *sp; dp += channels;
                        *dp = *sp; dp += channels;
                        sp += channels;
                }
                break;
        case 4:
                while (loop--) {
                        *dp = *sp; dp += channels;
                        *dp = *sp; dp += channels;
                        *dp = *sp; dp += channels;
                        *dp = *sp; dp += channels;
                        sp += channels;
                }
                break;
        case 3:
                while (loop--) {
                        *dp = *sp; dp += channels;
                        *dp = *sp; dp += channels;
                        *dp = *sp; dp += channels;
                        sp += channels;
                }
                break;
        case 2:
                while (loop--) {
                        *dp = *sp; dp += channels;
                        *dp = *sp; dp += channels;
                        sp += channels;
                }
                break;
        case 1:
                while (loop--) {
                        *dp = *sp; dp += channels;
                        sp += channels;
                }
                break;
        }
}

static void
extra_downsample(int offset, int channels, sample *src, int src_len, sample *dst, int dst_len)
{
        register short *sp, *dp;
        register int src_step, loop;

        src_step = channels * src_len / dst_len;
        sp = src + offset;
        dp = dst + offset;

        loop = min(src_len / src_step, dst_len / channels);

        while(loop--) {
                *dp = *sp;
                dp += channels;
                sp += src_step;
        }
}

static void
extra_init_state(extra_state_t *e, int src_freq, int dst_freq)
{
        if (src_freq > dst_freq) {
                e->convert_f = extra_downsample;
                e->scale     = src_freq / dst_freq;
        } else if (src_freq < dst_freq) {
                e->convert_f = extra_upsample;
                e->scale     = dst_freq / src_freq;
        }
        e->tmp_buf = NULL;
        e->tmp_len = 0;
}

int
extra_create (const converter_fmt_t *cfmt, u_char **state, uint32_t *state_len)
{
        extra_state_t *e;
        int denom, steps, g;

	if (((cfmt->src_freq % 8000) == 0 && (cfmt->dst_freq % 8000)) ||
	    ((cfmt->src_freq % 11025) == 0 && (cfmt->dst_freq % 11025))) {
		/* 11025 - 8000 not supported */
		return FALSE;
	}

        g        = gcd(cfmt->src_freq, cfmt->dst_freq);
        steps    = conversion_steps(cfmt->src_freq, cfmt->dst_freq);
        e        = (extra_state_t*)xmalloc(steps * sizeof(extra_state_t));
        e->steps = steps;

        switch(e->steps) {
        case 1:
                extra_init_state(e, cfmt->src_freq, cfmt->dst_freq);
                break;
        case 2:
                denom = g;
                extra_init_state(e, cfmt->src_freq, denom);
                extra_init_state(e + 1, denom, cfmt->dst_freq);
                break;
        }

        *state     = (u_char*)e;
        *state_len = steps * sizeof(extra_state_t);

        return TRUE;
}

void
extra_convert (const converter_fmt_t  *cfmt, u_char *state, sample* src_buf, int src_len, sample *dst_buf, int dst_len)
{
        extra_state_t *e;
        int i, channels;

        channels = cfmt->src_channels;
        e = (extra_state_t*)state;

        if (cfmt->src_channels == 2 && cfmt->dst_channels == 1) {
                /* stereo->mono then sample rate change */
                if (e->steps) {
                        /* inplace conversion needed */
                        converter_change_channels(src_buf, src_len, 2, src_buf, src_len / 2, 1);
                        src_len /= 2;
                } else {
                        /* this is only conversion */
                        converter_change_channels(src_buf, src_len, 2, dst_buf, dst_len, 1);
                }
                channels = 1;
        } else if (cfmt->src_channels == 1 && cfmt->dst_channels == 2) {
                dst_len /= 2;
        }

        switch(e->steps) {
        case 1:
                assert(e[0].convert_f);
                for(i = 0; i < channels; i++) {
                        e[0].convert_f(i, channels, src_buf, src_len, dst_buf, dst_len);
                }
                break;
        case 2:
                /* first step is always downsampling for moment */
                if (e->tmp_buf == NULL) {
                        e->tmp_len  = src_len / e->scale;
                        e->tmp_buf = (sample*)xmalloc(sizeof(sample) * e->tmp_len);
                }
                assert(e[0].convert_f);
                assert(e[1].convert_f);

                for(i = 0; i < channels; i++)
                        e[0].convert_f(i, channels, src_buf, src_len, e->tmp_buf, e->tmp_len);
                for(i = 0; i < channels; i++)
                        e[1].convert_f(i, channels, e->tmp_buf, e->tmp_len, dst_buf, dst_len);
                break;
        }

        if (cfmt->src_channels == 1 && cfmt->dst_channels == 2) {
                /* sample rate change before mono-> stereo */
                if (e->steps) {
                        /* in place needed */
                        converter_change_channels(dst_buf, dst_len, 1, dst_buf, dst_len * 2, 2);
                } else {
                        /* this is our only conversion here */
                        converter_change_channels(src_buf, src_len, 1, dst_buf, dst_len * 2, 2);
                }
        }
}

static void
extra_state_free(extra_state_t *e)
{
        if (e->tmp_buf) {
                xfree(e->tmp_buf);
                e->tmp_buf = NULL;
                e->tmp_len = 0;
        }
}

void
extra_destroy (u_char **state, uint32_t *state_len)
{
        int i;

        extra_state_t *e = (extra_state_t*)*state;

        assert(*state_len == e->steps * sizeof(extra_state_t));

        for(i = 0; i < e->steps; i++) {
                extra_state_free(e + i);
        }
        xfree(e);
        *state     = NULL;
        *state_len = 0;
}
