/*
 * FILE:    ui_send_stats.h
 * PROGRAM: RAT
 * AUTHOR:  Colin Perkins
 *
 * Routines which send stats updates to the user interface.
 *
 * Copyright (c) 2000-2001 University College London
 * All rights reserved.
 */

void ui_send_stats(session_t *sp, char *addr, uint32_t ssrc);
void ui_send_periodic_updates(session_t *sp, char *addr, int elapsed_time);

