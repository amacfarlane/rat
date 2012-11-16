/*
 * FILE:    playout_calc.h
 * PROGRAM: RAT
 * AUTHOR:  Orion Hodson
 *
 * Copyright (c) 1999-2001 University College London
 * All rights reserved.
 *
 * $Id: playout_calc.h 3477 2001-01-08 20:30:13Z ucaccsp $
 */

#ifndef __PLAYOUT_CALC_H__
#define __PLAYOUT_CALC_H__

timestamp_t playout_calc(struct s_session *sp, uint32_t ssrc, timestamp_t transit_ts, int new_spurt);

#endif /* __PLAYOUT_CALC_H__ */
