/*
 * FILE:    codec.h
 * AUTHORS: Orion Hodson
 *
 * Copyright (c) 1998-2001 University College London
 * All rights reserved.
 *
 * $Id: codec.h 3642 2004-01-12 17:14:56Z ucacoxh $
 */

#ifndef _CODEC_H_
#define _CODEC_H_

#include "codec_types.h"

/* Codec module startup and end */

void codec_init (void);
void codec_exit (void);

/* Use these two functions to finder number of available codecs
 * and to get an codec id of the num'th codec.
 */
uint32_t    codec_get_number_of_codecs (void);
codec_id_t codec_get_codec_number     (uint32_t num);

/* Use this function to check if codec id is valid / corrupted */

int codec_id_is_valid(codec_id_t id);

/* Use these functions to see what formats a codec supports
 * and whether they are encode from or decode to.
 */

const codec_format_t* codec_get_format        (codec_id_t id);
int                   codec_can_encode_from   (codec_id_t id,
                                               const audio_format *qfmt);
int                   codec_can_encode        (codec_id_t id);
int                   codec_can_decode_to     (codec_id_t id,
                                               const audio_format *qfmt);
int                   codec_can_decode        (codec_id_t id);
int                   codec_audio_formats_compatible(codec_id_t id1,
                                                     codec_id_t id2);

/* This is easily calculable but crops up everywhere */
uint32_t               codec_get_samples_per_frame (codec_id_t id);

/* Codec encoder functions */
int  codec_encoder_create  (codec_id_t id, codec_state **cs);
void codec_encoder_destroy (codec_state **cs);
int  codec_encode          (codec_state* cs,
                            coded_unit*  in_native,
                            coded_unit*  out);

/* Codec decoder functions */
int  codec_decoder_create  (codec_id_t id, codec_state **cs);
void codec_decoder_destroy (codec_state **cs);
int  codec_decode          (codec_state* cs,
                            coded_unit*  in,
                            coded_unit*  out_native);

/* Repair related */

int  codec_decoder_can_repair (codec_id_t id);
int  codec_decoder_repair     (codec_id_t id,
                               codec_state *cs,
                               uint16_t consec_missing,
                               coded_unit *prev,
                               coded_unit *miss,
                               coded_unit *next);

/* Peek function for variable frame size codecs */
uint32_t codec_peek_frame_size(codec_id_t id, u_char *data, uint16_t blk_len);

int     codec_clear_coded_unit(coded_unit *u);

/* RTP payload mapping interface */
int        payload_is_valid     (u_char pt);
int        codec_map_payload    (codec_id_t id, u_char pt);
int        codec_unmap_payload  (codec_id_t id, u_char pt);
u_char     codec_get_payload    (codec_id_t id);
codec_id_t codec_get_by_payload (u_char pt);

/* For compatibility only */
codec_id_t codec_get_first_mapped_with(uint32_t sample_rate, uint16_t channels);

/* Name to codec mappings */
codec_id_t codec_get_by_name      (const char *name);
codec_id_t codec_get_matching     (const char *short_name, uint32_t sample_rate, uint16_t channels);

codec_id_t codec_get_native_coding (uint32_t sample_rate, uint16_t channels);

int        codec_is_native_coding  (codec_id_t id);

int        codec_get_native_info   (codec_id_t cid,
                                    uint32_t *sample_rate,
                                    uint16_t *channels);
/* For layered codecs */
uint8_t     codec_can_layer         (codec_id_t id);
int        codec_get_layer         (codec_id_t id,
                                    coded_unit *cu_whole,
                                    uint8_t layer,
                                    uint16_t *markers,
                                    coded_unit *cu_layer);
int        codec_combine_layer     (codec_id_t id,
                                    coded_unit *cu_layer,
                                    coded_unit *whole,
                                    uint8_t nelem,
                                    uint16_t *markers);


#endif /* _CODEC_H_ */
