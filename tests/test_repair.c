#include "config_win32.h"
#include "config_unix.h"
#include "audio_types.h"
#include "codec_types.h"
#include "codec_state.h"
#include "codec.h"
#include "repair_types.h"
#include "repair.h"
#include "sndfile_types.h"
#include "sndfile.h"
#include "util.h"

#include <stdlib.h>
#include <stdio.h>

/*
 * test_repair: this test takes an audio file, codes it, and simulates
 * loss on the coded audio, and writes a file of what would be decoded
 * when error concealment is applied.
 */

/* drop code - want to have additive drop so if we run test with 5%
 * and 10% and the same seed, the loss pattern of the 10% is the 5% plus
 * another 5%.
 */

#define DROP_ARRAY_SIZE 192000
#define RANDOM_END 0x7fffffff

static unsigned char drop_priv[DROP_ARRAY_SIZE];
static int units_per_packet = 1;

static void
init_drop(int seed, double d)
{
  int ndrop     = (int)(d * sizeof(drop_priv));
  int dropped;
  unsigned int rand_ceil = RANDOM_END - (RANDOM_END % DROP_ARRAY_SIZE);
  unsigned int r;

  memset(drop_priv, 0, sizeof(drop_priv));
  srand48(seed);

  for (dropped = 0; dropped < ndrop; dropped ++) {
    /* Keep picking numbers until we get one in range and
     * that has not already been picked.
     */
          while( (r = (mrand48() & 0x7fffffff)) > rand_ceil &&
                 drop_priv[r % DROP_ARRAY_SIZE]);
          drop_priv[r % DROP_ARRAY_SIZE] = 1;
  }
  printf("# dropping %d of %d frames\n", ndrop, DROP_ARRAY_SIZE);
}

static int
do_drop()
{
  static long int idx;
  int d;

  d   = drop_priv[idx / units_per_packet];
  idx = (idx + 1) % DROP_ARRAY_SIZE;

  return d;
}

#define READ_BUF_SIZE 2560

static int
read_and_encode(coded_unit        *out,
                codec_state       *encoder,
                struct s_sndfile  *sf_in)
{
        const codec_format_t *cf;
        coded_unit           dummy;
        sample               *buf;
        uint16_t req_samples, act_samples;

        req_samples = codec_get_samples_per_frame(encoder->id);
        buf         = (sample*)block_alloc(sizeof(sample) * req_samples);

        act_samples = (uint16_t)snd_read_audio(&sf_in, buf, req_samples);

        if (req_samples != act_samples) {
                memset(buf + act_samples, 0, sizeof(short) * (req_samples - act_samples));
        }

        cf = codec_get_format(encoder->id);
        assert(cf != NULL);
        dummy.id = codec_get_native_coding((uint16_t)cf->format.sample_rate,
                                            (uint16_t)cf->format.channels);
        dummy.state     = NULL;
        dummy.state_len = 0;
        dummy.data      = (u_char*)buf;
        dummy.data_len  = req_samples * sizeof(short);

        assert(out != NULL);

        if (codec_encode(encoder, &dummy, out) == FALSE) {
                abort();
        }

        block_free(dummy.data, dummy.data_len);

        return (sf_in != NULL);
}

static void
decode_and_write(struct s_sndfile           *sf_out,
                 struct s_codec_state_store *decoder_states,
                 media_data                 *md)
{
        codec_state *decoder;
        coded_unit  *cu;
        int          i;

        assert(md->nrep > 0);
        for(i = 0; i < md->nrep; i++) {
                if (codec_is_native_coding(md->rep[i]->id)) {
                        break;
                }
        }

        if (i == md->nrep) {
                /* no decoded audio available for frame */
                cu = (coded_unit*)block_alloc(sizeof(coded_unit));
                assert(cu != NULL);
                memset(cu, 0, sizeof(coded_unit));
                decoder = codec_state_store_get(decoder_states, md->rep[0]->id);
                codec_decode(decoder, md->rep[0], cu);
                assert(md->rep[md->nrep] == NULL);
                md->rep[md->nrep] = cu;
                md->nrep++;
        }

        assert(codec_is_native_coding(md->rep[md->nrep - 1]->id));
        assert(sf_out != NULL);

        snd_write_audio(&sf_out,
                        (sample*)md->rep[md->nrep - 1]->data,
                        md->rep[md->nrep - 1]->data_len / sizeof(sample));

}

static void
test_repair(struct s_sndfile *sf_out,
            codec_id_t        cid,
            repair_id_t       repair_type,
            struct s_sndfile *sf_in)
{
        codec_state                *encoder;
        struct s_codec_state_store *decoder_states;
        media_data                 *md_prev, *md_cur;
        coded_unit                 *cu;
        int32_t                     consec_lost = 0, total_lost, total_done;
        const codec_format_t       *cf;
        uint16_t                    i;
        repair_id_t                 repair_none;

        for (i = 0; i < repair_get_count(); i++) {
                const repair_details_t *rd;
                rd = repair_get_details(i);
                if (strcasecmp(rd->name, "none") == 0) {
                        repair_none = rd->id;
                        break;
                }
        }

        codec_encoder_create(cid, &encoder);
        codec_state_store_create(&decoder_states, DECODER);
        cf = codec_get_format(cid);

        /* Read and write one unit to kick off with */
        media_data_create(&md_cur, 1);
        read_and_encode(md_cur->rep[0], encoder, sf_in);
        decode_and_write(sf_out, decoder_states, md_cur);

        /* Initialize next reading cycle */
        md_prev = md_cur;
        md_cur  = NULL;
        media_data_create(&md_cur, 1);

        total_lost = total_done = 0;

        while(read_and_encode(md_cur->rep[0], encoder, sf_in)) {
                total_done++;
                if (do_drop()) {
                        total_lost++;
                        media_data_destroy(&md_cur, sizeof(media_data));
                        media_data_create(&md_cur, 0);

                        cu = (coded_unit*)block_alloc(sizeof(coded_unit));
                        assert(cu != NULL);
                        memset(cu, 0, sizeof(coded_unit));

                        /* Loss happens - invoke repair */
                        if (repair_type != repair_none) {
                                cu->id = cid;
                                repair(repair_type,
                                       consec_lost,
                                       decoder_states,
                                       md_prev,
                                       cu);
                        } else {

                                /* Create a silent unit */
                                cu->id = codec_get_native_coding((uint16_t)cf->format.sample_rate,
                                                                 (uint16_t)cf->format.channels);
                                cu->state     = NULL;
                                cu->state_len = 0;
                                cu->data      = (u_char*)block_alloc(cf->format.bytes_per_block);
                                cu->data_len  = cf->format.bytes_per_block;
                                memset(cu->data, 0, cu->data_len);
                        }

                        /* Add repaired audio to frame */
                        md_cur->rep[md_cur->nrep] = cu;
                        md_cur->nrep++;

                        consec_lost++;
                } else {
                        consec_lost = 0;
                }

                decode_and_write(sf_out, decoder_states, md_cur);

                media_data_destroy(&md_prev, sizeof(media_data));
                md_prev = md_cur;
                md_cur  = NULL;
                media_data_create(&md_cur, 1);
        }

        printf("# Dropped %d frames out of %d (%f loss %%)\n", total_lost, total_done, 100.0 * total_lost / (double)total_done);

        media_data_destroy(&md_cur, sizeof(media_data));
        media_data_destroy(&md_prev, sizeof(media_data));

        codec_encoder_destroy(&encoder);
        codec_state_store_destroy(&decoder_states);
}

static void
usage(void)
{
        fprintf(stderr, "test_repair [options] -c <codec> -r <repair> -d <rate> <src_file> <dst_file> where options are:\
\t-codecs to list available codecs\
\t-repairs to list available repair schemes\
\t-n to disable codec specific repair (default csra permitted)\
\t-s <seed> to set seed of rng (default 0)\
\t-u <units> set audio frames per packet (default 1 == 20ms)\n");
        exit(-1);
}

static void
list_repairs(void)
{
        /* Repair interface is not consistent with other interfaces.
         * Does not have get_format get_details type fn, just get_name
         * (grumble, grumble, time, grumble) */

        const repair_details_t *rd;
        uint16_t i, cnt;

        cnt = repair_get_count();
        for(i = 0; i < cnt; i++) {
                rd = repair_get_details(i);
                printf("%s\n", rd->name);
        }

        return;
}

static int
file_and_codec_compatible(const sndfile_fmt_t *sff, codec_id_t cid)
{
        const codec_format_t *cf;
        cf = codec_get_format(cid);

        if (cf == NULL) {
                return FALSE;
        }

        if (sff->sample_rate == (uint32_t)cf->format.sample_rate &&
            sff->channels    == (uint32_t)cf->format.channels) {
                return TRUE;
        }

        return FALSE;
}

static void
list_codecs(void)
{
        uint32_t i, cnt;
        codec_id_t cid;
        const codec_format_t *cf;

        cnt = codec_get_number_of_codecs();

        for(i = 0; i < cnt; i++) {
                cid = codec_get_codec_number(i);
                cf  = codec_get_format(cid);
                printf("%s\n", cf->long_name);
        }

        return;
}

static void
resolve_repair(char *reqname, repair_id_t *rid, const char **actualname)
{
        uint16_t i,n;
        const repair_details_t *rd;

        n = repair_get_count();
        for (i = 0; i < n; i++) {
                rd = repair_get_details(i);
                if (strcasecmp(rd->name, reqname) == 0) {
                        *rid = rd->id;
                        *actualname = rd->name;
                        return;
                }
        }
        fprintf(stderr, "Repair type %s not recognized\n", reqname);
        exit(-1);
}

int
main(int argc, char *argv[])
{
        const char *codec_name, *repair_name;
        codec_id_t cid;
        repair_id_t rid;
        struct s_sndfile *sf_in = NULL, *sf_out = NULL;
        sndfile_fmt_t     sff;
        double drop = 0.0;
        int ac, did_query = FALSE;
        int csra  = TRUE; /* codec specific repair allowed */
        long seed = 100;

        codec_init();

        ac = 1;
        while(ac < argc && argv[ac][0] == '-') {
                if (strlen(argv[ac]) > 2) {
                        /* should be -codecs or -repairs */
                        switch(argv[ac][1]) {
                        case 'c':
                                list_codecs();
                                break;
                        case 'r':
                                list_repairs();
                                break;
                        default:
                                usage();
                        }
                        did_query = TRUE;
                } else {
                        if (argc - ac < 1) {
                                usage();
                        }
                        switch(argv[ac][1]) {
                        case 's':
                                seed = atoi(argv[++ac]);
                                break;
                        case 'd':
                                drop = strtod(argv[++ac], NULL);
                                break;
                        case 'c':
                                cid  = codec_get_by_name(argv[++ac]);
                                codec_name = argv[ac];
                                break;
                        case 'n':
                                csra = FALSE;
                                break;
                        case 'r':
                                resolve_repair(argv[++ac], &rid, &repair_name);
                                break;
                        case 'u':
                                units_per_packet = atoi(argv[++ac]);
                                break;
                        default:
                                usage();
                        }
                }
                ac++;
        }

        if (did_query == TRUE) {
                /* Not sensible to be running query and executing test */
                exit(-1);
        }

        if (argc - ac != 2) {
                usage();
        }


        if (snd_read_open(&sf_in, argv[ac], NULL) == FALSE) {
                fprintf(stderr, "Could not open %s\n", argv[ac]);
                exit(-1);
        }
        ac++;

        if (snd_get_format(sf_in, &sff) == FALSE) {
                fprintf(stderr, "Failed to get format of %s\n", argv[ac]);
                exit(-1);
        }

        if (snd_write_open(&sf_out, argv[ac], "au", &sff) == FALSE) {
                fprintf(stderr, "Could not open %s\n", argv[ac]);
                exit(-1);
        }

        if (file_and_codec_compatible(&sff, cid) == FALSE) {
                fprintf(stderr, "Codec and file type are not compatible\n");
                exit(-1);
        }

        printf("# Parameters \
#\tseed: %ld \
#\tdrop: %.2f \
#\tcodec:  %s \
#\tunits per packet: %d \
#\trepair: %s \
#\tcodec specific repair (when available): %d \
#\tsource file: %s \
#\tdestination file %s\n",
seed, drop, codec_name, units_per_packet, repair_name, csra, argv[argc - 2], argv[argc - 1]);

	repair_set_codec_specific_allowed(csra);

	init_drop(seed, drop);
        test_repair(sf_out, cid, rid, sf_in);

        /* snd_read_close(&sf_in) not needed because files gets closed
         * at eof automatically.
         */
        snd_write_close(&sf_out);

        codec_exit();
        xmemdmp();

        return 0;
}
