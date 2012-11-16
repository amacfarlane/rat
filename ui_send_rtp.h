/*
 * FILE:    ui_send_rtp.h
 * PROGRAM: RAT
 * AUTHOR:  Colin Perkins
 *
 * Routines which send RTP related mbus commands to the user interface.
 *
 * Copyright (c) 2000-2001 University College London
 * All rights reserved.
 */

void ui_send_rtp_name       (session_t *sp, char *addr, uint32_t ssrc);
void ui_send_rtp_cname      (session_t *sp, char *addr, uint32_t ssrc);
void ui_send_rtp_email      (session_t *sp, char *addr, uint32_t ssrc);
void ui_send_rtp_phone      (session_t *sp, char *addr, uint32_t ssrc);
void ui_send_rtp_loc        (session_t *sp, char *addr, uint32_t ssrc);
void ui_send_rtp_tool       (session_t *sp, char *addr, uint32_t ssrc);
void ui_send_rtp_note       (session_t *sp, char *addr, uint32_t ssrc);
void ui_send_rtp_priv       (session_t *sp, char *addr, uint32_t ssrc);
void ui_send_rtp_mute       (session_t *sp, char *addr, uint32_t ssrc);
void ui_send_rtp_gain       (session_t *sp, char *addr, uint32_t ssrc);
void ui_send_rtp_remove     (session_t *sp, char *addr, uint32_t ssrc);
void ui_send_rtp_active     (session_t *sp, char *addr, uint32_t ssrc);
void ui_send_rtp_inactive   (session_t *sp, char *addr, uint32_t ssrc);
void ui_send_rtp_packet_loss(session_t *sp, char *addr, uint32_t srce, uint32_t dest, int loss);
void ui_send_rtp_rtt        (session_t *sp, char *addr, uint32_t ssrc, double rtt_sec);
void ui_send_rtp_ssrc       (session_t *sp, char *addr);
void ui_send_rtp_addr       (session_t *sp, char *addr);
void ui_send_rtp_title      (session_t *sp, char *addr);
void ui_send_rtp_app_site   (session_t *sp, char *addr, uint32_t ssrc, char *siteid);
