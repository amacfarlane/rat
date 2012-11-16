/*
 * FILE: cx_wbs.h
 * PROGRAM: RAT
 * AUTHOR: Markus Iken
 *
 * Copyright (c) 1998-2001 University College London
 * All rights reserved.
 *
 * The WB-ADPCM algorithm was developed by British Telecommunications
 * plc.  Permission has been granted to use it for non-commercial
 * research and development projects.  BT retain the intellectual
 * property rights to this algorithm.
 *
 * $Id: cx_wbs.h 3642 2004-01-12 17:14:56Z ucacoxh $
 */

#ifndef _SUBBAND_H_
#define _SUBBAND_H_

#define LO_ENC_DBG 0 /* Low band Encoder Debug switch */
#define LO_DEC_DBG 0 /* Low band Decoder Debug switch */
#define HI_ENC_DBG 0 /* High band Encoder Debug switch */
#define HI_DEC_DBG 0 /* High band Decoder Debug switch */
#define QMF_SP_DBG 0 /* QMF Split operation Debug switch */
#define QMF_MX_DBG 0 /* QMF Combine operation Debug switch */
#define LO_STATE_DBG 0 /* Low band State transmission Debug switch */
#define HI_STATE_DBG 0 /* High band State transmission Debug switch */
#define QMF_STATE_DBG 0 /* QMF State transmission Debug switch */

#define NOISE_SHAPE_ON 1
#define NOISE_SHAPE_FACTOR 0.65

typedef short     int_16;
typedef int       int_32;

typedef struct wbs_state_struct_tag {
	int_32 low[6];
	int_32 hi[2];
} wbs_state_struct;

#define SAMPLES_PER_WBS_UNIT 160

#define WBS_STATE_SIZE sizeof(struct wbs_state_struct_tag)
#define WBS_UNIT_SIZE (SAMPLES_PER_WBS_UNIT / 2)

typedef struct subband_tag {
	sample Low[SAMPLES_PER_WBS_UNIT / 2];
	sample High[SAMPLES_PER_WBS_UNIT / 2];
} subband_struct;

int LowEnc(sample *data, unsigned char *cw, int_32 state[], sample *ns_state);
int LowDec(unsigned char *cw, sample *data, int_32 state[], sample *ns_state);
int HighEnc(sample *data, unsigned char *cw, int_32 state[]);
int HighDec(unsigned char *cw, sample *data, int_32 state[]);
int QMF(sample *data, subband_struct *SubBandData, double *, double *);
int deQMF(subband_struct *SubBandData, sample *decomp_data, double *LowBandIntState, double *HighBandIntState);
void wbs_state_init(wbs_state_struct *state, double qmf_lo[], double qmf_hi[], sample *ns);

#endif /* _SUBBAND_H_ */
