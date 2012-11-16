/*
 * FILE:      cc_rdncy.h
 * AUTHOR(S): Orion Hodson
 *
 *
 * Copyright (c) 1995-2001 University College London
 * All rights reserved.
 *
 * $Id: cc_rdncy.h 3642 2004-01-12 17:14:56Z ucacoxh $
 */

#ifndef __CC_RDNCY_H__
#define __CC_RDNCY_H__

/* Encoder functions *********************************************************/

int  redundancy_encoder_create  (u_char **state, uint32_t *len);

void redundancy_encoder_destroy (u_char **state, uint32_t  len);

int  redundancy_encoder_reset   (u_char  *state);

int  redundancy_encoder_encode  (u_char                  *state,
                                 struct s_pb *in,
                                 struct s_pb *out,
                                 uint32_t                  units_per_packet);

int  redundancy_encoder_set_parameters(u_char *state, char *cmd);
int  redundancy_encoder_get_parameters(u_char *state, char *buf, uint32_t blen);

/* Decoder functions *********************************************************/

int  redundancy_decoder_decode  (u_char                  *state,
                                 struct s_pb *in,
                                 struct s_pb *out,
                                 timestamp_t                     now);

int redundancy_decoder_peek     (uint8_t   pkt_pt,
                                 u_char  *data,
                                 uint32_t  len,
                                 uint16_t  *upp,
                                 uint8_t   *pt);

int redundancy_decoder_describe (uint8_t   pkt_pt,
                                 u_char  *data,
                                 uint32_t  len,
                                 char    *out,
                                 uint32_t  out_len);

#endif /* __CC_RDNCY_H__ */

