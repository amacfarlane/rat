#include "config_unix.h"
#include "codec_types.h"
#include "codec.h"

#include "util.h" /* Block alloc */
#include "debug.h"

static void
buffer_fill(sample *s, int s_bytes, int channels, int freq)
{
        int i, j, samples = s_bytes / sizeof(sample);

        xmemchk();
        assert(channels == 1 || channels == 2);
        assert(samples > 30);
        for(i = 0; i < samples; i+=channels) {
                for(j = 0; j<channels; j++) {
                        s[i+j] = (sample)
                                (16384.0 * sin(2 * M_PI * (float)i/(float)freq));
                }
        }
        xmemchk();
}

static double
snr(sample *src, sample *replica, int s_bytes)
{
        int i, n_samples = s_bytes / sizeof(sample);
        double st = 0.0, nt = 0.0;

        for(i = 0; i < n_samples; i++) {
                st += fabs((double)src[i]);
                nt += fabs((double)src[i] - (double)replica[i]);
        }

        if (nt == 0) return -100000000.0;

        return -20 * (log10(st / nt));
}

/* This function encodes and decodes frames of audio
 * containing tones and produces SNR estimate.
 *
 * For codecs that are not pitch synchronous this does not
 * make a lot of sense with first finding max correlation score
 * between buffers and then realigning.
 */

static void
test_codec(codec_id_t cid, const codec_format_t *cf)
{
        codec_state *enc, *dec;
        coded_unit  input, output, coded;
        int success, fill_freq;
        float sig_err;

        success = codec_encoder_create(cid, &enc);
        assert(success == 1);
        success = codec_decoder_create(cid, &dec);
        assert(success == 1);

        /* First make buffer of raw audio */
        input.id    = codec_get_native_coding(cf->format.sample_rate,
                                               cf->format.channels);
        input.state     = NULL;
        input.state_len = 0;
        input.data      = (u_char*)block_alloc(cf->format.bytes_per_block);
        input.data_len  = cf->format.bytes_per_block;

        for(fill_freq = 1000; fill_freq < 3500; fill_freq += 500) {
                buffer_fill((sample*)input.data,
                            input.data_len,
                            cf->format.channels,
                            fill_freq);

                memset(&coded, 0, sizeof(coded_unit));
                codec_encode(enc, &input, &coded);

                assert(codec_peek_frame_size(coded.id,
                                             coded.data,
                                             1000) == coded.data_len);

                memset(&output, 0, sizeof(coded_unit));
                codec_decode(dec, &coded, &output);

                /* Make sure raw audio frame sizes match */
                assert(input.data_len == output.data_len);

                /* Do snr thing */
                printf("%d\t",
                       fill_freq);
                sig_err = snr((sample*)input.data, (sample*)output.data, input.data_len);
                printf("%d\n", (int)(sig_err));

                /* Clear memory allocated by encoder */
                codec_clear_coded_unit(&coded);
                /* Clear memory allocated by decoder */
                codec_clear_coded_unit(&output);

        }
        codec_clear_coded_unit(&input);
        codec_encoder_destroy(&enc);
        codec_decoder_destroy(&dec);
}

int main()
{
        const codec_format_t *cf;
        u_int32               n_codecs, i;

        codec_id_t cid;
        codec_init();

        n_codecs = codec_get_number_of_codecs();
        for (i = 0; i < n_codecs; i++) {
                cid = codec_get_codec_number(i);
                assert(codec_id_is_valid(cid));
                cf = codec_get_format(cid);
                printf("Codec 0x%x: ",
                       (unsigned)cid);
                printf("%s (%s).\n",
                       cf->long_name,
                       cf->short_name);

                printf("%s\nDefault pt (%u)\n",
                       cf->description,
                       cf->default_pt
                       );
                if (codec_can_encode(cid) && codec_can_decode(cid)) {
                        test_codec(cid, cf);
                } else {
                        printf("*** Not tested encode(%d) decode(%d)\n",
                               codec_can_encode(cid),
                               codec_can_decode(cid));
                }

                xmemchk();
        }

        codec_exit();
        xmemdmp();
        return 1;
}
