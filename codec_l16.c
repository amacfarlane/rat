/*
 * FILE:    codec_l16.c
 * AUTHORS: Orion Hodson
 *
 * Copyright (c) 1998-2001 University College London
 * All rights reserved.
 */

#ifndef HIDE_SOURCE_STRINGS
static const char cvsid[] =
	"$Id: codec_l16.c 3642 2004-01-12 17:14:56Z ucacoxh $";
#endif /* HIDE_SOURCE_STRINGS */

#include "config_unix.h"
#include "config_win32.h"
#include "memory.h"
#include "util.h"
#include "debug.h"
#include "audio_types.h"
#include "codec_types.h"
#include "codec_l16.h"

/* Note payload numbers are dynamic and selected so:
 * (a) we always have one codec that can be used at each sample rate and freq
 * (b) to backwards match earlier releases.
 */

static codec_format_t cs[] = {
        {"Linear-16", "L16-8K-Mono",
         "Linear 16 uncompressed audio, please do not use wide area.",
         122, 0, 320, {DEV_S16,  8000, 16, 1, 160 * BYTES_PER_SAMPLE}}, /* 20  ms */
        {"Linear-16", "L16-8K-Stereo",
         "Linear 16 uncompressed audio, please do not use wide area.",
         111, 0, 640, {DEV_S16,  8000, 16, 2, 2 * 160 * BYTES_PER_SAMPLE}}, /* 20  ms */
        {"Linear-16", "L16-16K-Mono",
         "Linear 16 uncompressed audio, please do not use wide area.",
         112, 0, 320, {DEV_S16,  16000, 16, 1, 160 * BYTES_PER_SAMPLE}}, /* 10 ms */
        {"Linear-16", "L16-16K-Stereo",
         "Linear 16 uncompressed audio, please do not use wide area.",
         113, 0, 640, {DEV_S16,  16000, 16, 2, 2 * 160 * BYTES_PER_SAMPLE}}, /* 10 ms */
        {"Linear-16", "L16-32K-Mono",
         "Linear 16 uncompressed audio, please do not use wide area.",
         114, 0, 320, {DEV_S16,  32000, 16, 1, 160 * BYTES_PER_SAMPLE}}, /* 5 ms */
        {"Linear-16", "L16-32K-Stereo",
         "Linear 16 uncompressed audio, please do not use wide area.",
         115, 0, 640, {DEV_S16,  32000, 16, 2, 2 * 160 * BYTES_PER_SAMPLE}}, /* 5 ms */
        {"Linear-16", "L16-44K-Mono",
         "Linear 16 uncompressed audio, please do not use wide area.",
         11, 0, 320, {DEV_S16,  44100, 16, 1, 160 * BYTES_PER_SAMPLE}}, /* 3.6 ms */
        {"Linear-16", "L16-44K-Stereo",
         "Linear 16 uncompressed audio, please do not use wide area.",
         10, 0, 640, {DEV_S16,  44100, 16, 2, 2 * 160 * BYTES_PER_SAMPLE}}, /* 3.6 ms */
        {"Linear-16", "L16-48K-Mono",
         "Linear 16 uncompressed audio, please do not use wide area.",
         116, 0, 320, {DEV_S16,  48000, 16, 1, 160 * BYTES_PER_SAMPLE}}, /* 3.3 ms */
        {"Linear-16", "L16-48K-Stereo",
         "Linear 16 uncompressed audio, please do not use wide area.",
         117, 0, 640, {DEV_S16,  48000, 16, 2, 2 * 160 * BYTES_PER_SAMPLE}} /* 3.3 ms */
};

#define L16_NUM_FORMATS sizeof(cs)/sizeof(codec_format_t)

uint16_t
l16_get_formats_count()
{
        return (uint16_t)L16_NUM_FORMATS;
}

const codec_format_t *
l16_get_format(uint16_t idx)
{
        assert(idx < L16_NUM_FORMATS);
        return &cs[idx];
}

int
l16_encode(uint16_t idx, u_char *state, sample *in, coded_unit *out)
{
        int samples;
        sample *d, *de;

        assert(idx < L16_NUM_FORMATS);
        UNUSED(state);

        out->state     = NULL;
        out->state_len = 0;
        out->data      = (u_char*)block_alloc(cs[idx].mean_coded_frame_size);
        out->data_len  = cs[idx].mean_coded_frame_size;

        samples = out->data_len / 2;
        d = (sample*)out->data;
        de = d + samples;

        while (d != de) {
                *d = htons(*in);
                d++; in++;
        }
        return samples;
}

int
l16_decode(uint16_t idx, u_char *state, coded_unit *in, sample *out)
{
        int samples;
        sample *s = NULL, *se = NULL;

        assert(idx < L16_NUM_FORMATS);
        UNUSED(state);

        samples = in->data_len / BYTES_PER_SAMPLE;
        s = (sample*)in->data;
        se = s + samples;
        while(s != se) {
                *out = ntohs(*s);
                out++; s++;
        }
        return samples;
}


