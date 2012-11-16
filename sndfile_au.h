/*
 * FILE:    sndfile_au.h
 * PROGRAM: RAT
 * AUTHOR:  Orion Hodson
 *
 * Copyright (c) 1998-2001 University College London
 * All rights reserved.
 *
 * $Id: sndfile_au.h 3477 2001-01-08 20:30:13Z ucaccsp $
 */

#ifndef __SNDFILE_AU_H__
#define __SNDFILE_AU_H__

int sun_read_hdr(FILE *pf, char **state, sndfile_fmt_t *fmt);        /* Returns true if can decode header */

int sun_read_audio(FILE *pf, char* state, sample *buf, int samples); /* Returns the number of samples read */

int sun_write_hdr(FILE *fp, char **state, const sndfile_fmt_t *fmt);

int sun_write_audio(FILE *fp, char *state, sample *buf, int samples);

int sun_free_state(char **state);

int sun_get_format(char *state, sndfile_fmt_t *fmt);
#endif /* __SNDFILE_AU_H__ */
