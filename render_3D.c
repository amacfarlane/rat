/*
 * FILE:    render_3D.c
 * PROGRAM: RAT
 * AUTHORS: Marcus Iken
 * MODS:    Orion Hodson
 *
 * Copyright (c) 1998-2001 University College London
 * All rights reserved.
 */

#ifndef HIDE_SOURCE_STRINGS
static const char cvsid[] =
	"$Id: render_3D.c 3642 2004-01-12 17:14:56Z ucacoxh $";
#endif /* HIDE_SOURCE_STRINGS */

#include "config_unix.h"
#include "config_win32.h"
#include <math.h>
#include "audio_types.h"
#include "codec_types.h"
#include "memory.h"
#include "util.h"
#include "debug.h"
#include "render_3D.h"

#define MAX_RESPONSE_LENGTH 32
#define MIN_RESPONSE_LENGTH 8
#define DEFAULT_RESPONSE_LENGTH 32
#define LOWER_AZIMUTH -90
#define UPPER_AZIMUTH  90
#define IDENTITY_FILTER 0
/* A guess...*/
#define SAMPLE_BUFFER_SAMPLES 2048

void convolve(sample  *signal,
              sample  *answer,
              double *overlap,
              double *response,
              int response_length,
              int signal_length);

typedef struct s_3d_filter {
        char   name[16];
        double elem[32];
} three_d_filter_t;

static three_d_filter_t base_filters[] = {
        {"Identity",
         { 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
           0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 }
        },
        {"HRTF",
         { 0.063113, -0.107530, 0.315168, 0.015218, -0.300535, 1.000000, 0.359786, -0.601145, -0.676947,
           -0.167251, 0.203305, 0.261645, 0.059649, 0.026661, -0.011648, -0.335958, -0.276208, 0.037719,
           0.154546, 0.141399, -0.000902, -0.031835, -0.098318, -0.058072, -0.033449, 0.030325, 0.041670,
           -0.001182, -0.019692, -0.031318, -0.028427, -0.003031 }
        },
        {"Echo",
         { 0.4, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
           0.4, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
           0.4, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
           0.4, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 }
        }
};

#define NUM_FILTERS (sizeof(base_filters) / sizeof(three_d_filter_t))

int
render_3D_filter_get_count()
{
        return NUM_FILTERS;
}

char *
render_3D_filter_get_name(int id)
{
        if (id >= 0 && id < (signed)NUM_FILTERS) return base_filters[id].name;
        return base_filters[IDENTITY_FILTER].name;
}

int
render_3D_filter_get_by_name(char *name)
{
        int i;
        for(i = 0; i < (signed)NUM_FILTERS; i++) {
                if (!strcasecmp(name, base_filters[i].name)) return i;
        }
        return IDENTITY_FILTER;
}

/* At the present time there is only 1 possible filter length.  At a
 * later date it may be desirable to have shorter filter lengths to
 * reduce processing load and a suitable length selection algorithm.
 */

int
render_3D_filter_get_lengths_count()
{
        return 1;
}

int
render_3D_filter_get_length(int idx)
{
        UNUSED(idx);
        return DEFAULT_RESPONSE_LENGTH;
}

int
render_3D_filter_get_lower_azimuth()
{
        return LOWER_AZIMUTH;
}

int
render_3D_filter_get_upper_azimuth()
{
        return UPPER_AZIMUTH;
}

#define TMPBUFSIZE 132

typedef struct s_render_3D_dbentry {
        u_char   filter_number;                       /* Index number of original filter */
        short    azimuth;                             /* lateral angle of sound source */
        short    delay;                               /* based on interaural time difference (ITD); derived from 'azimuth' */
        double   attenuation;                         /* based on interaural intensity difference (IID); derived from 'azimuth' */
        sample   ipsi_buf[SAMPLE_BUFFER_SAMPLES];     /* buffer for ipsi-lateral channel before merging into stereo buffer */
        sample   contra_buf[SAMPLE_BUFFER_SAMPLES];   /* buffer for contra-lateral channel before merging into stereo buffer */
        sample   tmp_buf[TMPBUFSIZE];                 /* temporary storage for swapping samples */
        sample   excess_buf[TMPBUFSIZE];              /* buffer for excess samples due to delay */
        double   filter[MAX_RESPONSE_LENGTH];         /* filter used for convolution */
        double   overlap_buf[MAX_RESPONSE_LENGTH];    /* overlap buffer due to filter operation on the mono signal */
        int      response_length;
} render_3D_dbentry;

#define UNIQUE_ANGLES 32
/* This function calculates initial azimuth for user */
static int
render_3D_idx2azimuth(int idx)
{
        int delta, r, tick;

        delta = render_3D_filter_get_upper_azimuth() / 2;

        idx  = idx % UNIQUE_ANGLES;
        tick = UNIQUE_ANGLES - 1;
        r    = 0;
        do {
                r += (2 * (idx & 1) - 1) * delta;
                delta >>= 1;
                idx   >>= 1;
                tick  >>= 1;
        } while (tick);
        return r;
}

static int n_users_created;

render_3D_dbentry *
render_3D_init(int sampling_rate)
{
        int               azimuth, length;
        int               default_filter_num;
        char              *default_filter_name;
        render_3D_dbentry *render_3D_data;

        azimuth = render_3D_idx2azimuth(n_users_created);
        length  = DEFAULT_RESPONSE_LENGTH;

        default_filter_name = "HRTF";
        default_filter_num  = render_3D_filter_get_by_name(default_filter_name);

        render_3D_data = (render_3D_dbentry *) xmalloc(sizeof(render_3D_dbentry));
        memset(render_3D_data, 0, sizeof(render_3D_dbentry));

        render_3D_set_parameters(render_3D_data, sampling_rate, azimuth, default_filter_num, length);

#ifdef DEBUG_3D
        {
                int               i;
                fprintf(stdout, "\tdelay:\t%d\n", render_3D_data->delay);
                fprintf(stdout, "\tattenuation:\t%f\n", render_3D_data->attenuation);
                for (i=0; i<length; i++) {
                        fprintf(stdout, "\t%f\n", render_3D_data->filter[i]);
                }
        }
#endif /* DEBUG */

        n_users_created++;

        return render_3D_data;
}

void
render_3D_free(render_3D_dbentry **data)
{
        assert(*data);
        xfree(*data);
        *data = NULL;
}

void
render_3D_set_parameters(struct s_render_3D_dbentry *p_3D_data, int sampling_rate, int azimuth, int filter_number, int length)
{
        int i;
        double aux;
        double d_time;         /* delay in seconds. auxiliary to calculate delay in samples. */
        double d_intensity;    /* interaural intensity difference 0.0 <d_intensity < 1.0 */

        p_3D_data->azimuth = azimuth;

        /* derive interaural time difference from azimuth */
        aux= azimuth * 0.017453203;                                /* conversion into radians */
        d_time = 2.72727 * sin(aux);
        p_3D_data->delay = abs((int) (sampling_rate * d_time / 1000));


        while (p_3D_data->delay >= TMPBUFSIZE) {
                debug_msg("Delay too big for temp bufs reducing %d -> %d\n",
                          p_3D_data->delay,
                          TMPBUFSIZE - 1);
                /* Shift 2 degrees in */
                p_3D_data->azimuth -= 2 * p_3D_data->azimuth/abs(p_3D_data->azimuth);
                aux                 = p_3D_data->azimuth * 0.017453203;
                d_time              = 2.72727 * sin(aux);
                p_3D_data->delay    = abs((int) (sampling_rate * d_time / 1000));
        }

        /* derive interaural intensity difference from azimuth */
        d_intensity = 1.0 - (0.3 * fabs(sin(aux)));
        p_3D_data->attenuation = d_intensity;

        /* fill up participant's response filter */
        p_3D_data->response_length = length;

        assert((unsigned)filter_number < NUM_FILTERS);

        p_3D_data->filter_number = filter_number;

        /* right now it's only a copying of values, later decimation */
        for (i=0; i<MAX_RESPONSE_LENGTH; i++) {
                p_3D_data->filter[i] = base_filters[filter_number].elem[i];
        }
}

void
render_3D_get_parameters(struct s_render_3D_dbentry *p_3D_data, int *azimuth, int *filter_type, int *filter_length)
{
        *azimuth       = p_3D_data->azimuth;
        *filter_type   = p_3D_data->filter_number;
        *filter_length = p_3D_data->response_length;
}

/*=============================================================================================
  convolve()   time-domain, on-the-fly convolution

  Arguments:  signal           pointer to signal vector ('input')
              answer           pointer to answer vector (answer of the system)
              overlap          pointer to the overlap buffer
              response         pointer to coefficients vector (transfer function of the system)
              response_length  number of coefficients
              signal_length    number of values in 'signal'
=============================================================================================*/
void
convolve(sample *signal, sample *answer, double *overlap, double *response, int response_length, int signal_length)
{
        sample  *signal_rptr, *answer_rptr;       /* running pointers within signal and answer vector */
        int     i, j;                             /* loop counters */
        double  *response_rptr;                   /* running pointer within response vector */
        double  *overlap_rptr_1, *overlap_rptr_2; /* running pointer within the overlap buffer */
        double  current;                          /* currently calculated answer value */

        /* Initialise the running pointers for 'signal' and 'answer'. */
        signal_rptr = signal;
        answer_rptr = answer;
        /*  Loop over the length of the signal vector. */
        for(i = 0; i < signal_length ;i++) {
                overlap[response_length-1] = *signal_rptr++;
                response_rptr = response;
                overlap_rptr_1 = overlap_rptr_2 = overlap;
                current = *overlap_rptr_1++ * *response_rptr++;
                /*  Use convolution method for computation */
                for(j = 1; j < response_length ; j++) {
                        *overlap_rptr_2++ = *overlap_rptr_1;
                        current += *overlap_rptr_1++ * *response_rptr++;
                }
                /* Clamping */
                if (current > 32767.0) {
                        debug_msg("clipping %f\n", current);
                        current = 32767.0;
                } else if (current < -32767.0) {
                        debug_msg("clipping %f\n", current);
                        current = -32767.0;
                }
                /* store 'current' in answer vector. */
                *answer_rptr++ = (short)current;
        }
}

/* RAT specific */

#include "codec_types.h"
#include "codec.h"

void
render_3D(render_3D_dbentry *p_3D_data, coded_unit *in, coded_unit *out)
{
        int      i;
        size_t   n_bytes;    /* number of bytes in unspliced (mono!) buffer */
        sample   *proc_buf;
        sample   *mono_raw = NULL, *mono_filtered;  /* auxiliary buffers in case of stereo */
        int      mono_buf_len = 0; /* Mono buffer length in samples */
        uint16_t n_channels;
	uint32_t n_rate;

        assert(codec_is_native_coding(in->id));

        codec_get_native_info(in->id, &n_rate, &n_channels);

        assert(out->state   == NULL);
        assert(out->data    == NULL);
        assert(in->data     != NULL);
        assert(in->data_len != 0);

        /* Filtering operation needs mono buffer,
         * output is always stereo
         */

        assert(n_channels == 1 || n_channels == 2);

        switch(n_channels) {
        case 1:
                mono_buf_len  = in->data_len / sizeof(sample);
                mono_raw      = (sample*)in->data;
                out->id       = codec_get_native_coding(n_rate, 2);
                out->data     = (u_char*)block_alloc(in->data_len * 2);
                out->data_len = in->data_len * 2;
                break;
        case 2:
                mono_buf_len  = in->data_len / (n_channels * sizeof(sample));
                mono_raw      = (sample*)block_alloc(mono_buf_len * sizeof(sample));
                out->id       = in->id;
                out->data     = (u_char*)block_alloc(in->data_len);
                out->data_len = in->data_len;
                /* Convert stereo input to mono input */
                {
                        sample *s, *d;
                        int32_t   tmp,i,j;
                        s  = (sample*)in->data;
                        d  = mono_raw;

                        for(i = j = 0; j < mono_buf_len; j++) {
                                tmp = s[i] + s[i+1];
                                d[j] = (sample)(tmp / 2);
                                i += 2;
                        }
                }
                break;
        }

        proc_buf      = (sample*)out->data;
        mono_filtered = (sample*)block_alloc(mono_buf_len * sizeof(sample));

        /* EXTERNALISATION */
        convolve(mono_raw,
                 mono_filtered,
                 p_3D_data->overlap_buf,
                 p_3D_data->filter,
                 p_3D_data->response_length,
                 mono_buf_len);

        /* LATERALISATION */

        /* mono_filtered is input, and el->native_data[el->native_count-1] is the output (stereo). */
        /* 'n_samples' is number of samples in _stereo_ buffer */
        n_bytes = mono_buf_len * sizeof(sample);

        /* splice into two channels: ipsilateral and contralateral. */
        memcpy(p_3D_data->ipsi_buf,
               mono_filtered,
               n_bytes);
        memcpy(p_3D_data->contra_buf,
               mono_filtered,
               n_bytes);

        /* apply IID to contralateral buffer. */
        for (i=0; i < mono_buf_len; i++) {
                p_3D_data->contra_buf[i] = (short)((double)p_3D_data->contra_buf[i]*p_3D_data->attenuation);
        }

        /* apply ITD to contralateral buffer: delay mechanisam. */
        if (p_3D_data->delay >= mono_buf_len) {
                debug_msg("Delay too big shifting %d -> %d\n",
                          p_3D_data->delay,
                          mono_buf_len);
                p_3D_data->delay = mono_buf_len - 1;
        }

        memcpy(p_3D_data->tmp_buf,
               p_3D_data->contra_buf + mono_buf_len - p_3D_data->delay,
               p_3D_data->delay*sizeof(sample));
        memmove(p_3D_data->contra_buf + p_3D_data->delay,
                p_3D_data->contra_buf,
                (mono_buf_len - p_3D_data->delay) * sizeof(sample));
        memcpy(p_3D_data->contra_buf,
               p_3D_data->excess_buf,
               p_3D_data->delay * sizeof(sample));
        memcpy(p_3D_data->excess_buf,
               p_3D_data->tmp_buf,
               p_3D_data->delay * sizeof(sample));

        /* Merge ipsi- and contralateral buffers into proc_buf. */
        if (p_3D_data->azimuth > 0) {
                for (i=0; i<mono_buf_len; i++) {
                        proc_buf[2*i]   = p_3D_data->ipsi_buf[i];
                        proc_buf[2*i+1] = p_3D_data->contra_buf[i];
                }
        } else if (p_3D_data->azimuth <= 0) {
                for (i=0; i<mono_buf_len; i++) {
                        proc_buf[2*i]   = p_3D_data->contra_buf[i];
                        proc_buf[2*i+1] = p_3D_data->ipsi_buf[i];
                }
        }

        if (mono_raw != (sample*)in->data) {
                block_free(mono_raw, n_bytes);
        }
        block_free(mono_filtered, n_bytes);
        xmemchk();
        block_check((char*)in->data);
        block_check((char*)out->data);
}
