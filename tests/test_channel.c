#include "config_win32.h"
#include "config_unix.h"

#include "audio_types.h"
#include "codec_types.h"
#include "codec.h"

#include "memory.h"
#include "debug.h"
#include "new_channel.h"
#include "channel_types.h"

#include "playout.h"

typedef void (*freeproc)(u_char **, u_int32);

static void
do_test(codec_state *src_coder,
        struct s_channel_state *src_channel,
        codec_state *dst_coder,
        struct s_channel_state *dst_channel)
{
        struct s_playout_buffer *src_media, *src_cout, *dst_cin, *dst_media;
        media_data *m;
        coded_unit  cu_in, cu_out;
        const codec_format_t *cf;
        sample      buf[160];
        int         now;

        playout_buffer_create (&src_media, (freeproc) media_data_destroy,   1);
        playout_buffer_create (&src_cout,  (freeproc) channel_data_destroy, 1);
        playout_buffer_create (&dst_media, (freeproc) media_data_destroy,   1);
        playout_buffer_create (&dst_cin,   (freeproc) channel_data_destroy, 1);

        /* Set up input unit (must be native) */
        cf = codec_get_format(src_coder->id);
        cu_in.id = codec_get_native_coding(cf->format.sample_rate,
                                           cf->format.channels);
        cu_in.state     = NULL;
        cu_in.state_len = NULL;
        cu_in.data      = (u_char*)buf;
        cu_in.data_len  = 160 * 2;

        for(now = 0; now < 1760; now += 160) {
                if (now != 480) {
                        media_data_create(&m, 1);
                        codec_encode(src_coder, &cu_in, m->rep[0]);
                } else {
                        media_data_create(&m, 0);
                }
                playout_buffer_add(src_media, (u_char*)m, sizeof(media_data), now);
        }
        channel_encoder_encode(src_channel, src_media, src_cout);

        for(now = 0; now < 2000; now+=random() % 340) {
                memset(&cu_out, 0, sizeof(coded_unit));
                debug_msg("Now %u\n", now);
                channel_decoder_decode(dst_channel, src_cout, dst_media, now);
        }

        playout_buffer_destroy(&src_media);
        playout_buffer_destroy(&src_cout);
        playout_buffer_destroy(&dst_media);
        playout_buffer_destroy(&dst_cin);

        UNUSED(src_channel);
        UNUSED(dst_coder);
        UNUSED(dst_channel);
}

int
main()
{
        codec_id_t   id;
        codec_state *pscs, *prcs;
        struct s_channel_state *pscc, *prcc;
        int          i;

        cc_details   ccd;

        /* Initialize source coders */
        codec_init();
        id = codec_get_matching("gsm", 8000, 1);
        debug_msg("Codec id %d\n", id);
        codec_encoder_create(id, &pscs);
        codec_decoder_create(id, &prcs);

        /* Initialize channel coders */
        for(i = 0; i < channel_get_coder_count(); i++) {
                channel_get_coder_details(i, &ccd);
                debug_msg("id %d name \"%s\"\n", ccd.descriptor, ccd.name);
        }

        channel_encoder_create(ccd.descriptor, &pscc);
        channel_decoder_create(ccd.descriptor, &prcc);

        do_test(pscs, pscc, prcs, prcc);

        channel_encoder_destroy(&pscc);
        channel_decoder_destroy(&prcc);

        codec_encoder_destroy(&pscs);
        codec_decoder_destroy(&prcs);
        codec_exit();
        xmemchk();
        xmemdmp();
        return TRUE;
}
