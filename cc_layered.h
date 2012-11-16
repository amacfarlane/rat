/*
 * FILE:      cc_layered.h
 * AUTHOR(S): Orion Hodson + Tristan Henderson
 *
 *
 * Copyright (c) 1995-2001 University College London
 * All rights reserved.
 *
 * $Id: cc_layered.h 3642 2004-01-12 17:14:56Z ucacoxh $
 */

#ifndef __CC_LAYER_H__
#define __CC_LAYER_H__

#define LAY_MAX_LAYERS 2

int  layered_encoder_create  (u_char **state,
                              uint32_t *len);
void layered_encoder_destroy (u_char **state,
                              uint32_t  len);
int  layered_encoder_set_parameters(u_char *state,
                                    char *cmd);
int  layered_encoder_get_parameters(u_char *state,
                                    char *cmd,
                                    uint32_t cmd_len);
int  layered_encoder_reset   (u_char  *state);
int  layered_encoder_encode  (u_char                  *state,
                              struct s_pb *in,
                              struct s_pb *out,
                              uint32_t                  units_per_packet);
int  layered_decoder_decode  (u_char                  *state,
                              struct s_pb *in,
                              struct s_pb *out,
                              timestamp_t                     now);
int layered_decoder_peek     (uint8_t   pkt_pt,
                              u_char  *data,
                              uint32_t  len,
                              uint16_t  *upp,
                              uint8_t   *pt);

int layered_decoder_describe (uint8_t   pkt_pt,
                              u_char  *data,
                              uint32_t  len,
                              char    *out,
                              uint32_t  out_len);

#endif /* __CC_layered_H__ */

