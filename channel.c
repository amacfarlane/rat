/*
 * FILE:      channel.c
 * AUTHOR(S): Orion Hodson
 *
 *
 * Copyright (c) 1999-2001 University College London
 * All rights reserved.
 */

#ifndef HIDE_SOURCE_STRINGS
static const char cvsid[] =
	"$Id: channel.c 3642 2004-01-12 17:14:56Z ucacoxh $";
#endif /* HIDE_SOURCE_STRINGS */
#include "config_unix.h"
#include "config_win32.h"
#include "audio_types.h"
#include "codec_types.h"
#include "channel.h"
#include "playout.h"
#include "ts.h"
#include "memory.h"
#include "debug.h"

typedef struct s_channel_state {
        uint16_t coder;              /* Index of coder in coder table      */
        u_char  *state;              /* Pointer to state relevant to coder */
        uint32_t state_len;          /* The size of that state             */
        uint8_t  units_per_packet:7; /* The number of units per packet     */
        uint8_t  is_encoder:1;       /* For debugging                      */
} channel_state_t;

typedef struct {
        cc_details_t details;
        uint8_t  pt;
        uint8_t  layers;
        int     (*enc_create_state)   (u_char                **state,
                                       uint32_t                *len);
        void    (*enc_destroy_state)  (u_char                **state,
                                       uint32_t                 len);
        int     (*enc_set_parameters) (u_char                 *state,
                                       char                   *cmd);
        int     (*enc_get_parameters) (u_char                 *state,
                                       char                   *cmd,
                                       uint32_t                 cmd_len);
        int     (*enc_reset)          (u_char                  *state);
        int     (*enc_encode)         (u_char                  *state,
                                       struct s_pb *in,
                                       struct s_pb *out,
                                       uint32_t                  units_per_packet);
        int     (*dec_create_state)   (u_char                 **state,
                                       uint32_t                 *len);
        void    (*dec_destroy_state)  (u_char                 **state,
                                       uint32_t                  len);
        int     (*dec_reset)          (u_char                  *state);
        int     (*dec_decode)         (u_char                  *state,
                                       struct s_pb *in,
                                       struct s_pb *out,
                                       timestamp_t                     now);
        int     (*dec_peek)           (uint8_t                   ccpt,
                                       u_char                  *data,
                                       uint32_t                  len,
                                       uint16_t                 *upp,
                                       uint8_t                  *pt);
        int     (*dec_describe)       (uint8_t  pktpt,
                                       u_char *data,
                                       uint32_t data_len,
                                       char   *outstr,
                                       uint32_t out_len);
} channel_coder_t;

#include "cc_vanilla.h"
#include "cc_rdncy.h"
#include "cc_layered.h"

#define CC_REDUNDANCY_PT 121
#define CC_VANILLA_PT    255
#define CC_LAYERED_PT    125

#define CC_IDX_TO_ID(x) (((x)+1) | 0x0e00)
#define CC_ID_TO_IDX(x) (((x)-1) & 0x000f)

static const channel_coder_t table[] = {
        /* The vanilla coder goes first. Update channel_get_null_coder
         * and channel_coder_get_by_payload if it moves.
         */
        {
                {
                        CC_IDX_TO_ID(0),
                        "None"
                },
                CC_VANILLA_PT,
                1,
                vanilla_encoder_create,
                vanilla_encoder_destroy,
                NULL,                   /* No parameters to set ...*/
                NULL,                   /* ... or get. */
                vanilla_encoder_reset,
                vanilla_encoder_encode,
                NULL,
                NULL,
                NULL,
                vanilla_decoder_decode,
                vanilla_decoder_peek,
                vanilla_decoder_describe
        },
        {
                {
                        CC_IDX_TO_ID(1),
                        "Redundancy"
                },
                CC_REDUNDANCY_PT,
                1,
                redundancy_encoder_create,
                redundancy_encoder_destroy,
                redundancy_encoder_set_parameters,
                redundancy_encoder_get_parameters,
                redundancy_encoder_reset,
                redundancy_encoder_encode,
                NULL,
                NULL,
                NULL,
                redundancy_decoder_decode,
                redundancy_decoder_peek,
                redundancy_decoder_describe
        },
        {
                {
                        CC_IDX_TO_ID(2),
                        "Layering"
                },
                CC_LAYERED_PT,
                LAY_MAX_LAYERS,
                layered_encoder_create,
                layered_encoder_destroy,
                layered_encoder_set_parameters,
                layered_encoder_get_parameters,
                layered_encoder_reset,
                layered_encoder_encode,
                NULL,
                NULL,
                NULL,
                layered_decoder_decode,
                layered_decoder_peek,
                layered_decoder_describe
        }
};

#define CC_NUM_CODERS (sizeof(table)/sizeof(table[0]))

uint32_t
channel_get_coder_count()
{
        return (int)CC_NUM_CODERS;
}

const cc_details_t*
channel_get_coder_details(uint32_t idx)
{
        if (idx < CC_NUM_CODERS) {
                return &table[idx].details;
        }
        return NULL;
}

const cc_details_t*
channel_get_coder_identity(channel_state_t *cs)
{
        assert(cs->coder < CC_NUM_CODERS);
        return &table[cs->coder].details;
}

const cc_details_t*
channel_get_null_coder(void)
{
        return &table[0].details;
}

/* The create, destroy, and reset functions take the same arguments and so use
 * is_encoder to determine which function in the table to call.  It's dirty
 * but it saves typing.  This should be undone at some time [oh]
 */

int
_channel_coder_create(cc_id_t id, channel_state_t **ppcs, int is_encoder)
{
        channel_state_t *pcs;
        int (*create_state)(u_char**, uint32_t *len);

        pcs = (channel_state_t*)xmalloc(sizeof(channel_state_t));
        if (pcs == NULL) {
                return FALSE;
        }

        *ppcs = pcs;

        pcs->coder = (uint16_t)CC_ID_TO_IDX(id);
        assert(pcs->coder < CC_NUM_CODERS);

        pcs->units_per_packet = 2;
        pcs->is_encoder       = is_encoder;

        if (is_encoder) {
                create_state = table[pcs->coder].enc_create_state;
		debug_msg("Created encoder: \"%s\"\n", table[pcs->coder].details.name);
        } else {
                create_state = table[pcs->coder].dec_create_state;
		debug_msg("Created decoder: \"%s\"\n", table[pcs->coder].details.name);
        }

        if (create_state) {
                create_state(&pcs->state, &pcs->state_len);
        } else {
                pcs->state     = NULL;
		pcs->state_len = 0;
	}

        return TRUE;
}

void
_channel_coder_destroy(channel_state_t **ppcs, int is_encoder)
{
        channel_state_t *pcs = *ppcs;

        void (*destroy_state)(u_char**, uint32_t);

        assert(is_encoder == pcs->is_encoder);

        if (is_encoder) {
                destroy_state = table[pcs->coder].enc_destroy_state;
        } else {
                destroy_state = table[pcs->coder].dec_destroy_state;
        }

        if (destroy_state) {
                destroy_state(&pcs->state, pcs->state_len);
                pcs->state_len = 0;
        }

        assert(pcs->state     == NULL);
        assert(pcs->state_len == 0);

        xfree(pcs);
        *ppcs = NULL;
}

int
_channel_coder_reset(channel_state_t *pcs, int is_encoder)
{
        int (*reset) (u_char *state);

        assert(is_encoder == pcs->is_encoder);

        if (is_encoder) {
                reset = table[pcs->coder].enc_reset;
        } else {
                reset = table[pcs->coder].dec_reset;
        }

        return (reset != NULL) ? reset(pcs->state) : TRUE;
}

/* Encoder specifics */

int
channel_encoder_set_units_per_packet(channel_state_t *cs, uint16_t units)
{
        /* This should not be hardcoded, it should be based on packet
         *size [oth]
         */
        assert(cs->is_encoder);
        if (units != 0 && units <= MAX_UNITS_PER_PACKET) {
                cs->units_per_packet = (u_char)units;
                return TRUE;
        }
        return FALSE;
}

uint16_t
channel_encoder_get_units_per_packet(channel_state_t *cs)
{
        assert(cs->is_encoder);
        return cs->units_per_packet;
}

int
channel_encoder_set_parameters(channel_state_t *cs, char *cmd)
{
        if (table[cs->coder].enc_set_parameters) {
                return table[cs->coder].enc_set_parameters(cs->state, cmd);
        }
        return TRUE;
}

int
channel_encoder_get_parameters(channel_state_t *cs, char *cmd, int cmd_len)
{
        if (table[cs->coder].enc_get_parameters) {
                return table[cs->coder].enc_get_parameters(cs->state, cmd, cmd_len);
        } else {
                assert(cmd_len != 0);
                cmd[0] = 0;
        }
        return TRUE;
}

int
channel_encoder_encode(channel_state_t         *cs,
                       struct s_pb *media_buffer,
                       struct s_pb *channel_buffer)
{
        assert(table[cs->coder].enc_encode != NULL);
        return table[cs->coder].enc_encode(cs->state, media_buffer, channel_buffer, cs->units_per_packet);
}

int
channel_decoder_decode(channel_state_t         *cs,
                       struct s_pb *media_buffer,
                       struct s_pb *channel_buffer,
                       timestamp_t                     now)
{
        assert(table[cs->coder].dec_decode != NULL);
        return table[cs->coder].dec_decode(cs->state, media_buffer, channel_buffer, now);
}

int
channel_decoder_matches(cc_id_t          id,
                        channel_state_t *cs)
{
        uint32_t coder = CC_ID_TO_IDX(id);
        return (coder == cs->coder);
}

int
channel_verify_and_stat(cc_id_t  cid,
                        uint8_t   pktpt,
                        u_char  *data,
                        uint32_t  data_len,
                        uint16_t *units_per_packet,
                        u_char  *codec_pt)
{
        uint32_t idx = CC_ID_TO_IDX(cid);
        assert(idx < CC_NUM_CODERS);
        return table[idx].dec_peek(pktpt, data, data_len, units_per_packet, codec_pt);
}

int
channel_describe_data(cc_id_t cid,
                      uint8_t  pktpt,
                      u_char *data,
                      uint32_t data_len,
                      char *outstr,
                      uint32_t out_len)
{
        uint32_t idx = CC_ID_TO_IDX(cid);
        assert(idx < CC_NUM_CODERS);

        assert(outstr  != NULL);
        assert(out_len != 0);

        if (table[idx].dec_describe) {
                return (table[idx].dec_describe(pktpt, data, data_len, outstr, out_len-1));
        }

        strncpy(outstr, "Not implemented", out_len-1);
        outstr[out_len-1] = '\0'; /* Always zero terminated */
        return TRUE;
}

cc_id_t
channel_coder_get_by_payload(uint8_t payload)
{
	uint32_t i = 0;

        assert((payload & 0x80) == 0);

        for(i = 0; i < CC_NUM_CODERS; i++) {
                if (table[i].pt == payload) {
/*			debug_msg("Found channel coder for payload type %d \"%s\"\n", payload, table[i].details.name); */
                        return CC_IDX_TO_ID(i);
                }
        }
        /* Return vanilla if not found */
/*	debug_msg("No channel coder for payload type %d\n", payload); */
        return CC_IDX_TO_ID(0);
}

uint8_t
channel_coder_get_payload(channel_state_t *st, uint8_t media_pt)
{
        assert(st->coder <= CC_NUM_CODERS);

        if (table[st->coder].pt == CC_VANILLA_PT) {
                return media_pt;
        }
        return table[st->coder].pt;
}

int
channel_coder_exist_payload(uint8_t pt)
{
        uint32_t i;
        for(i = 0; i < CC_NUM_CODERS; i++) {
                if (table[i].pt == pt) {
                        return TRUE;
                }
        }
        return FALSE;
}

uint8_t
channel_coder_get_layers(cc_id_t cid)
{
        uint32_t idx = CC_ID_TO_IDX(cid);
        assert(idx < CC_NUM_CODERS);

        return (table[idx].layers);
}
