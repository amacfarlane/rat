/*
 * FILE:      cc_layered.c
 * AUTHOR(S): Orion Hodson + Tristan Henderson
 *
 *
 * Copyright (c) 1995-2001 University College London
 * All rights reserved.
 */

#ifndef HIDE_SOURCE_STRINGS
static const char cvsid[] =
	"$Id: cc_layered.c 3698 2006-04-07 18:05:08Z socrates $";
#endif /* HIDE_SOURCE_STRINGS */

#include "config_unix.h"
#include "config_win32.h"
#include "audio_types.h"
#include "codec_types.h"
#include "codec.h"
#include "channel_types.h"
#include "playout.h"
#include "cc_layered.h"

#include "memory.h"
#include "util.h"
#include "debug.h"

#define LAY_HDR32_PAT 0x80000000

#define LAY_HDR32_INIT(x)      (x)  = LAY_HDR32_PAT
#define LAY_HDR32_SET_PT(x,y)  (x) |= ((y)<<24)
#define LAY_HDR32_SET_MRK(x,y) (x) |= ((y)<<12)
#define LAY_HDR32_SET_LEN(x,y) (x) |= (y)
#define LAY_HDR32_GET_PT(z)    (((z) >> 24) & 0x7f)
#define LAY_HDR32_GET_MRK(z)   (((z) >> 12) & 0xfff)
#define LAY_HDR32_GET_LEN(z)   ((z) & 0xfff)

typedef struct {
        codec_id_t  codec_id;
        timestamp_t        playout;
        uint8_t      n_layers;
        uint32_t     nelem;
        media_data *elem[MAX_UNITS_PER_PACKET];
} lay_state;

int
layered_encoder_create(u_char **state, uint32_t *len)
{
        lay_state *le = (lay_state*)xmalloc(sizeof(lay_state));

        if (le) {
                *state = (u_char*)le;
                *len   = sizeof(lay_state);
                memset(le, 0, sizeof(lay_state));
                return TRUE;
        }

        return FALSE;
}

void
layered_encoder_destroy(u_char **state, uint32_t len)
{
        assert(len == sizeof(lay_state));
        layered_encoder_reset(*state);
        xfree(*state);
        *state = NULL;
}

int
layered_encoder_set_parameters(u_char *state, char *cmd)
{
        u_char     *nbuf;
        lay_state  *n, *cur;
        codec_id_t  cid;
        char       *s;
        uint8_t     layers;
        uint32_t    nl;
        int success = FALSE;

        assert(state != NULL);
        assert(cmd   != NULL);

        /* Create a temporary encoder, try to set its params */
        layered_encoder_create(&nbuf, &nl);
        n = (lay_state*)nbuf;
        assert(n != NULL);

        if(strcmp(cmd, "None")==0) { /*might happen from load_settings */
                debug_msg("layered codec not recognised\n");
                goto done;
        }

        s = (char *) strtok(cmd, "/");
        if(s==NULL) {
                debug_msg("layered_codec_not_recognised\n");
                goto done;
        }
        cid = codec_get_by_name(s);
        if (!codec_id_is_valid(cid)) {
                debug_msg("layered codec not recognized\n");
                goto done;
        }
        n->codec_id = cid;

        s = (char *) strtok(NULL, "/");
        layers = atoi(s);

        if(layers>codec_can_layer(cid) || layers>LAY_MAX_LAYERS) {
                debug_msg("Too many layers (%d)\n", layers);
                goto done;
        }
        n->n_layers = layers;

        layered_encoder_reset(state);
        /* Take bits from temporary encoder state we want */
        cur = (lay_state*)state;
	cur->codec_id = n->codec_id;
        cur->n_layers = n->n_layers;

        success = TRUE;

done:
        nbuf = (u_char*)n;
        layered_encoder_destroy(&nbuf, nl);
        return success;
}

int
layered_encoder_get_parameters(u_char *state, char *cmd, uint32_t cmd_len)
{
        const codec_format_t *cf;
        lay_state *l;
        uint32_t flen;

        char frag[CODEC_LONG_NAME_LEN+5]; /* XXX/nn/\0 + 1*/

        assert(cmd_len > 0);
        assert(cmd != NULL);

        l = (lay_state*)state;
        if (l->n_layers < 2) {
                debug_msg("Using layered coder with %d layers?\n", l->n_layers);
                return FALSE;
        }

        *cmd = '\0';
		flen = 0;

        cf = codec_get_format(l->codec_id);
        assert(cf!=NULL);
        sprintf(frag, "%s/%d", cf->long_name, l->n_layers);
        flen += strlen(frag);
        if (flen>cmd_len) {
                debug_msg("buffer overflow would have occurred.\n");
                *cmd = '\0';
                return FALSE;
        }
        strcat(cmd, frag);
        cmd[flen] = '\0';
        debug_msg("layered parameters: %s\n", cmd);
        return TRUE;
}

int
layered_encoder_reset(u_char *state)
{
        lay_state *le = (lay_state*)state;
        uint32_t   i;

        for(i = 0; i < le->nelem; i++) {
                media_data_destroy(&le->elem[i], sizeof(media_data));
        }
        le->nelem = 0;

       /* Should we be resetting the number of layers? *
        * This is only called in tx_stop().            */
        /*le->n_layers = 1; */

        debug_msg("layered_encoder_reset!\n");

        return TRUE;
}

/* Adds header to next free slot in channel_data */
static void
add_hdr(channel_unit *chu, uint8_t pt, uint16_t marker, uint16_t len)
{
        uint32_t *h;

        assert(chu != NULL);
        assert(chu->data == NULL);

        h = (uint32_t*)block_alloc(4);

        LAY_HDR32_INIT(*h);
        LAY_HDR32_SET_PT(*h, (uint32_t)pt);
        LAY_HDR32_SET_MRK(*h, (uint32_t)marker);
        LAY_HDR32_SET_LEN(*h, (uint32_t)len);

        *h = htonl(*h);
        chu->data     = (u_char*)h;
		chu->data_len = sizeof(*h);
}

/* layered_encoder_output transfers media data into channel_unit */

static void
layered_encoder_output(lay_state *le, struct s_pb *out)
{
        uint32_t i, used;
        channel_data *cd;
        uint8_t j;
        uint16_t cd_len[LAY_MAX_LAYERS], markers[LAY_MAX_LAYERS];
        coded_unit *lu;

        /* We have state for first unit and data for all others */
        channel_data_create(&cd, (le->nelem + 2)*(le->n_layers));

        /* Fill in payload */
        cd->elem[0]->pt           = codec_get_payload(le->codec_id);

        used = 0;

        /* leave space for headers */
        used += le->n_layers;

        /* Get state for first unit if there */
        if (le->elem[0]->rep[0]->state) {
                for(j=0; j<le->n_layers; j++) {
                        cd->elem[used]->data_len = le->elem[0]->rep[0]->state_len;
                        //SV-XXX cd->elem[used]->data = (char *)block_alloc(le->elem[0]->rep[0]->state_len);
			cd->elem[used]->data = (unsigned char*)block_alloc(le->elem[0]->rep[0]->state_len);
                        memcpy(cd->elem[used]->data, le->elem[0]->rep[0]->state, le->elem[0]->rep[0]->state_len);
                        used++;
                }
        }

        for(j = 0; j < le->n_layers; j++) {
                cd_len[j] = 0;
        }

        lu = (coded_unit*)block_alloc(sizeof(coded_unit));

        /* Transfer coded data to channel_data */
        for(i = 0; i < le->nelem; i++) {
                for(j = 0; j < le->n_layers; j++) {
                        codec_get_layer(le->codec_id, le->elem[i]->rep[0], j, markers, lu);
                        cd->elem[used]->data_len = lu->data_len;
			//SV-XXX cd->elem[used]->data = (char*)block_alloc(lu->data_len);
                        cd->elem[used]->data = (unsigned char*)block_alloc(lu->data_len);
                        memcpy(cd->elem[used]->data, lu->data, lu->data_len);
                        used++;
                        if(i==0) cd_len[j] = (uint16_t)lu->data_len;
                        if(lu->state_len) {
			  block_free(lu->state, lu->state_len);
			  lu->state = NULL;
			  lu->state_len = 0;
			}
			if(lu->data_len) {
			  block_free(lu->data, lu->data_len);
			  lu->data     = NULL;
			  lu->data_len = 0;
			}
                }
		block_free(le->elem[i]->rep[0]->data, le->elem[i]->rep[0]->data_len);
                le->elem[i]->rep[0]->data = NULL;
                le->elem[i]->rep[0]->data_len = 0;
                media_data_destroy(&le->elem[i], sizeof(media_data));
        }

        le->nelem = 0;
        assert(lu->data_len == 0);
        block_free(lu, sizeof(coded_unit));

        for(j=0; j<le->n_layers; j++) {
                add_hdr(cd->elem[j], cd->elem[0]->pt, markers[j], cd_len[j]);
        }

        assert(used <= cd->nelem);

        pb_add(out,
                (u_char*)cd,
                sizeof(channel_data),
                le->playout);
}

int
layered_encoder_encode (u_char      *state,
                        struct s_pb *in,
                        struct s_pb *out,
                        uint32_t      upp)
{
        uint32_t                   m_len;
        timestamp_t                playout;
        struct      s_pb_iterator *pi;
        u_char                    *m_get;
        media_data                *m;
        lay_state                 *le = (lay_state*)state;

        assert(upp != 0 && upp <= MAX_UNITS_PER_PACKET);

        pb_iterator_create(in, &pi);
        pb_iterator_advance(pi); /* Move to first element */

        while(pb_iterator_detach_at(pi, &m_get, &m_len, &playout)) {
                /* Remove element from playout buffer - it belongs to
                 * the layered encoder now.
                 */
                m = (media_data*)m_get;
                assert(m != NULL);

                if (le->nelem == 0) {
                        /* If it's the first unit make a note of it's
                         *  playout */
                        le->playout = playout;
                        if (m->nrep == 0) {
                                /* We have no data ready to go and no data
                                 * came off on incoming queue.
                                 */
                                media_data_destroy(&m, sizeof(media_data));
                                continue;
                        }
                } else {
                        /* Check for early send required:
                         * (a) if this unit has no media respresentations
                         *     e.g. end of talkspurt.
                         * (b) codec type of incoming unit is different
                         *     from what is on queue.
                         */
                        if (m->nrep == 0) {
                                layered_encoder_output(le, out);
                                media_data_destroy(&m, sizeof(media_data));
                                continue;
                        } else if (m->rep[0]->id != le->codec_id) {
                                layered_encoder_output(le, out);
                        }
                }

                assert(m_len == sizeof(media_data));

                le->codec_id = m->rep[0]->id;
                le->elem[le->nelem] = m;
                le->nelem++;

                if (le->nelem >= (uint32_t)upp) {
                        layered_encoder_output(le, out);
                }
        }

        pb_iterator_destroy(in, &pi);

        xmemchk();

        return TRUE;
}

/* This fn takes the channel_data, which should have all the received *
 * layers in its channel_units, and combines the channel_units into   *
 * one coded_unit, with zeros if a layer was not received.            */

static int
layered_decoder_reorganise(channel_data *in, struct s_pb *out, timestamp_t playout)
{
        const codec_format_t *cf;
        codec_id_t            id;
        coded_unit           *cu;
        u_char               *p[LAY_MAX_LAYERS], *end;
        uint32_t               hdr32, data_len;
        uint8_t hdrpt, i;
        uint16_t len[LAY_MAX_LAYERS], mrk[LAY_MAX_LAYERS];
        media_data           *m;
        timestamp_t                  playout_step;

        media_data_create(&m, 1);
        assert(m->nrep == 1);

        if(in->nelem > LAY_MAX_LAYERS) {
                debug_msg("Too many layers to reorganise\n");
		goto done;
        }


       /* Since layer_decoder_peek checks all the headers, we can
        * assume they are OK. We still need to check that they match
        * up, however, i.e. that all the layers are intact, and that
        * they are all using the same codec. Layers need to be sorted
        * into order as well. We use the markers to determine how to
        * join the layers together into one media_data, and then get
        * out of here.
        */

        p[0] = in->elem[0]->data;
        hdr32 = ntohl(*(uint32_t*)p[0]);
        if(hdr32 & LAY_HDR32_PAT) {
                hdrpt = (uint8_t)(LAY_HDR32_GET_PT(hdr32));
                mrk[0] = (uint8_t)(LAY_HDR32_GET_MRK(hdr32));
                len[0] = (uint8_t)(LAY_HDR32_GET_LEN(hdr32));
                p[0] += 4;
        }
        else {
                debug_msg("Invalid layered header\n");
		goto done;
        }

        for(i=1; i<in->nelem; i++) {
                p[i] = in->elem[i]->data;

                hdr32 = ntohl(*(uint32_t*)p[i]);
                if(hdr32 & LAY_HDR32_PAT) {
                        if(hdrpt != (uint8_t)(LAY_HDR32_GET_PT(hdr32))) {
                                debug_msg("layered headers do not match!\n");
                                goto done;
                        }
                        mrk[i] = (uint16_t)(LAY_HDR32_GET_MRK(hdr32));
                        len[i] = (uint16_t)(LAY_HDR32_GET_LEN(hdr32));
                        p[i] += 4;
                }
                else {
                        debug_msg("Invalid layered header\n");
                        goto done;
                }
        }
        end  = in->elem[in->nelem-1]->data + in->elem[in->nelem-1]->data_len;

        /* if layers missing say so */
        if(in->nelem!=LAY_MAX_LAYERS) {
                debug_msg("Not all layers arrived:\n");
                for(i=0; i<in->nelem; i++) {
                        debug_msg("marker[%d] = %d\n", i, mrk[i]);
                }
        }

        /* Everything matches, so we'll use the first layer's details */

        cu = (coded_unit*)block_alloc(sizeof(coded_unit));
        memset(cu, 0, sizeof(coded_unit));

        id = codec_get_by_payload(hdrpt);
        if (codec_id_is_valid(id) == FALSE) {
                debug_msg("Layered channel coder - codec_id not recognised.\n");
                goto fail;
        }
        cf = codec_get_format(id);
        assert(cf != NULL);

       /* Do first unit separately as that may have state */
        if (cf->mean_per_packet_state_size) {
                cu->state_len = cf->mean_per_packet_state_size;
                cu->state     = (u_char*)block_alloc(cu->state_len);
                memcpy(cu->state, p[0], cf->mean_per_packet_state_size);
                for(i=0; i<in->nelem; i++)
                        p[i] += cf->mean_per_packet_state_size;
        }

        data_len = codec_peek_frame_size(id, p[0], (uint16_t)(len[0]));
        m->rep[0]->id = cu->id = id;
        cu->data = (u_char*)block_alloc(data_len);
        cu->data_len = (uint16_t)data_len;
        memset(cu->data, 0, data_len);

        /* join the layers up here */

        for(i=0; i<in->nelem; i++) {
                memcpy(cu->data + mrk[i], p[i], len[i]);
                p[i] += len[i];
        }

        codec_combine_layer(id, cu, m->rep[0], in->nelem, mrk);

        if (cu->state_len) {
                block_free(cu->state, cu->state_len);
                cu->state     = NULL;
                cu->state_len = 0;
        }
        assert(cu->state_len == 0);
        if (cu->data_len) {
                block_free(cu->data, cu->data_len);
                cu->data     = NULL;
                cu->data_len = 0;
        }
        assert(cu->data_len == 0);

        if (pb_add(out, (u_char *)m, sizeof(media_data), playout) == FALSE) {
                debug_msg("layered decode failed\n");
                goto fail;
        }

        /* Now do other units which do not have state*/
        playout_step = ts_map32(cf->format.sample_rate, codec_get_samples_per_frame(id));
        while(p[in->nelem - 1] < end) {
                playout = ts_add(playout, playout_step);
                media_data_create(&m, 1);
                m->rep[0]->id = id;
                assert(m->nrep == 1);

                cu->data            = (u_char*)block_alloc(data_len);
                cu->data_len        = (uint16_t)data_len;
                memset(cu->data, 0, data_len);

                for(i=0; i<in->nelem; i++) {
                        memcpy(cu->data + mrk[i], p[i], len[i]);
                        p[i] += len[i];
                }

                codec_combine_layer(id, cu, m->rep[0], in->nelem, mrk);

                block_free(cu->data, cu->data_len);
                cu->data     = 0;
                cu->data_len = 0;

                if (pb_add(out, (u_char *)m, sizeof(media_data), playout) == FALSE) {
                        debug_msg("layered decode failed\n");
                        goto fail;
                }
        }
        assert(p[in->nelem - 1] == end);

        block_free(cu, sizeof(coded_unit));
	channel_data_destroy(&in, sizeof(channel_data));
        xmemchk();
        return TRUE;

fail:
        if (cu->state) {
                block_free(cu->state, cu->state_len);
                cu->state     = 0;
                cu->state_len = 0;
        }
        assert(cu->state_len == 0);
        if (cu->data) {
                block_free(cu->data, cu->data_len);
                cu->data     = 0;
                cu->data_len = 0;
        }
        assert(cu->data_len == 0);
        block_free(cu, sizeof(coded_unit));
done:
	media_data_destroy(&m, sizeof(media_data));
	channel_data_destroy(&in, sizeof(channel_data));
        xmemchk();
        return FALSE;
}

int
layered_decoder_decode(u_char      *state,
                       struct s_pb *in,
                       struct s_pb *out,
                       timestamp_t  now)
{
        struct s_pb_iterator *pi;
        u_char               *c_get;
        channel_data         *c;
        uint32_t              clen;
        timestamp_t           playout;

        UNUSED(state);

        pb_iterator_create(in, &pi);
        assert(pi != NULL);

        while(pb_iterator_get_at(pi, &c_get, &clen, &playout)) {
                c = (channel_data*)c_get;
                assert(c != NULL);
                assert(clen == sizeof(channel_data));

                if (ts_gt(playout, now)) {
                        /* Playout point of unit is after now.  Stop! */
                        break;
                }
                pb_iterator_detach_at(pi, &c_get, &clen, &playout);
                layered_decoder_reorganise(c, out, playout);
        }

        pb_iterator_destroy(in, &pi);

        return TRUE;
}

int
layered_decoder_peek(uint8_t   pkt_pt,
                     u_char  *buf,
                     uint32_t  len,
                     uint16_t  *upp,
                     uint8_t   *pt)
{
        codec_id_t cid;
        u_char               *p, *data;
        uint32_t hdr32;
        uint8_t hdrpt;
        uint16_t blen, mrk;
        assert(buf != NULL);
        assert(upp != NULL);
        assert(pt  != NULL);
        UNUSED(pkt_pt);

        p = data = buf;

        hdr32 = ntohl(*(uint32_t*)p);

        if(hdr32 & LAY_HDR32_PAT) {
                hdrpt = (uint8_t)(LAY_HDR32_GET_PT(hdr32));
                mrk = (uint16_t)(LAY_HDR32_GET_MRK(hdr32));
                blen = (uint16_t)(LAY_HDR32_GET_LEN(hdr32));
                p+=4;
                data += 4 + blen;
                hdr32 = ntohl(*(uint32_t*)p);
/*                assert(((uint32_t)data - (uint32_t)buf) <= blen); */
        }
        else {
                debug_msg("Invalid layered header\n");
                goto fail;
        }

        /* I'm haven't decided what exactly to do here yet, so for
         * the time being if the header seems OK we return TRUE. The
         * options are:
         * (i) have a new function codec_peek_layer_frame_size
         * (ii) work out length of total frame from length in header
         * (iii) just check that length of packet matches what is in
         *       the header
         * But what to do about *upp?
         * The problem is that codec_peek_frame_size, if used with
         * codec_vdvi, calls vdvi_decode, assuming a complete frame.
         * Of course we only have one layer at this stage, so the
         * decode function will fail. I am going to ignore this for
         * the time being.
         */

        *pt = hdrpt;
        cid = codec_get_by_payload(*pt);
        if (cid) {
                const codec_format_t *cf;
                uint32_t               unit, done, step;
                /* extra check since the header check seems
                 * to fail quite a lot (why?)
                */
                if(codec_can_layer(cid)==1) goto fail;
                cf   = codec_get_format(cid);
                unit = 0;
                done = cf->mean_per_packet_state_size;
                done += 4; /* step over header */
                while(done < len) {
                        step = codec_peek_frame_size(cid, buf+done, (uint16_t)(len));
                        if (step == 0) {
                                debug_msg("Zero data len for audio unit ?\n");
                                goto fail;
                        }
                        done += blen;
                        unit ++;
                }

/*                assert(done <= len);*/

                if (done != len) goto fail;
                *upp = (uint16_t)unit;
                return TRUE;
        }

        debug_msg("layered_decoder_peek - codec not found\n");
fail:
        debug_msg("layered_decoder_peek error (len = %d)\n", len);
        *upp = 0;
        *pt  = 255;
        return FALSE;
}


/* Just returns the long name of the codec.
 * Could display number of layers if really bothered.
 */
int
layered_decoder_describe (uint8_t   pkt_pt,
                          u_char  *data,
                          uint32_t  data_len,
                          char    *out,
                          uint32_t  out_len)
{
        uint32_t hdr32, slen;
        uint8_t hdrpt;
		uint16_t blen, mrk;
		codec_id_t            pri_id;
        const codec_format_t *pri_cf;

		UNUSED(pkt_pt);

        hdr32 = ntohl(*(uint32_t*)data);
        if(hdr32 & LAY_HDR32_PAT) {
                hdrpt = (uint8_t)(LAY_HDR32_GET_PT(hdr32));
                mrk = (uint16_t)(LAY_HDR32_GET_MRK(hdr32));
                blen = (uint16_t)(LAY_HDR32_GET_LEN(hdr32));

                pri_id = codec_get_by_payload(hdrpt);
                if(pri_id) {
                        pri_cf = codec_get_format(pri_id);
                        slen = strlen(pri_cf->long_name);
                        strncpy(out, pri_cf->long_name, out_len);
                        goto done;
                }
        }
        strncpy(out, "Unknown", out_len);

done:
        /* string safety - strncpy not always safe */
        out[out_len - 1] = '\0';

        UNUSED(data_len);

        return TRUE;
}
