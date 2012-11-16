/*
 * FILE:    settings.h
 * PROGRAM: RAT
 * AUTHORS: Colin Perkins
 *
 * Copyright (c) 1999-2001 University College London
 * All rights reserved.
 *
 * $Id: settings.h 3642 2004-01-12 17:14:56Z ucacoxh $
 */

void settings_load_early(session_t *sp);
void settings_load_late(session_t *sp);
void settings_save(session_t *sp);

