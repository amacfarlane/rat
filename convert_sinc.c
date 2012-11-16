/*
 * FILE:    convert_sinc.c
 * PROGRAM: RAT
 * AUTHOR:  O.Hodson <O.Hodson@cs.ucl.ac.uk>
 *
 * Copyright (c) 1998-2001 University College London
 * All rights reserved.
 */

#ifndef HIDE_SOURCE_STRINGS
static const char cvsid[] =
	"$Id: convert_sinc.c 3642 2004-01-12 17:14:56Z ucacoxh $";
#endif /* HIDE_SOURCE_STRINGS */

#include "config_unix.h"
#include "config_win32.h"
#include "audio_types.h"
#include "converter_types.h"
#include "convert_sinc.h"
#include "convert_util.h"
#include "util.h"
#include "memory.h"
#include "debug.h"

#include <math.h>

/* Fixed Point Sinc Interpolation Conversion                                 */
/* Using integer maths to reduce cost of type conversion.  SINC_SCALE is     */
/* scaling factor used in filter coefficients == (1 << SINC_ROLL)            */

#define SINC_ROLL     10
#define SINC_SCALE  1024

/* Integer changes between 2 and 6 times 8-16-24-32-40-48                    */

#define SINC_MAX_CHANGE 7
#define SINC_MIN_CHANGE 2

/* Theoretically we want an infinite width filter, instead go for            */
/* limited number of cycles (SINC_CYCLES * {up,down}sampling factor)         */

#define SINC_CYCLES     5

static int32_t *upfilter[SINC_MAX_CHANGE], *downfilter[SINC_MAX_CHANGE];

int
sinc_startup (void)
{
        double dv, ham;
        int m, k, c, w;

        /* Setup filters, because we are truncating sinc fn use
         * Hamming window to smooth artefacts.
         */
        for (m = SINC_MIN_CHANGE; m < SINC_MAX_CHANGE; m++) {
                w = 2 * m * SINC_CYCLES + 1;
                c = w/2;
                upfilter[m]     = (int32_t*)xmalloc(sizeof(int32_t) * w);
                downfilter[m]   = (int32_t*)xmalloc(sizeof(int32_t) * w);
                for (k = -c; k <= +c; k++) {
                        if (k != 0) {
                                dv = sin(M_PI * k / m) / (M_PI * k / (double)m);
                        } else {
                                dv = 1.0;
                        }
                        ham = 0.54 + 0.46 * cos(2.0*k*M_PI/ (double)w);
                        upfilter[m][k + c]   =  (int32_t)(ham * SINC_SCALE * dv);
                        downfilter[m][k + c] =  (int32_t)(ham * SINC_SCALE * dv / (double)m);
                }
        }
        return TRUE;
}

void
sinc_shutdown (void)
{
        int i;

        xmemchk();
        for (i = SINC_MIN_CHANGE; i < SINC_MAX_CHANGE; i++) {
                xfree(upfilter[i]);
                xfree(downfilter[i]);
        }
}

struct s_filter_state;

static void sinc_upsample_mono   (struct s_filter_state *s,
                                  sample *src, int src_len,
                                  sample *dst, int dst_len);
static void sinc_upsample_stereo (struct s_filter_state *s,
                                  sample *src, int src_len,
                                  sample *dst, int dst_len);

static void sinc_downsample_mono   (struct s_filter_state *s,
                                    sample *src, int src_len,
                                    sample *dst, int dst_len);
static void sinc_downsample_stereo (struct s_filter_state *s,
                                    sample *src, int src_len,
                                    sample *dst, int dst_len);

typedef void (*sinc_cf)(struct s_filter_state *s, sample *src, int src_len, sample *dst, int dst_len);

typedef struct s_filter_state {
        int32_t  *filter;
        uint16_t taps;
        sample *hold_buf;     /* used to hold samples from previous round. */
        uint16_t hold_bytes;
        sinc_cf fn;           /* function to be used */
        uint16_t scale;        /* ratio of sampling rates */
} filter_state_t;

typedef struct {
        int steps;            /* Number of conversion steps = 1 or 2 */
        filter_state_t fs[2]; /* Filter states used for each step    */
} sinc_state_t;

/* sinc_init_filter -  * selects filter to use. */

static void
sinc_init_filter(filter_state_t *fs, const converter_fmt_t *cfmt)
{
        if (cfmt->src_freq < cfmt->dst_freq) {
                assert(cfmt->dst_freq / cfmt->src_freq < SINC_MAX_CHANGE);
                fs->scale  = cfmt->dst_freq/cfmt->src_freq;
                fs->filter = upfilter[fs->scale];
                fs->taps   = 2 * SINC_CYCLES * (cfmt->dst_freq/cfmt->src_freq) + 1;
                fs->hold_bytes = fs->taps * cfmt->src_channels * sizeof(sample);
                fs->hold_buf   = (sample*)block_alloc(fs->hold_bytes);
                switch(cfmt->src_channels) {
                case 1:   fs->fn = sinc_upsample_mono;   break;
                case 2:   fs->fn = sinc_upsample_stereo; break;
                default:  abort();
                }
        } else if (cfmt->src_freq > cfmt->dst_freq) {
                assert(cfmt->src_freq / cfmt->dst_freq < SINC_MAX_CHANGE);
                fs->scale  = cfmt->src_freq/cfmt->dst_freq;
                fs->filter = downfilter[fs->scale];
                fs->taps   = 2 * SINC_CYCLES * (cfmt->src_freq/cfmt->dst_freq) + 1;
                fs->hold_bytes = fs->taps * cfmt->src_channels * sizeof(sample);
                fs->hold_buf = (sample*)block_alloc(fs->hold_bytes);
                switch(cfmt->src_channels) {
                case 1:   fs->fn = sinc_downsample_mono;   break;
                case 2:   fs->fn = sinc_downsample_stereo; break;
                default:  abort();
                }
        }
        memset(fs->hold_buf, 0, fs->hold_bytes);
}

static void
sinc_free_filter(filter_state_t *fs)
{
        block_free(fs->hold_buf, fs->hold_bytes);
}

int
sinc_create (const converter_fmt_t *cfmt, u_char **state, uint32_t *state_len)
{
	converter_fmt_t	sfmt, ufmt;
        sinc_state_t	*s;
        int		g;

	if (((cfmt->src_freq % 8000) == 0 && (cfmt->dst_freq % 8000)) ||
	    ((cfmt->src_freq % 11025) == 0 && (cfmt->dst_freq % 11025))) {
		/* 11025 - 8000 not supported */
		debug_msg("convert_sinc: %d -> %d not supported\n",
			  cfmt->src_freq, cfmt->dst_freq);
		return FALSE;
	}

        g = gcd(cfmt->src_freq, cfmt->dst_freq);

        s        = (sinc_state_t*) xmalloc(sizeof(sinc_state_t));
        memset(s, 0, sizeof(sinc_state_t));
        s->steps = conversion_steps(cfmt->src_freq, cfmt->dst_freq);

	sfmt = *cfmt;

	if (sfmt.src_channels != sfmt.dst_channels) {
		/* In addition to rate conversion, this requires
		 * channel number conversion, this can happen
		 * either side of rate conversion - see sinc_convert() */
		if (sfmt.src_channels == 2 && sfmt.dst_channels == 1) {
			sfmt.src_channels = 1; /* Stereo->Mono, R1->R2 */
		}
	}

        switch(s->steps) {
        case 1:
                sinc_init_filter(s->fs, &sfmt);
                break;
        case 2:
		ufmt = sfmt;
		ufmt.dst_freq = g;
                sinc_init_filter(s->fs,     &ufmt);
		ufmt = sfmt;
		ufmt.src_freq = g;
                sinc_init_filter(s->fs + 1, &ufmt);
                break;
        }
        *state     = (u_char*)s;
        *state_len = sizeof(sinc_state_t);
        return TRUE;
}

void
sinc_destroy (u_char **state, uint32_t *state_len)
{
        int i;

        sinc_state_t *s = (sinc_state_t*)*state;

        assert(*state_len == sizeof(sinc_state_t));

        for(i = 0; i < s->steps; i++) {
                sinc_free_filter(&s->fs[i]);
        }
        xfree(s);
        *state     = NULL;
        *state_len = 0;
}

void
sinc_convert (const converter_fmt_t *cfmt,
              u_char *state,
              sample* src_buf, int src_len,
              sample *dst_buf, int dst_len)
{
        sinc_state_t *s;
        int channels;
        sample *tmp_buf;
        int     tmp_len;

        channels = cfmt->src_channels;

        s = (sinc_state_t*)state;

        if (cfmt->src_channels == 2 && cfmt->dst_channels == 1) {
                /* stereo->mono then sample rate change */
                if (s->steps) {
                        /* inplace conversion needed */
                        converter_change_channels(src_buf, src_len, 2, src_buf, src_len / 2, 1);
                        src_len /= 2;
                } else {
                        /* this is only conversion */
                        converter_change_channels(src_buf, src_len, 2, dst_buf, dst_len, 1);
                        return;
                }
                channels = 1;
        } else if (cfmt->src_channels == 1 && cfmt->dst_channels == 2) {
                dst_len /= 2;
        }

        switch(s->steps) {
        case 1:
                assert(s->fs[0].fn);
                        s->fs[0].fn(&s->fs[0], src_buf, src_len, dst_buf, dst_len);
                break;
        case 2:
                /* first step is downsampling */
                tmp_len  = src_len / s->fs[0].scale;
                tmp_buf = (sample*)block_alloc(sizeof(sample) * tmp_len);
                assert(s->fs[0].fn);
                assert(s->fs[1].fn);

                s->fs[0].fn(&s->fs[0], src_buf, src_len, tmp_buf, tmp_len);
                s->fs[1].fn(&s->fs[1], tmp_buf, tmp_len, dst_buf, dst_len);
                block_free(tmp_buf, tmp_len * sizeof(sample));
        }

        if (cfmt->src_channels == 1 && cfmt->dst_channels == 2) {
                /* sample rate change before mono-> stereo */
                if (s->steps) {
                        /* in place needed */
                        converter_change_channels(dst_buf, dst_len, 1, dst_buf, dst_len * 2, 2);
                } else {
                        /* this is our only conversion here */
                        converter_change_channels(src_buf, src_len, 1, dst_buf, dst_len * 2, 2);
                }
        }
}

/* Here begin the conversion functions... A quick word on the
 * principle of operation.  We use fixed point arithmetic to save time
 * converting to floating point and back again.  All of the
 * conversions take place using a sample buffer called work_buf.  It's
 * allocated at the start of each conversion cycle, but it is done
 * with a memory re-cycler - block_alloc, rather than malloc.  We copy
 * the incoming samples into workbuf together with samples held over
 * from previous frame.  This allows for variable block sizes to be
 * converted and makes the maths homegenous.  In an earlier attempt we
 * did not use work_buf and broke operation up across boundary between
 * incoming samples and held-over samples, but this was fiddly coding
 * and was hard to debug.
 */

#define clip16(x) if (x > 32767) { x = 32767; } else if (x < -32767) { x = -32767; }

#define LIGHT
/* LIGHT or HEAVY */
#ifdef LIGHT

static void
sinc_upsample_mono (struct s_filter_state *fs,
                    sample *src, int src_len,
                    sample *dst, int dst_len)
{
        sample *work_buf, *out;
        int     work_len;
        int32_t   tmp, si_start, si_end, si, hold_bytes;
        int32_t  *h, hi_start, hi_end, hi;

        hold_bytes = fs->taps / fs->scale * sizeof(sample);
        work_len   = src_len + hold_bytes / sizeof(sample);
        work_buf   = (sample*)block_alloc(sizeof(sample)*work_len);

        /* Get samples into work_buf */
        memcpy(work_buf, fs->hold_buf, hold_bytes);
        memcpy(work_buf + hold_bytes / sizeof(sample),
               src,
               src_len * sizeof(sample));

        /* Save last samples in src into hold_buf for next time */
        if (src_len >= (int)(hold_bytes / sizeof(sample))) {
                memcpy(fs->hold_buf,
                       src + src_len - hold_bytes / sizeof(sample),
                       hold_bytes);
        } else {
                /* incoming chunk was shorter than hold buffer */
                memmove(fs->hold_buf,
                        fs->hold_buf + src_len,
                        hold_bytes - src_len * sizeof(sample));
                memcpy(fs->hold_buf + hold_bytes / sizeof(sample) - src_len,
                       src,
                       src_len * sizeof(sample));
        }

        h      = fs->filter;
        hi_end = fs->taps;

        si_start = 0;
        si_end   = work_len - (fs->taps / fs->scale);
        out      = dst;

        switch (fs->scale) {
        case 6:
                while (si_start < si_end) {
                        si  = si_start;
                        tmp = 0;
                        hi  = 5;
                        while (hi < hi_end) {
                                tmp += work_buf[si] * h[hi];
                                hi  += fs->scale;
                                si  += 1;
                        }
                        tmp /= SINC_SCALE;
                        clip16(tmp);
                        *out++ = (short)tmp;

                        si  = si_start;
                        tmp = 0;
                        hi  = 4;
                        while (hi < hi_end) {
                                tmp += work_buf[si] * h[hi];
                                hi  += fs->scale;
                                si  += 1;
                        }
                        tmp /= SINC_SCALE;
                        clip16(tmp);
                        *out++ = (short)tmp;

                        si  = si_start;
                        tmp = 0;
                        hi  = 3;
                        while (hi < hi_end) {
                                tmp += work_buf[si] * h[hi];
                                hi  += fs->scale;
                                si  += 1;
                        }
                        tmp /= SINC_SCALE;
                        clip16(tmp);
                        *out++ = (short)tmp;

                        si  = si_start;
                        tmp = 0;
                        hi  = 2;
                        while (hi < hi_end) {
                                tmp += work_buf[si] * h[hi];
                                hi  += fs->scale;
                                si  += 1;
                        }
                        tmp /= SINC_SCALE;
                        clip16(tmp);
                        *out++ = (short)tmp;

                        si  = si_start;
                        tmp = 0;
                        hi  = 1;
                        while (hi < hi_end) {
                                tmp += work_buf[si] * h[hi];
                                hi  += fs->scale;
                                si  += 1;
                        }
                        tmp /= SINC_SCALE;
                        clip16(tmp);
                        *out++ = (short)tmp;

                        si  = si_start;
                        tmp = 0;
                        hi  = 0;
                        while (hi < hi_end) {
                                tmp += work_buf[si] * h[hi];
                                hi  += fs->scale;
                                si  += 1;
                        }
                        tmp /= SINC_SCALE;
                        clip16(tmp);
                         *out++ = (short)tmp;

                        si_start++;
                }
                break;
        case 5:
                while (si_start < si_end) {
                        si  = si_start;
                        tmp = 0;
                        hi  = 4;
                        while (hi < hi_end) {
                                tmp += work_buf[si] * h[hi];
                                hi  += fs->scale;
                                si  += 1;
                        }
                        tmp /= SINC_SCALE;
                        clip16(tmp);
                        *out++ = (short)tmp;

                        si  = si_start;
                        tmp = 0;
                        hi  = 3;
                        while (hi < hi_end) {
                                tmp += work_buf[si] * h[hi];
                                hi  += fs->scale;
                                si  += 1;
                        }
                        tmp /= SINC_SCALE;
                        clip16(tmp);
                        *out++ = (short)tmp;

                        si  = si_start;
                        tmp = 0;
                        hi  = 2;
                        while (hi < hi_end) {
                                tmp += work_buf[si] * h[hi];
                                hi  += fs->scale;
                                si  += 1;
                        }
                        tmp /= SINC_SCALE;
                        clip16(tmp);
                        *out++ = (short)tmp;

                        si  = si_start;
                        tmp = 0;
                        hi  = 1;
                        while (hi < hi_end) {
                                tmp += work_buf[si] * h[hi];
                                hi  += fs->scale;
                                si  += 1;
                        }
                        tmp /= SINC_SCALE;
                        clip16(tmp);
                        *out++ = (short)tmp;

                        si  = si_start;
                        tmp = 0;
                        hi  = 0;
                        while (hi < hi_end) {
                                tmp += work_buf[si] * h[hi];
                                hi  += fs->scale;
                                si  += 1;
                        }
                        tmp /= SINC_SCALE;
                        clip16(tmp);
                        *out++ = (short)tmp;

                        si_start++;
                }
                break;
        case 4:
                while (si_start < si_end) {
                        si  = si_start;
                        tmp = 0;
                        hi  = 3;
                        while (hi < hi_end) {
                                tmp += work_buf[si] * h[hi];
                                hi  += fs->scale;
                                si  += 1;
                        }
                        tmp /= SINC_SCALE;
                        clip16(tmp);
                        *out++ = (short)tmp;

                        si  = si_start;
                        tmp = 0;
                        hi  = 2;
                        while (hi < hi_end) {
                                tmp += work_buf[si] * h[hi];
                                hi  += fs->scale;
                                si  += 1;
                        }
                        tmp /= SINC_SCALE;
                        clip16(tmp);
                        *out++ = (short)tmp;

                        si  = si_start;
                        tmp = 0;
                        hi  = 1;
                        while (hi < hi_end) {
                                tmp += work_buf[si] * h[hi];
                                hi  += fs->scale;
                                si  += 1;
                        }
                        tmp /= SINC_SCALE;
                        clip16(tmp);
                        *out++ = (short)tmp;

                        si  = si_start;
                        tmp = 0;
                        hi  = 0;
                        while (hi < hi_end) {
                                tmp += work_buf[si] * h[hi];
                                hi  += fs->scale;
                                si  += 1;
                        }
                        tmp /= SINC_SCALE;
                        clip16(tmp);
                        *out++ = (short)tmp;

                        si_start++;
                }
                break;
        case 3:
                while (si_start < si_end) {
                        si  = si_start;
                        tmp = 0;
                        hi  = 2;
                        while (hi < hi_end) {
                                tmp += work_buf[si] * h[hi];
                                hi  += fs->scale;
                                si  += 1;
                        }
                        tmp /= SINC_SCALE;
                        clip16(tmp);
                        *out++ = (short)tmp;

                        si  = si_start;
                        tmp = 0;
                        hi  = 1;
                        while (hi < hi_end) {
                                tmp += work_buf[si] * h[hi];
                                hi  += fs->scale;
                                si  += 1;
                        }
                        tmp /= SINC_SCALE;
                        clip16(tmp);
                        *out++ = (short)tmp;

                        si  = si_start;
                        tmp = 0;
                        hi  = 0;
                        while (hi < hi_end) {
                                tmp += work_buf[si] * h[hi];
                                hi  += fs->scale;
                                si  += 1;
                        }
                        tmp /= SINC_SCALE;
                        clip16(tmp);
                        *out++ = (short)tmp;

                        si_start++;
                }
                break;
        case 2:
                while (si_start < si_end) {
                        si  = si_start;
                        tmp = 0;
                        hi  = 1;
                        while (hi < hi_end) {
                                tmp += work_buf[si] * h[hi];
                                hi  += fs->scale;
                                si  += 1;
                        }
                        tmp /= SINC_SCALE;
                        clip16(tmp);
                        *out++ = (short)tmp;

                        si  = si_start;
                        tmp = 0;
                        hi  = 0;
                        while (hi < hi_end) {
                                tmp += work_buf[si] * h[hi];
                                hi  += fs->scale;
                                si  += 1;
                        }
                        tmp /= SINC_SCALE;
                        clip16(tmp);
                        *out++ = (short)tmp;

                        si_start++;
                }
                break;
        default:
                while (si_start < si_end) {
                        hi_start = fs->scale - 1;
                        while (hi_start >= 0) {
                                tmp = 0;
                                si  = si_start;
                                hi  = hi_start;
                                while (hi < hi_end) {
                                        tmp += work_buf[si] * h[hi];
                                        hi  += fs->scale;
                                        si  += 1;
                                }
                                tmp /= SINC_SCALE;
                                clip16(tmp);
                                *out++ = (short)tmp;
                                hi_start--;
                        }
                        si_start++;
                }
        }
        assert(si_start == si_end);
        assert(out == dst + dst_len);

        block_free(work_buf, work_len * sizeof(sample));
        xmemchk();
}

#endif/*  LIGHT */
#ifdef HEAVY

/* HEAVY and LIGHT should produce same result, at time of writing they do!   */
/* HEAVY is clumsy expand buffer method, LIGHT uses 1 less buffer and copies */

static void
sinc_upsample_mono (struct s_filter_state *fs,
                    sample *src, int src_len,
                    sample *dst, int dst_len)
{
        sample *work_buf, *out;
        sample *large_buf;

        int     work_len, i, large_buf_len;
        int32_t   tmp, si_start, si_end, hold_bytes;
        int32_t  *h;

        hold_bytes = fs->taps / fs->scale * sizeof(sample);
        work_len   = src_len + hold_bytes / sizeof(sample);
        work_buf   = (sample*)block_alloc(sizeof(sample)*work_len);

        /* Get samples into work_buf */
        memcpy(work_buf, fs->hold_buf, hold_bytes);
        memcpy(work_buf + hold_bytes / sizeof(sample),
               src,
               src_len * sizeof(sample));

        /* Save last samples in src into hold_buf for next time */
        if (src_len >= (int)(hold_bytes / sizeof(sample))) {
                memcpy(fs->hold_buf,
                       src + src_len - hold_bytes / sizeof(sample),
                       hold_bytes);
        } else {
                /* incoming chunk was shorter than hold buffer */
                memmove(fs->hold_buf,
                        fs->hold_buf + src_len,
                        hold_bytes - src_len * sizeof(sample));
                memcpy(fs->hold_buf + hold_bytes / sizeof(sample) - src_len,
                       src,
                       src_len * sizeof(sample));
        }

        h = fs->filter;

        large_buf_len = fs->scale * src_len + fs->taps;
        large_buf     = (sample*)xmalloc(large_buf_len * sizeof(sample));
        memset(large_buf, 0, sizeof(sample) * large_buf_len);

        for (i = 0; i < work_len; i++) {
                large_buf[fs->scale * i + fs->scale - 1] = work_buf[i];
        }

        out = dst;
        si_start = 0;
        si_end   = large_buf_len - fs->taps;

        while (si_start < si_end) {
                tmp = 0;
                for(i = 0; i < fs->taps; i++) {
                        tmp += h[i] * large_buf[si_start + i];
                }
                tmp /= SINC_SCALE;
                clip16(tmp);
                *out++ = (short)tmp;
                si_start++;
        }

        assert(out == dst + dst_len);

        xfree(large_buf);
}

#endif /* HEAVY */

static void
sinc_upsample_stereo (struct s_filter_state *fs,
                    sample *src, int src_len,
                    sample *dst, int dst_len)
{
        sample *work_buf, *out;
        int     work_len;
        int32_t   tmp[2], si_start, si_end, si, hold_bytes;
        int32_t  *h, hi_start, hi_end, hi;

        hold_bytes = fs->taps / fs->scale * sizeof(sample) * 2;
        work_len   = src_len + hold_bytes / sizeof(sample);
        work_buf   = (sample*)block_alloc(sizeof(sample)*work_len);

        /* Get samples into work_buf */
        memcpy(work_buf, fs->hold_buf, hold_bytes);
        memcpy(work_buf + hold_bytes / sizeof(sample),
               src,
               src_len * sizeof(sample));

        /* Save last samples in src into hold_buf for next time */
        if (src_len >= (int)(hold_bytes / sizeof(sample))) {
                memcpy(fs->hold_buf,
                       src + src_len - hold_bytes / sizeof(sample),
                       hold_bytes);
        } else {
                /* incoming chunk was shorter than hold buffer */
                memmove(fs->hold_buf,
                        fs->hold_buf + src_len,
                        hold_bytes - src_len * sizeof(sample));
                memmove(fs->hold_buf + hold_bytes / sizeof(sample) - src_len,
                        src,
                        src_len * sizeof(sample));
        }

        h      = fs->filter;
        hi_end = fs->taps;

        si_start = 0;
        si_end   = work_len - (fs->taps / fs->scale) * 2;
        out      = dst;

        switch (fs->scale) {
        case 6:
                while (si_start < si_end) {
                        si  = si_start;
                        tmp[0] = tmp[1] = 0;
                        hi  = 5;
                        while (hi < hi_end) {
                                tmp[0] += work_buf[si] * h[hi];
                                tmp[1] += work_buf[si + 1] * h[hi];
                                hi  += fs->scale;
                                si  += 2;
                        }
                        tmp[0] /= SINC_SCALE;
                        tmp[1] /= SINC_SCALE;
                        clip16(tmp[0]);
                        clip16(tmp[1]);
                        *out++ = (short)tmp[0];
                        *out++ = (short)tmp[1];

                        si  = si_start;
                        tmp[0] = tmp[1] = 0;
                        hi  = 4;
                        while (hi < hi_end) {
                                tmp[0] += work_buf[si] * h[hi];
                                tmp[1] += work_buf[si + 1] * h[hi];
                                hi  += fs->scale;
                                si  += 2;
                        }
                        tmp[0] /= SINC_SCALE;
                        tmp[1] /= SINC_SCALE;
                        clip16(tmp[0]);
                        clip16(tmp[1]);
                        *out++ = (short)tmp[0];
                        *out++ = (short)tmp[1];

                        si  = si_start;
                        tmp[0] = tmp[1] = 0;
                        hi  = 3;
                        while (hi < hi_end) {
                                tmp[0] += work_buf[si] * h[hi];
                                tmp[1] += work_buf[si + 1] * h[hi];
                                hi  += fs->scale;
                                si  += 2;
                        }
                        tmp[0] /= SINC_SCALE;
                        tmp[1] /= SINC_SCALE;
                        clip16(tmp[0]);
                        clip16(tmp[1]);
                        *out++ = (short)tmp[0];
                        *out++ = (short)tmp[1];

                        si  = si_start;
                        tmp[0] = tmp[1] = 0;
                        hi  = 2;
                        while (hi < hi_end) {
                                tmp[0] += work_buf[si] * h[hi];
                                tmp[1] += work_buf[si + 1] * h[hi];
                                hi  += fs->scale;
                                si  += 2;
                        }
                        tmp[0] /= SINC_SCALE;
                        tmp[1] /= SINC_SCALE;
                        clip16(tmp[0]);
                        clip16(tmp[1]);
                        *out++ = (short)tmp[0];
                        *out++ = (short)tmp[1];

                        si  = si_start;
                        tmp[0] = tmp[1] = 0;
                        hi  = 1;
                        while (hi < hi_end) {
                                tmp[0] += work_buf[si] * h[hi];
                                tmp[1] += work_buf[si + 1] * h[hi];
                                hi  += fs->scale;
                                si  += 2;
                        }
                        tmp[0] /= SINC_SCALE;
                        tmp[1] /= SINC_SCALE;
                        clip16(tmp[0]);
                        clip16(tmp[1]);
                        *out++ = (short)tmp[0];
                        *out++ = (short)tmp[1];

                        si  = si_start;
                        tmp[0] = tmp[1] = 0;
                        hi  = 0;
                        while (hi < hi_end) {
                                tmp[0] += work_buf[si] * h[hi];
                                tmp[1] += work_buf[si + 1] * h[hi];
                                hi  += fs->scale;
                                si  += 2;
                        }
                        tmp[0] /= SINC_SCALE;
                        tmp[1] /= SINC_SCALE;
                        clip16(tmp[0]);
                        clip16(tmp[1]);
                        *out++ = (short)tmp[0];
                        *out++ = (short)tmp[1];

                        si_start += 2;
                }
                break;
        case 5:
                while (si_start < si_end) {
                        si  = si_start;
                        tmp[0] = tmp[1] = 0;
                        hi  = 4;
                        while (hi < hi_end) {
                                tmp[0] += work_buf[si] * h[hi];
                                tmp[1] += work_buf[si + 1] * h[hi];
                                hi  += fs->scale;
                                si  += 2;
                        }
                        tmp[0] /= SINC_SCALE;
                        tmp[1] /= SINC_SCALE;
                        clip16(tmp[0]);
                        clip16(tmp[1]);
                        *out++ = (short)tmp[0];
                        *out++ = (short)tmp[1];

                        si  = si_start;
                        tmp[0] = tmp[1] = 0;
                        hi  = 3;
                        while (hi < hi_end) {
                                tmp[0] += work_buf[si] * h[hi];
                                tmp[1] += work_buf[si + 1] * h[hi];
                                hi  += fs->scale;
                                si  += 2;
                        }
                        tmp[0] /= SINC_SCALE;
                        tmp[1] /= SINC_SCALE;
                        clip16(tmp[0]);
                        clip16(tmp[1]);
                        *out++ = (short)tmp[0];
                        *out++ = (short)tmp[1];

                        si  = si_start;
                        tmp[0] = tmp[1] = 0;
                        hi  = 2;
                        while (hi < hi_end) {
                                tmp[0] += work_buf[si] * h[hi];
                                tmp[1] += work_buf[si + 1] * h[hi];
                                hi  += fs->scale;
                                si  += 2;
                        }
                        tmp[0] /= SINC_SCALE;
                        tmp[1] /= SINC_SCALE;
                        clip16(tmp[0]);
                        clip16(tmp[1]);
                        *out++ = (short)tmp[0];
                        *out++ = (short)tmp[1];

                        si  = si_start;
                        tmp[0] = tmp[1] = 0;
                        hi  = 1;
                        while (hi < hi_end) {
                                tmp[0] += work_buf[si] * h[hi];
                                tmp[1] += work_buf[si + 1] * h[hi];
                                hi  += fs->scale;
                                si  += 2;
                        }
                        tmp[0] /= SINC_SCALE;
                        tmp[1] /= SINC_SCALE;
                        clip16(tmp[0]);
                        clip16(tmp[1]);
                        *out++ = (short)tmp[0];
                        *out++ = (short)tmp[1];

                        si  = si_start;
                        tmp[0] = tmp[1] = 0;
                        hi  = 0;
                        while (hi < hi_end) {
                                tmp[0] += work_buf[si] * h[hi];
                                tmp[1] += work_buf[si + 1] * h[hi];
                                hi  += fs->scale;
                                si  += 2;
                        }
                        tmp[0] /= SINC_SCALE;
                        tmp[1] /= SINC_SCALE;
                        clip16(tmp[0]);
                        clip16(tmp[1]);
                        *out++ = (short)tmp[0];
                        *out++ = (short)tmp[1];

                        si_start += 2;
                }
                break;
        case 4:
                while (si_start < si_end) {
                        si  = si_start;
                        tmp[0] = tmp[1] = 0;
                        hi  = 3;
                        while (hi < hi_end) {
                                tmp[0] += work_buf[si] * h[hi];
                                tmp[1] += work_buf[si + 1] * h[hi];
                                hi  += fs->scale;
                                si  += 2;
                        }
                        tmp[0] /= SINC_SCALE;
                        tmp[1] /= SINC_SCALE;
                        clip16(tmp[0]);
                        clip16(tmp[1]);
                        *out++ = (short)tmp[0];
                        *out++ = (short)tmp[1];

                        si  = si_start;
                        tmp[0] = tmp[1] = 0;
                        hi  = 2;
                        while (hi < hi_end) {
                                tmp[0] += work_buf[si] * h[hi];
                                tmp[1] += work_buf[si + 1] * h[hi];
                                hi  += fs->scale;
                                si  += 2;
                        }
                        tmp[0] /= SINC_SCALE;
                        tmp[1] /= SINC_SCALE;
                        clip16(tmp[0]);
                        clip16(tmp[1]);
                        *out++ = (short)tmp[0];
                        *out++ = (short)tmp[1];

                        si  = si_start;
                        tmp[0] = tmp[1] = 0;
                        hi  = 1;
                        while (hi < hi_end) {
                                tmp[0] += work_buf[si] * h[hi];
                                tmp[1] += work_buf[si + 1] * h[hi];
                                hi  += fs->scale;
                                si  += 2;
                        }
                        tmp[0] /= SINC_SCALE;
                        tmp[1] /= SINC_SCALE;
                        clip16(tmp[0]);
                        clip16(tmp[1]);
                        *out++ = (short)tmp[0];
                        *out++ = (short)tmp[1];

                        si  = si_start;
                        tmp[0] = tmp[1] = 0;
                        hi  = 0;
                        while (hi < hi_end) {
                                tmp[0] += work_buf[si] * h[hi];
                                tmp[1] += work_buf[si + 1] * h[hi];
                                hi  += fs->scale;
                                si  += 2;
                        }
                        tmp[0] /= SINC_SCALE;
                        tmp[1] /= SINC_SCALE;
                        clip16(tmp[0]);
                        clip16(tmp[1]);
                        *out++ = (short)tmp[0];
                        *out++ = (short)tmp[1];

                        si_start += 2;
                }
                break;
        case 3:
                while (si_start < si_end) {
                        si  = si_start;
                        tmp[0] = tmp[1] = 0;
                        hi  = 2;
                        while (hi < hi_end) {
                                tmp[0] += work_buf[si] * h[hi];
                                tmp[1] += work_buf[si + 1] * h[hi];
                                hi  += fs->scale;
                                si  += 2;
                        }
                        tmp[0] /= SINC_SCALE;
                        tmp[1] /= SINC_SCALE;
                        clip16(tmp[0]);
                        clip16(tmp[1]);
                        *out++ = (short)tmp[0];
                        *out++ = (short)tmp[1];

                        si  = si_start;
                        tmp[0] = tmp[1] = 0;
                        hi  = 1;
                        while (hi < hi_end) {
                                tmp[0] += work_buf[si] * h[hi];
                                tmp[1] += work_buf[si + 1] * h[hi];
                                hi  += fs->scale;
                                si  += 2;
                        }
                        tmp[0] /= SINC_SCALE;
                        tmp[1] /= SINC_SCALE;
                        clip16(tmp[0]);
                        clip16(tmp[1]);
                        *out++ = (short)tmp[0];
                        *out++ = (short)tmp[1];

                        si  = si_start;
                        tmp[0] = tmp[1] = 0;
                        hi  = 0;
                        while (hi < hi_end) {
                                tmp[0] += work_buf[si] * h[hi];
                                tmp[1] += work_buf[si + 1] * h[hi];
                                hi  += fs->scale;
                                si  += 2;
                        }
                        tmp[0] /= SINC_SCALE;
                        tmp[1] /= SINC_SCALE;
                        clip16(tmp[0]);
                        clip16(tmp[1]);
                        *out++ = (short)tmp[0];
                        *out++ = (short)tmp[1];

                        si_start += 2;
                }
                break;
        case 2:
                while (si_start < si_end) {
                        si  = si_start;
                        tmp[0] = tmp[1] = 0;
                        hi  = 1;
                        while (hi < hi_end) {
                                tmp[0] += work_buf[si] * h[hi];
                                tmp[1] += work_buf[si + 1] * h[hi];
                                hi  += fs->scale;
                                si  += 2;
                        }
                        tmp[0] /= SINC_SCALE;
                        tmp[1] /= SINC_SCALE;
                        clip16(tmp[0]);
                        clip16(tmp[1]);
                        *out++ = (short)tmp[0];
                        *out++ = (short)tmp[1];

                        si  = si_start;
                        tmp[0] = tmp[1] = 0;
                        hi  = 0;
                        while (hi < hi_end) {
                                tmp[0] += work_buf[si] * h[hi];
                                tmp[1] += work_buf[si + 1] * h[hi];
                                hi  += fs->scale;
                                si  += 2;
                        }
                        tmp[0] /= SINC_SCALE;
                        tmp[1] /= SINC_SCALE;
                        clip16(tmp[0]);
                        clip16(tmp[1]);
                        *out++ = (short)tmp[0];
                        *out++ = (short)tmp[1];
                        si_start += 2;
                }
                break;
        default:
                while (si_start < si_end) {
                        hi_start = fs->scale - 1;
                        while (hi_start >= 0) {
                                tmp[0] = tmp[1] = 0;
                                si  = si_start;
                                hi  = hi_start;
                                while (hi < hi_end) {
                                        tmp[0] += work_buf[si] * h[hi];
                                        tmp[1] += work_buf[si + 1] * h[hi];
                                        hi  += fs->scale;
                                        si  += 2;
                                }
                                tmp[0] /= SINC_SCALE;
                                tmp[1] /= SINC_SCALE;
                                clip16(tmp[0]);
                                clip16(tmp[1]);
                                *out++ = (short)tmp[0];
                                *out++ = (short)tmp[1];
                                hi_start--;
                        }
                        si_start += 2;
                }
        }
        assert(si_start == si_end);
        assert(out == dst + dst_len);

        block_free(work_buf, work_len * sizeof(sample));
        xmemchk();
}

static void
sinc_downsample_mono(struct s_filter_state *fs,
                      sample *src, int src_len,
                      sample *dst, int dst_len)
{
        int32_t *hc, *he, t, work_len;

        sample *work_buf, *ss, *sc, *de, *d;

        work_len = src_len + fs->taps;
        work_buf = (sample*)block_alloc(work_len * sizeof(sample));

        /* Get samples into work_buf */
        memcpy(work_buf, fs->hold_buf, fs->hold_bytes);
        memcpy(work_buf + fs->hold_bytes / sizeof(sample), src, src_len * sizeof(sample));

        /* Save last samples in src into hold_buf for next time */
        if (src_len >= (int)(fs->hold_bytes / sizeof(sample))) {
                memcpy(fs->hold_buf,
                       src + src_len - fs->hold_bytes / sizeof(sample),
                       fs->hold_bytes);
        } else {
                /* incoming chunk was shorter than hold buffer */
                memmove(fs->hold_buf,
                        fs->hold_buf + src_len,
                        fs->hold_bytes - src_len * sizeof(sample));
                memcpy(fs->hold_buf + fs->hold_bytes / sizeof(sample) - src_len,
                       src,
                       src_len * sizeof(sample));
        }

        d  = dst;
        de = dst + dst_len;
        sc = ss = work_buf;
        he      = fs->filter + fs->taps;

        while (d != de) {
                t = 0;
                hc = fs->filter;
                while(hc < he) {
                        t += (*sc) * (*hc);
                        sc++;
                        hc++;
                }
                t = t / SINC_SCALE;
                clip16(t);
                *d = (sample) t;

                d++;
                ss += fs->scale;
                sc = ss;
        }

        assert(d  == dst + dst_len);
        block_free(work_buf, work_len * sizeof(sample));
        xmemchk();
}

static void
sinc_downsample_stereo(struct s_filter_state *fs,
                       sample *src, int src_len,
                       sample *dst, int dst_len)
{
        int32_t *hc, *he, t0, t1, work_len;

        sample *work_buf, *ss, *sc, *d, *de;

/*        work_len = src_len + 2 * (fs->taps - fs->scale); */
	work_len = src_len + fs->hold_bytes / sizeof(sample);
        work_buf = (sample*)block_alloc(work_len * sizeof(sample));

        /* Get samples into work_buf */
        memcpy(work_buf, fs->hold_buf, fs->hold_bytes);
	xmemchk();
        memcpy(work_buf + fs->hold_bytes / sizeof(sample), src, src_len * sizeof(sample));
	xmemchk();

        /* Save last samples in src into hold_buf for next time */
        if (src_len >= (int)(fs->hold_bytes / sizeof(sample))) {
                memcpy(fs->hold_buf,
                       src + src_len - fs->hold_bytes / sizeof(sample),
                       fs->hold_bytes);
        } else {
                /* incoming chunk was shorter than hold buffer */
                memmove(fs->hold_buf,
                        fs->hold_buf + src_len,
                        fs->hold_bytes - src_len * sizeof(sample));
                memcpy(fs->hold_buf + fs->hold_bytes / sizeof(sample) - src_len,
                       src,
                       src_len * sizeof(sample));
        }

        d  = dst;
        de = dst + dst_len;
        sc = ss = work_buf;

        he = fs->filter + fs->taps;

        while (d < de) {
                t0 = t1 = 0;
                hc = fs->filter;
                sc = ss;
                while(hc < he) {
                        t0 += (*sc) * (*hc);
                        sc++;
                        t1 += (*sc) * (*hc);
                        sc++;
                        hc++;
                }

                t0 = t0 / SINC_SCALE;
                if (t0 > 32767) {
                        *d = 32767;
                } else if (t0 < -32768) {
                        *d = -32768;
                } else {
                        *d = (sample) t0;
                }
                d++;

                t1 = t1 / SINC_SCALE;
                if (t1 > 32767) {
                        *d = 32767;
                } else if (t1 < -32768) {
                        *d = -32768;
                } else {
                        *d = (sample) t1;
                }
                d++;

                ss += fs->scale * 2;
        }

        assert(d  == dst + dst_len);
        block_free(work_buf, work_len * sizeof(sample));
        xmemchk();
}


