
/* test_convert [-list] -c <n> -ifmt <fmt1> -ofmt <fmt2> */
/* Where :
   -list  lists converters.
   -c <n> selects converter n
   and <fmt> has form rate=8000,channels=1
 */

#include "config_unix.h"
#include "config_win32.h"
#include "debug.h"
#include "memory.h"
#include "util.h"
#include "audio_types.h"
#include "codec_types.h"
#include "codec.h"
#include "converter_types.h"
#include "converter.h"

static int
list_converters()
{
        const converter_details_t *cd;
        int i, n;
        n = converter_get_count();
        for(i = 0; i < n; i++) {
                cd = converter_get_details(i);
                printf("%d %s\n", i, cd->name);
        }
        fflush(stdout);
        exit(-1);
}

static void
usage()
{
	printf("test_convert [-list] -c <n> -ifmt <fmt> -ofmt <ofmt>\n"\
	       "where:\n\t-list\t lists converters.\n" \
	       "\t-c <n> \t selects converter n\n" \
	       "\tand formats are of form \"rate=8000,channels=1\n");

        exit(-1);
}

static char*
get_token(char *name, char *src, int src_len)
{
        char *cur;
        int cur_len, done;

        done = 0;
        cur  = src;
        while (done < src_len) {
                cur_len = strlen(cur);
                if (!strcmp(cur, name) && done + cur_len + 1 < src_len) {
                        return cur + cur_len + 1;
                }
                cur += cur_len + 1;
        }
        return NULL;
}

static int
parse_fmt(char *fmt, uint16_t *channels, uint16_t *rate)
{
        char *f;
        int fmtlen;
        int found = 0;

        if (fmt == NULL) {
                return FALSE;
        }

        fmtlen = strlen(fmt);
        f = fmt;
        while(*f) {
                if (*f ==',' || *f == '=') {
                        *f = 0;
                }
                f++;
        }

        f = get_token("rate", fmt, fmtlen);
        if (f != NULL) {
                *rate = (uint16_t)(atoi(f));
                found++;
        }

        f = get_token("channels", fmt, fmtlen);
        if (f != NULL) {
                *channels = (uint16_t)(atoi(f));
                found++;
        }

        return (found==2);
}

static void
dump_frame(coded_unit *cu, int offset)
{
        uint16_t channels;
        uint32_t rate;
        sample *src;
        int i, j, n;
        codec_get_native_info( cu->id, &rate, &channels);

        src = (sample*)cu->data;

        n = cu->data_len / (sizeof(sample) * channels);
        for (i = 0; i < n; i++) {
                printf("%.4f ", (double)(i + offset) * 1000.0/(double)rate);
                for(j = 0; j < channels; j++) {
                        printf("%d ", src[i*channels+j]);
                }
                printf("\n");
        }
        printf("\n");
}

#define TEST_FRAME_LEN 160
#define TEST_FRAMES    8

static int
test_converter(int idx, converter_fmt_t *cf)
{
        struct s_converter *c;
        const converter_details_t *cd;
        sample *src;
        coded_unit cu_in, cu_out;
        int i, j, n;

        n = converter_get_count();
        if (idx < 0 || idx > n) {
                fprintf(stderr, "Converter index out of range\n");
                return FALSE;
        }

        cd = converter_get_details(idx);
        printf("#Using %s\n", cd->name);
        if (converter_create(cd->id, cf, &c) == FALSE) {
                fprintf(stderr, "Could not create converter\n");
                return FALSE;
        }

        /* Allocate and initialize source buffer */
        src = (sample*)xmalloc(TEST_FRAME_LEN * TEST_FRAMES * cf->src_channels * sizeof(sample));
        for(i = 0; i < TEST_FRAME_LEN * TEST_FRAMES; i++) {
                for(j = 0; j < cf->src_channels; j++) {
                        src[i * cf->src_channels + j] = 16384 *
                                sin(64 *1000 * M_PI * i / (cf->src_freq * TEST_FRAME_LEN)) *
                                cos(16 *M_PI * i * 1000 / (TEST_FRAME_LEN * cf->src_freq));
                }
        }

        memset(&cu_in, 0, sizeof(cu_in));
        memset(&cu_out, 0, sizeof(cu_out));

        for(i = 0; i < TEST_FRAMES; i++) {
                cu_in.id       = codec_get_native_coding(cf->src_freq, cf->src_channels);
                cu_in.data     = (u_char*)(src + cf->src_channels * TEST_FRAME_LEN * i);
                cu_in.data_len = cf->src_channels * TEST_FRAME_LEN * sizeof(sample);
                dump_frame(&cu_in, i * TEST_FRAME_LEN);
                converter_process(c, &cu_in, &cu_out);
                dump_frame(&cu_out, i * TEST_FRAME_LEN * cf->dst_freq / cf->src_freq);
                block_free(cu_out.data, cu_out.data_len);
                cu_out.data     = 0;
                cu_out.data_len = 0;
        }

        converter_destroy(&c);
        return TRUE;
}

int
main(int argc, char *argv[])
{
        int i;
        int idx;
        converter_fmt_t cf;
        converters_init();

        if (argc == 1) {
                usage();
        }

        i = 1;
        while(i < argc) {
                switch(argv[i][1]) {
                case 'h':
                case '?':
                        usage();
                        break;
                case 'l':
                        list_converters();
                        break;
                case 'c':
                        idx = atoi(argv[i+1]);
                        i+=2;
                        break;
                case 'i':
                        if (parse_fmt(argv[i+1], &cf.src_channels, &cf.src_freq) == FALSE) {
                                usage();
                        }
                        i+=2;
                        break;
                case 'o':
                        if (parse_fmt(argv[i+1], &cf.dst_channels, &cf.dst_freq) == FALSE) {
                                usage();
                        }
                        i+=2;
                        break;
                }
        }

        test_converter(idx, &cf);

        converters_free();
        return 0;
}
