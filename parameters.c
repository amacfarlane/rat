/*
 *	FILE:    parameters.c
 *	PROGRAM: RAT
 *	AUTHOR:	 O. Hodson
 *
 * Copyright (c) 1998-2001 University College London
 * All rights reserved.
 */

#ifndef HIDE_SOURCE_STRINGS
static const char cvsid[] =
	"$Id: parameters.c 3750 2006-05-23 16:54:42Z socrates $";
#endif /* HIDE_SOURCE_STRINGS */

#include "config_unix.h"
#include "config_win32.h"
#include "debug.h"
#include "memory.h"
#include "math.h"
#include "auddev.h"
#include "transmit.h"
#include "session.h"
#include "parameters.h"

#define SD_MAX_CHANNELS  5
#define Q3_MAX 4096
#define L16TOQ3(x) ((x)>>3)
#define Q3TOL16(x) ((x)<<3)
#define MAX_VU 255.0

static u_char vu_tbl[4096];

void
vu_table_init()
{
        int i;
        vu_tbl[0] = 0;
        for (i=1; i<Q3_MAX; i++) {
                vu_tbl[i] = (u_char)(255.0 * (2.0 / (1.0 + exp(-Q3TOL16(i)*1.0/1000)) - 1));
        }
}

int
lin2vu(uint16_t energy, int range, int io_dir)
{
        static double v[2];
        double gain;

        gain = vu_tbl[L16TOQ3(energy)]/MAX_VU;

        v[io_dir] = max(v[io_dir] - 0.1, 0.0);

        if (gain > v[io_dir]) {
                v[io_dir] += 0.80 * (gain - v[io_dir]);
        }

        return (int)(v[io_dir] * range);
}

/* The silence detection algorithm is:
 *
 *    Everytime someone adjusts volume, or starts talking, use
 *    a short parole period to calculate reasonable threshold.
 *
 *    This assumes that person is not talking as they adjust the
 *    volume, or click on start talking button.  This can be false
 *    when the source is music, or the speaker is a politician,
 *    project leader, etc...
 */

/* snapshot in ms to adjust silence threshold */
#define SD_PAROLE_PERIOD 100
#define SD_LOWER_COUNT  3
#define SD_RAISE_COUNT  10

typedef struct s_sd {
        uint32_t parole_period;
        int32_t tot, tot_sq;
        uint32_t history;
        int32_t thresh;
        int32_t m;
        double mds;
        double ltmds;
        uint32_t lt_cnt;  /* Number intervals less than threshold        */
        uint32_t lt_max;  /* Maximum energy of those less than threshold */
        uint32_t gt_cnt;  /* Number intervals more than threshold        */
        uint32_t gt_min;  /* Minimum energy of those less than threshold */
        uint32_t peak;
        uint32_t eval_period;
        uint32_t eval_cnt;
        uint32_t cnt;
} sd_t;

sd_t *
sd_init(uint16_t blk_dur, uint16_t freq)
{
	sd_t *s = (sd_t *)xmalloc(sizeof(sd_t));
        s->parole_period = SD_PAROLE_PERIOD * freq / (blk_dur*1000) + 1;
        s->eval_period   = s->parole_period;
        sd_reset(s);
	return (s);
}

void
sd_reset(sd_t *s)
{
        uint32_t tmp = s->parole_period;
        memset(s, 0, sizeof(sd_t));
        s->parole_period = tmp;
        s->eval_period   = 4 * tmp;
        s->gt_min = 0xffff;
}

void
sd_destroy(sd_t *s)
{
        xfree(s);
}

#define SD_RES 8

/* Returns 1 if silence detected, 0 otherwise */
int
sd(sd_t *s, uint16_t energy)
{
        energy = vu_tbl[L16TOQ3(energy)];

        if (s->cnt < s->parole_period) {
                if (energy > s->thresh) {
                        s->thresh = energy + (energy - s->thresh) / 2;
                } else {
                        s->thresh = (energy + s->thresh)/2 + 1;
                }
                s->thresh = max(s->thresh, energy);
                s->cnt++;
                return (energy < s->thresh);
        }

        if (energy > s->thresh) {
                s->gt_min = min(s->gt_min, energy);
                s->gt_cnt++;
        } else if (energy < s->thresh) {
                s->lt_max = max(s->lt_max, energy);
                s->lt_cnt++;
        }

        if (s->eval_cnt == s->eval_period) {
                if (s->lt_cnt == s->eval_period) {
                        /* Every block had lower energy */
                        s->thresh = (s->thresh + s->lt_max) / 2 + 1;
                } else if (s->gt_cnt == s->eval_period) {
                        /* Every block had greater energy */
                        s->thresh++;
                } else if (s->lt_cnt > s->gt_cnt) {
                        /* We are skimming threshold ? */
                        s->thresh++;
                }
                s->eval_cnt = 0;
                s->lt_max = 0;
                s->lt_cnt = 0;
                s->gt_min = 0xffff;
                s->gt_cnt = 0;
        }
        s->eval_cnt ++;
        return (energy < s->thresh);
}

/* Manual silence detector */

typedef struct s_manual_sd {
        double   sltmean;
        uint16_t thresh;
        double   alpha; /* EWA constant */
} manual_sd_t;

manual_sd_t*
manual_sd_init(uint16_t blk_dur, uint16_t freq, uint16_t thresh)
{
        manual_sd_t *m;
        uint16_t     blocks_per_sec;

        m = (manual_sd_t*)xmalloc(sizeof(manual_sd_t));
        if (m != NULL) {
                m->sltmean = 0;
                m->thresh  = thresh;
                /* Calculate time constant should = 1/8 when blocks_per_sec
                 * is 50 (a la VAT silence detection algorithm).
                 */
                blocks_per_sec = freq / blk_dur;
                m->alpha = pow(1.0 / 8.0, blocks_per_sec / 50.0);
        }

        return m;
}

void
manual_sd_destroy(manual_sd_t *m)
{
        xfree(m);
}

/* Returns 1 if silence detected, 0 otherwise */
int
manual_sd(manual_sd_t *m, uint16_t energy, uint16_t max)
{
        double delta;
        m->sltmean += (energy - m->sltmean) * m->alpha;
        delta = max - m->sltmean - m->thresh;
        return delta < 0;
}

void
manual_sd_set_thresh(manual_sd_t *m, uint16_t thresh)
{
        m->thresh = thresh;
}

/* Voice activity detection */

typedef struct {
        u_char sig;
        u_char pre;
        u_char post;
} vad_limit_t;

typedef struct s_vad {
        /* limits */
        vad_limit_t limit[2];
        uint32_t tick;
        uint32_t spurt_cnt;
        /* state */
        u_char state;
        u_char sig_cnt;
        u_char post_cnt;
} vad_t;

vad_t *
vad_create(uint16_t blockdur, uint16_t freq)
{
        vad_t *v = (vad_t*)xmalloc(sizeof(vad_t));
        memset(v,0,sizeof(vad_t));
        vad_config(v, blockdur, freq);
        return v;
}

const char*
sd_name(int silence_detector)
{
        switch(silence_detector) {
        case SILENCE_DETECTION_AUTO:
                return "Automatic";
        case SILENCE_DETECTION_MANUAL:
                return "Manual";
        }
        return "Off";
}

int
sd_name_to_type(const char *name)
{
        switch(tolower((int)name[0])) { //SV-XXX: NetBSD
        case 'a':
                return SILENCE_DETECTION_AUTO;
        case 'm':
                return SILENCE_DETECTION_MANUAL;
        }
        return SILENCE_DETECTION_OFF;
}

/* Duration of limits in ms */
#define VAD_SIG_LECT     40
#define VAD_SIG_CONF     60
#define VAD_PRE_LECT     60
#define VAD_PRE_CONF     20
#define VAD_POST_LECT   200
#define VAD_POST_CONF   200

void
vad_config(vad_t *v, uint16_t blockdur, uint16_t freq)
{
        uint32_t time_ms;

        assert(blockdur != 0);
        assert(freq     != 0);

        time_ms = (blockdur * 1000) / freq;

        v->limit[VAD_MODE_LECT].sig  = (u_char)(VAD_SIG_LECT  / time_ms);
        v->limit[VAD_MODE_LECT].pre  = (u_char)(VAD_PRE_LECT  / time_ms);
        v->limit[VAD_MODE_LECT].post = (u_char)(VAD_POST_LECT / time_ms);

        v->limit[VAD_MODE_CONF].sig  = (u_char)(VAD_SIG_CONF  / time_ms);
        v->limit[VAD_MODE_CONF].pre  = (u_char)(VAD_PRE_CONF  / time_ms);
        v->limit[VAD_MODE_CONF].post = (u_char)(VAD_POST_CONF / time_ms);
}

void
vad_destroy(vad_t *v)
{
        assert (v != NULL);
        xfree(v);
}

#define VAD_SILENT        0
#define VAD_SPURT         1

uint16_t
vad_to_get(vad_t *v, u_char silence, u_char mode)
{
        vad_limit_t *l = &v->limit[mode];

        assert(mode == VAD_MODE_LECT || mode == VAD_MODE_CONF);

        v->tick++;

        switch (v->state) {
        case VAD_SILENT:
                if (silence == FALSE) {
                        v->sig_cnt++;
                        if (v->sig_cnt == l->sig) {
                                v->state = VAD_SPURT;
                                v->spurt_cnt++;
                                v->post_cnt = 0;
                                v->sig_cnt  = 0;
                                return l->pre;
			}
                } else {
                        v->sig_cnt = 0;
                }
                return 0;
                break;
        case VAD_SPURT:
                if (silence == FALSE) {
                        v->post_cnt = 0;
                        return 1;
                } else {
                        if (++v->post_cnt < l->post) {
                                return 1;
                        } else {
                                v->sig_cnt  = 0;
                                v->post_cnt = 0;
                                v->state = VAD_SILENT;
                                return 0;
                        }
                }
                break;
        }
        return 0; /* never arrives here */
}

uint16_t
vad_max_could_get(vad_t *v)
{
        if (v->state == VAD_SILENT) {
                return v->limit[VAD_MODE_LECT].pre;
        } else {
                return 1;
        }
}

void
vad_reset(vad_t* v)
{
        v->state    = VAD_SILENT;
        v->sig_cnt  = 0;
        v->post_cnt = 0;
}

u_char
vad_in_talkspurt(vad_t *v)
{
        return (v->state == VAD_SPURT) ? TRUE : FALSE;
}

uint32_t
vad_talkspurt_no(vad_t *v)
{
        return v->spurt_cnt;
}

void
vad_dump(vad_t *v)
{
        debug_msg("vad tick %05d state %d sig %d post %d\n",
                v->tick,
                v->state,
                v->sig_cnt,
                v->post_cnt
                );
}

#define AGC_HISTORY_LEN      3
#define AGC_PEAK_LOWER    5000
#define AGC_PEAK_UPPER   14000

typedef struct s_agc {
        uint16_t peak;
        uint16_t cnt;
        uint32_t spurtno;
        u_char  new_gain;
        u_char  change;
        session_t *sp; /* this is unpleasant to have and i wrote it! */
} agc_t;

agc_t *
agc_create(session_t *sp)
{
        agc_t *a = (agc_t*)xmalloc(sizeof(agc_t));
        memset(a,0,sizeof(agc_t));
        a->spurtno = 0xff;
        a->sp      = sp;
        return a;
}

void
agc_destroy(agc_t *a)
{
        xfree(a);
}

void
agc_reset(agc_t *a)
{
        a->peak    = 0;
        a->cnt     = 0;
        a->new_gain = 0;
        a->change  = FALSE;
}

/* This limit stops agc oscillating around close values, which cause
 * silence suppression recallibration to occur too often [oth].
 */

#define AGC_GAIN_SIG 5

static void
agc_consider(agc_t *a)
{
        int32_t gain;

        a->change = FALSE;
        if (a->peak > AGC_PEAK_UPPER) {
                gain        = audio_get_igain(a->sp->audio_device);
                a->new_gain = min(gain * AGC_PEAK_UPPER / a->peak, 99);
                if ((gain - a->new_gain) > AGC_GAIN_SIG) {
                        a->change   = TRUE;
                }
        } else if (a->peak < AGC_PEAK_LOWER) {
                gain        = audio_get_igain(a->sp->audio_device);
                a->new_gain = min(gain * AGC_PEAK_LOWER / a->peak, 99);
                if ((a->new_gain - gain) > AGC_GAIN_SIG) {
                        a->change   = TRUE;
                }
        }
}

void
agc_update(agc_t *a, uint16_t energy, uint32_t spurtno)
{
        a->peak = max(a->peak, energy);
        if (a->spurtno != spurtno) {
                a->spurtno = spurtno;
                a->cnt++;
                if (a->cnt == AGC_HISTORY_LEN) {
                        agc_consider(a);
                        a->cnt = 0;
                        return;
                }
        }
}

u_char
agc_apply_changes(agc_t *a)
{
        if (a->change == TRUE) {
                audio_set_igain(a->sp->audio_device, a->new_gain);
                tx_igain_update(a->sp->tb);
                agc_reset(a);
                a->change = FALSE;
                return TRUE;
        }
        return FALSE;
}


