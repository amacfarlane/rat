#include "config_unix.h"
#include "config_win32.h"
#include "ts.h"

#define ITERS 100000000
#define MARK  (ITERS/100)

u_int32 f[] = {8000, 11025, 16000, 22050, 24000, 32000, 40000, 44100, 48000, 90000};

#define NUM_F (sizeof(f)/sizeof(u_int32))

static int
test1()
{
        ts_t a, b, c, dac;
        int ib, i, fa, fb;
        u_int32 ta, tb;

        for(ib = 0; ib < ITERS; ib += MARK) {
                for(i = ib; i < ib+MARK; i++) {
                        fa = f[random() % NUM_F];
                        fb = f[random() % NUM_F];
                        ta = (u_int32)random();
                        tb = (u_int32)random();

                        a = ts_map32(fa,ta);
                        b = ts_map32(fb,tb);
                        c = ts_sub(a, b);

                        assert(ts_valid(a));
                        assert(ts_valid(b));
                        assert(ts_valid(c));

                        dac = ts_sub(a,c);
                        assert(ts_eq(dac,b));
                }
                printf("."); fflush(stdout);
        }
        fprintf(stderr, "\n");
        return TRUE;
}

#define STEP 4000

static int
test2()
{
        u_int32      t32;
        ts_t         tsc,tso,step;
        ts_sequencer s;

        int i;

        t32 = random();
        tso = ts_seq32_in(&s, 8000, t32);

        step = ts_map32(8000,STEP);

        for(i = 0; i < 1000000; i++) {
                t32 += STEP;
                tsc = ts_seq32_in(&s, 8000, t32);
                assert(ts_eq(ts_add(tso, step),tsc));
                tso = tsc;
        }
        printf("Forwards passed \n");
        for(i = 0; i < 1000000; i++) {
                t32 -= STEP;
                tsc = ts_seq32_in(&s, 8000, t32);
                assert(ts_eq(ts_sub(tso, step),tsc));
                tso = tsc;
        }
        printf("Backwards passed \n");
        return TRUE;
}


int
main()
{
        test2();
        test1();
        return TRUE;
}
