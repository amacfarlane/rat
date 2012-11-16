/*
 * FILE:    converter.c
 * PROGRAM: RAT
 * AUTHOR:  O.Hodson <O.Hodson@cs.ucl.ac.uk>
 *
 * Copyright (c) 1998-2001 University College London
 * All rights reserved.
 */

#ifndef HIDE_SOURCE_STRINGS
static const char cvsid[] =
	"$Id: convert_util.c 3642 2004-01-12 17:14:56Z ucacoxh $";
#endif /* HIDE_SOURCE_STRINGS */

#include "config_unix.h"
#include "config_win32.h"
#include "audio_types.h"
#include "converter_types.h"
#include "convert_util.h"
#include "debug.h"

/* Mono-Stereo Conversion ***************************************************/
/* Note src_len is length block in number of samples                        */
/* i.e nChannels * nSamplingIntervals                                       */

void
converter_change_channels (sample *src,
                           int src_len,
                           int src_channels,
                           sample *dst,
                           int dst_len,
                           int dst_channels)
{
        int di, si;
        int t;

        assert(src_channels == 1 || src_channels == 2);
        assert(dst_channels == 1 || dst_channels == 2);
        assert(dst_channels != src_channels);
        assert(src_len/src_channels == dst_len/dst_channels);

        if (src_len == 0) {
                return;
        }

        /* Differing directions of conversions means we can do in place
         * conversion if necessary.
         */

        switch(src_channels) {
        case 1:
                di = dst_len - 1;
                si = src_len - 1;
                do {
                        dst[di--] = src[si];
                        dst[di--] = src[si--];
                } while (si >= 0);
                assert(di == si);
                break;
        case 2:
                si = 0;
                di = 0;
                do {
                        t  = src[si++];
                        t += src[si++];
                        t /= 2;
                        dst[di++] = t;
                } while (si != src_len);
                assert(di == dst_len);
                break;
        }
        UNUSED(dst_channels);
}

int
gcd (int a, int b)
{
        if (b) return gcd(b, a%b);
        return a;
}

int
conversion_steps(int f1, int f2)
{
        int minf, maxf, r;

        minf = min(f1, f2);
        maxf = max(f1, f2);
        r = maxf / minf;

        if (f1 == f2) {
                return 0;
        } else if (r * minf == maxf) {
                return 1;
        } else {
                return 2;
        }
}

int
converter_format_valid(const converter_fmt_t *cfmt)
{
        if (cfmt->src_freq % 8000 &&
            cfmt->src_freq % 11025) {
                return FALSE;
        }

        if (cfmt->src_channels != 1 &&
            cfmt->src_channels != 2) {
                return FALSE;
        }

        if (cfmt->dst_freq % 8000 &&
            cfmt->dst_freq % 11025) {
                return FALSE;
        }

        if (cfmt->dst_channels != 1 &&
            cfmt->dst_channels != 2) {
                return FALSE;
        }

        return TRUE;
}
