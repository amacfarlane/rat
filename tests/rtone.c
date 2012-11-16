#include "assert.h"
#include "ratconf.h"
#include "config_unix.h"
#include "config_win32.h"
#include "audio_types.h"
#include "sndfile.h"
#include "codec.h"
#include "rtp.h"
#include "util.h"
#include <math.h>
#include <stdlib.h>

#ifndef UNUSED
#define UNUSED(x) x = x
#endif

/* This program is a BIG KLUDGE that acts as an RTP audio source. It will:
 *
 * - encode a tone or audio file (-f) and transmit it.
 * - add jitter to departure times (-j <jitter_ms>).
 * - modulate transmission with pseudo-voice modulation (-b).
 */

/* ------------------------------------------------------------------------- */
/* Outgoing packet buffering for jitter */

struct pkt {
	uint32_t depart_ts;
	uint32_t rtp_ts;
	uint8_t  pt;
	int		 m;
	void     *data;
	int32_t   data_len;
	struct pkt *next;
};

static struct pkt pkt_queue;

static void
pkt_queue_init() {
	pkt_queue.next = &pkt_queue;
}

static void
pkt_queue_add(uint32_t rtp_ts,
	      uint8_t  pt, int marker,
	      void *data, int32_t data_len) {

	struct pkt *p, **c, *sentinel;

	p = (struct pkt *)xmalloc(sizeof(*p));
	p->rtp_ts	= rtp_ts;
	p->pt		= pt;
	p->m		= marker;
	p->data		= data;
	p->data_len	= data_len;

	sentinel = &pkt_queue;
	c = &sentinel->next;
	while (*c != sentinel && (*c)->rtp_ts < rtp_ts) {
		c = &((*c)->next);
	}
	p->next = *c;
	*c = p;
}

static void
pkt_queue_send(struct rtp *session, uint32_t now) {
	struct pkt *sentinel, **c, *n;
	struct timeval tv;

	sentinel = &pkt_queue;
	c = &sentinel->next;
	while (*c != sentinel && (*c)->rtp_ts <= now) {
		n  = *c;
		*c = n->next;
		gettimeofday(&tv, NULL);
		rtp_send_data(session, n->rtp_ts, n->pt, n->m, 0, 0,
			      n->data, n->data_len, NULL, 0, 0);
		xfree(n->data);
		xfree(n);
	}
}

/* speed is the fraction we are faster than the system clock */
/* all us to spoof larger clock drifts for debugging playout */
/* adjustment tweaks.                                        */
static double speed = 1.0;

/* gettime returns a timeval which is relative to first call */
/* and scaled by clock rate.                                 */
static
void gettime(struct timeval *tv)
{
	static struct timeval s;
	double sec, usec, m;
	gettimeofday(tv, NULL);

	if (s.tv_sec == 0) {
		s.tv_sec  = tv->tv_sec;
		s.tv_usec = tv->tv_usec;
	}

	sec  = (double)(tv->tv_sec - s.tv_sec);
	usec = (double)(tv->tv_usec - s.tv_usec);
	if (usec < 0) {
		usec += 1e6;
		sec  -= 1;
	}

	sec  *= speed;
	usec *= speed;

	m = fmod(sec, 1.0);
	sec  -= m;
	usec += 1e6 * m;

	if (usec > 1e6) {
		m    = fmod(usec,1e6);
		sec += (usec - m) / 1e6;
		usec = m;
	}

	m    = fmod(sec, 1.0);
	sec  -= m;
	usec += 1e6 * m;

	tv->tv_sec  = (long)sec;
	tv->tv_usec = (long)usec;
}

static void
tv_bound (struct timeval *t) {
	while (t->tv_usec > 1000000) {
		t->tv_usec -= 1000000;
		t->tv_sec  += 1;
	}
	while (t->tv_usec < 0) {
		t->tv_usec += 1000000;
		t->tv_sec  -= 1;
	}
}

static void
tv_diff(struct timeval *delta, struct timeval *a, struct timeval *b)
{
	assert(a->tv_sec >= b->tv_sec);
	assert(a->tv_usec <= 1000000);
	assert(b->tv_usec <= 1000000);

	delta->tv_sec  = a->tv_sec  - b->tv_sec;
	delta->tv_usec = a->tv_usec - b->tv_usec;
	tv_bound(delta);
	assert(delta->tv_sec >= 0);
}

static void
usage()
{
	fprintf(stderr, "rtone [-c <codec>] [-f freq] [-g gain] [-l] [-s sp] [-t ttl] [-u <upp>] addr/port where:\
\t-c selects the codec used.\
\t-f sets the tone frequency.\
\t-F <filename> stream file.\
\t-g sets the gain (0-32767).\
\t-j <jitter> set jitter in ms.\
\t-l lists available codecs.\
\t-s sets the speed relative to real time (>1.0 == faster).\
\t-S set rtp SSRC.\
\t-t sets ttl.\
\t-u sets units per packet.\n");
	exit(-1);
}

static
void list_codecs()
{
	const codec_format_t *cf;
	codec_id_t cid;
	uint32_t i, n;

	fprintf(stderr, "Available Codecs:\n");

	n   = codec_get_number_of_codecs();
	for(i = 0; i < n; i++) {
		cid = codec_get_codec_number(i);
		cf  = codec_get_format(cid);
		fprintf(stderr, "\t%s\n", cf->long_name);
	}
}

static void
tone_gen	(sample *buffer, int samples, int tone_gain, int tone_rate, int sample_rate, int channels)
{
	static int t = 0;
	int i;
	if (channels == 1) {
		for(i = 0; i < samples; i++) {
			double f  = (double)tone_gain * sin(2 * M_PI * t * tone_rate / (double)sample_rate);
			buffer[i] = (sample)f;
			t++;
		}
	} else if (channels == 2) {
		for(i = 0; i < samples; i++) {
			double f          = (double)tone_gain * sin(2 * M_PI * t * tone_rate / (double)sample_rate);
			buffer[2 * i]     = (sample)f;
			buffer[2 * i + 1] = (sample)f;
			t++;
		}
	}
}

static void
rtone_rtp_callback(struct rtp *session, rtp_event *e)
{
	UNUSED(session);
	if (e->type == RX_RTP) {
		xfree(e->data);
	}
	return;
}

int
main(int argc, char* argv[])
{
	const codec_format_t *cf;
	codec_id_t cid;
	struct timeval last, now, delta, pause, wakeup;
	struct s_sndfile *sf = NULL;
	int      ac, gain = 5000, freq = 400, ttl = 8, upp = 2, i, ulen, done, file_mode = 0, bursty = 0, sleeping = 0, m = 1;
	int      duration = -1, duration_step = 0;
	long int packet_us, avail_us, jitter_ms = 0;
	codec_state *cs;
	coded_unit  *in;
	coded_unit  *out;
	uint32_t      ts, timeout, my_ssrc, set_ssrc = 0;
	char *u, *addr = NULL, *port = NULL, pt;
	struct rtp *session;
	ac = 1;

	codec_init();
	cid = codec_get_by_name("pcmu-8k-mono");

	pkt_queue_init();

	while((ac + 1) < argc && argv[ac][0] == '-') {
		switch(argv[ac][1]) {
		case 'b':
			bursty = 1;
			break;
		case 'c':
			cid = codec_get_by_name(argv[++ac]);
			if (codec_id_is_valid(cid) == FALSE) {
				fprintf(stderr,
					"Codec %s is not one of:\n",
					argv[ac]);
				list_codecs();
				exit(-1);
			}
			break;
		case 'd':
			duration      = atoi(argv[++ac]);
			duration_step = 1;
			break;
		case 'f':
			freq = atoi(argv[++ac]);
			break;
		case 'F':
			if (snd_read_open(&sf, argv[++ac], NULL)) {
				fprintf(stderr, "Could not open %s\n", argv[ac]);
			}
			file_mode = 1;
			break;
		case 'g':
			gain = atoi(argv[++ac]);
			break;
		case 'j':
			jitter_ms = atoi(argv[++ac]);
			break;
		case 'l':
			list_codecs();
			exit(-1);
			break;
		case 's':
			speed = atof(argv[++ac]);
			break;
		case 'S':
			my_ssrc = strtoul(argv[++ac], NULL, 10);
			set_ssrc = 1;
			break;
		case 't':
			ttl = atoi(argv[++ac]);
			break;
		case 'u':
			upp = atoi(argv[++ac]);
			break;
		}
		ac++;
	}

	if (ac != argc - 1) {
		usage();
	}

	addr = strtok(argv[ac], "/");
	port = strtok(NULL, "/");
	if (addr == NULL || port == NULL) {
		usage();
	}

	session = rtp_init(addr, atoi(port), atoi(port), ttl, 64000, rtone_rtp_callback, NULL);
	if (session == NULL) {
		fprintf(stderr, "Failed with -t %d %s/%s\n", ttl, addr, port);
		exit(-1);
	}
	if (set_ssrc) {
		rtp_set_my_ssrc(session, my_ssrc);
		printf("Setting my_ssrc 0x%08x\n", my_ssrc);
	}
	cf = codec_get_format(cid);
	pt = codec_get_payload(cid);
	packet_us = 1000000 * upp * cf->format.bytes_per_block / (cf->format.channels * cf->format.sample_rate * (cf->format.bits_per_sample / 8));

	printf("%s port %s ttl %d\n", addr, port, ttl);
	printf("Codec: %s %d units per packet Tone freq: %d gain: %d (%f dBov) Packets every %ld us\n",
	       cf->long_name, upp,
	       freq, gain, -20 * log(32767.0/(double)gain),
	       packet_us);
	printf("Speed %.2f real-time.\n", speed);

	codec_encoder_create(cid, &cs);
	in  = (coded_unit*)calloc(1, sizeof(coded_unit));
	out = (coded_unit*)calloc(upp, sizeof(coded_unit));

	in->id       = codec_get_native_coding(cf->format.sample_rate, cf->format.channels);
	in->data     = (u_char*)malloc(cf->format.bytes_per_block);
	in->data_len = cf->format.bytes_per_block;

	gettime(&last);
	srand48(last.tv_usec);

	avail_us = 0;
	while(duration != 0) {
		gettime(&now);

		if (bursty && drand48() < 0.005) {
			sleeping = 1;
			wakeup = now;
			wakeup.tv_usec += lrand48() % 10000000;
			wakeup.tv_sec += (wakeup.tv_usec / 1000000);
			wakeup.tv_usec %= 1000000;
		}
		if (sleeping) {
			if (now.tv_sec > wakeup.tv_sec ||
			    ((now.tv_sec == wakeup.tv_sec) && (now.tv_usec > wakeup.tv_usec))) {
				sleeping = 0;
				m = 1;
			}
		}

		tv_diff(&delta, &now, &last);
		avail_us += delta.tv_usec + 1000000 * delta.tv_sec;

		while(avail_us > packet_us) {
			ulen = 0;
			for(i = 0; i < upp; i++) {
				tone_gen((sample*)in->data, in->data_len / (sizeof(sample) * cf->format.channels),
					 gain, freq,
					 cf->format.sample_rate, cf->format.channels);
				if (sf) {
					snd_read_audio(&sf, (sample*)in->data, in->data_len / (sizeof(sample) * cf->format.channels));
				}
				memset(&out[i], 0, sizeof(coded_unit));
				codec_encode(cs, in, &out[i]);
				ulen += out[i].data_len;
				ts   += in->data_len / (sizeof(sample) * cf->format.channels);
			}

			ulen += out[0].state_len;
			u     = (char*)xmalloc(ulen);
			done  = 0;
			for(i = 0; i < upp; i++) {
				if (i == 0 && out[i].state_len) {
					memcpy(u, out[i].state, out[i].state_len);
					done += out[i].state_len;
					block_free(out[i].state, out[i].state_len);
				}
				memcpy(u + done, out[i].data, out[i].data_len);
				done += out[i].data_len;
				block_free(out[i].data, out[i].data_len);
			}
			timeout = jitter_ms * cf->format.sample_rate / 1000;

			if (sleeping == 0) {
				pkt_queue_add(ts + timeout, pt, m, u, ulen);
			}

			timeout = (uint32_t)(timeout * drand48());
			pkt_queue_send(session, ts + timeout);
			avail_us -= packet_us;
			m = 0;
		}
		pause.tv_sec  = 0;
		pause.tv_usec = 20000;
		rtp_recv(session, &pause, ts);

		rtp_send_ctrl(session, ts, NULL);
		rtp_update(session);

		last = now;
		if (file_mode && sf == NULL) {
			break;
		}
		duration -= duration_step;
	}
	free(in);
	free(out);
	rtp_done(session);
	codec_encoder_destroy(&cs);
	codec_exit();
	return 0;
}
