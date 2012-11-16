/*
 * FILE:    sndfile_wav.c
 * PROGRAM: RAT
 * AUTHOR:  Orion Hodson
 *
 * Copyright (c) 1998-2001 University College London
 * All rights reserved.
 */

#ifndef HIDE_SOURCE_STRINGS
static const char cvsid[] =
	"$Id: sndfile_wav.c 3642 2004-01-12 17:14:56Z ucacoxh $";
#endif /* HIDE_SOURCE_STRINGS */

#include "config_unix.h"
#include "config_win32.h"
#include "debug.h"
#include "memory.h"
#include "util.h"
#include "audio_types.h"
#include "codec_g711.h"
#include "sndfile_types.h"
#include "sndfile_wav.h"

/* Microsoft WAV file handling (severly restricted subset)
 * Spec. was a text file called RIFF-format
 * Mirrored lots of places, try http://ftpsearch.ntnu.no/
 *
 * We use the same partial implementation on Windows and UNIX
 * to save code.  This implementation only passes first block
 * of data if it is waveform audio, strictly we should decode
 * all blocks we understand and just ignore those we don't.
 * It's not that hard to do properly - just want to get
 * something up and running for the time being.
 */

typedef struct {
        uint32_t ckId; /* First four characters of chunk type e.g RIFF, WAVE, LIST */
        uint32_t ckSize;
} riff_chunk;

typedef struct {
        riff_chunk rc;
        uint32_t    type;
} riff_chunk_hdr;

/* Note PCM_FORMAT_SIZE is 18 bytes as it has no extra information
 * cbExtra (below) is not included.  We only record 16-bit pcm
 */

#define PCM_FORMAT_SIZE 18

typedef struct {
        uint16_t wFormatTag;
        uint16_t wChannels;
        uint32_t dwSamplesPerSec;
        uint32_t dwAvgBytesPerSec;
        uint16_t wBlockAlign;
        uint16_t wBitsPerSample;
        uint16_t cbExtra;
} wave_format; /* Same as WAVEFORMATEX */

typedef struct {
        wave_format  wf;
        int          cbRemain; /* Number of bytes read    */
        int          cbUsed;   /* Number of bytes written */
} riff_state;

#define MS_AUDIO_FILE_ENCODING_PCM  (0x0001)
#define MS_AUDIO_FILE_ENCODING_ULAW (0x0007)
#define MS_AUDIO_FILE_ENCODING_ALAW (0x0006)
/* In the spec IBM u/alaw are 0x0102/0x0101 but this seems to be outdated. */

#define btoll(x) (((x) >> 24) | (((x)&0x00ff0000) >> 8) | (((x)&0x0000ff00) << 8) | ((x) << 24))
#define btols(x) (((x) >> 8) | ((x&0xff) << 8))

#ifndef WIN32
static uint32_t
MAKEFOURCC(char a, char b, char c, char d)
{
        uint32_t r;
        if (htons(1) == 1) {
		r = (((uint32_t)(char)(a) <<24 )| ((uint32_t)(char)(b) << 16) |
                     ((uint32_t)(char)(c) << 8) | (uint32_t)(char)(d));
        } else {
                r = (((uint32_t)(char)(d) <<24 )| ((uint32_t)(char)(c) << 16) |
                     ((uint32_t)(char)(b) << 8) | (uint32_t)(char)(a));
        }
        return r;
}
#endif /* WIN32 */

static void
wave_fix_hdr(wave_format *wf)
{
        /* If we are a big endian machine convert from little endian */
        if (htonl(1) == 1) {
                wf->wFormatTag       = (uint16_t)btols(wf->wFormatTag);
                wf->wChannels        = (uint16_t)btols(wf->wChannels);
                wf->dwSamplesPerSec  = btoll(wf->dwSamplesPerSec);
                wf->dwAvgBytesPerSec = btoll(wf->dwAvgBytesPerSec);
                wf->wBlockAlign      = (uint16_t)btols(wf->wBlockAlign);
                wf->wBitsPerSample   = (uint16_t)btols(wf->wBitsPerSample);
                wf->cbExtra          = (uint16_t)btols(wf->cbExtra);
        }
}

static void
riff_fix_chunk_hdr(riff_chunk *rc)
{
        if (htonl(1) == 1) {
                rc->ckSize = btoll(rc->ckSize);
        }
}

static uint32_t
riff_proceed_to_chunk(FILE *fp, char *id)
{
        riff_chunk rc;
        uint32_t ckId;

        ckId = MAKEFOURCC(id[0],id[1],id[2],id[3]);

        while(fread(&rc, sizeof(rc), 1, fp)) {
                riff_fix_chunk_hdr(&rc);
                if (rc.ckId == ckId) {
                        return rc.ckSize;
                }
                fseek(fp, rc.ckSize, SEEK_CUR);
        }

        return 0;
}

int
riff_read_hdr(FILE *fp, char **state, sndfile_fmt_t *fmt)
{
        riff_chunk_hdr rch;
        wave_format wf;
        uint32_t chunk_size;

        riff_state *rs;

        if (!fread(&rch, 1, sizeof(rch), fp)) {
                debug_msg("Could read RIFF header");
                return FALSE;
        }
        riff_fix_chunk_hdr(&rch.rc);
        debug_msg("Header chunk size (%d)\n", rch.rc.ckSize);

        if (MAKEFOURCC('R','I','F','F') != rch.rc.ckId ||
            MAKEFOURCC('W','A','V','E') != rch.type) {
                uint32_t riff = MAKEFOURCC('R','I','F','F');
                uint32_t wave = MAKEFOURCC('W','A','V','E');

                debug_msg("Riff 0x%08x 0x%08x\n", riff, rch.rc.ckId);
                debug_msg("Wave 0x%08x 0x%08x\n", wave, rch.type);
                debug_msg("Not WAVE file\n");
                return FALSE;
        }

        chunk_size = riff_proceed_to_chunk(fp, "fmt ");
        if (!chunk_size) {
                debug_msg("Format chunk not found\n");
                return FALSE;
        }
        debug_msg("Fmt chunk size (%d)\n", chunk_size);

        memset(&wf,0,sizeof(wf));
        if (chunk_size > sizeof(wave_format)||
            !fread(&wf,  1, chunk_size, fp)) {
                /* the formats we are interested in carry no extra information */
                debug_msg("Wave format too big (%d).\n", chunk_size);
                return FALSE;
        }

        wave_fix_hdr(&wf);

        switch(wf.wFormatTag) {
        case MS_AUDIO_FILE_ENCODING_ULAW:
                debug_msg(".wav file encoding is ulaw\n");
                break;
        case MS_AUDIO_FILE_ENCODING_ALAW:
                debug_msg(".wav file encoding is alaw\n");
                break;
        case MS_AUDIO_FILE_ENCODING_PCM:
                if (wf.wBitsPerSample != 16 && wf.wBitsPerSample != 8) {
                        debug_msg("%d bits per sample not supported.\n", wf.wBitsPerSample);
                        return FALSE;
                }
                debug_msg(".wav file encoding is L%d\n", wf.wBitsPerSample);
                break;
        default:
                /* We could be really flash and open an acm stream and convert any
                 * windows audio file data, but that would be too much ;-)
                 */
                debug_msg("Format (%4x) not supported.\n", wf.wFormatTag);
                return FALSE;
        }

        debug_msg("Channels (%d) SamplesPerSec (%d) BPS (%d) Align (%d) bits_per_samples (%d)\n",
                  wf.wChannels,
                  wf.dwSamplesPerSec,
                  wf.dwAvgBytesPerSec,
                  wf.wBlockAlign,
                  wf.wBitsPerSample);

        chunk_size = riff_proceed_to_chunk(fp, "data");
        if (!chunk_size) {
                debug_msg("No data ?\n");
                return FALSE;
        }

        rs = (riff_state*)xmalloc(sizeof(riff_state));
        if (rs) {
                rs->cbRemain = chunk_size;
		rs->cbUsed   = 0;
                memcpy(&rs->wf, &wf, sizeof(wf));
                *state = (char*)rs;
                riff_get_format(*state, fmt);
                return TRUE;
        }

        return FALSE;
}

int /* Returns the number of samples read */
riff_read_audio(FILE *pf, char* state, sample *buf, int samples)
{
        riff_state *rs;
        int unit_sz, samples_read, i;
        u_char *law;
        sample *bp;

        rs = (riff_state*)state;

        switch(rs->wf.wFormatTag) {
        case MS_AUDIO_FILE_ENCODING_ALAW:
        case MS_AUDIO_FILE_ENCODING_ULAW:
                unit_sz = 1;
                break;
        case MS_AUDIO_FILE_ENCODING_PCM:
                unit_sz = rs->wf.wBitsPerSample / 8;
                break;
        default:
                return 0;
        }

        if (rs->cbRemain <= 0) {
		debug_msg("Nothing remaining...\n");
		return 0;
	}

        samples_read = fread(buf, unit_sz, samples, pf);
	if (samples_read <= 0) {
		debug_msg("Read ended...\n");
		return 0;
	}
        rs->cbRemain -= (samples_read * unit_sz);

        switch(rs->wf.wFormatTag) {
        case MS_AUDIO_FILE_ENCODING_ALAW:
                law = ((u_char*)buf) + samples_read - 1;
                bp  = buf + samples_read - 1;
                for(i = 0; i < samples_read; i++) {
                        *bp-- = a2s(*law--);
                }
                break;
        case MS_AUDIO_FILE_ENCODING_ULAW:
                law = ((u_char*)buf) + samples_read - 1;
                bp  = buf + samples_read - 1;
                for(i = 0; i < samples_read; i++) {
                        *bp-- = u2s(*law--);
                }
                break;
        case MS_AUDIO_FILE_ENCODING_PCM:
                if (rs->wf.wBitsPerSample == 16) {
                        if (htons(1) == 1) {
                                for(i = 0; i < samples_read; i++) {
                                        buf[i] = (uint16_t)btols((u_char)buf[i]);
                                }
                        }
                } else if (rs->wf.wBitsPerSample == 8) {
			/* 8-bit range is unsigned 0-255, not -127 to 128 */
                        law = ((u_char*)buf) + samples_read - 1;
                        bp  = buf + samples_read - 1;
                        for(i = 0; i < samples_read; i++) {
                                *bp-- = ((sample)(*law) * 256) - 32767;
                                law--;
                        }
                        break;
                } else {
                        debug_msg("Not supported PCM %d bits per sample\n", rs->wf.wBitsPerSample);
                }
                break;
        }

	return samples_read;
}

int
riff_write_hdr(FILE *fp, char **state, const sndfile_fmt_t *fmt)
{
        riff_state *rs;
        int         hdr_len;

        rs = (riff_state*)xmalloc(sizeof(riff_state));
        if (!rs) {
                return FALSE;
        }
        *state = (char*)rs;

        switch(fmt->encoding) {
        case SNDFILE_ENCODING_L16:
                rs->wf.wFormatTag       = MS_AUDIO_FILE_ENCODING_PCM;
                rs->wf.wBitsPerSample   = 16;
                break;
        case SNDFILE_ENCODING_L8:
                rs->wf.wFormatTag       = MS_AUDIO_FILE_ENCODING_PCM;
                rs->wf.wBitsPerSample   = 8;
                break;
        case SNDFILE_ENCODING_PCMU:
                rs->wf.wFormatTag       = MS_AUDIO_FILE_ENCODING_ULAW;
                rs->wf.wBitsPerSample   = 8;
                break;
        case SNDFILE_ENCODING_PCMA:
                rs->wf.wFormatTag       = MS_AUDIO_FILE_ENCODING_ALAW;
                rs->wf.wBitsPerSample   = 8;
                break;
        }

        rs->cbUsed = 0;
        rs->wf.wChannels        = (uint16_t)fmt->channels;
        rs->wf.dwSamplesPerSec  = fmt->sample_rate;
        rs->wf.dwAvgBytesPerSec = fmt->sample_rate * fmt->channels * rs->wf.wBitsPerSample / 8;
        rs->wf.wBlockAlign      = (uint16_t)(fmt->channels * rs->wf.wBitsPerSample / 8);

        hdr_len = sizeof(riff_chunk_hdr) /* RIFF header */
                + 2 * sizeof(riff_chunk) /* Sub-block ("data") */
                + PCM_FORMAT_SIZE;   /* Wave format description (PCM ONLY) */

        /* Roll forward leaving space for header. We don't write it here because
         * we need to know the amount of audio written before we can write header.
         */
        if (fseek(fp, hdr_len, SEEK_SET)) {
                debug_msg("Seek Failed.\n");
                return FALSE;
        }

        return TRUE;
}

int
riff_write_audio(FILE *fp, char *state, sample *buf, int samples)
{
        int i, bytes_per_sample = 1;
        riff_state *rs = (riff_state*)state;
        u_char *outbuf = NULL;

        switch(rs->wf.wFormatTag) {
        case MS_AUDIO_FILE_ENCODING_PCM:
                bytes_per_sample = sizeof(sample);
                if (rs->wf.wBitsPerSample == 16) {
                        if (ntohs(1) == 1) {
                                sample *l16buf;
                                l16buf = (sample*)block_alloc(samples * sizeof(sample));

                                /* If we are on a big endian machine fix samples before
                                 * writing them out.
                                 */
                                for(i = 0; i < samples; i++) {
                                        l16buf[i] = (uint16_t)btols((uint16_t)buf[i]);
                                }
                                outbuf = (u_char*)l16buf;
                        } else {
                                outbuf  = (u_char*)buf;
                        }
                } else if (rs->wf.wBitsPerSample == 8) {
                        outbuf = (u_char*)block_alloc(samples);
                        bytes_per_sample = 1;
                        for(i = 0; i < samples; i++) {
                                outbuf[i] = (u_char)((buf[i]+32767) >> 8);
                        }
                        break;
                }
                break;
        case MS_AUDIO_FILE_ENCODING_ALAW:
                outbuf = (u_char*)block_alloc(samples);
                bytes_per_sample = 1;
                for(i = 0; i < samples; i++) {
                        outbuf[i] = s2a(buf[i]);
                }
                break;
        case MS_AUDIO_FILE_ENCODING_ULAW:
                outbuf = (u_char*)block_alloc(samples);
                bytes_per_sample = 1;
                for(i = 0; i < samples; i++) {
                        outbuf[i] = s2u(buf[i]);
                }
                break;
        }

        fwrite(outbuf, bytes_per_sample, samples, fp);
        rs->cbUsed += bytes_per_sample * samples;

        /* outbuf only equals buf if no sample conversion done */
        if (outbuf != (u_char*)buf) {
                block_free(outbuf, bytes_per_sample * samples);
        }

        return TRUE;
}

int
riff_write_end(FILE *fp, char *state)
{
        riff_chunk_hdr rch;
        riff_chunk fmt, data;
        riff_state *rs = (riff_state*)state;

        /* Back to the beginning */
        if (fseek(fp, 0, SEEK_SET)) {
                debug_msg("Rewind failed\n");
                return FALSE;
        }

        rch.rc.ckId   = MAKEFOURCC('R','I','F','F');
        /* Size includes FOURCC(WAVE) and size of all sub-components. */
        rch.rc.ckSize = 4 + sizeof(fmt) + sizeof(data) + PCM_FORMAT_SIZE + rs->cbUsed;
        rch.type      = MAKEFOURCC('W','A','V','E');
        riff_fix_chunk_hdr(&rch.rc);
        fwrite(&rch, sizeof(rch), 1, fp);

        /* Write format header */
        fmt.ckId   = MAKEFOURCC('f','m','t',' ');
        fmt.ckSize = PCM_FORMAT_SIZE;
        riff_fix_chunk_hdr(&fmt);
        fwrite(&fmt, sizeof(fmt), 1, fp);

        /* Write wave format - cannot use wave fix hdr as constituents
         * must be written out in correct order */
        if (htonl(1) == 1) {
                wave_fix_hdr(&rs->wf);
                fwrite(&rs->wf.wFormatTag,       sizeof(uint16_t), 1, fp);
                fwrite(&rs->wf.wChannels,        sizeof(uint16_t), 1, fp);
                fwrite(&rs->wf.dwSamplesPerSec,  sizeof(uint32_t), 1, fp);
                fwrite(&rs->wf.dwAvgBytesPerSec, sizeof(uint32_t), 1, fp);
                fwrite(&rs->wf.wBlockAlign,      sizeof(uint16_t), 1, fp);
                fwrite(&rs->wf.wBitsPerSample,   sizeof(uint16_t), 1, fp);
                fwrite(&rs->wf.cbExtra,          sizeof(uint16_t), 1, fp);
        } else {
                fwrite(&rs->wf, PCM_FORMAT_SIZE, 1, fp);
        }

        /* Write data header */
        data.ckId   = MAKEFOURCC('d','a','t','a');
        data.ckSize = rs->cbUsed;
        riff_fix_chunk_hdr(&data);
        fwrite(&data, sizeof(data), 1, fp);

        return TRUE;
}

int
riff_free_state(char **state)
{
        riff_state *rs;

        if (state && *state) {
                rs = (riff_state*)*state;
                debug_msg("Used (%d) Remain (%d)\n", rs->cbUsed, rs->cbRemain);
                xfree(rs);
                *state = NULL;
        }
        return TRUE;
}

int
riff_get_format(char *state, sndfile_fmt_t *fmt)
{
        wave_format *wf = (wave_format*)state;

        if (wf == NULL || fmt == NULL) {
                return FALSE;
        }

        switch(wf->wFormatTag) {
        case MS_AUDIO_FILE_ENCODING_PCM:
                if (wf->wBitsPerSample == 16) {
                        fmt->encoding = SNDFILE_ENCODING_L16;
                } else if (wf->wBitsPerSample == 8) {
                        fmt->encoding = SNDFILE_ENCODING_L8;
                }
                break;
        case MS_AUDIO_FILE_ENCODING_ULAW:
                fmt->encoding = SNDFILE_ENCODING_PCMU;
                break;
        case MS_AUDIO_FILE_ENCODING_ALAW:
                fmt->encoding = SNDFILE_ENCODING_PCMA;
                break;
        }

        fmt->sample_rate = (uint32_t)wf->dwSamplesPerSec;
        fmt->channels    = wf->wChannels;

        return TRUE;
}
