/*
 * FILE:    ui_send_audio.h
 * PROGRAM: RAT
 * AUTHOR:  Colin Perkins
 *
 * Routines which send audio related mbus commands to the user interface.
 *
 * Copyright (c) 2000-2001 University College London
 * All rights reserved.
 */

void ui_send_audio_input_port       (session_t *sp, char *addr);
void ui_send_audio_input_port_list  (session_t *sp, char *addr);
void ui_send_audio_input_mute       (session_t *sp, char *addr);
void ui_send_audio_input_gain       (session_t *sp, char *addr);
void ui_send_audio_input_powermeter (session_t *sp, char *addr, int level);

void ui_send_audio_output_port      (session_t *sp, char *addr);
void ui_send_audio_output_port_list (session_t *sp, char *addr);
void ui_send_audio_output_mute      (session_t *sp, char *addr);
void ui_send_audio_output_gain      (session_t *sp, char *addr);
void ui_send_audio_output_powermeter(session_t *sp, char *addr, int level);

void ui_send_audio_device_list      (session_t *sp, char *addr);
void ui_send_audio_device           (session_t *sp, char *addr);

void ui_send_audio_suppress_silence (session_t *sp, char *addr);

void ui_send_audio_channel_repair   (session_t *sp, char *addr);
void ui_send_audio_channel_coding   (session_t *sp, char *addr);
void ui_send_audio_codec            (session_t *sp, char *addr);

void ui_send_audio_file_play_ready  (session_t *sp, char *addr, char *name);
void ui_send_audio_file_record_ready(session_t *sp, char *addr, char *name);
void ui_send_audio_file_alive       (session_t *sp, char *addr, char *mode, int valid);

void ui_send_audio_3d_options       (session_t *sp, char *addr);
void ui_send_audio_3d_enabled       (session_t *sp, char *addr);
void ui_send_audio_3d_settings      (session_t *sp, char *addr, uint32_t ssrc);

void ui_send_audio_update           (session_t *sp, char *addr);

