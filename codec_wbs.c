/*
 * FILE:    codec_wbs.c
 * AUTHORS: Orion Hodson
 *
 * Copyright (c) 1998-2001 University College London
 * All rights reserved.
 */

#ifndef HIDE_SOURCE_STRINGS
static const char cvsid[] =
	"$Id: codec_wbs.c 3642 2004-01-12 17:14:56Z ucacoxh $";
#endif /* HIDE_SOURCE_STRINGS */

#include "config_unix.h"
#include "config_win32.h"
#include "memory.h"
#include "util.h"
#include "debug.h"
#include "audio_types.h"
#include "codec_types.h"
#include "codec_wbs.h"
#include "cx_wbs.h"

static codec_format_t cs[] = {
        {"WBS", "WBS-16K-Mono",
         "Wide band speech coder. Implemented by Markus Iken, University College London.",
         /* NB payload 109 for backward compatibility */
         109, WBS_STATE_SIZE, WBS_UNIT_SIZE,
         {DEV_S16, 16000, 16, 1, 160 * BYTES_PER_SAMPLE}
        }
};

#define WBS_NUM_FORMATS sizeof(cs)/sizeof(codec_format_t)
#define WBS_NUM_LAYERS 2

uint16_t
wbs_get_formats_count()
{
        return (uint16_t)WBS_NUM_FORMATS;
}

const codec_format_t *
wbs_get_format(uint16_t idx)
{
        assert(idx < WBS_NUM_FORMATS);
        return &cs[idx];
}

typedef struct s_wbs_state {
        wbs_state_struct        state;
        double                  qmf_lo[16];
        double                  qmf_hi[16];
        short                   ns;             /* Noise shaping state */
} wbs_t;

int
wbs_state_create(uint16_t idx, u_char **s)
{
        wbs_t *st;
        int    sz;

        if (idx < WBS_NUM_FORMATS) {
                sz = sizeof(wbs_t);
                st = (wbs_t*)xmalloc(sz);
                if (st) {
                        memset(st, 0, sz);
                        wbs_state_init(&st->state,
                                       st->qmf_lo,
                                       st->qmf_hi,
                                       &st->ns);
                        *s = (u_char*)st;
                        return sz;
                }
        }
        *s = NULL;
        return 0;
}

void
wbs_state_destroy(uint16_t idx, u_char **s)
{
        UNUSED(idx);
        assert(idx < WBS_NUM_FORMATS);
        xfree(*s);
        *s = (u_char*)NULL;
}

int
wbs_encoder(uint16_t idx, u_char *encoder_state, sample *inbuf, coded_unit *c)
{
        subband_struct SubBandData;
        wbs_t *wsp;
        uint8_t i;

        assert(encoder_state);
        assert(inbuf);
        assert(idx < WBS_NUM_FORMATS);
        UNUSED(idx);

        /* Transfer state and fix ordering */
        c->state     = (u_char*)block_alloc(WBS_STATE_SIZE);
        c->state_len = WBS_STATE_SIZE;
        c->data      = (u_char*)block_alloc(WBS_UNIT_SIZE);
        c->data_len  = WBS_UNIT_SIZE;

        wsp = (wbs_t*)encoder_state;
        memcpy(c->state, &wsp->state, WBS_STATE_SIZE);
        for(i=0; i<WBS_STATE_SIZE/4; i++) {
                *((uint32_t *)c->state + i) = htonl(*((uint32_t *)c->state+i));
        }
        QMF(inbuf, &SubBandData, wsp->qmf_lo, wsp->qmf_hi);
        LowEnc(SubBandData.Low, c->data, wsp->state.low, &wsp->ns);
        HighEnc(SubBandData.High, c->data, wsp->state.hi);

        return c->data_len;
}

int
wbs_decoder(uint16_t idx, u_char *decoder_state, coded_unit *c, sample *data)
{
        subband_struct SubBandData;
        wbs_t   *wsp = (wbs_t *)decoder_state;
        uint8_t i;

        assert(decoder_state);
        assert(c);
        assert(data);
        assert(idx < WBS_NUM_FORMATS);

        if (c->state_len > 0) {
                assert(c->state_len == WBS_STATE_SIZE);
                for(i=0; i<WBS_STATE_SIZE/4; i++) {
                        *((uint32_t *)c->state + i) = ntohl(*((uint32_t *)c->state+i));
                }
                memcpy(&wsp->state, c->state, WBS_STATE_SIZE);
        }

        LowDec(c->data, SubBandData.Low, wsp->state.low, &wsp->ns);
        HighDec(c->data, SubBandData.High, wsp->state.hi);
        deQMF(&SubBandData, data, wsp->qmf_lo, wsp->qmf_hi);
        return 160; /* Only does this size */
}

uint8_t
wbs_max_layers(void)
{
        return (uint8_t)WBS_NUM_LAYERS;
}

int wbs_get_layer (uint16_t idx, coded_unit *in, uint8_t layer, uint16_t *markers, coded_unit *out)
{
        int i, j;
        u_char base[WBS_UNIT_SIZE];
        u_char enh[WBS_UNIT_SIZE];
        u_char tmp1, tmp2, tmp3, tmp4;
        u_char tmp_enh;
        u_char tmp_base[3];
        coded_unit *tmp_out;

		UNUSED(idx);

        if(layer >= WBS_NUM_LAYERS) {
                debug_msg("Too many layers: WBS only supports %d\n", WBS_NUM_LAYERS);
                return 0;
        }

        /* don't care about state */
	out->state = NULL;
	out->state_len = 0;

        tmp_out = (coded_unit*)block_alloc(sizeof(coded_unit));
        tmp_out->data      = (u_char*)block_alloc(in->data_len);
        tmp_out->data_len  = in->data_len;

        for (i=0; i<WBS_UNIT_SIZE; i++) {
                tmp1 = tmp2 = tmp3 = tmp4 = *(in->data+i);
                base[i] = (u_char)(((tmp1 & 0x1e) >> 1) | ((tmp2 & 0xc0) >> 2));
                enh[i] = (u_char)(((tmp3 & 0x20) << 2) | ((tmp4 & 0x01) << 6));
        }

        /* Need to shift everything about so that it    *
        * fits nicely into bytes. At present enh layer *
        * occupies 2 bits and base layer 6 bits. So 4  *
        * units will fit into 4 bytes (1 enh, 3 base). *
        * There must, of course be an easier way to do *
        * this.                                        */

        j = 0;

        for (i=0; i<WBS_UNIT_SIZE; i+=4) {
                tmp_enh = tmp_base[0] = tmp_base[1] = tmp_base[2] = 0;

                tmp_enh = (u_char)(((enh[i] & 0xc0) >> 6) | ((enh[i+1] & 0xc0) >> 4) | ((enh[i+2] & 0xc0) >> 2) | (enh[i+3] & 0xc0));

                tmp1 = tmp2 = base[i+1];
                tmp3 = tmp4 = base[i+2];

                tmp_base[0] = (u_char)((base[i] & 0x3f) | ((tmp1 & 0x03) << 6));
                tmp_base[1] = (u_char)(((tmp2 & 0x3c) >> 2) | ((tmp3 & 0x0f) << 4));
                tmp_base[2] = (u_char)(((tmp4 & 0x30) >> 4) | ((base[i+3] & 0x3f) << 2));

                switch(layer) {
                case 0:
                        *(tmp_out->data + j) = tmp_base[0];
                        j++;
                        *(tmp_out->data + j) = tmp_base[1];
                        j++;
                        *(tmp_out->data + j) = tmp_base[2];
                        j++;
                        break;
                case 1:
                        *(tmp_out->data + j) = tmp_enh;
                        j++;
                        break;
                }
        }

        /* this should be made less specific */
        switch(layer) {
                case 0: markers[0] = 0;
                        break;
                case 1: markers[1] = 3*WBS_UNIT_SIZE/4;
                        break;
        }

        /* Now that we know that out->data_len = j, we can create *
         * a coded_unit of the correct length and return it       */
        out->data      = (u_char*)block_alloc(j);
        out->data_len  = j;
        memcpy(out->data, tmp_out->data, j);

        /* Delete tmp_out before exiting */
        block_free(tmp_out->data, tmp_out->data_len);
        tmp_out->data     = 0;
        tmp_out->data_len = 0;
        assert(tmp_out->data_len == 0);
        block_free(tmp_out, sizeof(coded_unit));

        xmemchk();

        return out->data_len;
}

int wbs_combine_layer (uint16_t idx, coded_unit *in, coded_unit *out, uint8_t nelem, uint16_t *markers)
{
        int i, j, k, marker;
        u_char cont_layer[WBS_UNIT_SIZE];
        u_char tmp1, tmp2, tmp3, tmp4;

        assert(in);
        UNUSED(idx);
		UNUSED(nelem);
        UNUSED(markers);

       /* By the time we get here we assume that the  *
        * data is in one contiguous block again, and  *
        * that markers indicates where the layers are *
        * divided. If the enhancement layer is lost,  *
        * it should have been replaced with zeros.    *
        * Thus all that is needed is to extract the   *
        * layers and reshuffle everything back        *
        * together again.                             */

        out->data_len  = in->data_len;
        out->data      = (u_char*)block_alloc(in->data_len);

        marker = 3*WBS_UNIT_SIZE/4; /* ie 60 for base, 20 for enh */

        j = k = 0;

        for (i=0; i<WBS_UNIT_SIZE; i+=4) {
                tmp1 = *(in->data + j);
                tmp2 = *(in->data + j + 1);
                tmp3 = *(in->data + j + 2);
                tmp4 = *(in->data + k + marker);

                cont_layer[i] = (u_char)((tmp1 & 0x3f) | ((tmp4 & 0x03) << 6));

                tmp1 = *(in->data + j);
                tmp4 = *(in->data + k + marker);

                cont_layer[i+1] = (u_char)(((tmp1 & 0xc0) >> 6) | ((tmp2 & 0x0f) << 2) | ((tmp4 & 0x0c) << 4));

                tmp1 = *(in->data + j);
                tmp2 = *(in->data + j + 1);
                tmp4 = *(in->data + k + marker);

                cont_layer[i+2] = (u_char)(((tmp2 & 0xf0) >> 4) | ((tmp3 & 0x03) << 4) | ((tmp4 & 0x30) << 2));

                tmp3 = *(in->data + j + 2);
                tmp4 = *(in->data + k + marker);

                cont_layer[i+3] = (u_char)(((tmp3 & 0xfc) >> 2) | (tmp4 & 0xc0));

                j+=3;
                k++;
        }

        for (i=0; i<WBS_UNIT_SIZE; i++) {
                tmp1 = tmp2 = tmp3 = tmp4 = cont_layer[i];
                *(out->data + i) = (u_char)(((tmp1 & 0x80) >> 2) | ((tmp2 & 0x40) >> 6) | ((tmp3 & 0x30) << 2) | ((tmp4 & 0x0f) << 1));
        }

        if (in->state_len > 0) {
                assert(in->state_len == WBS_STATE_SIZE);
                out->state_len = WBS_STATE_SIZE;
                out->state     = (u_char*)block_alloc(WBS_STATE_SIZE);
                memcpy(out->state, in->state, WBS_STATE_SIZE);
        }

        xmemchk();

        return (out->data_len);
}
