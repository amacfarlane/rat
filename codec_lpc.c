/*
 * FILE:    codec_lpc.c
 * AUTHORS: Orion Hodson
 *
 * Copyright (c) 1998-2001 University College London
 * All rights reserved.
 */

#ifndef HIDE_SOURCE_STRINGS
static const char cvsid[] =
	"$Id: codec_lpc.c 3642 2004-01-12 17:14:56Z ucacoxh $";
#endif /* HIDE_SOURCE_STRINGS */

#include "config_unix.h"
#include "config_win32.h"
#include "memory.h"
#include "util.h"
#include "debug.h"
#include "audio_types.h"
#include "codec_types.h"
#include "codec_lpc.h"
#include "cx_lpc.h"

static codec_format_t cs[] = {
        {"LPC", "LPC-8K-Mono",
         "Pitch excited linear prediction codec (C) R. Zuckerman. Contributed by R. Frederick.",
         7, 0, LPCTXSIZE,
         {DEV_S16, 8000, 16, 1, 160 * BYTES_PER_SAMPLE}}
};

#define LPC_NUM_FORMATS (sizeof(cs)/sizeof(codec_format_t))

uint16_t
lpc_get_formats_count()
{
        return LPC_NUM_FORMATS;
}

const codec_format_t*
lpc_get_format(uint16_t idx)
{
        assert(idx < LPC_NUM_FORMATS);
        return &cs[idx];
}

void
lpc_setup(void)
{
        lpc_init();
}

int
lpc_encoder_state_create(uint16_t idx, u_char **state)
{
        assert(idx < LPC_NUM_FORMATS);
        UNUSED(idx);
        *state = (u_char*) xmalloc(sizeof(lpc_encstate_t));
        lpc_enc_init((lpc_encstate_t*) *state);
        return sizeof(lpc_encstate_t);
}

void
lpc_encoder_state_destroy(uint16_t idx, u_char **state)
{
        assert(idx < LPC_NUM_FORMATS);
        UNUSED(idx);

        xfree(*state);
        *state = (u_char*)NULL;
}

int
lpc_decoder_state_create(uint16_t idx, u_char **state)
{
        assert(idx < LPC_NUM_FORMATS);
        UNUSED(idx);
        *state = (u_char*) xmalloc(sizeof(lpc_intstate_t));
        lpc_dec_init((lpc_intstate_t*) *state);
        return sizeof(lpc_intstate_t);
}

void
lpc_decoder_state_destroy(uint16_t idx, u_char **state)
{
        assert(idx < LPC_NUM_FORMATS);
        UNUSED(idx);

        xfree(*state);
        *state = (u_char*)NULL;
}

int
lpc_encoder  (uint16_t idx, u_char *state, sample *in, coded_unit *out)
{
        assert(idx < LPC_NUM_FORMATS);
        assert(in);
        assert(out);
        UNUSED(idx);
        UNUSED(state);

        out->state     = NULL;
        out->state_len = 0;
        out->data      = (u_char*)block_alloc(LPCTXSIZE);
        out->data_len  = LPCTXSIZE;

        lpc_analyze((const short*)in,
                    (lpc_encstate_t*)state,
                    (lpc_txstate_t*)out->data);
        return out->data_len;
}

int
lpc_decoder (uint16_t idx, u_char *state, coded_unit *in, sample *out)
{
        assert(idx < LPC_NUM_FORMATS);
        assert(state);
        assert(in && in->data);
        assert(out);

        UNUSED(idx);
        lpc_synthesize((short*)out,
                       (lpc_txstate_t*)in->data,
                       (lpc_intstate_t*)state);
        return cs[idx].format.bytes_per_block / BYTES_PER_SAMPLE;
}

int
lpc_repair (uint16_t idx, u_char *state, uint16_t consec_lost,
            coded_unit *prev, coded_unit *missing, coded_unit *next)
{
        lpc_txstate_t *lps;

        assert(prev);
        assert(missing);

        if (missing->data) {
                debug_msg("lpc_repair: missing unit had data!\n");
                block_free(missing->data, missing->data_len);
        }

        missing->data     = (u_char*)block_alloc(LPCTXSIZE);
        missing->data_len = LPCTXSIZE;

        assert(prev->data);
        assert(prev->data_len == LPCTXSIZE);
        memcpy(missing->data, prev->data, LPCTXSIZE);

        lps = (lpc_txstate_t*)missing->data;
        lps->gain = (u_char)((float)lps->gain * 0.8f);

        UNUSED(next);
        UNUSED(consec_lost);
        UNUSED(state);
        UNUSED(idx);

        return TRUE;
}
