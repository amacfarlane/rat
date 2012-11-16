/*
 * FILE:      cc_vanilla.h
 * AUTHOR(S): Orion Hodson
 *
 *
 * Copyright (c) 1999-2001 University College London
 * All rights reserved.
 *
 * $Id: cc_vanilla.h 3642 2004-01-12 17:14:56Z ucacoxh $
 */

#ifndef __CC_VANILLA_H__
#define __CC_VANILLA_H__

int  vanilla_encoder_create  (u_char **state, uint32_t *len);
void vanilla_encoder_destroy (u_char **state, uint32_t  len);
int  vanilla_encoder_reset   (u_char  *state);
int  vanilla_encoder_encode  (u_char                  *state,
                              struct s_pb *in,
                              struct s_pb *out,
                              uint32_t                  units_per_packet);
int  vanilla_decoder_decode  (u_char                  *state,
                              struct s_pb *in,
                              struct s_pb *out,
                              timestamp_t                     now);
int vanilla_decoder_peek     (uint8_t   pkt_pt,
                              u_char  *data,
                              uint32_t  len,
                              uint16_t  *upp,
                              uint8_t   *pt);

int vanilla_decoder_describe (uint8_t   pkt_pt,
                              u_char  *data,
                              uint32_t  len,
                              char    *out,
                              uint32_t  out_len);

#endif /* __CC_VANILLA_H__ */

