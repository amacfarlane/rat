/*
 * FILE:      cc_rdncy.c
 * AUTHOR(S): Orion Hodson
 *
 *
 * Copyright (c) 1995-2001 University College London
 * All rights reserved.
 */

#ifndef HIDE_SOURCE_STRINGS
static const char cvsid[] =
	"$Id: cc_rdncy.c 3637 2004-01-09 18:35:41Z ucacoxh $";
#endif /* HIDE_SOURCE_STRINGS */

#include "config_unix.h"
#include "config_win32.h"
#include "audio_types.h"
#include "codec_types.h"
#include "codec.h"
#include "channel_types.h"
#include "playout.h"
#include "cc_rdncy.h"

#include "memory.h"
#include "util.h"
#include "debug.h"

#define RED_MAX_LAYERS 3
#define RED_MAX_OFFSET 16383u
#define RED_MAX_LEN    1023u

#define RED_PRIMARY 1
#define RED_EXTRA   2

#define RED_HDR32_PAT 0x80000000

#define RED_HDR32_INIT(x)      (x)  = RED_HDR32_PAT
#define RED_HDR32_SET_PT(x,y)  (x) |= ((y)<<24)
#define RED_HDR32_SET_OFF(x,y) (x) |= ((y)<<10)
#define RED_HDR32_SET_LEN(x,y) (x) |= (y)
#define RED_HDR32_GET_PT(z)    (((z) >> 24) & 0x7f)
#define RED_HDR32_GET_OFF(z)   (((z) >> 10) & 0x3fff)
#define RED_HDR32_GET_LEN(z)   ((z) & 0x3ff)

#define RED_HDR8_INIT(x)       (x) = 0
#define RED_HDR8_SET_PT(x,y)   (x) = (y)
#define RED_HDR8_GET_PT(z)     (z)

typedef struct s_red_layer {
        codec_id_t cid;
        uint32_t   pkts_off;
} red_layer;

typedef struct {
        red_layer             layer[RED_MAX_LAYERS];
        uint32_t              n_layers;
        struct s_pb          *media_buffer;
        struct s_pb_iterator *media_pos;
        uint32_t              units_ready;
        timestamp_t           history; /* How much audio history is needed for coding */
        timestamp_t           last_in; /* timestamp of last media unit accepted */
} red_enc_state;

int
redundancy_encoder_create(u_char **state, uint32_t *len)
{
        red_enc_state *re = (red_enc_state*)xmalloc(sizeof(red_enc_state));

        if (re == NULL) {
                debug_msg("Failed to allocate encoder\n");
                goto fail_alloc;
        }
        memset(re, 0, sizeof(red_enc_state));

        *state = (u_char*)re;
        *len   = sizeof(red_enc_state);

        if (pb_create(&re->media_buffer,
                      (playoutfreeproc)media_data_destroy) == FALSE) {
                debug_msg("Gailed to create media buffer\n");
                goto fail_pb;
        }

        if (pb_iterator_create(re->media_buffer, &re->media_pos) == FALSE) {
                debug_msg("failed to create iterator\n");
                goto fail_pb_iterator;
        }

        re->n_layers    = 0;
        re->units_ready = 0;

        return TRUE;

fail_pb_iterator:
        pb_destroy(&re->media_buffer);

fail_pb:
        xfree(re);

fail_alloc:
        *state = NULL;
        return FALSE;
}

void
redundancy_encoder_destroy(u_char **state, uint32_t len)
{
        red_enc_state *re = *((red_enc_state**)state);

        assert(len == sizeof(red_enc_state));

        pb_iterator_destroy(re->media_buffer,
                            &re->media_pos);

        pb_destroy(&re->media_buffer);

        xfree(*state);
        *state = NULL;
}

int
redundancy_encoder_reset(u_char *state)
{
        red_enc_state *re = (red_enc_state*)state;

        pb_flush(re->media_buffer);
        re->units_ready = 0;

        return TRUE;
}

/* Adds header to next free slot in channel_data */
static void
add_hdr(channel_unit *chu, int hdr_type, codec_id_t cid, uint32_t uo, uint32_t len)
{
        uint32_t so;             /* sample time offset */
        u_char  pt;

        assert(chu != NULL);
        assert(chu->data == NULL);

        pt = codec_get_payload(cid);
        assert(payload_is_valid(pt));

        so = codec_get_samples_per_frame(cid) * uo;

        assert(so <= RED_MAX_OFFSET);
        assert(len <= RED_MAX_LEN );

        if (hdr_type == RED_EXTRA) {
                uint32_t *h;
                h = (uint32_t*)block_alloc(4);
                RED_HDR32_INIT(*h);
                RED_HDR32_SET_PT(*h, (uint32_t)pt);
                RED_HDR32_SET_OFF(*h, so);
                RED_HDR32_SET_LEN(*h, len);
                *h = htonl(*h);
                chu->data     = (u_char*)h;
                chu->data_len = sizeof(*h);
        } else {
                u_char *h;
                assert(hdr_type == RED_PRIMARY);
                h = (u_char*)block_alloc(1);
                RED_HDR8_INIT(*h);
                RED_HDR8_SET_PT(*h, pt);
                chu->data     = h;
                chu->data_len = sizeof(*h);
        }
}

/* make_pdu - converts a string of coded units into a channel_unit */

static uint32_t
make_pdu(struct s_pb_iterator *pbi,
         uint32_t               upp,
         codec_id_t            cid,
         channel_data         *out)
{
        struct s_pb_iterator *p;
        uint32_t        i, j, md_len, used;
        u_char        *md_get;
        media_data    *md;
        timestamp_t           playout;
        int            success;

        pb_iterator_dup(&p, pbi);

        used = 0;
        for (i = 0; i < upp; i++) {
                success = pb_iterator_get_at(p, &md_get, &md_len, &playout);
                md = (media_data*)md_get;
                assert(success); /* We could rewind this far so must be able to get something! */

                /* Find first compatible coding */
                for(j = 0; j < md->nrep && md->rep[j]->id != cid; j++);
                if (j == md->nrep) {
                        /* could not find coding */
                        debug_msg("coding not found\n");
                        break;
                }

                if (i == 0 && md->rep[j]->state != NULL) {
                        /* This is first unit in block so we want state */
                        assert(out->elem[used]->data == NULL);
                        out->elem[used]->data     = md->rep[j]->state;
                        out->elem[used]->data_len = md->rep[j]->state_len;
                        md->rep[j]->state     = NULL;
                        md->rep[j]->state_len = 0;
                        used++;
                }
                assert(used < out->nelem);
                assert(out->elem[used]->data == NULL);
                out->elem[used]->data     = md->rep[j]->data;
                out->elem[used]->data_len = md->rep[j]->data_len;
                md->rep[j]->data     = NULL;
                md->rep[j]->data_len = 0;
                md->rep[j]->id       = 0; /* nobble this unit since we have taken it's data */
                used++;
                assert(used <= out->nelem);

                pb_iterator_advance(p);
        }

        pb_iterator_destroy(pb_iterator_get_playout_buffer(pbi), &p);
        xmemchk();

        return used;
}

static channel_data *
redundancy_encoder_output(red_enc_state *re, uint32_t upp)
{
        struct s_pb_iterator *pbm;
        channel_data         *cd_coded[RED_MAX_LAYERS], *cd_out;
        uint32_t               offset ;
        int                   i, j, layers, success = 0,  used = 0;

        pbm = re->media_pos;
        pb_iterator_ffwd(pbm);

        /*** Stage 1: Packing coded audio units ******************************/

        /* Rewind iterator to start of first pdu */
        for(i = 1; (uint32_t)i < upp; i++) {
                success = pb_iterator_retreat(pbm);
                assert(success);
        }

        offset = 0;
        layers = 0;
        for (i = 0; (uint32_t)i < re->n_layers; i++) {
                if (re->units_ready <= re->layer[i].pkts_off * upp) {
                        break;
                }
                /* Move back to start of this layer */
                while (offset < re->layer[i].pkts_off * upp) {
                        success = pb_iterator_retreat(pbm);
                        if (success == FALSE) break;
                        offset++;
                }
                xmemchk();
                /* need upp data elements + 1 for state */
                channel_data_create(&cd_coded[i], upp + 1);
                success = make_pdu(pbm, upp, re->layer[i].cid, cd_coded[i]);
                /* make_pdu may fail because coding not available */
                if (success == FALSE) {
                        channel_data_destroy(&cd_coded[i], sizeof(channel_data));
                        break;
                }
                layers++;
        }

#ifdef DEBUG_REDUNDANCY
        debug_msg("end of data collection\n");
#endif /* DEBUG_REDUNDANCY */
        assert(layers != 0);

        /* Create channel_data unit that will get output */
        channel_data_create(&cd_out, layers * (upp + 1) + re->n_layers);

        /*** Stage 2: Packing redundancy headers *****************************/
        used = 0;
        if ((uint32_t)layers != re->n_layers) {
                /* Add max offset if we didn't make all units */
                add_hdr(cd_out->elem[used],
                        RED_EXTRA,
                        re->layer[re->n_layers - 1].cid,
                        re->layer[re->n_layers - 1].pkts_off * upp,
                        0);
                used++;
        }

        i = layers - 1;
        while (i > 0) {
                add_hdr(cd_out->elem[used],
                        RED_EXTRA,
                        re->layer[re->n_layers - 1].cid,
                        re->layer[re->n_layers - 1].pkts_off * upp,
                        channel_data_bytes(cd_coded[i]));
                used++;
                i--;
        }

        add_hdr(cd_out->elem[used],
                RED_PRIMARY,
                re->layer[0].cid,
                re->layer[0].pkts_off * upp,
                0);
        used++;

        /*** Stage 3: Transfering coded units into output unit ***************/

        for(i = layers - 1; i >= 0; i--) {
                for (j = 0; j < cd_coded[i]->nelem && cd_coded[i]->elem[j]->data != NULL; j++) {
                        cd_out->elem[used]->data       =  cd_coded[i]->elem[j]->data;
                        cd_out->elem[used]->data_len   =  cd_coded[i]->elem[j]->data_len;
                        cd_coded[i]->elem[j]->data     = NULL;
                        cd_coded[i]->elem[j]->data_len = 0;
                        used++;
                        assert(used <= cd_out->nelem);
                }
                assert(used <= cd_out->nelem);
                channel_data_destroy(&cd_coded[i], sizeof(channel_data));
        }

        pb_iterator_audit(pbm, re->history); /* Clear old rubbish */

        return  cd_out;
}

int
redundancy_encoder_encode (u_char      *state,
                           struct s_pb *in,
                           struct s_pb *out,
                           uint32_t      upp)
{
        uint32_t        m_len;
        timestamp_t           playout;
        struct s_pb_iterator *pi;
        u_char         *m_get;
        media_data     *m;
        red_enc_state  *re = (red_enc_state*)state;

        assert(upp != 0 && upp <= MAX_UNITS_PER_PACKET);

        pb_iterator_create(in, &pi);

        assert(pi != NULL);
        pb_iterator_advance(pi);
        while(pb_iterator_detach_at(pi, &m_get, &m_len, &playout)) {
                m = (media_data*)m_get;
                /* Remove element from playout buffer - it belongs to
                 * the redundancy encoder now.  */
#ifdef DEBUG_REDUNDANCY
                debug_msg("claimed %d, prev %d\n", playout.ticks, re->last_in.ticks);
#endif /* DEBUG_REDUNDANCY */
                assert(m != NULL);

                if (re->units_ready == 0) {
                        re->last_in = playout;
                        re->last_in.ticks--;
                }

                assert(ts_gt(playout, re->last_in));
                re->last_in = playout;

                if (m->nrep > 0) {
                        pb_add(re->media_buffer,
                               (u_char*)m,
                               m_len,
                               playout);
                        re->units_ready++;
                } else {
                        /* Incoming unit has no data so transmission is
                         * not happening.
                         */
#ifdef DEBUG_REDUNDANCY
                        debug_msg("No incoming data\n");
#endif /* DEBUG_REDUNDANCY */
                        media_data_destroy(&m, sizeof(media_data));
                        pb_flush(re->media_buffer);
                        re->units_ready = 0;
                        continue;
                }

                if (re->units_ready && (re->units_ready % upp) == 0) {
                        channel_data *cd;
                        int s;
                        cd = redundancy_encoder_output(re, upp);
                        assert(cd != NULL);
                        s  = pb_add(out, (u_char*)cd, sizeof(channel_data), playout);
#ifdef DEBUG_REDUNDANCY
                        debug_msg("Ready %d, Added %d\n", re->units_ready, playout.ticks);
#endif /* DEBUG_REDUNDANCY */
                        assert(s);
                }
        }

        pb_iterator_destroy(in, &pi);

        return TRUE;
}

/* Redundancy {get,set} parameters expects strings like:
 *            dvi-8k-mono/0/lpc-8k-mono/2
 * where number is the offset in number of units.
 */

int
redundancy_encoder_set_parameters(u_char *state, char *cmd)
{
        u_char *encbuf;
        red_enc_state *n, *cur;
        const codec_format_t *cf;
        uint32_t nl, po;
        codec_id_t  cid;
        char *s;
        int success = FALSE;

        assert(state != NULL);
        assert(cmd   != NULL);

        /* Create a temporary encoder, try to set it's params */
        redundancy_encoder_create(&encbuf, &nl);
        n = (red_enc_state*)encbuf;
        assert(n != NULL);

        s = (char *) strtok(cmd, "/");
        cid = codec_get_by_name(s);
        if (!codec_id_is_valid(cid)) {
                debug_msg("codec not recognized\n");
                goto done;
        }

        s = (char *) strtok(NULL, "/");
        po = atoi(s);

        if (po > 20) {
                debug_msg("offset too big\n");
                goto done;
        }

        n->layer[0].cid       = cid;
        n->layer[0].pkts_off  = po;
        n->n_layers           = 1;

        while (n->n_layers < RED_MAX_LAYERS) {
                s = (char *) strtok(NULL, "/");
                if (s == NULL) break;
                cid = codec_get_by_name(s);
                if (!codec_id_is_valid(cid)) {
                        debug_msg("codec not recognized\n");
                        goto done;
                }

                s = (char *) strtok(NULL, "/");
                if (s == NULL) {
                        debug_msg("Incomplete layer info\n");
                        goto done;
                }
                po = atoi(s);
                if (po > 20) {
                        debug_msg("offset too big\n");
                        goto done;
                }

                n->layer[n->n_layers].cid      = cid;
                n->layer[n->n_layers].pkts_off = po;
                n->n_layers ++;
        }


        redundancy_encoder_reset(state);
        /* Take bits from temporary encoder state we want */
        cur = (red_enc_state*)state;
        memcpy(cur->layer, n->layer, sizeof(red_layer)*RED_MAX_LAYERS);
        cur->n_layers = n->n_layers;

        /* work out history = duration of audio frame * maximum offset */
        cf = codec_get_format(cur->layer[cur->n_layers - 1].cid);
        cur->history = ts_map32(cf->format.sample_rate,
                                codec_get_samples_per_frame(cur->layer[cur->n_layers - 1].cid) *
                                cur->layer[cur->n_layers - 1].pkts_off);

        success = TRUE;
done:
        encbuf = (u_char*)n;
        redundancy_encoder_destroy(&encbuf, nl);
        return success;
}

int
redundancy_encoder_get_parameters(u_char *state, char *buf, uint32_t blen)
{
        const codec_format_t *cf;
        red_enc_state *r;
        uint32_t i, used, flen;

        char frag[CODEC_LONG_NAME_LEN+5]; /* XXX/nn/\0 + 1*/

        assert(blen > 0);
        assert(buf != NULL);

        r = (red_enc_state*)state;
        if (r->n_layers < 2) {
                debug_msg("Redundancy encoder has not had parameters set!\n");
                return FALSE;
        }

        *buf = '\0';
	flen = 0;

        for(i = 0, used = 0; i < r->n_layers; i++) {
                cf = codec_get_format(r->layer[i].cid);
                assert(cf != NULL);
                sprintf(frag,
                        "%s/%d/",
                        cf->long_name,
                        r->layer[i].pkts_off);
                flen += strlen(frag);
                if (used+flen > blen) {
                        debug_msg("buffer overflow would have occured.\n");
                        *buf = '\0';
                        return FALSE;
                }
                strcat(buf + used, frag);
                used += flen;
        }
        buf[used - 1] = '\0';
        debug_msg("red parameters: %s\n", buf);
        return TRUE;
}

int
redundancy_decoder_peek(uint8_t   pkt_pt,
                        u_char  *buf,
                        uint32_t  len,
                        uint16_t  *upp,
                        uint8_t   *pt)
{
        const codec_format_t *cf;
        codec_id_t            cid;
        u_char               *p, *data;
        uint32_t               hdr32, dlen, blen;
        uint16_t               units;
        assert(buf != NULL);
        assert(upp != NULL);
        assert(pt  != NULL);

        /* Just check primary, so skip over other headers and
         * advance data pointer past them.
         */
        p = data = buf;
        hdr32 = ntohl(*(uint32_t*)p);
        while ((hdr32 & RED_HDR32_PAT)) {
                blen = RED_HDR32_GET_LEN(hdr32);
                p    += 4; /* goto next hdr */
                data += 4 + blen;
                hdr32 = ntohl(*(uint32_t*)p);
                assert(((unsigned long)data - (unsigned long)buf) <= len);
        }

        *pt = *p;
        data += 1; /* step over payload field of primary */

        cid = codec_get_by_payload(*pt);

        if (!cid) {
                debug_msg("Codec not found\n");
                return FALSE;
        }

        /* Primary data length */
        dlen = len - (uint32_t)(data - buf);

        cf = codec_get_format(cid);
        assert(cf);

        data += cf->mean_per_packet_state_size;
        dlen -= cf->mean_per_packet_state_size;

        assert(((unsigned long)data - (unsigned long)buf) <= len);

        units = 0;
        while (dlen != 0) {
                blen = codec_peek_frame_size(cid, p, (uint16_t)dlen);
                assert(blen != 0);
                data += blen;
                dlen -= blen;
                units ++;
                assert(((unsigned long)data - (unsigned long)buf) <= len);
        }

        *upp = units;
        assert(*upp < 50);

        UNUSED(pkt_pt);

        return TRUE;
}

/* redundancy_decoder_describe - produces a text string describing the
 * format from a packet of data.  We go through layers one at a time,
 * shift the text from previous layers on, and insert text for current
 * layer.  Not a very attractive method.
 */

int
redundancy_decoder_describe (uint8_t   pkt_pt,
                             u_char  *data,
                             uint32_t  data_len,
                             char    *out,
                             uint32_t  out_len)
{
        const codec_format_t *cf;
        codec_id_t            cid;
        uint32_t hdr32, slen, blksz, off, nlen;
        u_char  *p, pt;

        UNUSED(pkt_pt);

        *out = '\0';
        slen = 0;

        p   = data;
        hdr32 = ntohl(*((uint32_t*)p));
        while (hdr32 & RED_HDR32_PAT) {
                pt    = (u_char)RED_HDR32_GET_PT(hdr32);
                off   = RED_HDR32_GET_OFF(hdr32);
                blksz = RED_HDR32_GET_LEN(hdr32);
                cid   = codec_get_by_payload(pt);

                if (cid == 0) {
                        p += 4;
                        hdr32 = ntohl(*((uint32_t*)p));
                        continue;
                }

                cf = codec_get_format(cid);
                assert(cf != NULL);

                nlen = strlen(cf->long_name);

                if (slen + nlen >=  out_len) {
                        debug_msg("Out of buffer space\n");
                        return FALSE;
                }
                if (slen != 0) {
                        memmove(out + nlen + 1, out, slen);
                }
                strncpy(out, cf->long_name, nlen);
                slen += nlen;
                out[nlen] = '/';
                slen++;
                out[nlen+1] = '\0';
                p += 4;
                assert((uint32_t)(p - data) < data_len);
                hdr32 = ntohl(*((uint32_t*)p));
        }

        pt  = *p;
        cid = codec_get_by_payload(pt);
        if (cid == 0) {
                return FALSE;
        }

        cf = codec_get_format(cid);
        assert(cf != NULL);

        nlen = strlen(cf->long_name);

        if (slen + nlen >=  out_len) {
                debug_msg("Out of buffer space\n");
                return FALSE;
        }
        memmove(out + nlen + 1, out, slen);
        strncpy(out, cf->long_name, nlen);
        out[nlen] = '/';
        slen += nlen + 1;

        /* Axe trailing separator */
        out[slen-1] = '\0';

        return TRUE;
}

static int
place_unit(media_data *md, coded_unit *cu)
{
        int16_t i;
#ifdef DEBUG_REDUNDANCY
        const codec_format_t *cf;
        cf = codec_get_format(cu->id);
        debug_msg("%d %s\n", md->nrep, cf->long_name);
#endif /* DEBUG_REDUNDANCY */
        assert(md->nrep < MAX_MEDIA_UNITS);

        for (i = 0; i < md->nrep; i++) {
                if (md->rep[i]->id == cu->id) {
                        return FALSE;
                }
        }

        if (md->nrep > 0 && codec_is_native_coding(md->rep[md->nrep - 1]->id)) {
                /* Buffer shifts can mean redundancy is received after primary decoded.  */
                /* Just discard. i.e. pkt 1 (t1 t0) pkt2 (t2 t1), if pkt2 arrives after  */
                /* pkt1 decoded we don't want to append redundant t1 data as it confuses */
                /* decoder.                                                              */
                /* XXX Should check for packet re-ordering i.e. pkt2 arrives and is      */
                /* decoded before pkt 1 then should use pkt 1's data as this will be     */
                /* higher quality under normal circumstances.                            */
                return FALSE;
        }

        md->rep[md->nrep] = cu;
        md->nrep++;
        return TRUE;
}

static media_data *
red_media_data_create_or_get(struct s_pb *p, timestamp_t playout)
{
        struct s_pb_iterator *pi;
        u_char               *md_get;
        media_data           *md;
        uint32_t              md_len, success;
        timestamp_t           md_playout;

        pb_iterator_create(p, &pi);
        /* iterator is attached to sentinel - can move back or forwards */

        while(pb_iterator_retreat(pi)) {
                success = pb_iterator_get_at(pi, &md_get, &md_len, &md_playout);
                md = (media_data*)md_get;
                assert(success);
                if (ts_eq(md_playout, playout)) {
                        goto done;
                } else if (ts_gt(playout, md_playout)) {
                        /* we have gone too far back */
                        break;
                }
        }

        /* Not found in playout buffer */
        media_data_create(&md, 0);
        success = pb_add(p, (u_char*)md, sizeof(media_data), playout);
        assert(success);

done:
        pb_iterator_destroy(p, &pi);
        return md;
}

static void
red_split_unit(u_char  ppt,        /* Primary payload type */
               u_char  bpt,        /* Block payload type   */
               u_char *b,          /* Block pointer        */
               uint32_t blen,       /* Block len            */
               timestamp_t    playout,    /* Block playout time   */
               struct s_pb *out)   /* media buffer         */
{
        const codec_format_t *cf;
        media_data *md;
        codec_id_t  cid, pid;
        coded_unit *cu;
        u_char     *p,*pe;
        timestamp_t        step;

        pid = codec_get_by_payload(ppt);
        if (!pid) {
                debug_msg("Payload not recognized\n");
                return;
        }

        cid = codec_get_by_payload(bpt);
        if (!cid) {
                debug_msg("Payload not recognized\n");
                return;
        }

        if (!codec_audio_formats_compatible(pid, cid)) {
                debug_msg("Primary (%d) and redundant (%d) not compatible\n", ppt, bpt);
                return;
        }

        cf = codec_get_format(cid);
        assert(cf != NULL);
        step = ts_map32(cf->format.sample_rate, codec_get_samples_per_frame(cid));

        p  = b;
        pe = b + blen;
        while(p < pe) {
                cu = (coded_unit*)block_alloc(sizeof(coded_unit));
                cu->id = cid;
                if (p == b && cf->mean_per_packet_state_size) {
                        cu->state_len = cf->mean_per_packet_state_size;
                        cu->state     = block_alloc(cu->state_len);
                        memcpy(cu->state, p, cu->state_len);
                        p            += cu->state_len;
                } else {
                        cu->state     = NULL;
                        cu->state_len = 0;
                }

                cu->data_len = (uint16_t)codec_peek_frame_size(cid, p, (uint16_t)(pe - p));
                cu->data     = block_alloc(cu->data_len);
                memcpy(cu->data, p, cu->data_len);
                p += cu->data_len;
                md = red_media_data_create_or_get(out, playout);
                if (md->nrep == MAX_MEDIA_UNITS) continue;
                if (place_unit(md, cu) == TRUE) {
                        playout = ts_add(playout, step);
                } else {
                        /* unit could not be placed - destroy */
                        if (cu->state_len) {
                                block_free(cu->state, cu->state_len);
                        }
                        block_free(cu->data, cu->data_len);
                        block_free(cu, sizeof(coded_unit));
                }
        }
}

static void
redundancy_decoder_output(channel_unit *chu, struct s_pb *out, timestamp_t playout)
{
        const codec_format_t *cf;
        codec_id_t cid;
        u_char  *hp, *dp, *de, ppt, bpt;
        uint32_t hdr32, blen, boff;
        timestamp_t ts_max_off, ts_blk_off, this_playout;

        hp = dp = chu->data;
        de = chu->data + chu->data_len;

        /* move data pointer past header */
        while (ntohl(*((uint32_t*)dp)) & RED_HDR32_PAT) {
                dp += 4;
        }

        if (dp == hp) {
                debug_msg("Not a redundant block\n");
                return;
        }

        /* At this point dp points to primary payload type.
         * This is a most useful quantity... */
        ppt   = *dp;
        dp += 1;
        assert(dp < de);

        /* Max offset should be in first header.  Want max offset
         * as we nobble timestamps to be:
         *              playout + max_offset - this_offset
         */

        cid   = codec_get_by_payload(ppt);
        if (codec_id_is_valid(cid) == FALSE) {
                debug_msg("Primary not recognized.\n");
                return;
        }

        cf = codec_get_format(cid);
        assert(cf != NULL);

        hdr32 = ntohl(*(uint32_t*)hp);
        ts_max_off = ts_map32(cf->format.sample_rate, RED_HDR32_GET_OFF(hdr32));
	blen = 0;

        while (hdr32 & RED_HDR32_PAT) {
                boff  = RED_HDR32_GET_OFF(hdr32);
                blen  = RED_HDR32_GET_LEN(hdr32);
                bpt   = (u_char)RED_HDR32_GET_PT(hdr32);

                /* Calculate playout point = playout + max_offset - offset */
                ts_blk_off = ts_map32(cf->format.sample_rate, boff);
                this_playout = ts_add(playout, ts_max_off);
                this_playout = ts_sub(this_playout, ts_blk_off);
                hp += 4; /* hdr */
                red_split_unit(ppt, bpt, dp, blen, this_playout, out);
                xmemchk();
                dp += blen;
                hdr32 = ntohl(*(uint32_t*)hp);
        }

        this_playout = ts_add(playout, ts_max_off);
        hp += 1;
        blen = (uint32_t) (de - dp);
        red_split_unit(ppt, ppt, dp, blen, this_playout, out);
        xmemchk();
}

int
redundancy_decoder_decode(u_char       *state,
                           struct s_pb *in,
                           struct s_pb *out,
                           timestamp_t  now)
{
        struct s_pb_iterator *pi;
        u_char               *c_get;
        channel_data         *c;
        uint32_t              clen;
        timestamp_t           cplayout;

        pb_iterator_create(in, &pi);
        assert(pi != NULL);

        assert(state == NULL); /* No decoder state necesssary */
        UNUSED(state);

        while(pb_iterator_get_at(pi, &c_get, &clen, &cplayout)) {
                c = (channel_data*)c_get;
                assert(c != NULL);
                assert(clen == sizeof(channel_data));
                if (ts_gt(cplayout, now)) {
                        break;
                }
                pb_iterator_detach_at(pi, &c_get, &clen, &cplayout);
                c = (channel_data*)c_get;
                assert(c->nelem == 1);
                redundancy_decoder_output(c->elem[0], out, cplayout);
                channel_data_destroy(&c, sizeof(channel_data));
        }

        pb_iterator_destroy(in, &pi);
        return TRUE;
}

