/*
 * FILE:    sndfile_au.c
 * PROGRAM: RAT
 * AUTHOR:  Orion Hodson
 *
 * Copyright (c) 1998-2001 University College London
 * All rights reserved.
 */

#ifndef HIDE_SOURCE_STRINGS
static const char cvsid[] =
	"$Id: sndfile_au.c 3642 2004-01-12 17:14:56Z ucacoxh $";
#endif /* HIDE_SOURCE_STRINGS */

#include "config_unix.h"
#include "config_win32.h"
#include "debug.h"
#include "memory.h"
#include "util.h"
#include "audio_types.h"
#include "codec_g711.h"
#include "sndfile_types.h"
#include "sndfile_au.h"

/* NeXT/Sun .au file handling ****************************************************
   Portions of the definitions come from the Sun's header file audio_filehdr.h ***/

typedef struct {
	uint32_t		magic;		/* magic number */
	uint32_t		hdr_size;	/* size of this header */
	uint32_t		data_size;	/* length of data (optional) */
	uint32_t		encoding;	/* data encoding format */
	uint32_t		sample_rate;	/* samples per second */
	uint32_t		channels;	/* number of interleaved channels */
} sun_audio_filehdr;

/* Define the magic number */
#define	SUN_AUDIO_FILE_MAGIC		((uint32_t)0x2e736e64)
#define SUN_AUDIO_UNKNOWN_SIZE          ((uint32_t)(~0))

/* Define the encoding fields */
/* We only support trivial conversions */
#define	SUN_AUDIO_FILE_ENCODING_ULAW	        (1u)	/* u-law PCM         */
#define SUN_AUDIO_FILE_ENCODING_LINEAR_8        (2u)    /* 8-bit linear PCM  */
#define	SUN_AUDIO_FILE_ENCODING_LINEAR_16	(3u)	/* 16-bit linear PCM */
#define SUN_AUDIO_FILE_ENCODING_ALAW	       (27u) 	/* A-law PCM         */

static void
sun_ntoh_hdr(sun_audio_filehdr *saf)
{
        uint32_t *pi;
        int j;
        pi = (uint32_t*)saf;
        for(j = 0; j < 6; j++) {
                pi[j] = htonl(pi[j]);
        }
}

int /* Returns true if can decode header */
sun_read_hdr(FILE *pf, char **state, sndfile_fmt_t *fmt)
{
        sun_audio_filehdr afh;

        /* Attempt to read header */
        if (!fread(&afh, sizeof(sun_audio_filehdr), 1, pf)) {
                /* file must be too small! */
                debug_msg("Could not read header\n");
                return FALSE;
        }

        /* Fix byte ordering */
        sun_ntoh_hdr(&afh);

        /* Test if supported */
        if ((afh.magic == SUN_AUDIO_FILE_MAGIC) &&
            (afh.encoding == SUN_AUDIO_FILE_ENCODING_ULAW ||
             afh.encoding == SUN_AUDIO_FILE_ENCODING_LINEAR_16 ||
             afh.encoding == SUN_AUDIO_FILE_ENCODING_LINEAR_8 ||
             afh.encoding == SUN_AUDIO_FILE_ENCODING_ALAW)) {
                /* File ok copy header */
                sun_audio_filehdr *pafh = (sun_audio_filehdr *)xmalloc(sizeof(sun_audio_filehdr));
                memcpy(pafh,&afh, sizeof(sun_audio_filehdr));

                /* For .au's state is just file header */
                *state = (char*)pafh;

                /* Roll past header */
                fseek(pf, afh.hdr_size, SEEK_SET);

                sun_get_format(*state, fmt);

                return TRUE;
        }
        return FALSE;
}

int /* Returns the number of samples read */
sun_read_audio(FILE *pf, char* state, sample *buf, int samples)
{
        sun_audio_filehdr *afh;
        int unit_sz, samples_read, i;
        u_char *law;
        sample *bp;

        afh = (sun_audio_filehdr*)state;

        switch(afh->encoding) {
        case SUN_AUDIO_FILE_ENCODING_ALAW:
        case SUN_AUDIO_FILE_ENCODING_ULAW:
        case SUN_AUDIO_FILE_ENCODING_LINEAR_8:
                unit_sz = 1;
                break;
        case SUN_AUDIO_FILE_ENCODING_LINEAR_16:
                unit_sz = 2;
                break;
        default:
                return 0;
        }

        samples_read = fread(buf, unit_sz, samples, pf);

        switch(afh->encoding) {
        case SUN_AUDIO_FILE_ENCODING_ALAW:
                law = ((u_char*)buf) + samples_read - 1;
                bp  = buf + samples_read - 1;
                for(i = 0; i < samples_read; i++) {
                        *bp-- = a2s(*law--);

                }
                break;
        case SUN_AUDIO_FILE_ENCODING_ULAW:
                law = ((u_char*)buf) + samples_read - 1;
                bp  = buf + samples_read - 1;
                for(i = 0; i < samples_read; i++) {
                        *bp-- = u2s(*law--);
                }
                break;
        case SUN_AUDIO_FILE_ENCODING_LINEAR_8:
                law = ((u_char*)buf) + samples_read - 1;
                bp  = buf + samples_read - 1;
                for(i = 0; i < samples_read; i++) {
                        *bp-- = (sample)(*law) * 256;
                        law--;
                }
                break;
        case SUN_AUDIO_FILE_ENCODING_LINEAR_16:
                for(i = 0; i < samples_read; i++) {
                        buf[i] = htons(buf[i]);
                }
                break;
        }
        return samples_read;
}

int
sun_write_hdr(FILE *fp, char **state, const sndfile_fmt_t *fmt)
{
        sun_audio_filehdr *saf;

        saf = (sun_audio_filehdr*)xmalloc(sizeof(sun_audio_filehdr));
        if (!saf) {
                debug_msg("failed to allocate sun audio file header\n");
                return FALSE;
        }

        *state = (char*)saf;
        saf->magic       = SUN_AUDIO_FILE_MAGIC;
        saf->hdr_size    = sizeof(sun_audio_filehdr);
        saf->data_size   = SUN_AUDIO_UNKNOWN_SIZE; /* Optional - we could fill this in when the file closes */
        saf->sample_rate = fmt->sample_rate;
        saf->channels    = fmt->channels;

        switch(fmt->encoding) {
        case SNDFILE_ENCODING_L16:
                saf->encoding = SUN_AUDIO_FILE_ENCODING_LINEAR_16;
                break;
        case SNDFILE_ENCODING_L8:
                saf->encoding = SUN_AUDIO_FILE_ENCODING_LINEAR_8;
                break;
        case SNDFILE_ENCODING_PCMA:
                saf->encoding = SUN_AUDIO_FILE_ENCODING_ALAW;
                break;
        case SNDFILE_ENCODING_PCMU:
                saf->encoding = SUN_AUDIO_FILE_ENCODING_ULAW;
                break;
        }

        sun_ntoh_hdr(saf);
        if (fwrite(saf, sizeof(sun_audio_filehdr), 1, fp)) {
                sun_ntoh_hdr(saf);
                return TRUE;
        } else {
                xfree(saf);
                *state = NULL;
                return FALSE;
        }
}

int
sun_write_audio(FILE *fp, char *state, sample *buf, int samples)
{
        int i, bytes_per_sample = 1;
        sun_audio_filehdr *saf;
        u_char *outbuf = NULL;

        saf = (sun_audio_filehdr*)state;

        switch(saf->encoding) {
        case SUN_AUDIO_FILE_ENCODING_LINEAR_16:
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
        case SUN_AUDIO_FILE_ENCODING_LINEAR_8:
                outbuf = (u_char*)block_alloc(samples);
                bytes_per_sample = 1;
                for(i = 0; i < samples; i++) {
                        outbuf[i] = (u_char)(buf[i]>>8);
                }
                break;
        case SUN_AUDIO_FILE_ENCODING_ALAW:
                outbuf = (u_char*)block_alloc(samples);
                bytes_per_sample = 1;
                for(i = 0; i < samples; i++) {
                        outbuf[i] = s2a(buf[i]);
                }
                break;
        case SUN_AUDIO_FILE_ENCODING_ULAW:
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
sun_free_state(char **state)
{
        if (state && *state) {
                xfree(*state);
                *state = NULL;
        }
        return TRUE;
}

int
sun_get_format(char *state, sndfile_fmt_t *fmt)
{
        sun_audio_filehdr *saf = (sun_audio_filehdr*)state;

        if (fmt == NULL || saf == NULL) {
                return FALSE;
        }

        switch (saf->encoding) {
        case SUN_AUDIO_FILE_ENCODING_LINEAR_16:
                fmt->encoding = SNDFILE_ENCODING_L16;
                break;
        case SUN_AUDIO_FILE_ENCODING_ULAW:
                fmt->encoding = SNDFILE_ENCODING_PCMU;
                break;
        case SUN_AUDIO_FILE_ENCODING_ALAW:
                fmt->encoding = SNDFILE_ENCODING_PCMA;
                break;
        }

        fmt->sample_rate = saf->sample_rate;
        fmt->channels    = saf->channels;

        return TRUE;
}

