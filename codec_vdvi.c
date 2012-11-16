/*
 * FILE:    codec_vdvi.c
 * AUTHORS: Orion Hodson
 *
 * Copyright (c) 1998-2001 University College London
 * All rights reserved.
 */

#ifndef HIDE_SOURCE_STRINGS
static const char cvsid[] =
	"$Id: codec_vdvi.c 3642 2004-01-12 17:14:56Z ucacoxh $";
#endif /* HIDE_SOURCE_STRINGS */

#include "config_unix.h"
#include "config_win32.h"
#include "memory.h"
#include "util.h"
#include "debug.h"
#include "audio_types.h"
#include "codec_types.h"
#include "codec_vdvi.h"
#include "cx_dvi.h"
#include "bitstream.h"
#include "cx_vdvi.h"


#define CODEC_PAYLOAD_NO(x) x

static codec_format_t cs[] = {
        {"VDVI", "VDVI-8K-Mono",
         "Variable Rate IMA ADPCM codec.",
         CODEC_PAYLOAD_NO(77), 4, 80,
         {DEV_S16,  8000, 16, 1, 160 * BYTES_PER_SAMPLE}}, /* 20  ms */
        {"VDVI", "VDVI-16K-Mono",
         "Variable Rate IMA ADPCM codec.",
         CODEC_PAYLOAD_NO(78), 4, 80,
         {DEV_S16, 16000, 16, 1, 160 * BYTES_PER_SAMPLE}}, /* 10  ms */
        {"VDVI", "VDVI-32K-Mono",
         "Variable Rate IMA ADPCM codec.",
         CODEC_PAYLOAD_NO(79), 4, 80,
         {DEV_S16, 32000, 16, 1, 160 * BYTES_PER_SAMPLE}}, /* 5   ms */
        {"VDVI", "VDVI-48K-Mono",
         "Variable Rate IMA ADPCM codec.",
         CODEC_PAYLOAD_NO(80), 4, 80,
         {DEV_S16, 48000, 16, 1, 160 * BYTES_PER_SAMPLE}}  /* 3.3 ms */
};

#define VDVI_NUM_FORMATS sizeof(cs)/sizeof(codec_format_t)

uint16_t
vdvi_get_formats_count()
{
        return (uint16_t)VDVI_NUM_FORMATS;
}

const codec_format_t *
vdvi_get_format(uint16_t idx)
{
        assert(idx < VDVI_NUM_FORMATS);
        return &cs[idx];
}

typedef struct {
        struct adpcm_state *as;
        bitstream_t        *bs;
} vdvi_state_t;

int
vdvi_state_create(uint16_t idx, u_char **s)
{
        vdvi_state_t *v;

        if (idx < VDVI_NUM_FORMATS) {
                v = (vdvi_state_t*)xmalloc(sizeof(vdvi_state_t));
                if (v == NULL) {
                        return FALSE;
                }
                v->as = (struct adpcm_state*)xmalloc(sizeof(struct adpcm_state));
                if (v->as == NULL) {
                        xfree(v);
                        return FALSE;
                }
                memset(v->as, 0, sizeof(struct adpcm_state));
                if (bs_create(&v->bs) == FALSE) {
                        xfree(v->as);
                        xfree(v);
                        return FALSE;
                }
                *s = (unsigned char*)v;
                return TRUE;
        }
        return 0;
}

void
vdvi_state_destroy(uint16_t idx, u_char **s)
{
        vdvi_state_t *v;

        v = (vdvi_state_t*)*s;
        xfree(v->as);
        bs_destroy(&v->bs);
        xfree(v);
        *s = (u_char*)NULL;
        UNUSED(idx);
}

/* Buffer of maximum length of vdvi coded data - never know how big
 * it needs to be
 */

int
vdvi_encoder(uint16_t idx, u_char *encoder_state, sample *inbuf, coded_unit *c)
{
        int samples, len;

        u_char dvi_buf[80];
        u_char vdvi_buf[160];
        vdvi_state_t *v;

        assert(encoder_state);
        assert(inbuf);
        assert(idx < VDVI_NUM_FORMATS);
        UNUSED(idx);

        v = (vdvi_state_t*)encoder_state;

        /* Transfer state and fix ordering */
        c->state     = (u_char*)block_alloc(sizeof(struct adpcm_state));
        c->state_len = sizeof(struct adpcm_state);
        memcpy(c->state, v->as, sizeof(struct adpcm_state));

        /* Fix coded state for byte ordering */
	((struct adpcm_state*)c->state)->valprev = htons(((struct adpcm_state*)c->state)->valprev);

        samples = cs[idx].format.bytes_per_block * 8 / cs[idx].format.bits_per_sample;

        assert(samples == 160);

        adpcm_coder(inbuf, dvi_buf, samples, v->as);

        bs_attach(v->bs, vdvi_buf, sizeof(vdvi_buf)/sizeof(vdvi_buf[0]));
        memset(vdvi_buf, 0, sizeof(vdvi_buf)/sizeof(vdvi_buf[0]));
        len = vdvi_encode(dvi_buf, 160, v->bs);
        c->data     = (u_char*)block_alloc(len);
        c->data_len = len;
        memcpy(c->data, vdvi_buf, len);

        return len;
}

int
vdvi_decoder(uint16_t idx, u_char *decoder_state, coded_unit *c, sample *data)
{
        int samples, len;
        u_char dvi_buf[80];
        vdvi_state_t *v;

        assert(decoder_state);
        assert(c);
        assert(data);
        assert(idx < VDVI_NUM_FORMATS);

        v = (vdvi_state_t*)decoder_state;

	if (c->state_len > 0) {
		assert(c->state_len == sizeof(struct adpcm_state));
		memcpy(v->as, c->state, sizeof(struct adpcm_state));
		v->as->valprev = ntohs(v->as->valprev);
	}

        bs_attach(v->bs, c->data, c->data_len);
        len = vdvi_decode(v->bs, dvi_buf, 160);

        samples = cs[idx].format.bytes_per_block / sizeof(sample);
	adpcm_decoder(dvi_buf, data, samples, v->as);

        return samples;
}

int
vdvi_peek_frame_size(uint16_t idx, u_char *data, int data_len)
{
        bitstream_t *bs;
        u_char       dvi_buf[80];
        int          len;

        UNUSED(idx);

        bs_create(&bs);
        bs_attach(bs, data, data_len);
        len = vdvi_decode(bs, dvi_buf, 160);
        bs_destroy(&bs);
        assert(len <= data_len);
        return len;
}

