#include "config_unix.h"
#include "codec_types.h"
#include "codec.h"
#include "render_3D.h"

#include "util.h"
#include "debug.h"
#include "memory.h"

int rates[] = {8000,16000,32000,48000};

#define SAMPLES 320

static void
test_render(struct s_render_3D_dbentry *r, int rate)
{
        int n_channels, azimuth, filter, lindex, length,i;
        sample *s;
        coded_unit in, out;
        memset(&in, 0, sizeof(coded_unit));
        memset(&out, 0, sizeof(coded_unit));
        in.data      = (u_char*)block_alloc(SAMPLES*sizeof(sample));
        in.data_len = SAMPLES * sizeof(sample);

        s = (sample*)in.data;
        for(i = 0; i < SAMPLES; i++) {
                s[i] = (sample)(12000 * sin(M_PI* (float)i/(float)SAMPLES));
        }

        for(n_channels = 1; n_channels <=2; n_channels++) {
                for(filter = 0; filter < render_3D_filter_get_count(); filter++) {
                        for(lindex = 0; lindex < render_3D_filter_get_lengths_count(); lindex++) {
                                length = render_3D_filter_get_length(lindex);

                                printf("%d %d % 5d ",
                                       n_channels,
                                       length,
                                       rate);
                                printf("%s(%d)\t",
                                       render_3D_filter_get_name(filter),
                                       filter);
                                for(azimuth  = render_3D_filter_get_lower_azimuth();
                                    azimuth <= render_3D_filter_get_upper_azimuth();
                                    azimuth += 5) {
                                        render_3D_set_parameters(r, rate, azimuth, filter, length);
                                        in.id = codec_get_native_coding(rate, n_channels);
                                        render_3D(r, &in, &out);
                                        codec_clear_coded_unit(&out);
                                        printf(".");
                                }

                                printf("\n");
                        }
                }
        }
        codec_clear_coded_unit(&in);
}


int main()
{
        unsigned int i;
        struct s_render_3D_dbentry *rdata;

        for(i = 0; i < sizeof(rates)/sizeof(int); i++) {
                rdata = render_3D_init(rates[i]);
                test_render(rdata, rates[i]);
                render_3D_free(&rdata);
        }

        xmemdmp();

        return 1;
}



