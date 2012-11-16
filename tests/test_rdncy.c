#include "config_unix.h"

#include "ts.h"
#include "playout.h"

#include "codec_types.h"
#include "codec.h"
#include "channel_types.h"
#include "cc_rdncy.h"

#include "assert.h"
#include "debug.h"

int
main()
{
        u_char  *s;
        u_int32  slen;

        redundancy_encoder_create(&s, &slen);

        redundancy_encoder_set_parameters(s, "dvi-8k-mono/0/dvi-8k-mono/2");


        redundancy_encoder_destroy(&s, slen);
        xmemdmp();
        return TRUE;
}
