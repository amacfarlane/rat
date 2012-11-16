/*
 * FILE:    codec_types.h
 * PROGRAM: RAT
 * AUTHOR:  Orion Hodson
 *
 * Copyright (c) 1995-2001 University College London
 * All rights reserved.
 *
 * $Id: codec_types.h 3477 2001-01-08 20:30:13Z ucaccsp $
 */

#ifndef _CODEC_TYPES_H_
#define _CODEC_TYPES_H_

#define CODEC_PAYLOAD_DYNAMIC   255

typedef uint32_t codec_id_t;

typedef struct {
        u_char    *state;
        codec_id_t id;
} codec_state;

#define CODEC_SHORT_NAME_LEN   16
#define CODEC_LONG_NAME_LEN    32
#define CODEC_DESCRIPTION_LEN 128

typedef struct s_codec_format {
        char         short_name[CODEC_SHORT_NAME_LEN];
        char         long_name[CODEC_LONG_NAME_LEN];
        char         description[CODEC_DESCRIPTION_LEN];
        u_char       default_pt;
        uint16_t      mean_per_packet_state_size;
        uint16_t      mean_coded_frame_size;
        const audio_format format;
} codec_format_t;

typedef struct s_coded_unit {
        codec_id_t id;
	u_char  *state;
	uint16_t  state_len;
	u_char	*data;
	uint16_t  data_len;
} coded_unit;

#define MAX_MEDIA_UNITS  5
/* This data structure is for storing multiple representations of
 * coded audio for a given time interval.
 */
typedef struct {
        uint8_t      nrep;
        coded_unit *rep[MAX_MEDIA_UNITS];
} media_data;

int  media_data_create    (media_data **m, int nrep);
void media_data_destroy   (media_data **m, uint32_t md_size);
int  media_data_dup       (media_data **dst, media_data *src);

int  coded_unit_dup       (coded_unit *dst, coded_unit *src);

void coded_unit_layer_split (coded_unit *in, coded_unit *out, uint8_t layer, uint8_t *layer_markers);

#endif /* _CODEC_TYPES_H_ */

