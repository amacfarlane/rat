/*
 * FILE:    sndfile.c
 * PROGRAM: RAT
 * AUTHOR:  Orion Hodson
 *
 * Copyright (c) 1998-2001 University College London
 * All rights reserved.
 */

#ifndef HIDE_SOURCE_STRINGS
static const char cvsid[] =
	"$Id: sndfile.c 3642 2004-01-12 17:14:56Z ucacoxh $";
#endif /* HIDE_SOURCE_STRINGS */

#include "config_unix.h"
#include "config_win32.h"
#include "debug.h"
#include "memory.h"
#include "audio_types.h"
#include "codec_types.h"
#include "codec_g711.h"
#include "sndfile_types.h"
#include "sndfile_au.h"
#include "sndfile_raw.h"
#include "sndfile_wav.h"
#include "sndfile.h"

/* This code uses the same function pointer arrangement as other files in RAT.
 * It is basically just a cumbersome way of implementing virtual functions
 */

/* Generic file handling *********************************************************/

typedef int (*pf_open_hdr)    (FILE *, char **state, sndfile_fmt_t *fmt);
typedef int (*pf_read_audio)  (FILE *, char * state, sample *buf, int samples);
typedef int (*pf_write_hdr)   (FILE *, char **state, const sndfile_fmt_t *);
typedef int (*pf_write_audio) (FILE *, char * state, sample *buf, int samples);
typedef int (*pf_write_end)   (FILE *, char *state);
typedef int (*pf_free_state)  (char **state);
typedef int (*pf_get_format)  (char *state, sndfile_fmt_t *fmt);

typedef struct s_file_handler {
        char name[12];           /* Sound handler name        */
        char extension[12];      /* Recognized file extension */
        pf_open_hdr    open_hdr;
        pf_read_audio  read_audio;
        pf_write_hdr   write_hdr;
        pf_write_audio write_audio;
        pf_write_end   write_end;
        pf_free_state  free_state;
        pf_get_format  get_format;
} sndfile_handler_t;

/* Sound file handlers */

static sndfile_handler_t snd_handlers[] = {
        {"NeXT/Sun",
         "au",
         sun_read_hdr,
         sun_read_audio,
         sun_write_hdr,
         sun_write_audio,
         NULL, /* No post write handling required */
         sun_free_state,
         sun_get_format
        },
        {"MS RIFF",
         "wav",
         riff_read_hdr,
         riff_read_audio,
         riff_write_hdr,
         riff_write_audio,
         riff_write_end,
         riff_free_state,
         riff_get_format
        },
        /* RAW should go last because it'll always succeed if we open a file
         * using this handler and have specified format.
         */
        {"Raw",
         "raw",
         raw_read_hdr,
         raw_read_audio,
         raw_write_hdr,
         raw_write_audio,
         NULL, /* No post write handling required */
         raw_free_state,
         raw_get_format
        }
};

#define NUM_SND_HANDLERS (int)(sizeof(snd_handlers)/sizeof(snd_handlers[0]))

#define SND_ACTION_PLAYING    1
#define SND_ACTION_RECORDING  2
#define SND_ACTION_PAUSED     4

typedef struct s_sndfile {
        FILE *fp;
        char *state;
        sndfile_handler_t *sfh;
        uint32_t action; /* Playing, recording, paused */
} sndfile_t;

int
snd_read_open (sndfile_t **sndfile, char *path, sndfile_fmt_t *fmt)
{
        sndfile_t *s;
        FILE       *fp;
        int         i;

        if (*sndfile) {
                debug_msg("File not closed before opening\n");
                snd_read_close(sndfile);
        }
#ifdef WIN32
        fp = fopen(path, "rb");
#else
        fp = fopen(path, "r");
#endif
        if (!fp) {
                debug_msg("Could not open %s\n",path);
                return FALSE;
        }

        s     = (sndfile_t*)xmalloc(sizeof(sndfile_t));
        if (!s) return FALSE;

        s->fp = fp;

        for(i = 0; i < NUM_SND_HANDLERS; i++) {
                if (snd_handlers[i].open_hdr(fp,&s->state, fmt)) {
                        s->sfh    = snd_handlers + i;
                        s->action = SND_ACTION_PLAYING;
                        *sndfile = s;
                        return TRUE;
                }
                rewind(fp);
        }

        xfree(s);

        return FALSE;
}

int
snd_read_close(sndfile_t **sf)
{
        sndfile_handler_t *sfh = (*sf)->sfh;

        /* Release state */
        sfh->free_state(&(*sf)->state);

        /* Close file */
        fclose((*sf)->fp);

        /* Release memory */
        xfree(*sf);
        *sf = NULL;

        return TRUE;
}

int
snd_read_audio(sndfile_t **sf, sample *buf, uint16_t samples)
{
        sndfile_handler_t *sfh;
        int samples_read;

        if ((*sf)->action & SND_ACTION_PAUSED) {
		return 0;
	}

        sfh = (*sf)->sfh;

        samples_read = sfh->read_audio((*sf)->fp, (*sf)->state, buf, samples);

        if (samples_read != samples) {
                /* Looks like the end of the line */
                memset(buf+samples_read, 0, sizeof(sample) * (samples - samples_read));
                snd_read_close(sf);
        }

        return samples_read;
}

static char *
snd_get_extension(char *path)
{
        if (path) {
                char *ext = path + strlen(path) - 1;
                while(ext > path) {
                        if (*ext == '.') return ext + 1;
                        ext--;
                }
        }
        return NULL;
}

int
snd_write_open (sndfile_t **sf, char *path, char *default_extension, const sndfile_fmt_t *fmt)
{
        sndfile_t *s;
        FILE       *fp;
        int         i;
        char *extension;

        if (*sf) {
                debug_msg("File not closed before opening\n");
                snd_write_close(sf);
        }

        if (fmt == NULL) {
                debug_msg("No format specified\n");
                snd_write_close(sf);
        }

        extension = snd_get_extension(path);
        if (extension == NULL) {
                debug_msg("No extension in file name (%s),using default %s\n", path, default_extension);
                extension = default_extension;
        }

#ifdef WIN32
        fp = fopen(path, "wb");
#else
        fp = fopen(path, "w");
#endif
        if (!fp) {
                debug_msg("Could not open %s\n",path);
                return FALSE;
        }

        s     = (sndfile_t*)xmalloc(sizeof(sndfile_t));
        if (!s) return FALSE;

        s->fp = fp;

        for(i = 0; i < NUM_SND_HANDLERS; i++) {
                if (!strcasecmp(extension, snd_handlers[i].extension)) {
                        s->sfh    = snd_handlers + i;
                        s->action = SND_ACTION_RECORDING;
                        s->sfh->write_hdr(fp, &s->state, fmt);
                        *sf = s;
                        return TRUE;
                }
        }

        xfree(s);

        return FALSE;
}

int
snd_write_close(sndfile_t **pps)
{
        sndfile_t *ps = *pps;

        if (ps->sfh->write_end) ps->sfh->write_end(ps->fp, ps->state);

        ps->sfh->free_state(&ps->state);

        fclose(ps->fp);

        xfree(ps);
        *pps = NULL;

        return TRUE;
}

int
snd_write_audio(sndfile_t **pps, sample *buf, uint16_t buf_len)
{
        sndfile_t *ps = *pps;
        int success;

        if (ps->action & SND_ACTION_PAUSED) return FALSE;

        if (buf_len == 0) {
                /* We succeeded although we didn't write anything */
                return TRUE;
        }

        success = ps->sfh->write_audio(ps->fp, ps->state, buf, buf_len);
        if (!success) {
                debug_msg("Closing file\n");
                snd_write_close(pps);
                return FALSE;
        }

        return TRUE;
}

int
snd_get_format(sndfile_t *sf, sndfile_fmt_t *fmt)
{
        return sf->sfh->get_format(sf->state, fmt);
}

int
snd_valid_format(sndfile_fmt_t *fmt)
{
        if (fmt->channels != 1 &&
            fmt->channels != 2) {
                debug_msg("Invalid channels %d\n", fmt->channels);
                return FALSE;
        }

        if ((fmt->sample_rate % 8000)  != 0 &&
            (fmt->sample_rate % 11025) != 0) {
                debug_msg("Invalid rate %d\n", fmt->sample_rate);
                return FALSE;
        }

        switch (fmt->encoding) {
        case SNDFILE_ENCODING_PCMU:
        case SNDFILE_ENCODING_PCMA:
        case SNDFILE_ENCODING_L8:
        case SNDFILE_ENCODING_L16:
                break;
        default:
                debug_msg("Invalid encoding %d\n", fmt->encoding);
                return FALSE;
        }

        return TRUE;
}

int
snd_pause(sndfile_t  *sf)
{
        sf->action = sf->action | SND_ACTION_PAUSED;
        return TRUE;
}

int
snd_resume(sndfile_t *sf)
{
        sf->action = sf->action & ~SND_ACTION_PAUSED;
        return TRUE;
}

