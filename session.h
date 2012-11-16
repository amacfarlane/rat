/*
 * FILE:    session.h
 * PROGRAM: RAT
 * AUTHORS: Vicky Hardman + Isidor Kouvelas + Colin Perkins + Orion Hodson
 *
 * Copyright (c) 1995-2001 University College London
 * All rights reserved.
 *
 * $Id: session.h 4545 2010-01-07 11:25:13Z piers $
 */

#ifndef _session_h_
#define _session_h_

#include "net_udp.h"
#include "ts.h"
#include "audio_types.h"
#include "converter_types.h"
#include "repair_types.h"

#include "rtp.h"

/* This will have to be raised in the future */
#define MAX_LAYERS      2

#define MAX_ENCODINGS	7
#define MAX_NATIVE      4

#define MAX_PACKET_SAMPLES	1280
#define PACKET_LENGTH		MAX_PACKET_SAMPLES + 100

#define PORT_UNINIT     0

/* Rat mode def's */
#define AUDIO_TOOL              1
#define TRANSCODER              2
#define FLAKEAWAY               4

#define SILENCE_DETECTION_OFF    0
#define SILENCE_DETECTION_AUTO   1
#define SILENCE_DETECTION_MANUAL 2

/*- global clock frequency -*/
#define GLOBAL_CLOCK_FREQ 96000

#define PT_VANILLA         -1
#define PT_INTERLEAVED    108
#define PT_REDUNDANCY     121		/* This has to be 121 for compatibility with RAT-3.0 */

#define SESSION_TITLE_LEN 40

extern int thread_pri;

typedef struct s_session {
	struct s_session	*other_session;			/* Only valid when we're a transcoder... */
        short           	 id;   				/* unique session id */
	int			 mode; 				/* audio tool, transcoder */
        char            	*title;
	char            	 asc_address[MAX_LAYERS][MAXHOSTNAMELEN+1];
	u_short	        	 rx_rtp_port[MAX_LAYERS];
	u_short	        	 tx_rtp_port[MAX_LAYERS];
	u_short	        	 rx_rtcp_port[MAX_LAYERS];
	u_short	        	 tx_rtcp_port[MAX_LAYERS];
	int             	 ttl;
        struct rtp     		*rtp_session[MAX_LAYERS];
        int             	 rtp_session_count;
        char                    *rtp_session_app_site;
        rtcp_app                *rtcp_app_packet;
        uint32_t                 rtcp_app_packet_ts;
	uint8_t          	 layers; 			/* number of layers == rtp_session_count */
        int             	 filter_loopback;
        timestamp_t              cur_ts; 			/* current device time */
        struct s_cushion_struct *cushion;
        struct s_audio_config   *new_config;
        struct s_mixer          *ms;
	u_char		         encodings[MAX_ENCODINGS];
	int                      num_encodings; 		/* no of unique encs in used */
        struct s_channel_state  *channel_coder;
	int                 	 playing_audio;
	repair_id_t	    	 repair;           		/* Loss concealment algorithm */
        converter_id_t      	 converter;        		/* Sample-rate Converter */
	int		    	 lecture;          		/* UI lecture mode */
	int		    	 render_3d;
        int                 	 echo_suppress;
        int                 	 echo_tx_active; 		/* Mute state when suppressing */
	int                 	 auto_lecture;     		/* Used for dummy lecture mode */
 	int                 	 receive_audit_required;
	int                 	 silence_detection;             /* Which silence detection scheme is active off / auto / manual */
        struct s_sd*             auto_sd;
        struct s_manual_sd*      manual_sd;
        uint16_t                 manual_sd_thresh;
	int                 	 meter;       			/* if powermeters are on */
        uint32_t             	 meter_period;
	int		    	 ui_activated;			/* If our ssrc is highlighted in the ui */
	int		    	 sync_on;
	int		    	 agc_on;
        int                 	 ui_on;				/* If we have a user interface... */
	struct s_sndfile   	*in_file;                       /* File being transmitted over network */
        struct s_converter      *in_file_converter;             /* Input file may be at wrong rate and needs converting */
	struct s_sndfile   	*out_file;                      /* File being recorded from network */
        struct s_voxlet         *local_file_player;             /* File that we are playing to ourselves (local playback) */
        struct s_tonegen        *tone_generator;                /* Tone generator for local audio test */
	audio_desc_t	    	 audio_device;
	struct s_tx_buffer 	*tb;
        struct s_pdb       	*pdb; 				/* persistent participant information database.  */
        struct s_source_list 	*active_sources;
        ts_sequencer        	 decode_sequencer;
        int                 	 limit_playout;
        uint32_t             	 min_playout;
        uint32_t             	 max_playout;
        uint32_t             	 last_depart_ts;
	struct mbus	   	*mbus_engine;
	char		   	*mbus_engine_addr;
	char		   	*mbus_ui_addr;
	char		   	*mbus_video_addr;
	int		    	 loopback_gain;
	char			*encrkey;
	FILE			*logger;			/*logstats file*/
	int			 mbus_waiting;
	char			*mbus_waiting_token;
	int			 mbus_go;
	char			*mbus_go_token;
	uint32_t		 magic;				/* Magic number for debugging purposes */
	int             	 rtp_promiscuous_mode; 		/* Recieve and decode RTP packets without waiting for RTCP nor probation period (in sequence sequence RTP numbers).  WARNING: Can lead to crashes with encrypted sessions as erroneous packets pass the relatively simple rtp_validation*/
	int             	 rtp_wait_for_rtcp;		/* Wait for RTCP and probation before decoding RTP packets in a session, if unset decode packets once they have passed probation (a number of RTP packets in sequence) */
} session_t;

void session_init(session_t *sp, int index, int mode);
void session_exit(session_t *sp);
void session_validate(session_t *sp);

#endif /* _session_h_ */
