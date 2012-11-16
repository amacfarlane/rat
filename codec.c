/*
 * FILE:    codec.c
 * AUTHORS: Orion Hodson
 *
 * Copyright (c) 1998-2001 University College London
 * All rights reserved.
 */

#ifndef HIDE_SOURCE_STRINGS
static const char cvsid[] =
	"$Id: codec.c 3642 2004-01-12 17:14:56Z ucacoxh $";
#endif /* HIDE_SOURCE_STRINGS */

#include "config_unix.h"
#include "config_win32.h"
#include "debug.h"

/* These two just for UCL memory debugging fn's */
#include "memory.h"
#include "util.h"
#include "audio_types.h"
#include "codec_types.h"
#include "codec.h"
#include "codec_l8.h"
#include "codec_l16.h"
#include "codec_g711.h"
#include "codec_g726.h"
#include "codec_dvi.h"
#include "codec_gsm.h"
#include "codec_lpc.h"
#include "codec_vdvi.h"
#ifndef REMOVE_WBS_CODEC
#include "codec_wbs.h"
#endif
#include "codec_g728.h"

/* Codec class initialization - hey didn't c++ happen somewhere along the
 * timeline ;-)
 */

/* One time codec {con,de}struction */
typedef void    (*cx_init_f) (void);
typedef void    (*cx_exit_f) (void);

/* Codec Probing functions */
typedef uint16_t               (*cx_get_formats_count_f) (void);
typedef const codec_format_t* (*cx_get_format_f)        (uint16_t);

/* Codec Encoding functions */
typedef int     (*cx_encoder_create_f)    (uint16_t idx, u_char **state);
typedef void    (*cx_encoder_destroy_f)   (uint16_t idx, u_char **state);
typedef int     (*cx_encode_f)            (uint16_t idx, u_char *state,
                                           sample *in, coded_unit *out);
typedef int     (*cx_can_encode_f)        (uint16_t idx);

/* Codec Decoding functions */
typedef int     (*cx_decoder_create_f)    (uint16_t idx, u_char **state);
typedef void    (*cx_decoder_destroy_f)   (uint16_t idx, u_char **state);
typedef int     (*cx_decode_f)            (uint16_t idx, u_char *state,
                                           coded_unit *in, sample *out);
typedef int     (*cx_can_decode_f)        (uint16_t idx);

/* For determining frame sizes of variable bit rate codecs */
typedef int     (*cx_peek_size_f)         (uint16_t idx, u_char *data, int data_len);

/* For codec domain repair schemes */
typedef int     (*cx_repair_f)            (uint16_t idx,
                                           u_char *state,
                                           uint16_t consec_missing,
                                           coded_unit *prev,
                                           coded_unit *missing,
                                           coded_unit *next);

/* For layered codecs */
typedef uint8_t		(*cx_can_layer_f)	  (void);
typedef int             (*cx_get_layer_f)         (uint16_t idx, coded_unit *cu_whole, uint8_t layer,
                                                   uint16_t *markers, coded_unit *cu_layer);
typedef int             (*cx_combine_layer_f)     (uint16_t idx, coded_unit *cu_layer, coded_unit *cu_whole, uint8_t nelem, uint16_t *markers);

typedef struct s_codec_fns {
        cx_init_f               cx_init;
        cx_exit_f               cx_exit;
        cx_get_formats_count_f  cx_get_formats_count;
        cx_get_format_f         cx_get_format;
        cx_encoder_create_f     cx_encoder_create;
        cx_encoder_destroy_f    cx_encoder_destroy;
        cx_encode_f             cx_encode;
        cx_can_encode_f         cx_can_encode;
        cx_decoder_create_f     cx_decoder_create;
        cx_decoder_destroy_f    cx_decoder_destroy;
        cx_decode_f             cx_decode;
        cx_can_decode_f         cx_can_decode;
        cx_peek_size_f          cx_peek_size;
        cx_repair_f             cx_repair;
	cx_can_layer_f	        cx_can_layer;
        cx_get_layer_f          cx_get_layer;
        cx_combine_layer_f      cx_combine_layer;
} codec_fns_t;

static codec_fns_t codec_table[] = {
	{
                NULL,
                NULL,
                l16_get_formats_count,
                l16_get_format,
                NULL, /* No encoder setup / tear down */
                NULL,
                l16_encode,
                NULL,
                NULL,
                NULL, /* No decoder setup / tear down */
                l16_decode,
                NULL,
                NULL,
                NULL,
                NULL,
                NULL,
                NULL
        },
        {
                NULL,
                NULL,
                l8_get_formats_count,
                l8_get_format,
                NULL, /* No encoder setup / tear down */
                NULL,
                l8_encode,
                NULL,
                NULL,
                NULL, /* No decoder setup / tear down */
                l8_decode,
                NULL,
                NULL,
                NULL,
                NULL,
                NULL,
                NULL
        },
        {       g711_init,
                NULL,
                g711_get_formats_count,
                g711_get_format,
                NULL,
                NULL,
                g711_encode,
                NULL,
                NULL,
                NULL,
                g711_decode,
                NULL,
                NULL,
                NULL,
                NULL,
                NULL,
                NULL
        },
#ifdef HAVE_G728
        {
                NULL,
                NULL,
                g728_get_formats_count,
                g728_get_format,
                g728_encoder_create,
                g728_encoder_destroy,
                g728_encoder_do,
                NULL,
                g728_decoder_create,
                g728_decoder_destroy,
                g728_decoder_do,
                NULL,
                NULL,
                NULL,
                NULL,
                NULL,
                NULL
        },
#endif /* HAVE_G728 */
        {
                NULL,
                NULL,
                g726_get_formats_count,
                g726_get_format,
                g726_state_create,
                g726_state_destroy,
                g726_encode,
                NULL,
                g726_state_create,
                g726_state_destroy,
                g726_decode,
                NULL,
                NULL,
                NULL,
                NULL,
                NULL,
                NULL
        },
        {
                NULL,
                NULL,
                dvi_get_formats_count,
                dvi_get_format,
                dvi_state_create,
                dvi_state_destroy,
                dvi_encode,
                NULL,
                dvi_state_create,
                dvi_state_destroy,
                dvi_decode,
                NULL,
                NULL,
                NULL,
                NULL,
                NULL,
                NULL
        },
        {
                NULL,
                NULL,
                vdvi_get_formats_count,
                vdvi_get_format,
                vdvi_state_create,
                vdvi_state_destroy,
                vdvi_encoder,
                NULL,
                vdvi_state_create,
                vdvi_state_destroy,
                vdvi_decoder,
                NULL,
                vdvi_peek_frame_size,
                NULL,
                NULL,
                NULL,
                NULL
        },
#ifndef REMOVE_WBS_CODEC
	/* The WB-ADPCM algorithm was developed by British Telecommunications */
	/* plc.  Permission has been granted to use it for non-commercial     */
	/* research and development projects.  BT retain the intellectual     */
	/* property rights to this algorithm.                                 */
        {
                NULL,
                NULL,
                wbs_get_formats_count,
                wbs_get_format,
                wbs_state_create,
                wbs_state_destroy,
                wbs_encoder,
                NULL,
                wbs_state_create,
                wbs_state_destroy,
                wbs_decoder,
                NULL,
                NULL,
                NULL,
                wbs_max_layers,
                wbs_get_layer,
                wbs_combine_layer
        },
#endif
        {
                NULL,
                NULL,
                gsm_get_formats_count,
                gsm_get_format,
                gsm_state_create,
                gsm_state_destroy,
                gsm_encoder,
                NULL,
                gsm_state_create,
                gsm_state_destroy,
                gsm_decoder,
                NULL,
                NULL,
                gsm_repair,
                NULL,
                NULL,
                NULL
        },
        {
                lpc_setup,
                NULL,
                lpc_get_formats_count,
                lpc_get_format,
                lpc_encoder_state_create,
                lpc_encoder_state_destroy,
                lpc_encoder,
                NULL,
                lpc_decoder_state_create,
                lpc_decoder_state_destroy,
                lpc_decoder,
                NULL,
                NULL,
                lpc_repair,
                NULL,
                NULL,
                NULL
        }
};

/* NUM_CODEC_INTERFACES = number of codec interfaces */
/* Applications never know this...                   */
#define NUM_CODEC_INTERFACES (sizeof(codec_table)/sizeof(codec_fns_t))

/* These are used to save multiple function calls in
 * codec_get_codec_number function. */
static uint16_t num_fmts_supported[NUM_CODEC_INTERFACES];
static uint16_t total_fmts_supported;

/* Codec identifier is 32 bits long. It's like an MS handle.
 * First byte is always zero.
 * Second byte is index in codec_table above plus 1.
 * Third and fourth bytes hold index for encoding format
 * used by codec.
 */

#define CODEC_GET_IFS_INDEX(id)     (uint16_t)(((id  & 0x00ff0000) >> 16) - 1)
#define CODEC_GET_FMT_INDEX(id)     (uint16_t)((id & 0x0000ffff) - 1)
#define CODEC_MAKE_ID(ifs,fmt)      (((ifs) + 1) << 16)|((fmt+1)&0xffff)

#define CODEC_VALID_PAD(id)          (!(id & 0xff000000))
#define CODEC_VALID_IFS(id)           (id & 0x00ff0000)
#define CODEC_VALID_FMT(id)           (id & 0x0000ffff)

int
codec_id_is_valid(codec_id_t id)
{
        uint32_t ifs, fmt;

        if (codec_is_native_coding(id)) {
                /* Native codings should be tested with
                 * codec_is_native_coding */
                debug_msg("Coding is invalid because it is a native coding\n");
                return FALSE;
        }

        if (!CODEC_VALID_PAD(id) ||
            !CODEC_VALID_IFS(id) ||
            !CODEC_VALID_FMT(id)) {
                debug_msg("Codec id (0x%08x) invalid (pad %x, ifs %x, fmt %x)",
                          id, CODEC_VALID_PAD(id), CODEC_VALID_IFS(id),
                          CODEC_VALID_FMT(id));
                return FALSE;
        }

        ifs = CODEC_GET_IFS_INDEX(id);
        fmt = CODEC_GET_FMT_INDEX(id);

        if (ifs >= NUM_CODEC_INTERFACES) {
                /* Table index too large */
                debug_msg("Codec index outside table\n");
                return FALSE;
        }

        if (fmt >= num_fmts_supported[ifs]) {
                /* Format index too large */
                debug_msg("Format index outside table %d / %d\n", fmt, num_fmts_supported[ifs]);
                return FALSE;
        }
        return TRUE;
}

static void codec_map_init(void);
static void codec_map_exit(void);

void
codec_init()
{
        const codec_format_t *cf;
        uint32_t i;
        uint16_t j;

        if (total_fmts_supported == 0) {
                for(i = 0; i < NUM_CODEC_INTERFACES; i++) {
                        if (codec_table[i].cx_init) codec_table[i].cx_init();
                        num_fmts_supported[i] = codec_table[i].cx_get_formats_count();
                        total_fmts_supported += num_fmts_supported[i];
                        for(j = 0; j < num_fmts_supported[i]; j++) {
                                cf = codec_table[i].cx_get_format(j);
                                /* Most compilers should spot this check anyway */
                                assert(strlen(cf->short_name) < CODEC_SHORT_NAME_LEN);
                                assert(strlen(cf->long_name) < CODEC_LONG_NAME_LEN);
                                assert(strlen(cf->description) < CODEC_DESCRIPTION_LEN);
                        }
                }
                codec_map_init();
        } else {
                debug_msg("codec_init already called - ignoring.\n");
        }
}

void
codec_exit()
{
        uint32_t i;
        if (total_fmts_supported != 0) {
                for(i = 0; i < NUM_CODEC_INTERFACES; i++) {
                        if (codec_table[i].cx_exit) codec_table[i].cx_exit();
                }
                codec_map_exit();
                total_fmts_supported = 0;
        } else {
                debug_msg("codec_exit not inited - ignoring\n");
        }
}

uint32_t
codec_get_number_of_codecs()
{
        return total_fmts_supported;
}

codec_id_t
codec_get_codec_number(uint32_t n)
{
        codec_id_t id;
        uint32_t    ifs;
        assert(n < total_fmts_supported);

        for(ifs = 0; n >= num_fmts_supported[ifs]; ifs++) {
                n = n - num_fmts_supported[ifs];
        }

        id = CODEC_MAKE_ID(ifs, n);

        assert(codec_id_is_valid(id));

        return id;
}

const codec_format_t*
codec_get_format(codec_id_t id)
{
        uint16_t ifs, fmt;

        assert(codec_id_is_valid(id));

        ifs = CODEC_GET_IFS_INDEX(id);
        fmt = CODEC_GET_FMT_INDEX(id);
        return codec_table[ifs].cx_get_format(fmt);
}

int
codec_can_encode(codec_id_t id)
{
        uint16_t ifs;

        ifs = CODEC_GET_IFS_INDEX(id);

        assert(codec_id_is_valid(id));
        assert(ifs < NUM_CODEC_INTERFACES);

        if (codec_table[ifs].cx_can_encode) {
                /* cx_can_encode only needs to exist if encoder and decoder are asymmetric */
                return codec_table[ifs].cx_can_encode(CODEC_GET_FMT_INDEX(id));
        } else {
                const codec_format_t *cf;
                cf = codec_get_format(id);
                if (cf->format.sample_rate % 8000) {
                        return FALSE; /* only m * 8k at the moment */
                }
        }

        return TRUE;
}

int
codec_can_decode(codec_id_t id)
{
        uint32_t ifs;

        ifs = CODEC_GET_IFS_INDEX(id);

        assert(codec_id_is_valid(id));
        assert(ifs < NUM_CODEC_INTERFACES);

        if (codec_table[ifs].cx_can_decode) {
                /* cx_can_encode only needs to exist if encoder and decoder are asymmetric */
                return codec_table[ifs].cx_can_decode(CODEC_GET_FMT_INDEX(id));
        }  else {
                const codec_format_t *cf;
                cf = codec_get_format(id);
                if (cf->format.sample_rate % 8000) {
                        return FALSE; /* Only m * 8k at moment */
                }
        }

        return TRUE;
}

int
codec_audio_formats_compatible(codec_id_t id1, codec_id_t id2)
{
        const codec_format_t *cf1, *cf2;
        int match;

        assert(codec_id_is_valid(id1));
        assert(codec_id_is_valid(id2));

        cf1 = codec_get_format(id1);
        cf2 = codec_get_format(id2);

        match = !memcmp(&cf1->format, &cf2->format, sizeof(audio_format));

        return match;
}

uint32_t
codec_get_samples_per_frame(codec_id_t id)
{
        const codec_format_t *cf;
        uint32_t spf;

        assert(codec_id_is_valid(id));
        cf = codec_get_format(id);
        spf = cf->format.bytes_per_block * 8 /
                (cf->format.channels * cf->format.bits_per_sample);

        return spf;
}

/* Encoder related ***********************************************************/
int
codec_encoder_create(codec_id_t id, codec_state **cs)
{
        if (codec_id_is_valid(id)) {
                uint16_t ifs, fmt;
                *cs = (codec_state*)block_alloc(sizeof(codec_state));
                if (!cs) {
                        *cs = NULL;
                        return 0;
                }
                (*cs)->state = NULL;
                (*cs)->id = id;
                ifs = CODEC_GET_IFS_INDEX(id);
                fmt = CODEC_GET_FMT_INDEX(id);
                if (codec_table[ifs].cx_encoder_create) {
                        /* Must also have a destructor */
                        assert(codec_table[ifs].cx_encoder_destroy != NULL);
                        codec_table[ifs].cx_encoder_create(fmt,
                                                           &(*cs)->state);
                }
                return TRUE;
        } else {
                debug_msg("Attempting to initiate invalid codec\n");
                abort();
        }
        return 0;
}

void
codec_encoder_destroy(codec_state **cs)
{
        codec_id_t id;
        assert(*cs != NULL);
        id = (*cs)->id;
        if (codec_id_is_valid(id)) {
                uint16_t ifs, fmt;
                ifs = CODEC_GET_IFS_INDEX(id);
                fmt = CODEC_GET_FMT_INDEX(id);
                if (codec_table[ifs].cx_encoder_destroy) {
                        /* Must also have a destructor */
                        codec_table[ifs].cx_encoder_destroy(fmt,
                                                               &(*cs)->state);
                }
                block_free(*cs, sizeof(codec_state));
                *cs = NULL;
        } else {
                debug_msg("Destroying corrupted codec\n");
                abort();
        }
}

int
codec_encode(codec_state *cs,
             coded_unit  *in_native,
             coded_unit  *cu)
{
        uint16_t    ifs, fmt;
        int        success;

        assert(cs        != NULL);
        assert(in_native != NULL);
        assert(cu        != NULL);

        assert(codec_is_native_coding(in_native->id));
        assert (in_native->state == NULL);
#ifdef DEBUG
        {
                const codec_format_t *cf = codec_get_format(cs->id);
                assert (cf->format.bytes_per_block == in_native->data_len);
        }
#endif
        cu->id = cs->id;
        ifs = CODEC_GET_IFS_INDEX(cu->id);
        fmt = CODEC_GET_FMT_INDEX(cu->id);

        xmemchk();
        success = codec_table[ifs].cx_encode(fmt, cs->state, (sample*)in_native->data, cu);
        xmemchk();

        return success;
}

/* Decoder related ***********************************************************/
int
codec_decoder_create(codec_id_t id, codec_state **cs)
{
        if (codec_id_is_valid(id)) {
                uint16_t ifs, fmt;
                *cs = (codec_state*)block_alloc(sizeof(codec_state));
                if (!cs) {
                        *cs = NULL;
                        return 0;
                }
                (*cs)->state = NULL;
                (*cs)->id = id;
                ifs = CODEC_GET_IFS_INDEX(id);
                fmt = CODEC_GET_FMT_INDEX(id);
                if (codec_table[ifs].cx_decoder_create) {
                        /* Must also have a destructor */
                        assert(codec_table[ifs].cx_decoder_destroy != NULL);
                        codec_table[ifs].cx_decoder_create(fmt,
                                                               &(*cs)->state);
                }
                return TRUE;
        } else {
                debug_msg("Attempting to initiate invalid codec\n");
                abort();
        }
        return 0;
}

void
codec_decoder_destroy(codec_state **cs)
{
        codec_id_t id;
        assert(*cs != NULL);
        id = (*cs)->id;
        if (codec_id_is_valid(id)) {
                uint16_t ifs, fmt;
                ifs = CODEC_GET_IFS_INDEX(id);
                fmt = CODEC_GET_FMT_INDEX(id);
                if (codec_table[ifs].cx_decoder_destroy) {
                        /* Must also have a destructor */
                        codec_table[ifs].cx_decoder_destroy(fmt,
                                                               &(*cs)->state);
                }
                block_free(*cs, sizeof(codec_state));
                *cs = NULL;
        } else {
                debug_msg("Destroying corrupted codec\n");
                abort();
        }
}

int
codec_decode(codec_state *cs,
             coded_unit  *in,
             coded_unit  *out)
{
        const codec_format_t *cf;
        codec_id_t           id;
        uint16_t              ifs, fmt, rate, channels;
        int                  success;

        assert(cs  != NULL);
        assert(out != NULL);
        assert(in  != NULL);

        id = cs->id;
        assert(in->id == cs->id);
        assert(codec_is_native_coding(in->id) == FALSE);

        ifs = CODEC_GET_IFS_INDEX(id);
        fmt = CODEC_GET_FMT_INDEX(id);

        /* Setup outgoing data block */
        cf = codec_get_format(id);
        assert(out->state == NULL);
        assert(out->data  == NULL);
        rate     = (uint32_t)cf->format.sample_rate;
        channels = (uint16_t)cf->format.channels;
        out->id       = codec_get_native_coding(rate, channels);
        out->data_len = cf->format.bytes_per_block;
        out->data     = (u_char*)block_alloc(out->data_len);

        /* Decode */
        xmemchk();
        success = codec_table[ifs].cx_decode(fmt, cs->state, in, (sample*)out->data);
        xmemchk();

        return success;
}

int
codec_decoder_can_repair (codec_id_t id)
{
        uint16_t ifs;
        assert(codec_id_is_valid(id));
        ifs = CODEC_GET_IFS_INDEX(id);
        if (codec_table[ifs].cx_repair) {
                return TRUE;
        } else {
                return FALSE;
        }
}

int
codec_decoder_repair(codec_id_t id, codec_state *cs,
                     uint16_t consec_missing,
                     coded_unit *prev,
                     coded_unit *miss,
                     coded_unit *next)
{
        uint16_t    ifs, fmt;

        assert(codec_id_is_valid(id));
        assert(id == cs->id);

        ifs = CODEC_GET_IFS_INDEX(id);
        fmt = CODEC_GET_FMT_INDEX(id);

        if (miss->id != id) {
                debug_msg("Wrong previous unit supplied for repair.  Probably a transition in stream.\n");
                return FALSE;
        }
        miss->id = prev->id;

        assert(codec_table[ifs].cx_repair != NULL);
        return codec_table[ifs].cx_repair(fmt,
                                          cs->state,
                                          consec_missing,
                                          prev, miss, next);
}

uint32_t
codec_peek_frame_size(codec_id_t id, u_char *data, uint16_t blk_len)
{
        uint16_t    ifs, fmt;

        assert(codec_id_is_valid(id));

        ifs = CODEC_GET_IFS_INDEX(id);
        fmt = CODEC_GET_FMT_INDEX(id);

        if (codec_table[ifs].cx_peek_size) {
                return codec_table[ifs].cx_peek_size(fmt, data, (int)blk_len);
        } else {
                const codec_format_t *cf = codec_get_format(id);
                return cf->mean_coded_frame_size;
        }
}

/* Codec clear coded unit - not sure where this should go - away? :-) */
int
codec_clear_coded_unit(coded_unit *u)
{
        if (u->state_len) {
                assert(u->state != NULL);
                block_free(u->state, u->state_len);
        }
        if (u->data_len) {
                assert(u->data != NULL);
                block_free(u->data, u->data_len);
        }
        memset(u, 0, sizeof(coded_unit));
        return TRUE;
}

/* RTP related things like mapping and payload sanity checking ***************/

int
payload_is_valid(u_char pt)
{
        /* Per rfc1890.txt (actually 72-95 is unassigned, but we use it anyway */
        if (pt < 29 || (pt>=72 && pt <=127)) return TRUE;
        return FALSE;
}

/* RTP Mapping interface -
 * 2 maps one from payload to codec id and the other from codec id to
 * to payload.
 */

#define NUM_PAYLOADS 128
static codec_id_t payload_map[NUM_PAYLOADS];
static u_char    *codec_map[NUM_CODEC_INTERFACES];

static void
codec_map_init()
{
        uint16_t i,j;
        const codec_format_t *fmt;

        memset(payload_map, 0, NUM_PAYLOADS * sizeof(codec_id_t));
        for(i = 0; i < NUM_CODEC_INTERFACES; i++) {
                codec_map[i] = (u_char*)xmalloc(num_fmts_supported[i]);
                memset(codec_map[i], CODEC_PAYLOAD_DYNAMIC, num_fmts_supported[i]);
                for(j = 0; j < num_fmts_supported[i]; j++) {
                        fmt = codec_table[i].cx_get_format(j);
                        if (fmt->default_pt == CODEC_PAYLOAD_DYNAMIC) {
                                continue;
                        }
                        codec_map_payload(CODEC_MAKE_ID(i,j), fmt->default_pt);
                }
        }
}

static void
codec_map_exit()
{
        uint32_t i;
        for(i = 0; i < NUM_CODEC_INTERFACES; i++) {
                xfree(codec_map[i]);
        }
}

int
codec_map_payload(codec_id_t id, u_char pt)
{
        if (payload_is_valid(pt) && codec_id_is_valid(id)) {
                if (payload_map[pt] != 0) {
                        codec_unmap_payload(id, pt);
                }
                payload_map[pt] = id;
                codec_map[CODEC_GET_IFS_INDEX(id)][CODEC_GET_FMT_INDEX(id)] = pt;
                return TRUE;
        }
#ifdef DEBUG
        {
                const codec_format_t *cf;
                cf = codec_get_format(id);
                debug_msg("Failed to map payload for %s\n", cf->long_name);
        }
#endif /* DEBUG */
        return FALSE;
}

u_char
codec_get_payload(codec_id_t id)
{
        u_char pt;

        assert(codec_id_is_valid(id));
        pt = codec_map[CODEC_GET_IFS_INDEX(id)][CODEC_GET_FMT_INDEX(id)];
        if (payload_is_valid(pt)) {
                assert(codec_get_by_payload(pt) == id);
                return pt;
        }
        return CODEC_PAYLOAD_DYNAMIC;
}

int
codec_unmap_payload(codec_id_t id, u_char pt)
{
        if (payload_is_valid(pt) &&
            codec_id_is_valid(id) &&
            payload_map[pt] == id) {
                payload_map[pt] = 0;
                codec_map[CODEC_GET_IFS_INDEX(id)][CODEC_GET_FMT_INDEX(id)] = CODEC_PAYLOAD_DYNAMIC;
                return TRUE;
        }
        debug_msg("Failed to unmap payload\n");
        return FALSE;
}

codec_id_t
codec_get_by_payload (u_char pt)
{
        if (payload_is_valid(pt)) {
#ifdef DEBUG
                if (payload_map[pt] == 0) {
                        debug_msg("No codec for payload %d\n", pt);
                }
#endif
                return payload_map[pt];
        } else {
                debug_msg("codec_get_by_payload - invalid payload (%d)\n", pt);
                return 0;
        }
}

/* For compatibility only */
codec_id_t
codec_get_first_mapped_with(uint32_t sample_rate, uint16_t channels)
{
        const codec_format_t *cf;
        int pt;

        for(pt = 0; pt < NUM_PAYLOADS; pt++) {
                if (payload_map[pt]) {
                        cf = codec_get_format(payload_map[pt]);
                        if (cf->format.sample_rate == sample_rate &&
                            cf->format.channels    == channels) {
                                return payload_map[pt];
                        }
                }
        }
        debug_msg("No mapped codecs compatible (%d, %d)\n",
                  sample_rate, channels);
        return 0;
}


codec_id_t
codec_get_by_name(const char *name)
{
        const codec_format_t *cf;
        uint16_t ifs, fmt;

        for(ifs = 0; ifs < NUM_CODEC_INTERFACES; ifs++) {
                for(fmt = 0; fmt < num_fmts_supported[ifs]; fmt++) {
                        cf = codec_table[ifs].cx_get_format(fmt);
                        if (!strcasecmp(cf->long_name, name)) {
                                return CODEC_MAKE_ID(ifs,fmt);
                        }
                }
        }

        return 0;
}


codec_id_t
codec_get_matching(const char *short_name, uint32_t freq, uint16_t channels)
{
        /* This has been changed to try really hard to find a matching codec.
         * The reason is that it's now called as part of the command-line
         * parsing, and so has to cope with user entered codec names. Also, it
         * should recognise the names sdr gives the codecs, for compatibility
         * with rat-v3.0.                                                [csp]
         */

        /* This is not quite as inefficient as it looks, since stage 1 will
         * almost always find a match.
         */

        const codec_format_t  *cf = NULL;
        codec_id_t             cid = 0;
        uint32_t                i, codecs;
        char                  *long_name;

        /* Stage 1: Try the designated short names... */
        codecs = codec_get_number_of_codecs();
        for(i = 0; i < codecs; i++) {
                cid = codec_get_codec_number(i);
                cf  = codec_get_format(cid);
                if (cf->format.sample_rate == freq  &&
                    cf->format.channels == channels &&
                    !strcasecmp(short_name, cf->short_name)) {
                        return cid;
                }
        }

        /* Stage 2: Try to generate a matching name... */
        long_name = (char *) xmalloc(strlen(short_name) + 12);
        sprintf(long_name, "%s-%dK-%s", short_name, freq/1000, channels==1?"MONO":"STEREO");
        for(i = 0; i < codecs; i++) {
                cid = codec_get_codec_number(i);
                cf  = codec_get_format(cid);
                if (cf->format.sample_rate == freq  &&
                    cf->format.channels == channels &&
                    !strcasecmp(long_name, cf->long_name)) {
                        xfree(long_name);
                        return cid;
                }
        }

        /* Stage 3: Nasty hack... PCM->PCMU for compatibility with sdr
         * and old rat versions
         */
        if (strncasecmp(short_name, "pcm", 3) == 0) {
                sprintf(long_name, "PCMU-%dK-%s", freq/1000, channels==1?"MONO":"STEREO");
                for(i = 0; i < codecs; i++) {
                        cid = codec_get_codec_number(i);
                        cf  = codec_get_format(cid);
                        if (cf->format.sample_rate == freq  &&
                            cf->format.channels == channels &&
                            !strcasecmp(long_name, cf->long_name)) {
                                xfree(long_name);
                                return cid;
                        }
                }
        }

        xfree(long_name);

        debug_msg("Unable to find codec \"%s\" at rate %d channels %d\n", short_name, freq, channels);
        return 0;
}


/* These constants are what are supported as native codings */

static uint16_t sampling_rates[] = {8000, 11025, 16000, 22050, 32000, 44100, 48000};
static uint16_t num_sampling_rates = sizeof(sampling_rates)/sizeof(sampling_rates[0]);
static uint16_t max_channels = 2;

/* The following three functions are a hack so we can have encode and
 * decode functions take coded_units as input and output.  This makes
 * paths cleaner since we don't have two data types for coded and raw
 * units.  */

codec_id_t
codec_get_native_coding(uint32_t sample_rate, uint16_t channels)
{
        codec_id_t cid;
        uint32_t i, index;

        for (i = 0; i < num_sampling_rates; i++) {
                if (sampling_rates[i] == sample_rate) {
                        break;
                }
        }
        assert(i != num_sampling_rates);
        assert(channels <= max_channels);
        index = i * max_channels + (channels - 1);
        /* There is no codec corresponding to this but make it
         * have right form we set it interfaces to the number
         * of interfaces, i.e. one more than is legal.
         */
        cid = CODEC_MAKE_ID(NUM_CODEC_INTERFACES, index);
        return cid;
}

int
codec_is_native_coding(codec_id_t cid)
{
        return (CODEC_GET_IFS_INDEX(cid) == NUM_CODEC_INTERFACES &&
                CODEC_GET_FMT_INDEX(cid) < num_sampling_rates * max_channels);
}

int
codec_get_native_info(codec_id_t cid,
                      uint32_t   *p_rate,
                      uint16_t   *p_channels)
{
        uint32_t i, c, index;

        if (codec_is_native_coding(cid)) {
                index = CODEC_GET_FMT_INDEX(cid);
                /* Calculate and verify index in table of acceptable rates */
                i = index / max_channels;
                if (p_rate != NULL) {
                        *p_rate = sampling_rates[i];
                }
                /* Calculate and verify number of channels */
                c = (index % max_channels) + 1;
                if (p_channels != NULL) {
                        *p_channels = (uint16_t)c;
                }
                return TRUE;
        }
        return FALSE;
}

/* Layered codecs ***********************************************************/
uint8_t
codec_can_layer(codec_id_t id)
{
        uint16_t ifs;

        ifs = CODEC_GET_IFS_INDEX(id);

        assert(codec_id_is_valid(id));
        assert(ifs < NUM_CODEC_INTERFACES);

        if (codec_table[ifs].cx_can_layer) {
                return codec_table[ifs].cx_can_layer();
        }
        return 1;

}

int
codec_get_layer(codec_id_t id, coded_unit *cu_whole, uint8_t layer, uint16_t *markers, coded_unit *cu_layer)
{
        uint16_t ifs, fmt;

        ifs = CODEC_GET_IFS_INDEX(id);
        fmt = CODEC_GET_FMT_INDEX(id);

        if (codec_table[ifs].cx_get_layer) {
                return codec_table[ifs].cx_get_layer(fmt, cu_whole, layer, markers, cu_layer);
        }
        return FALSE;
}

int
codec_combine_layer (codec_id_t id, coded_unit *cu_layer, coded_unit *cu_whole, uint8_t nelem, uint16_t *markers)
{
        uint16_t ifs, fmt;

        ifs = CODEC_GET_IFS_INDEX(id);
        fmt = CODEC_GET_FMT_INDEX(id);

        assert(codec_table[ifs].cx_combine_layer);
        return codec_table[ifs].cx_combine_layer(fmt, cu_layer, cu_whole, nelem, markers);
}
