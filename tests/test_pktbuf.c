#include "config_unix.h"

#include "pktbuf.h"
#include "rtp.h"
#include "debug.h"

#define PKTBUF_SIZE 23

static int 		buf_est;

static void
add_thing(pktbuf_t *pb, rtp_packet *pp) {
	pktbuf_enqueue(pb, pp);
	if (buf_est < PKTBUF_SIZE) {
		buf_est++;
	}
	assert(buf_est == pktbuf_get_count(pb));
	xmemchk();
}

static void
remove_thing(pktbuf_t *pb) {
	rtp_packet *pp;
	if (pktbuf_dequeue(pb, &pp)) {
		xfree(pp);
		buf_est --;
	}
	assert(buf_est == pktbuf_get_count(pb));
	xmemchk();
}

int main() {
	static rtp_packet	*pp;
	static pktbuf_t		*pb;
	int32_t			i, j, n,ts;

	if (pktbuf_create(&pb, PKTBUF_SIZE) == 0) {
		printf("Failed to create buffer\n");
		exit(-1);
	}
	xmemchk();
	for(i = 0; i < 100000; i++) {
		n = lrand48() % 16;
		for(j = 0; j <= n; j++) {
			pp = (rtp_packet*)xmalloc(sizeof(rtp_packet));
			pp->fields.ts = ts ++;
			add_thing(pb, pp);
		}
		n = lrand48() % 16;
		for(j = 0; j < n; j++) {
			remove_thing(pb);
		}
	}
	pktbuf_destroy(&pb);
	xmemdmp();
	printf("Okay\n");
	return 0;
}
