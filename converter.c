/*
 * FILE:    converter.c
 * PROGRAM: RAT
 * AUTHOR:  O.Hodson <O.Hodson@cs.ucl.ac.uk>
 *
 * Copyright (c) 1998-2001 University College London
 * All rights reserved.
 */

#ifndef HIDE_SOURCE_STRINGS
static const char cvsid[] =
	"$Id: converter.c 3642 2004-01-12 17:14:56Z ucacoxh $";
#endif /* HIDE_SOURCE_STRINGS */

#include "config_unix.h"
#include "config_win32.h"
#include "memory.h"
#include "util.h"
#include "audio_types.h"
#include "converter_types.h"
#include "converter.h"
#include "convert_util.h"
#include "debug.h"

#define MAGIC 0xface0ff0

typedef struct s_converter {
        int                     idx;
        struct s_converter_fmt *cfmt;
        u_char                 *state;
        uint32_t                state_len;
        uint32_t                magic;
} converter_t;

typedef int  (*cv_startup)     (void);  /* converter specific one time initialization */
typedef void (*cv_shutdown)    (void);  /* converter specific one time cleanup */
typedef int  (*cv_conv_init_f) (const converter_fmt_t *c, u_char **state, uint32_t *state_len);
typedef void (*cv_conv_do_f)   (const converter_fmt_t *c, u_char *state,
                                sample* src_buf, int src_len,
                                sample *dst_buf, int dst_len);
typedef void (*cv_conv_free_f) (u_char **state, uint32_t *state_len);

typedef struct s_pcm_converter{
        converter_details_t details;
        u_char         enabled;
        cv_startup     startf;
        cv_shutdown    shutdownf;
        cv_conv_init_f initf;
        cv_conv_do_f   convertf;
        cv_conv_free_f freef;
} pcm_converter_t;

/* In this table of converters the platform specific converters should go at the
 * beginning, before the default (and worst) linear interpolation conversion.  The
 * intension is to have a mechanism which enables/disables more complex default schemes
 * such as interpolation with filtering, cubic interpolation, etc...
 */

#include "convert_acm.h"
#include "convert_extra.h"
#include "convert_linear.h"
#include "convert_sinc.h"

pcm_converter_t converter_tbl[] = {
#ifdef WIN32
        {
                {0, "Microsoft Converter"},
                FALSE,
                acm_cv_startup,
                acm_cv_shutdown,
                acm_cv_create,
                acm_cv_convert,
                acm_cv_destroy
        },
#endif
        {
                {1, "High Quality"},
                TRUE,
                sinc_startup,
                sinc_shutdown,
                sinc_create,
                sinc_convert,
                sinc_destroy
        },
        {
                {2, "Intermediate Quality"},
                TRUE,
                NULL,
                NULL,
                linear_create,
                linear_convert,
                linear_destroy
        },
        {
                {3, "Low Quality"},
                TRUE,
                NULL,
                NULL,
                extra_create,
                extra_convert,
                extra_destroy
        }
};

#define NUM_CONVERTERS sizeof(converter_tbl)/sizeof(pcm_converter_t)

/* Index to converter_id_t mapping macros */
#define CONVERTER_ID_TO_IDX(x) (((x)>>2) - 17)
#define IDX_TO_CONVERTER_ID(x) ((x+17) << 2)

int
converter_create(const converter_id_t   cid,
                 const converter_fmt_t *cfmt,
                 converter_t          **cvtr)
{
        converter_t *c  = NULL;
        uint32_t      tbl_idx;

        tbl_idx = CONVERTER_ID_TO_IDX(cid);

        if (tbl_idx >= NUM_CONVERTERS) {
                debug_msg("Converter ID invalid\n");
                return FALSE;
        }

        if (cfmt == NULL) {
                debug_msg("No format specified\n");
                return FALSE;
        }

        c  = (converter_t*)xmalloc(sizeof(converter_t));
        if (c == NULL) {
                debug_msg("Could not allocate converter\n");
                return FALSE;
        }

        memset(c, 0, sizeof(converter_t));

        /* Copy format */
        c->cfmt = (converter_fmt_t*)xmalloc(sizeof(converter_fmt_t));
        if (c->cfmt == NULL) {
                converter_destroy(&c);
                return FALSE;
        }
        memcpy(c->cfmt, cfmt, sizeof(converter_fmt_t));
        c->idx = tbl_idx;

        /* Initialize */
        if ((converter_tbl[tbl_idx].initf) &&
            (converter_tbl[tbl_idx].initf(cfmt, &c->state, &c->state_len) == FALSE)) {
		xfree(c->cfmt);
		xfree(c);
		debug_msg("Failed to create converter\n");
                return FALSE;
        }

        c->magic = MAGIC; /* debugging */
        *cvtr = c;

        xmemchk();
        return TRUE;
}

void
converter_destroy(converter_t **cvtr)
{
        converter_t *c = *cvtr;

        if (c == NULL) {
                return;
        }

        assert(c->magic == MAGIC);

        if (converter_tbl[c->idx].freef && c->state != NULL) {
                converter_tbl[c->idx].freef(&c->state, &c->state_len);
        }

        if (c->cfmt) {
                xfree(c->cfmt);
        }

        xfree(c);
        (*cvtr) = NULL;
}

void
converters_init()
{
        uint32_t i = 0;

        for(i = 0; i < NUM_CONVERTERS; i++) {
                if (converter_tbl[i].startf) {
                        converter_tbl[i].enabled = converter_tbl[i].startf();
                }
                converter_tbl[i].details.id = IDX_TO_CONVERTER_ID(i);
        }
}

void
converters_free()
{
        uint32_t i = 0;

        for(i = 0; i < NUM_CONVERTERS; i++) {
                if (converter_tbl[i].shutdownf) {
                        converter_tbl[i].shutdownf();
                }
        }
}

const converter_details_t *
converter_get_details(uint32_t idx)
{
        if (idx < NUM_CONVERTERS) {
                return &converter_tbl[idx].details;
        }
        debug_msg("Getting invalid converter details\n");
        return NULL;
}

uint32_t
converter_get_count()
{
        return NUM_CONVERTERS;
}

#include "codec_types.h"
#include "codec.h"

int
converter_process (converter_t *c, coded_unit *in, coded_unit *out)
{
        converter_fmt_t *cf;
        uint32_t        n_in, n_out;
	uint32_t	ticks_in, ticks_out;

        assert(c->magic == MAGIC);
#ifdef DEBUG
        {
                uint32_t sample_rate;
                uint16_t channels;
                codec_get_native_info(in->id, &sample_rate, &channels);
                assert(sample_rate == c->cfmt->src_freq);
                assert(channels == c->cfmt->src_channels);
        }
#endif /* DEBUG */

        assert(c);
        assert(in->data != NULL);
        assert(in->data_len != 0);

        cf = c->cfmt;

	ticks_in = in->data_len / (sizeof(sample) * cf->src_channels);
	ticks_out = ticks_in * cf->dst_freq / cf->src_freq;

        n_in  = ticks_in  * cf->src_channels;
        n_out = ticks_out * cf->dst_channels;

        assert(converter_format_valid(cf));
        assert(out->state     == NULL);
        assert(out->state_len == 0);
        assert(out->data      == NULL);
        assert(out->data_len  == 0);

        out->id       = codec_get_native_coding(cf->dst_freq, cf->dst_channels);
        out->data_len = sizeof(sample) * n_out;
        out->data     = (u_char*)block_alloc(out->data_len);

        if ((c->cfmt->src_freq     != c->cfmt->dst_freq) ||
            (c->cfmt->src_channels != c->cfmt->dst_channels)) {
                converter_tbl[c->idx].convertf(c->cfmt,
                                               c->state,
                                               (sample*)in->data,
                                               n_in,
                                               (sample*)out->data,
                                               n_out);
        } else {
                /* No conversion is actually necessary */
                debug_msg("No conversion necessary\n");
                memcpy(out->data, in->data, out->data_len);
        }
        assert(c->magic == MAGIC);
        xmemchk();
        return TRUE;
}

const converter_fmt_t*
converter_get_format (converter_t *c)
{
        assert(c != NULL);
        assert(c->magic == MAGIC);
        return c->cfmt;
}





