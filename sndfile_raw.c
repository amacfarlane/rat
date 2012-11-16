/*
 * FILE:    sndfile_raw.c
 * PROGRAM: RAT
 * AUTHOR:  Orion Hodson
 *
 * Copyright (c) 1998-2001 University College London
 * All rights reserved.
 */

#ifndef HIDE_SOURCE_STRINGS
static const char cvsid[] =
	"$Id: sndfile_raw.c 3642 2004-01-12 17:14:56Z ucacoxh $";
#endif /* HIDE_SOURCE_STRINGS */

#include "config_unix.h"
#include "config_win32.h"
#include "debug.h"
#include "memory.h"
#include "util.h"
#include "audio_types.h"
#include "codec_g711.h"
#include "sndfile_types.h"
#include "sndfile_raw.h"

typedef struct {
        sndfile_fmt_t fmt;
} raw_state_t;

int /* Returns true if can decode header */
raw_read_hdr(FILE *pf, char **state, sndfile_fmt_t *fmt)
{
        raw_state_t *rs;

        if (pf == NULL || fmt == NULL) {
                return FALSE;
        }

        rs = (raw_state_t*)xmalloc(sizeof(raw_state_t));
        if (rs == NULL) {
                return FALSE;
        }

        memcpy(&rs->fmt, fmt, sizeof(*fmt));

        *state = (char*)rs;
        return TRUE;
}

int /* Returns the number of samples read */
raw_read_audio(FILE *pf, char* state, sample *buf, int samples)
{
        raw_state_t *rs;
        int unit_sz, samples_read, i;
        u_char *law;
        sample *bp;

        rs = (raw_state_t*)state;

        switch(rs->fmt.encoding) {
        case SNDFILE_ENCODING_PCMA:
        case SNDFILE_ENCODING_PCMU:
        case SNDFILE_ENCODING_L8:
                unit_sz = 1;
                break;
        case SNDFILE_ENCODING_L16:
                unit_sz = 2;
                break;
        default:
                unit_sz = 0;
                abort();
        }

        samples_read = fread(buf, unit_sz, samples, pf);

        switch(rs->fmt.encoding) {
        case SNDFILE_ENCODING_PCMA:
                law = ((u_char*)buf) + samples_read - 1;
                bp  = buf + samples_read - 1;
                for(i = 0; i < samples_read; i++) {
                        *bp-- = a2s(*law--);

                }
                break;
        case SNDFILE_ENCODING_PCMU:
                law = ((u_char*)buf) + samples_read - 1;
                bp  = buf + samples_read - 1;
                for(i = 0; i < samples_read; i++) {
                        *bp = u2s(*law);
                        assert(*law = s2u(*law));
                        bp--; law--;
                }
                break;
        case SNDFILE_ENCODING_L8:
                law = ((u_char*)buf) + samples_read - 1;
                bp  = buf + samples_read - 1;
                for(i = 0; i < samples_read; i++) {
                        *bp = (sample)(*law)*256;
                        bp--; law--;
                }
                break;
        case SNDFILE_ENCODING_L16:
                for(i = 0; i < samples_read; i++) {
                        buf[i] = htons(buf[i]);
                }
                break;
        }
        return samples_read;
}

int
raw_write_hdr(FILE *fp, char **state, const sndfile_fmt_t *fmt)
{
        raw_state_t *rs;

        rs = (raw_state_t*)xmalloc(sizeof(raw_state_t));
        if (!rs) {
                debug_msg("failed to allocate sun audio file header\n");
                return FALSE;
        }

        memcpy(&rs->fmt, fmt, sizeof(sndfile_fmt_t));
        *state = (char*)rs;

        /* Nothing to write to file */
        UNUSED(fp);

        return TRUE;
}

int
raw_write_audio(FILE *fp, char *state, sample *buf, int samples)
{
        int i, bytes_per_sample = 1;
        raw_state_t *rs;
        u_char *outbuf = NULL;

        rs = (raw_state_t*)state;

        switch(rs->fmt.encoding) {
        case SNDFILE_ENCODING_L16:
                bytes_per_sample = (int)sizeof(sample);
                if (ntohs(1) != 1) {
                        sample *l16buf;
                        l16buf = (sample*)block_alloc(sizeof(sample)*samples);
                        /* If we are on a little endian machine fix samples before
                         * writing them out.
                         */
                        for(i = 0; i < samples; i++) {
                                l16buf[i] = ntohs(buf[i]);
                        }
                        outbuf = (u_char*)l16buf;
                } else {
                        outbuf = (u_char*)buf;
                }
                break;
        case SNDFILE_ENCODING_L8:
                outbuf = (u_char*)block_alloc(samples);
                bytes_per_sample = 1;
                for(i = 0; i < samples; i++) {
                        outbuf[i] = (u_char)(buf[i]/256);
                }
                break;
        case SNDFILE_ENCODING_PCMA:
                outbuf = (u_char*)block_alloc(samples);
                bytes_per_sample = 1;
                for(i = 0; i < samples; i++) {
                        outbuf[i] = s2a(buf[i]);
                }
                break;
        case SNDFILE_ENCODING_PCMU:
                outbuf = (u_char*)block_alloc(samples);
                bytes_per_sample = 1;
                for(i = 0; i < samples; i++) {
                        outbuf[i] = s2u(buf[i]);
                }
                break;
        }

        fwrite(outbuf, bytes_per_sample, samples, fp);

        /* outbuf only equals buf if no sample type conversion was done */
        if (outbuf != (u_char*)buf) {
                block_free(outbuf, bytes_per_sample * samples);
        }

        return TRUE;
}

int
raw_free_state(char **state)
{
        if (state && *state) {
                xfree(*state);
                *state = NULL;
        }

        return TRUE;
}

int
raw_get_format(char *state, sndfile_fmt_t *fmt)
{
        raw_state_t *rs = (raw_state_t*)state;

        if (fmt == NULL || rs == NULL) {
                return FALSE;
        }

        memcpy(fmt, &rs->fmt, sizeof(sndfile_fmt_t));

        return TRUE;
}

