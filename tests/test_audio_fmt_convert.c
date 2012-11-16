#include "config_unix.h"
#include "config_win32.h"

#include "audio_types.h"
#include "audio_fmt.h"

#define SAMPLES_PER_BLOCK 16

/* Memory debugging.  To check conversion routines do not overstep bounds  */
/* Could have used common library routines, but they depend on being built */
/* with -DDEBUG_MEM */

/* Magic alloc ->  | size | Magic No | Memory ....| Magic no. | */

#define OUR_MAGIC_NO 0xdeadbeef

static int magic_check(void *);

static void*
magic_alloc(size_t size)
{
        uint32_t *m, *s;
        void *blk;
        uint8_t  *t;

        s = (uint32_t*)malloc(size * 3 * sizeof(uint32_t));
        *s = size;
        m  = s + 1;
        *m = OUR_MAGIC_NO;
        t = ((uint8_t*)s) + 2 * sizeof(uint32_t) + size;
        memcpy(t, m, sizeof(uint32_t));

        blk = (void*)s;
        blk += 2 * sizeof(uint32_t);

        memset(blk, 0, size);
        assert(magic_check(blk));
        return blk;
}

static int
magic_check(void *blk)
{
        uint8_t  *t;
        uint32_t *m, *s;

        s = ((uint32_t*)blk) - 2;
        m = s + 1;
        if (*m != OUR_MAGIC_NO) {
                return FALSE;
        }
        t = ((uint8_t*)s) + 2 * sizeof(uint32_t) + *s;

        return !memcmp(m, t, sizeof(uint32_t));
}

static void
magic_free(void *blk)
{
        uint32_t *s = (uint32_t*)(blk - 2 * sizeof(uint32_t));
        free(s);
}

static int
test_sample_conversion(audio_format *fmtin, audio_format *fmtout)
{
        uint8_t *bufin, *bufout;
        int success;

        bufin  = (uint8_t*)magic_alloc(fmtin->bytes_per_block);
        bufout = (uint8_t*)magic_alloc(fmtout->bytes_per_block);

        success = magic_check(bufin) & magic_check(bufout);
        if (success == 0) {
                printf("Test is wrong\n");
                return FALSE;
        }

        audio_format_buffer_convert(fmtin,  bufin,  fmtin->bytes_per_block,
                                    fmtout, bufout, fmtout->bytes_per_block);

        success = 0;
        if (magic_check(bufin) == 0) {
                printf("Input buffer corrupted.\n");
        } else if (magic_check(bufout) == 0) {
                printf("Output buffer corrupted.\n");
        } else {
                success = TRUE;
        }

        magic_free(bufin);
        magic_free(bufout);
        return success;
}

static int
test_sample_conversions(void)
{
        const deve_e encodings[] = {
                DEV_PCMU, DEV_PCMA, DEV_S8, DEV_U8, DEV_S16
        };
        const int    encoding_width[] = {
                8,        8,        8,      8,      16,
        };

        char fmtname[64];

        int ein, eout, msin, msout;
        int num_encodings = sizeof(encodings)/sizeof(encodings[0]);
        audio_format fmtin, fmtout;

        for(msin = 1; msin <= 2; msin++) {
                for(ein = 0; ein < num_encodings; ein++) {
                        fmtin.encoding        = encodings[ein];
                        fmtin.sample_rate     = 8000;
                        fmtin.bits_per_sample = encoding_width[ein];
                        fmtin.channels        = msin;
                        fmtin.bytes_per_block = SAMPLES_PER_BLOCK * fmtin.channels * fmtin.bits_per_sample / 8;
                        audio_format_name(&fmtin, fmtname, sizeof(fmtname));
                        printf("\n\tFrom: %s to:", fmtname);
                        for(msout = 1; msout <= 2; msout++) {

                                for(eout = 0; eout < num_encodings; eout++) {
                                        fmtout.encoding        = encodings[eout];
                                        fmtout.sample_rate     = 8000;
                                        fmtout.bits_per_sample = encoding_width[eout];
                                        fmtout.channels        = msout;
                                        fmtout.bytes_per_block = SAMPLES_PER_BLOCK * fmtout.channels * fmtout.bits_per_sample / 8;
                                        audio_format_name(&fmtout, fmtname, sizeof(fmtname));
                                        printf("\n\t\t%s", fmtname);

                                        if (test_sample_conversion(&fmtin, &fmtout) == FALSE) {
                                                printf("....failed.");
                                        }
                                        printf("....passed");
                                }
                        }
                }
        }

        return TRUE;
}

int main()
{
        printf("Testing sample type conversions");
        if (test_sample_conversions() == FALSE) {
                printf("failed\n");
        }
        return 0;
}

