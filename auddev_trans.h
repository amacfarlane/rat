/*
 * FILE: auddev_trans.h
 * PROGRAM: RAT
 * AUTHOR: Michael Wallbaum <wallbaum@informatik.rwth-aachen.de>
 *
 * Transcoder audio device.
 *
 * $Id: auddev_trans.h 3642 2004-01-12 17:14:56Z ucacoxh $
 */

#ifndef _AUDDEV_TRANS_H_
#define _AUDDEV_TRANS_H_

#if defined(__cplusplus)
extern "C" {
#endif

int  trans_audio_init(void);
int  trans_audio_device_count(void);
char*
     trans_audio_device_name(audio_desc_t ad);
int  trans_audio_open       (audio_desc_t ad, audio_format* ifmt, audio_format *ofmt);
void trans_audio_close      (audio_desc_t ad);
void trans_audio_drain      (audio_desc_t ad);
int  trans_audio_duplex     (audio_desc_t ad);
void trans_audio_set_igain  (audio_desc_t ad, int gain);
int  trans_audio_get_igain  (audio_desc_t ad);
void trans_audio_set_ogain  (audio_desc_t ad, int vol);
int  trans_audio_get_ogain  (audio_desc_t ad);
void trans_audio_loopback   (audio_desc_t ad, int gain);
int  trans_audio_read       (audio_desc_t ad, u_char *buf, int buf_len);
int  trans_audio_write      (audio_desc_t ad, u_char *buf, int buf_len);
void trans_audio_non_block  (audio_desc_t ad);
void trans_audio_block      (audio_desc_t ad);

void         trans_audio_oport_set     (audio_desc_t ad, audio_port_t port);
audio_port_t trans_audio_oport_get     (audio_desc_t ad);
int          trans_audio_oport_count   (audio_desc_t ad);
const audio_port_details_t*
             trans_audio_oport_details (audio_desc_t ad, int idx);

void         trans_audio_iport_set     (audio_desc_t ad, audio_port_t port);
audio_port_t trans_audio_iport_get     (audio_desc_t ad);
int          trans_audio_iport_count   (audio_desc_t ad);
const audio_port_details_t*
             trans_audio_iport_details (audio_desc_t ad, int idx);


int  trans_audio_is_ready  (audio_desc_t ad);
void trans_audio_wait_for  (audio_desc_t ad, int delay_ms);
int  trans_audio_supports  (audio_desc_t ad, audio_format *fmt);

#if defined(__cplusplus)
}
#endif

#endif /* _AUDDEV_TRANS_H_ */
