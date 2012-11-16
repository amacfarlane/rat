/*
 * FILE:    ui_send_prefs.h
 * PROGRAM: RAT
 * AUTHOR:  Colin Perkins
 *
 * Routines which send audio related mbus commands to the user interface.
 *
 * Copyright (c) 2000-2001 University College London
 * All rights reserved.
 */

void ui_send_converter_list    (session_t *sp, char *addr);
void ui_send_converter         (session_t *sp, char *addr);
void ui_send_repair_scheme_list(session_t *sp, char *addr);
void ui_send_codec_list        (session_t *sp, char *addr);
void ui_send_codec_details     (session_t *sp, char *addr, codec_id_t cid);
void ui_send_sampling_mode_list(session_t *sp, char *addr);
void ui_send_powermeter        (session_t *sp, char *addr);
void ui_send_playout_bounds    (session_t *sp, char *addr);
void ui_send_agc               (session_t *sp, char *addr);
void ui_send_loopback_gain     (session_t *sp, char *addr);
void ui_send_echo_suppression  (session_t *sp, char *addr);
void ui_send_lecture_mode      (session_t *sp, char *addr);
void ui_send_encryption_key    (session_t *sp, char *addr);
void ui_send_device_config     (session_t *sp, char *addr);
void ui_send_rate              (session_t *sp, char *addr);

