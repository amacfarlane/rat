/*
 * FILE:      channel.h
 * AUTHOR(S): Orion Hodson
 *
 *
 * Copyright (c) 1999-2001 University College London
 * All rights reserved.
 *
 * $Id: channel.h 3642 2004-01-12 17:14:56Z ucacoxh $
 */

#ifndef   __NEW_CHANNEL_H__
#define   __NEW_CHANNEL_H__

#include "playout.h"
#include "channel_types.h"

struct s_channel_state;

/* Channel coder query functions:
 * channel_get_coder_count returns number of available channel coders,
 * and channel_get_coder_details copies details of idx'th coder into ccd.
 */

uint32_t             channel_get_coder_count   (void);
const cc_details_t* channel_get_coder_details (uint32_t idx);
const cc_details_t* channel_get_null_coder    (void);

/* channel_get_coder_identity fills coder name and descriptor into ccd */
const cc_details_t* channel_get_coder_identity(struct s_channel_state *cs);

/* Don't use these two functions directly use macros channel_encoder_{create, destory, reset},
 * and channel_encoder_{create, destory, reset} instead.
 */

int       _channel_coder_create      (cc_id_t id, struct s_channel_state **cs, int is_encoder);
void      _channel_coder_destroy     (struct s_channel_state **cs, int is_encoder);
int       _channel_coder_reset       (struct s_channel_state *cs,  int is_encoder);

/* Encoder specifics *********************************************************/

#define   channel_encoder_create(id, cs)  _channel_coder_create  (id, cs, TRUE)
#define   channel_encoder_destroy(cs)     _channel_coder_destroy (cs, TRUE)
#define   channel_encoder_reset(cs)       _channel_coder_reset   (cs, TRUE)

int       channel_encoder_set_units_per_packet (struct s_channel_state *cs, uint16_t);
uint16_t   channel_encoder_get_units_per_packet (struct s_channel_state *cs);

int       channel_encoder_set_parameters (struct s_channel_state *cs, char *cmd);
int       channel_encoder_get_parameters (struct s_channel_state *cs, char *cmd, int cmd_len);

int       channel_encoder_encode (struct s_channel_state  *cs,
                                  struct s_pb *media_buffer,
                                  struct s_pb *channel_buffer);

/* Decoder specifics *********************************************************/
#define   channel_decoder_create(id, cs)  _channel_coder_create  (id, cs, FALSE)
#define   channel_decoder_destroy(cs)     _channel_coder_destroy (cs, FALSE)
#define   channel_decoder_reset(cs)       _channel_coder_reset   (cs, FALSE)

int       channel_decoder_decode (struct s_channel_state  *cs,
                                  struct s_pb *channel_buffer,
                                  struct s_pb *media_buffer,
                                  timestamp_t                     now);

int       channel_decoder_matches (cc_id_t                 cid,
                                   struct s_channel_state *cs);

int       channel_get_compatible_codec (uint8_t  pt,
                                        u_char *data,
                                        uint32_t data_len);

int       channel_verify_and_stat (cc_id_t  cid,
                                   uint8_t   pktpt,
                                   u_char  *data,
                                   uint32_t  data_len,
                                   uint16_t *units_per_packet,
                                   u_char  *codec_pt);

int       channel_describe_data   (cc_id_t cid,
                                   uint8_t  pktpt,
                                   u_char *data,
                                   uint32_t data_len,
                                   char   *outstr,
                                   uint32_t out_len);


/* Payload mapping functions */
cc_id_t   channel_coder_get_by_payload (uint8_t pt);
uint8_t    channel_coder_get_payload    (struct s_channel_state* st, uint8_t media_pt);
int       channel_coder_exist_payload  (uint8_t pt);

/* Layered coding functions */
uint8_t    channel_coder_get_layers     (cc_id_t cid);

#endif /* __NEW_CHANNEL_H__ */
