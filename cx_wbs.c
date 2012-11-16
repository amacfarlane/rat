/*
 * FILE:     codec_wbs.c
 * PROGRAM:  RAT
 * AUTHOR:   Markus Iken
 *
 * Copyright (c) 1995-2001 University College London
 * All rights reserved.
 *
 * The WB-ADPCM algorithm was developed by British Telecommunications
 * plc.  Permission has been granted to use it for non-commercial
 * research and development projects.  BT retain the intellectual
 * property rights to this algorithm.
 */

#ifndef HIDE_SOURCE_STRINGS
static const char cvsid[] =
	"$Id: cx_wbs.c 3642 2004-01-12 17:14:56Z ucacoxh $";
#endif /* HIDE_SOURCE_STRINGS */

#include "config_unix.h"
#include "config_win32.h"
#include "audio_types.h"
#include "cx_wbs.h"

int aStepsizes[]={
	1,	1,	1,	1,	1,	1,
	1,	1,	1,	1,	1,	1,
	1,	1,	2,	2,	2,	2,
	2,	2,	2,	2,	2,	3,
	3,	3,	3,	3,	4,	4,
	4,	4,	4,	5,	5,	5,
	6,	6,	6,	6,	7,	7,
	8,	8,	8,	9,	9,	10,
	10,	11,	12,	12,	13,	14,
	14,	15,	16,	17,	18,	18,
	19,	20,	21,	23,	24,	25,
	26,	28,	29,	31,	32,	34,
	36,	38,	40,	42,	44,	46,
	48,	51,	53,	56,	59,	62,
	65,	69,	72,	76,	80,	84,
	88,	93,	98,	103,	108,	113,
	119,	125,	132,	139,	146,	153,
	161,	169,	178,	187,	197,	207,
	217,	229,	240,	253,	265,	279,
	293,	308,	324,	341,	358,	377,
	396,	416,	437,	460,	483,	508,
	534,	561,	590,	620,	652,	685,
	720,	757,	796,	837,	880,	925,
	972,	1021
};

int aInvStepsizes[]={
	1024,	974,	926,	881,	838,	798,
	759,	722,	687,	653,	622,	591,
	562,	535,	509,	484,	461,	438,
	417,	397,	377,	359,	341,	325,
	309,	294,	280,	266,	253,	241,
	229,	218,	207,	197,	188,	178,
	170,	161,	154,	146,	139,	132,
	126,	120,	114,	108,	103,	98,
	93,	89,	84,	80,	76,	72,
	69,	65,	62,	59,	56,	54,
	51,	48,	46,	44,	42,	40,
	38,	36,	34,	32,	31,	29,
	28,	26,	25,	24,	23,	22,
	20,	19,	18,	18,	17,	16,
	15,	14,	14,	13,	12,	12,
	11,	10,	10,	9,	9,	8,
	8,	8,	7,	7,	7,	6,
	6,	6,	5,	5,	5,	4,
	4,	4,	4,	4,	3,	3,
	3,	3,	3,	3,	2,	2,
	2,	2,	2,	2,	2,	2,
	1,	1,	1,	1,	1,	1,
	1,	1,	1,	1,	1,	1,
	1,	1
};

int aStepsizeIndex[][8]={
	{0,	0,	2,	3,	9,	16,	21,	24},
	{0,	1,	3,	4,	10,	17,	22,	25},
	{0,	2,	4,	5,	11,	18,	23,	26},
	{1,	3,	5,	6,	12,	19,	24,	27},
	{2,	4,	6,	6,	13,	20,	25,	28},
	{3,	5,	7,	7,	14,	21,	26,	29},
	{4,	6,	7,	8,	15,	22,	27,	30},
	{5,	7,	8,	9,	16,	23,	28,	31},
	{6,	8,	9,	10,	17,	23,	29,	32},
	{7,	9,	10,	11,	18,	24,	30,	33},
	{8,	10,	11,	12,	19,	25,	31,	34},
	{9,	11,	12,	13,	20,	26,	32,	35},
	{10,	12,	13,	14,	21,	27,	33,	36},
	{11,	13,	14,	15,	22,	28,	34,	37},
	{12,	14,	15,	16,	23,	29,	35,	38},
	{13,	15,	16,	17,	24,	30,	36,	39},
	{14,	16,	17,	18,	25,	31,	37,	40},
	{15,	17,	18,	19,	26,	32,	38,	41},
	{16,	18,	19,	20,	27,	33,	39,	42},
	{17,	19,	20,	21,	28,	34,	40,	43},
	{18,	20,	21,	22,	29,	35,	41,	44},
	{19,	20,	22,	23,	30,	36,	42,	45},
	{20,	21,	23,	24,	31,	37,	43,	46},
	{21,	22,	24,	25,	32,	38,	44,	47},
	{22,	23,	25,	26,	33,	39,	45,	48},
	{23,	24,	26,	27,	34,	40,	46,	49},
	{24,	25,	27,	28,	35,	41,	47,	50},
	{25,	26,	28,	29,	36,	42,	48,	51},
	{26,	27,	29,	30,	37,	43,	49,	52},
	{27,	28,	30,	31,	37,	44,	50,	53},
	{28,	29,	31,	32,	38,	45,	51,	54},
	{29,	30,	32,	33,	39,	46,	52,	55},
	{30,	31,	33,	34,	40,	47,	53,	56},
	{31,	32,	34,	35,	41,	48,	54,	57},
	{32,	33,	35,	36,	42,	49,	55,	58},
	{33,	34,	36,	37,	43,	50,	56,	59},
	{34,	35,	37,	38,	44,	51,	57,	60},
	{35,	36,	38,	39,	45,	52,	58,	61},
	{36,	37,	39,	40,	46,	53,	59,	62},
	{37,	38,	40,	41,	47,	54,	60,	63},
	{38,	39,	41,	42,	48,	55,	61,	64},
	{39,	40,	42,	43,	49,	56,	62,	65},
	{40,	41,	43,	44,	50,	57,	63,	66},
	{41,	42,	44,	45,	51,	58,	64,	67},
	{42,	43,	45,	46,	52,	59,	65,	68},
	{43,	44,	46,	47,	53,	60,	66,	69},
	{44,	45,	47,	48,	54,	61,	67,	70},
	{45,	46,	48,	49,	55,	62,	68,	71},
	{46,	47,	49,	50,	56,	63,	69,	72},
	{47,	48,	50,	51,	57,	64,	70,	73},
	{48,	49,	51,	52,	58,	65,	71,	74},
	{49,	50,	52,	53,	59,	66,	72,	75},
	{50,	51,	53,	54,	60,	67,	73,	76},
	{51,	52,	54,	55,	61,	68,	74,	77},
	{52,	53,	55,	56,	62,	69,	75,	78},
	{53,	54,	56,	57,	63,	70,	76,	79},
	{54,	55,	57,	58,	64,	71,	77,	80},
	{54,	56,	58,	59,	65,	72,	77,	81},
	{55,	57,	59,	60,	66,	73,	78,	82},
	{56,	58,	60,	61,	67,	74,	79,	83},
	{57,	59,	61,	62,	68,	75,	80,	84},
	{58,	60,	62,	63,	69,	76,	81,	84},
	{59,	61,	63,	64,	70,	77,	82,	85},
	{60,	62,	64,	65,	71,	78,	83,	86},
	{61,	63,	65,	66,	72,	79,	84,	87},
	{62,	64,	66,	67,	73,	80,	85,	88},
	{63,	65,	67,	68,	74,	81,	86,	89},
	{64,	66,	68,	69,	75,	82,	87,	90},
	{65,	67,	69,	69,	76,	83,	88,	91},
	{66,	68,	70,	70,	77,	84,	89,	92},
	{67,	69,	70,	71,	78,	85,	90,	93},
	{68,	70,	71,	72,	79,	86,	91,	94},
	{69,	71,	72,	73,	80,	86,	92,	95},
	{70,	72,	73,	74,	81,	87,	93,	96},
	{71,	73,	74,	75,	82,	88,	94,	97},
	{72,	74,	75,	76,	83,	89,	95,	98},
	{73,	75,	76,	77,	84,	90,	96,	99},
	{74,	76,	77,	78,	85,	91,	97,	100},
	{75,	77,	78,	79,	86,	92,	98,	101},
	{76,	78,	79,	80,	87,	93,	99,	102},
	{77,	79,	80,	81,	88,	94,	100,	103},
	{78,	80,	81,	82,	89,	95,	101,	104},
	{79,	81,	82,	83,	90,	96,	102,	105},
	{80,	82,	83,	84,	91,	97,	103,	106},
	{81,	83,	84,	85,	92,	98,	104,	107},
	{82,	83,	85,	86,	93,	99,	105,	108},
	{83,	84,	86,	87,	94,	100,	106,	109},
	{84,	85,	87,	88,	95,	101,	107,	110},
	{85,	86,	88,	89,	96,	102,	108,	111},
	{86,	87,	89,	90,	97,	103,	109,	112},
	{87,	88,	90,	91,	98,	104,	110,	113},
	{88,	89,	91,	92,	99,	105,	111,	114},
	{89,	90,	92,	93,	100,	106,	112,	115},
	{90,	91,	93,	94,	100,	107,	113,	116},
	{91,	92,	94,	95,	101,	108,	114,	117},
	{92,	93,	95,	96,	102,	109,	115,	118},
	{93,	94,	96,	97,	103,	110,	116,	119},
	{94,	95,	97,	98,	104,	111,	117,	120},
	{95,	96,	98,	99,	105,	112,	118,	121},
	{96,	97,	99,	100,	106,	113,	119,	122},
	{97,	98,	100,	101,	107,	114,	120,	123},
	{98,	99,	101,	102,	108,	115,	121,	124},
	{99,	100,	102,	103,	109,	116,	122,	125},
	{100,	101,	103,	104,	110,	117,	123,	126},
	{101,	102,	104,	105,	111,	118,	124,	127},
	{102,	103,	105,	106,	112,	119,	125,	128},
	{103,	104,	106,	107,	113,	120,	126,	129},
	{104,	105,	107,	108,	114,	121,	127,	130},
	{105,	106,	108,	109,	115,	122,	128,	131},
	{106,	107,	109,	110,	116,	123,	129,	132},
	{107,	108,	110,	111,	117,	124,	130,	133},
	{108,	109,	111,	112,	118,	125,	131,	134},
	{109,	110,	112,	113,	119,	126,	132,	135},
	{110,	111,	113,	114,	120,	127,	133,	136},
	{111,	112,	114,	115,	121,	128,	134,	137},
	{112,	113,	115,	116,	122,	129,	135,	138},
	{113,	114,	116,	117,	123,	130,	136,	139},
	{114,	115,	117,	118,	124,	131,	137,	139},
	{115,	116,	118,	119,	125,	132,	138,	139},
	{116,	117,	119,	120,	126,	133,	139,	139},
	{117,	118,	120,	121,	127,	134,	139,	139},
	{117,	119,	121,	122,	128,	135,	139,	139},
	{118,	120,	122,	123,	129,	136,	139,	139},
	{119,	121,	123,	124,	130,	137,	139,	139},
	{120,	122,	124,	125,	131,	138,	139,	139},
	{121,	123,	125,	126,	132,	139,	139,	139},
	{122,	124,	126,	127,	133,	139,	139,	139},
	{123,	125,	127,	128,	134,	139,	139,	139},
	{124,	126,	128,	129,	135,	139,	139,	139},
	{125,	127,	129,	130,	136,	139,	139,	139},
	{126,	128,	130,	131,	137,	139,	139,	139},
	{127,	129,	131,	132,	138,	139,	139,	139},
	{128,	130,	132,	132,	139,	139,	139,	139},
	{129,	131,	133,	133,	139,	139,	139,	139},
	{130,	132,	133,	134,	139,	139,	139,	139},
	{131,	133,	134,	135,	139,	139,	139,	139},
	{132,	134,	135,	136,	139,	139,	139,	139},
	{133,	135,	136,	137,	139,	139,	139,	139},
	{134,	136,	137,	138,	139,	139,	139,	139},
	{135,	137,	138,	139,	139,	139,	139,	139}
};

int aHighSSIndex[][2]={
	{0,	13},
	{0,	14},
	{0,	15},
	{1,	16},
	{2,	17},
	{3,	18},
	{4,	19},
	{5,	20},
	{6,	21},
	{7,	22},
	{8,	22},
	{9,	23},
	{10,	24},
	{11,	25},
	{12,	26},
	{12,	27},
	{13,	28},
	{14,	29},
	{15,	30},
	{16,	31},
	{17,	32},
	{18,	33},
	{19,	34},
	{20,	35},
	{21,	36},
	{22,	37},
	{23,	38},
	{24,	39},
	{25,	40},
	{26,	41},
	{27,	42},
	{28,	43},
	{29,	44},
	{30,	45},
	{31,	46},
	{32,	47},
	{33,	48},
	{34,	49},
	{35,	50},
	{36,	51},
	{37,	52},
	{38,	53},
	{39,	54},
	{40,	55},
	{41,	56},
	{42,	57},
	{43,	58},
	{44,	59},
	{45,	60},
	{46,	61},
	{47,	62},
	{48,	63},
	{49,	64},
	{50,	65},
	{51,	66},
	{52,	67},
	{53,	68},
	{54,	69},
	{55,	70},
	{56,	71},
	{57,	72},
	{58,	73},
	{59,	74},
	{60,	75},
	{61,	76},
	{62,	77},
	{63,	78},
	{64,	79},
	{65,	80},
	{66,	81},
	{67,	82},
	{68,	83},
	{69,	84},
	{70,	85},
	{71,	85},
	{72,	86},
	{73,	87},
	{74,	88},
	{75,	89},
	{75,	90},
	{76,	91},
	{77,	92},
	{78,	93},
	{79,	94},
	{80,	95},
	{81,	96},
	{82,	97},
	{83,	98},
	{84,	99},
	{85,	100},
	{86,	101},
	{87,	102},
	{88,	103},
	{89,	104},
	{90,	105},
	{91,	106},
	{92,	107},
	{93,	108},
	{94,	109},
	{95,	110},
	{96,	111},
	{97,	112},
	{98,	113},
	{99,	114},
	{100,	115},
	{101,	116},
	{102,	117},
	{103,	118},
	{104,	119},
	{105,	120},
	{106,	121},
	{107,	122},
	{108,	123},
	{109,	124},
	{110,	125},
	{111,	126},
	{112,	127},
	{113,	128},
	{114,	129},
	{115,	130},
	{116,	131},
	{117,	132},
	{118,	133},
	{119,	134},
	{120,	135},
	{121,	136},
	{122,	137},
	{123,	138},
	{124,	139},
	{125,	139},
	{126,	139},
	{127,	139},
	{128,	139},
	{129,	139},
	{130,	139},
	{131,	139},
	{132,	139},
	{133,	139},
	{134,	139},
	{135,	139}
};

/* LB and HB Predictor Coefficients multiplied by 2^14 to enable integer arithmetic */
int_32 PredCoeffs[] = { -3160, -1336, 5513, 9558 };
int_32 HiPredCoeff = -9836;

int
LowEnc(sample *lb_data, unsigned char *cw, int_32 state[], sample *ns_state)
{
    int    StepsizeIndex;
    register int_32 lbInput;
    int_32 Estimate;
    int_32 Stepsize, InvStepsize;
    int_32 oldStepsize;
    int    i, j;
    int    column;
    int_32 PredIntState[4];
    sample ns_state_tmp;

#if LO_ENC_DBG
    FILE   *DebDataRaw;
    DebDataRaw = fopen("encraw", "w");
    if (DebDataRaw == NULL) {
        printf("Error opening file for writing: encraw\n");
	return EXIT_FAILURE;
    }
#endif

    /* Initialisation */
    StepsizeIndex = state[0];
    for (i=0; i<4; i++) {
	 PredIntState[i] = state[i+1];
    }
    Estimate = state[5];
    Stepsize = aStepsizes[StepsizeIndex];
    InvStepsize = aInvStepsizes[StepsizeIndex];

#if NOISE_SHAPE_ON
    /* Pre-emphasis filter */
    ns_state_tmp = lb_data[(SAMPLES_PER_WBS_UNIT/2)-1];
    for (i=((SAMPLES_PER_WBS_UNIT/2)-1); i>0; i--) {
	 lb_data[i] -= (sample) (NOISE_SHAPE_FACTOR * (float) lb_data[i-1]);
    }
    lb_data[0] -= (sample) (NOISE_SHAPE_FACTOR * (float) *ns_state);
    *ns_state = ns_state_tmp;
#endif

    for (i=0; i<(SAMPLES_PER_WBS_UNIT/2); i++) {
#if LO_STATE_DBG
	 fprintf(stderr, "%d\t", StepsizeIndex);
	 for (j=0; j<4; j++) fprintf(stderr, "%d\t", PredIntState[j]);
	 fprintf(stderr, "%d\n", Estimate);
#endif

        /* Phase 1 */
        lbInput = (int_32) *(lb_data+i);
#if LO_ENC_DBG
	    fprintf(DebDataRaw, "%d;", lbInput);
	    fprintf(DebDataRaw, "%d;", Estimate);
#endif

        /* Phase 2 */
        lbInput -= Estimate;
#if LO_ENC_DBG
	    fprintf(DebDataRaw, "%d;", lbInput);
#endif

	/* Phase 3 */
	lbInput *= InvStepsize;
        /* InvStepSize is by a factor of 1024 too high */
	lbInput >>= 10;
#if LO_ENC_DBG
	    fprintf(DebDataRaw, "%d;", lbInput);
#endif

	/* Phase 4 */
	if (lbInput > 15) lbInput = 15;
	if (lbInput < -16) lbInput = -16;
	/* 16 is added to make lbInput castable into an unsigned quantity for bitmasking */
	*(cw+i) = (unsigned char) (lbInput + 16);
#if 0
	fprintf(stderr, "%X\n", *(cw+i));
#endif
#if LO_ENC_DBG
	    fprintf(DebDataRaw, "%X;", *(cw+i));
#endif

        /* Phase 5 */
	lbInput = (lbInput & (-2)) + 1; /* delete LSB and add (0.5 * Stepsize) */
#if LO_ENC_DBG
            fprintf(DebDataRaw, "%d;", lbInput);
#endif

	/* Phase 6 */
	column = (lbInput >= 0) ? lbInput : ((-1) * lbInput) - 1;
	column >>= 1;
	StepsizeIndex = aStepsizeIndex[StepsizeIndex][column];
	oldStepsize = Stepsize;
	Stepsize = aStepsizes[StepsizeIndex];
	InvStepsize = aInvStepsizes[StepsizeIndex];
#if LO_ENC_DBG
	    fprintf(DebDataRaw, "%d;%d;%d;%d;", column, StepsizeIndex, Stepsize, InvStepsize);
#endif

	/* Phase 7 */
	lbInput *= oldStepsize;
#if LO_ENC_DBG
	    fprintf(DebDataRaw, "%d;", lbInput);
#endif

	/* Phase 8 */
	lbInput += Estimate;
#if LO_ENC_DBG
	    fprintf(DebDataRaw, "%d;", lbInput);
#endif

        /* Phase 9 */
	for (j=0; j<3; j++) {
	    PredIntState[j] = PredIntState[j+1];
	}
	PredIntState[3] = lbInput;
	Estimate = 0;
	for (j=0; j<4; j++) {
	    Estimate += PredCoeffs[j] * PredIntState[j];
	}
	Estimate >>= 14;

#if LO_ENC_DBG
	    for (j=0; j<4; j++) {
	        fprintf(DebDataRaw, "%d;", PredIntState[j]);
	    }
	    fprintf(DebDataRaw, "%d\n", Estimate);
#endif

#if LO_STATE_DBG
	fprintf(stderr, "%d\t", StepsizeIndex);
	for (j=0; j<4; j++) fprintf(stderr, "%d\t", PredIntState[j]);
	fprintf(stderr, "%d\n", Estimate);
#endif
    }

    /* Fill up the state array */
    state[0] = StepsizeIndex;
    for (j=1; j<5; j++) {
	 state[j] = PredIntState[j-1];
    }
    state[5] = Estimate;

#if LO_ENC_DBG
    fclose(DebDataRaw);
#endif

    return EXIT_SUCCESS;
}

int
LowDec(unsigned char *cw, sample *data, int_32 state[], sample *ns_state)
{
    int    StepsizeIndex;
    register int_32  lbInput, lbCWTrain;
    int_32 Estimate;
    int_32 Stepsize, InvStepsize;
    int_32 oldStepsize;
    int    i, j;
    int    column;
    int_32 PredIntState[4];


#if LO_DEC_DBG
    FILE   *DebDataRaw;
    DebDataRaw = fopen("decraw", "w");
    if (DebDataRaw == NULL) {
        printf("Error opening file for writing: encraw\n");
	return EXIT_FAILURE;
    }
#endif

    /* Initialisation */
    StepsizeIndex = state[0];
    for (i=0; i<4; i++) {
	 PredIntState[i] = state[i+1];
    }
    Estimate = state[5];
    Stepsize = aStepsizes[StepsizeIndex];
    InvStepsize = aInvStepsizes[StepsizeIndex];

    for (i=0; i<(SAMPLES_PER_WBS_UNIT/2); i++) {
         #if LO_STATE_DBG
	 fprintf(stderr, "%d\t", StepsizeIndex);
	 for (j=0; j<4; j++) fprintf(stderr, "%d\t", PredIntState[j]);
	 fprintf(stderr, "%d\n", Estimate);
         #endif

        /* Phase 1 */
        lbInput = (int_32) (*(cw+i) >> 0) & ~(~0 << 5); /* Get the 5 lsb of the codeword. */
	lbInput -= 16;
	lbCWTrain = lbInput;
#if LO_DEC_DBG
	    fprintf(DebDataRaw, "%d;", lbCWTrain);
#endif

	/* Phase 2 */
#if LO_DEC_DBG
	    fprintf(DebDataRaw, "%d;", lbCWTrain);
#endif

	/* Phase 3 */
	lbCWTrain *= Stepsize;
#if LO_DEC_DBG
	    fprintf(DebDataRaw, "%d;", Stepsize);
	    fprintf(DebDataRaw, "%d;", lbCWTrain);
#endif

	/* Phase 4 */
	lbCWTrain += Estimate;
	if (lbCWTrain > 32767) lbCWTrain = 32767;
	if (lbCWTrain < -32768) lbCWTrain = -32768;
	*(data+i) = (sample) lbCWTrain;
#if 0
	fprintf(stdout, "%hd\n", *(data+i));
#endif
#if LO_DEC_DBG
	    fprintf(DebDataRaw, "%d;", Estimate);
            fprintf(DebDataRaw, "%hd;", *(data+i));
#endif

        /* Phase 5 */
	lbInput = (lbInput & (-2)) + 1; /* delete LSB and add (0.5 * Stepsize) */
#if LO_DEC_DBG
            fprintf(DebDataRaw, "%d;", lbInput);
#endif

	/* Phase 6 */
	column = (lbInput >= 0) ? lbInput : ((-1) * lbInput) - 1;
	column >>= 1;
	StepsizeIndex = aStepsizeIndex[StepsizeIndex][column];
	oldStepsize = Stepsize;
	Stepsize = aStepsizes[StepsizeIndex];
	InvStepsize = aInvStepsizes[StepsizeIndex];
#if LO_DEC_DBG
	    fprintf(DebDataRaw, "%d;%d;%d;%d;", column, StepsizeIndex, Stepsize, InvStepsize);
#endif

	/* Phase 7 */
	lbInput *= oldStepsize;
#if LO_DEC_DBG
	    fprintf(DebDataRaw, "%d;", lbInput);
#endif

	/* Phase 8 */
	lbInput += Estimate;
#if LO_DEC_DBG
	    fprintf(DebDataRaw, "%d;", lbInput);
#endif

        /* Phase 9 */
	for (j=0; j<3; j++) {
	    PredIntState[j] = PredIntState[j+1];
	}
	PredIntState[3] = lbInput;
	Estimate = 0;
	for (j=0; j<4; j++) {
	    Estimate += PredCoeffs[j] * PredIntState[j];
	}
	Estimate >>= 14;
#if LO_DEC_DBG
	    for (j=0; j<4; j++) {
	        fprintf(DebDataRaw, "%d;", PredIntState[j]);
	    }
	    fprintf(DebDataRaw, "%d\n", Estimate);
#endif

    }

#if NOISE_SHAPE_ON
    /* De-emphasis filter */
    data[0] += (sample) (NOISE_SHAPE_FACTOR * (float) *ns_state);
    for (i=1; i<(SAMPLES_PER_WBS_UNIT/2); i++) {
	 data[i] += (sample) (NOISE_SHAPE_FACTOR * (float) data[i-1]);
    }
    *ns_state = data[(SAMPLES_PER_WBS_UNIT/2)-1];
#endif

    /* Fill up the state array */
    state[0] = StepsizeIndex;
    for (j=1; j<5; j++) {
	 state[j] = PredIntState[j-1];
    }
    state[5] = Estimate;

#if LO_DEC_DBG
    fclose(DebDataRaw);
#endif

    return EXIT_SUCCESS;
}

int
HighEnc(sample *hb_data, unsigned char *cw, int_32 state[])
{
    int    StepsizeIndex = state[0];
    register int_32 lbInput;
    int_32 Estimate = state[1];
    int_32 Stepsize = aStepsizes[StepsizeIndex], InvStepsize = aInvStepsizes[StepsizeIndex];
    int_32 oldStepsize;
    int    i;
    int    column;
    unsigned char aux;

#if HI_STATE_DBG
    fprintf(stderr, "%d\t", StepsizeIndex);
    fprintf(stderr, "%d\n", Estimate);
#endif

#if HI_ENC_DBG
    FILE   *DebDataRaw;
    DebDataRaw = fopen("encraw", "w");
    if (DebDataRaw == NULL) {
        printf("Error opening file for writing: encraw\n");
	return EXIT_FAILURE;
    }
#endif

    for (i=0; i<(SAMPLES_PER_WBS_UNIT/2); i++) {
        /* Phase 1 */
        lbInput = (int_32) *(hb_data+i);
#if HI_ENC_DBG
	    fprintf(DebDataRaw, "%d;", lbInput);
	    fprintf(DebDataRaw, "%d;", Estimate);
#endif

        /* Phase 2 */
        lbInput -= Estimate;
#if HI_ENC_DBG
	    fprintf(DebDataRaw, "%d;", lbInput);
#endif

	/* Phase 3 */
	lbInput *= InvStepsize;
        /* InvStepSize is by a factor of 1024 too high */
	lbInput >>= 10;
#if HI_ENC_DBG
	    fprintf(DebDataRaw, "%d;", lbInput);
#endif

	/* Phase 4 */
	if (lbInput > 3) lbInput = 3;
	if (lbInput < -4) lbInput = -4;
        /* 4 is added to make lbInput castable into an unsigned quantity for bitmasking */
	/* Bitmasking to set the 3 msb of codeword with the content of lbInput. */
	aux = (unsigned char) (lbInput + 4);
	*(cw+i) = (unsigned char)((*(cw+i) & ~(~(~0 << 3) << 5)) | ((aux & ~(~0 << 3)) << 5));
#if 0
	fprintf(stderr, "%X\n", *(cw+i));
#endif
#if HI_ENC_DBG
	    fprintf(DebDataRaw, "%X;", *(cw+i));
#endif

        /* Phase 5 */
	lbInput = (lbInput & (-2)) + 1; /* delete LSB and add (0.5 * Stepsize) */
#if HI_ENC_DBG
            fprintf(DebDataRaw, "%d;", lbInput);
#endif

	/* Phase 6 */
	column = (lbInput >= 0) ? lbInput : ((-1) * lbInput) - 1;
	column >>= 1;
	StepsizeIndex = aHighSSIndex[StepsizeIndex][column];
	oldStepsize = Stepsize;
	Stepsize = aStepsizes[StepsizeIndex];
	InvStepsize = aInvStepsizes[StepsizeIndex];
#if HI_ENC_DBG
	    fprintf(DebDataRaw, "%d;%d;%d;%d;", column, StepsizeIndex, Stepsize, InvStepsize);
#endif

	/* Phase 7 */
	lbInput *= oldStepsize;
#if HI_ENC_DBG
	    fprintf(DebDataRaw, "%d;", lbInput);
#endif

	/* Phase 8 */
	lbInput += Estimate;
#if HI_ENC_DBG
	    fprintf(DebDataRaw, "%d;", lbInput);
#endif

        /* Phase 9 */
	Estimate = HiPredCoeff * lbInput;
	Estimate >>= 14;
#if HI_ENC_DBG
	    fprintf(DebDataRaw, "%d\n", Estimate);
#endif
    }

#if HI_STATE_DBG
    fprintf(stderr, "%d\t", StepsizeIndex);
    fprintf(stderr, "%d\n", Estimate);
#endif

    state[0] = StepsizeIndex;
    state[1] = Estimate;
#if HI_ENC_DBG
    fclose(DebDataRaw);
#endif

    return EXIT_SUCCESS;
}

int
HighDec(unsigned char *cw, sample *data, int_32 state[])
{
    int    StepsizeIndex = state[0];
    register int_32  lbInput, lbCWTrain;
    int_32 Estimate = state[1];
    int_32 Stepsize = aStepsizes[StepsizeIndex], InvStepsize = aInvStepsizes[StepsizeIndex];
    int_32 oldStepsize;
    int    i;
    int    column;

#if HI_STATE_DBG
    fprintf(stderr, "%d\t", StepsizeIndex);
    fprintf(stderr, "%d\n\n", Estimate);
#endif

#if HI_DEC_DBG
    FILE   *DebDataRaw;
    DebDataRaw = fopen("decraw", "w");
    if (DebDataRaw == NULL) {
        printf("Error opening file for writing: encraw\n");
	return EXIT_FAILURE;
    }
#endif

    for (i=0; i<(SAMPLES_PER_WBS_UNIT/2); i++) {
        /* Phase 1 */
	lbInput = (int_32) (*(cw+i) >> 5) & ~(~0 << 3); /* Get the 3 msb of the codeword. */
	lbInput -= 4;
	lbCWTrain = lbInput;
#if HI_DEC_DBG
	    fprintf(DebDataRaw, "%d;", lbCWTrain);
#endif

	/* Phase 2 */
#if HI_DEC_DBG
	    fprintf(DebDataRaw, "%d;", lbCWTrain);
#endif

	/* Phase 3 */
	lbCWTrain *= Stepsize;
#if HI_DEC_DBG
	    fprintf(DebDataRaw, "%d;", Stepsize);
	    fprintf(DebDataRaw, "%d;", lbCWTrain);
#endif

	/* Phase 4 */
	lbCWTrain += Estimate;
	if (lbCWTrain > 32767) lbCWTrain = 32767;
	if (lbCWTrain < -32768) lbCWTrain = -32768;
	*(data+i) = (sample) lbCWTrain;
#if 0
	fprintf(stdout, "%hd\n", *(data+i));
#endif
#if HI_DEC_DBG
	    fprintf(DebDataRaw, "%d;", Estimate);
            fprintf(DebDataRaw, "%hd;", *(data+i));
#endif

        /* Phase 5 */
	lbInput = (lbInput & (-2)) + 1; /* delete LSB and add (0.5 * Stepsize) */
#if HI_DEC_DBG
            fprintf(DebDataRaw, "%d;", lbInput);
#endif

	/* Phase 6 */
	column = (lbInput >= 0) ? lbInput : ((-1) * lbInput) - 1;
	column >>= 1;
	StepsizeIndex = aHighSSIndex[StepsizeIndex][column];
	oldStepsize = Stepsize;
	Stepsize = aStepsizes[StepsizeIndex];
	InvStepsize = aInvStepsizes[StepsizeIndex];
#if HI_DEC_DBG
	    fprintf(DebDataRaw, "%d;%d;%d;%d;", column, StepsizeIndex, Stepsize, InvStepsize);
#endif

	/* Phase 7 */
	lbInput *= oldStepsize;
#if HI_DEC_DBG
	    fprintf(DebDataRaw, "%d;", lbInput);
#endif

	/* Phase 8 */
	lbInput += Estimate;
#if HI_DEC_DBG
	    fprintf(DebDataRaw, "%d;", lbInput);
#endif

        /* Phase 9 */
	Estimate = HiPredCoeff * lbInput;
	Estimate >>= 14;
#if HI_DEC_DBG
	    fprintf(DebDataRaw, "%d\n", Estimate);
#endif

    }

    state[0] = StepsizeIndex;
    state[1] = Estimate;

#if HI_DEC_DBG
    fclose(DebDataRaw);
#endif

    return EXIT_SUCCESS;
}

int
QMF(sample *OrgData, subband_struct *SubBandData, double *LowBandIntState, double *HighBandIntState)
{
     int   i, j;
     sample *data;
     register double LowBandSum, HighBandSum; /* State of the summation block */
     double tmp;
     double LowBandCoeff[] = { -0.001403793,  0.004234195, -0.0094583180,  0.01798145,
			       -0.03123862,   0.05294745,  -0.09980243,    0.4664053,
			        0.1285579,   -0.03934878,   0.01456844,   -0.004187483,
			       -0.0001303859, 0.001414246, -0.0012683030,  0.0006910579 };
     double HighBandCoeff[] = { 0.0006910579, -0.0012683030, 0.001414246, -0.0001303859,
			       -0.004187483,   0.01456844,  -0.03934878,   0.1285579,
				0.4664053,    -0.09980243,   0.05294745,  -0.03123862,
				0.01798145,   -0.0094583180, 0.004234195, -0.001403793 };

#if QMF_SP_DBG
     FILE   *DebDataRaw;
     DebDataRaw = fopen("rawqmfsp", "w");
     if (DebDataRaw == NULL) {
	  printf("Error opening file for writing: encraw\n");
	  return EXIT_FAILURE;
     }
#endif

     data = OrgData;   /* Local copy to the Original Data */

     for (i=0; i< (SAMPLES_PER_WBS_UNIT/2); i++) {
	  LowBandSum = 0.0;
	  HighBandSum = 0.0;
	  /* Feed the new sample in and shift the internal state one up */
	  for (j=15; j>0; j--) {
	       LowBandIntState[j] = LowBandIntState[j-1];
	       HighBandIntState[j] = HighBandIntState[j-1];
	  }
	  LowBandIntState[0] = *data;
	  data++;
	  HighBandIntState[0] = *data;
	  data++;

#if QMF_STATE_DBG
	  if (i<300) {
	       for (j=0; j<1; j++)
		    fprintf(stderr, "%6.0f ", LowBandIntState[j]);
	       fprintf(stderr, "\n");
	  }
#endif

	  /* Multiply the coeffs with the internal state and derive the sum */
	  for (j=0; j<16; j++) {
	       LowBandSum += LowBandIntState[j] * LowBandCoeff[j];
	       HighBandSum += HighBandIntState[j] * HighBandCoeff[j];
	  }
	  /* Crosslink Low and High Band */
	  tmp = LowBandSum;
	  LowBandSum += HighBandSum;
	  HighBandSum -= tmp;
	  SubBandData->Low[i] = (sample) LowBandSum;
	  SubBandData->High[i] = (sample) HighBandSum;
#if 0
	  fprintf(stderr, "%6hd\t", SubBandData->Low[i]);
	  fprintf(stderr, "%6hd\n", SubBandData->High[i]);
#endif

#if QMF_SP_DBG
	  fprintf(DebDataRaw, "%6hd\t", SubBandData->Low[i]);
	  fprintf(DebDataRaw, "%6hd\n", SubBandData->High[i]);
#endif
     }

#if QMF_SP_DBG
     fclose(DebDataRaw);
#endif
     return EXIT_SUCCESS;
}

int
deQMF(subband_struct *SubBandData, sample *decomp_data, double *LowBandIntState, double *HighBandIntState)
{
     int   i, j;
     sample *lDecData;
     register double LowBandSum, HighBandSum; /* State of the summation block */
     double tmp;
     double HighBandCoeff[] = { -0.001403793,  0.004234195, -0.0094583180,  0.01798145,
				-0.03123862,   0.05294745,  -0.09980243,    0.4664053,
			         0.1285579,   -0.03934878,   0.01456844,   -0.004187483,
				-0.0001303859, 0.001414246, -0.0012683030,  0.0006910579 };
     double LowBandCoeff[] = {  0.0006910579, -0.0012683030, 0.001414246, -0.0001303859,
			       -0.004187483,   0.01456844,  -0.03934878,   0.1285579,
				0.4664053,    -0.09980243,   0.05294745,  -0.03123862,
				0.01798145,   -0.0094583180, 0.004234195, -0.001403793 };

#if QMF_MX_DBG
     FILE   *DebDataRaw;
     DebDataRaw = fopen("rawqmfmx", "w");
     if (DebDataRaw == NULL) {
	  printf("Error opening file for writing: encraw\n");
	  return EXIT_FAILURE;
     }
#endif

     lDecData = decomp_data;

     for (i=0; i< (SAMPLES_PER_WBS_UNIT/2); i++) {
	  LowBandSum = 0.0;
	  HighBandSum = 0.0;
	  /* Feed the new sample in and shift the internal state one up */
	  for (j=15; j>0; j--) {
	       LowBandIntState[j] = LowBandIntState[j-1];
	       HighBandIntState[j] = HighBandIntState[j-1];
	  }
	  LowBandIntState[0] = *(SubBandData->Low+i);
	  HighBandIntState[0] = *(SubBandData->High+i);
	  /* Crosslink Low and High Band */
	  tmp = LowBandIntState[0];
	  LowBandIntState[0] -= HighBandIntState[0];
	  HighBandIntState[0] += tmp;

#if QMF_STATE_DBG
	  if (i<300) {
	       for (j=0; j<1; j++)
		    fprintf(stdout, "%6.0f ", LowBandIntState[j]);
	       fprintf(stdout, "\n");
	  }
#endif

	  /* Multiply the coeffs with the internal state and derive the sum */
	  for (j=0; j<16; j++) {
	       LowBandSum += LowBandIntState[j] * LowBandCoeff[j];
	       HighBandSum += HighBandIntState[j] * HighBandCoeff[j];
	  }

	  *lDecData = (sample) LowBandSum;
#if QMF_MX_DBG
	  fprintf(stdout, "%hd\n", *lDecData);
	  fprintf(DebDataRaw, "%hd\n", *lDecData);
#endif
	  lDecData++;
	  *lDecData = (sample) HighBandSum;
#if QMF_MX_DBG
	  fprintf(stdout, "%hd\n", *lDecData);
	  fprintf(DebDataRaw, "%hd\n", *lDecData);
#endif
	  lDecData++;
     }
#if QMF_MX_DBG
     fclose(DebDataRaw);
#endif
     return EXIT_SUCCESS;
}

void
wbs_state_init(wbs_state_struct *state, double qmf_lo[], double qmf_hi[], sample *ns)
{
     int i;

     /* Initializing the transmitted state. */
     state->low[0] = 74;
     for (i=1; i<6; i++) {
          state->low[i] = 0;
     }
     state->hi[0] = 74;
     state->hi[1] = 0;

     /* Initializing the locally persistent state. */
     for (i=0; i<16; i++) {
          qmf_lo[i] = 0;
          qmf_hi[i] = 0;
     }
     *ns = 0;
}
