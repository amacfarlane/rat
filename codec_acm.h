/*
 * FILE:    codec_acm.h
 * PROGRAM: RAT
 * AUTHOR:  O.Hodson
 *
 * Copyright (c) 1995-2001 University College London
 * All rights reserved.
 *
 * $Id: codec_acm.h 3642 2004-01-12 17:14:56Z ucacoxh $
 */

#if  !defined(_CODEC_ACM_H_)
#define _CODEC_ACM_H_

#ifdef WIN32
struct s_acm_state;

void acmStartup(void);
void acmShutdown(void);

struct s_acm_state*
acmEncoderCreate(struct s_codec *cp);

void
acmEncode(struct s_acm_state *s, sample *src, struct s_coded_unit *dst);

void
acmEncoderDestroy(struct s_acm_state *s);

#endif /* WIN32 */

#endif /* _CODEC_ACM_H_ */
