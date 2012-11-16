#include "config_unix.h"
#include "codec_types.h"
#include "codec.h"

#include "assert.h"
#include "debug.h"

int main()
{
        codec_id_t cid;
        u_int16 ic, ir, oc, or;
        for(ic = 1; ic <= 2; ic++) {
                for(ir = 8000; ir <= 48000; ir += 8000) {
                        cid = codec_get_native_coding(ir, ic);
                        codec_get_native_info(cid, &or, &oc);
                        debug_msg("0x%08x % 5d %d % 5d %d\n",
                                  cid, ir, ic, or, oc);
                        assert(ir == or);
                        assert(ic == oc);
                }
        }

        return 1;

}
