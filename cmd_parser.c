/*
 * FILE:    cmd_parser.c
 * PROGRAM: RAT - controller
 * AUTHOR:  Colin Perkins / Orion Hodson
 *
 * Copyright (c) 1999-2001 University College London
 * All rights reserved.
 */

#ifndef HIDE_SOURCE_STRINGS
static const char cvsid[] = "$Id";
#endif /* HIDE_SOURCE_STRINGS */

#include "config_unix.h"
#include "config_win32.h"
#include "debug.h"
#include "mbus.h"
#include "mbus_parser.h"
#include "codec_compat.h"
#include "cmd_parser.h"
#include "version.h"

void
usage(char *szOffending)
{
#ifdef WIN32
	char win_usage[] = "\
                      RAT is a multicast (or unicast) audio tool. It is best to start it\n\
                      using a multicast directory tool, like sdr or multikit. If desired RAT\n\
                      can be launched from the command line using:\n\n\
                      rat <address>/<port>\n\n\
                      where <address> is machine name, or a multicast IP address, and <port> is\n\
                      the connection identifier (an even number between 1024-65536).\n\n\
                      For more details see:\n\n\
                      http://www-mice.cs.ucl.ac.uk/multimedia/software/rat/faq.html\
                      ";

	if (szOffending == NULL) {
		szOffending = win_usage;
	}
	MessageBox(NULL, szOffending, "RAT v" RAT_VERSION " Usage", MB_ICONINFORMATION | MB_OK);
#else
	printf("Usage: rat [options] -t <ttl> <addr>/<port>\n");
	if (szOffending) {
		printf(szOffending);
	}
#endif
}

static int
cmd_logstats(struct mbus *m, char *addr, int argc, char *argv[])
{
        assert(argc == 0);
        mbus_qmsgf(m, addr, TRUE, "tool.rat.logstats", "1");
        UNUSED(argc);
        UNUSED(argv);
        return TRUE;
}

static int
cmd_logdebug(struct mbus *m, char *addr, int argc, char *argv[])
{
        assert(argc == 1);
	printf("cmdparser logdebug\n");
        mbus_qmsgf(m, addr, TRUE, "tool.rat.logdebug", "1 %s", mbus_encode_str(argv[0]));
        UNUSED(argc);
        UNUSED(argv);
        return TRUE;
}

static int 
cmd_layers(struct mbus *m, char *addr, int argc, char *argv[])
{
        int layers;
        assert(argc == 1);
        layers = atoi(argv[0]);
        if (layers > 1) {
                mbus_qmsgf(m, addr, TRUE, "tool.rat.layers", "%d", argv[0]);
                return TRUE;
        }
        UNUSED(argc);
        return FALSE;
}

static int
cmd_allowloop(struct mbus *m, char *addr, int argc, char *argv[])
{
        assert(argc == 0);
        mbus_qmsgf(m, addr, TRUE, "tool.rat.filter.loopback", "0");
        UNUSED(argc);
        UNUSED(argv);
        return TRUE;
}

static int
cmd_session_name(struct mbus *m, char *addr, int argc, char *argv[])
{
        char *enc_name;
        assert(argc == 1);
        enc_name = mbus_encode_str(argv[0]);
        mbus_qmsgf(m, addr, TRUE, "session.title", enc_name);
        xfree(enc_name);
        UNUSED(argc);
        return TRUE;
}

static int
cmd_payload_map(struct mbus *m, char *addr, int argc, char *argv[])
{
        const char *compat;
        char       *codec;
        int         codec_pt;

        assert(argc == 1);
        /* Dynamic payload type mapping. Format: "-pt pt/codec" */
        /* Codec is of the form "pcmu-8k-mono"                  */
        codec_pt = atoi((char*)strtok(argv[0], "/"));
        compat   = codec_get_compatible_name(strtok(NULL, "/"));
        if (compat == NULL) {
                usage("Usage: -pt <pt>/<codec>");
                return FALSE;
        }
        codec = mbus_encode_str(compat);
        mbus_qmsgf(m, addr, TRUE, "tool.rat.payload.set", "%s %d", codec, codec_pt);
        xfree(codec);

        UNUSED(argc);
        return TRUE;
}

static int
cmd_crypt(struct mbus *m, char *addr, int argc, char *argv[])
{
        char *key;

        assert(argc == 1);
        key = mbus_encode_str(argv[0]);
        mbus_qmsgf(m, addr, TRUE, "security.encryption.key", key);
        xfree(key);

        UNUSED(argc);
        return TRUE;
}

static int
cmd_agc(struct mbus *m, char *addr, int argc, char *argv[])
{
        assert(argc == 1);
        if (strcmp(argv[0], "on") == 0) {
                mbus_qmsgf(m, addr, TRUE, "tool.rat.agc", "1");
                return TRUE;
        } else if (strcmp(argv[0], "off") == 0) {
                mbus_qmsgf(m, addr, TRUE, "tool.rat.agc", "0");
                return TRUE;
        }
        UNUSED(argc);
        usage("Usage: -agc on|off\n");
        return FALSE;
}

static int
cmd_silence(struct mbus *m, char *addr, int argc, char *argv[])
{
        assert(argc == 1);
        if (strcmp(argv[0], "on") == 0) {
                mbus_qmsgf(m, addr, TRUE, "tool.rat.silence", "1");
                return TRUE;
        } else if (strcmp(argv[0], "off") == 0) {
                mbus_qmsgf(m, addr, TRUE, "tool.rat.silence", "0");
                return TRUE;
        }
        UNUSED(argc);
        usage("Usage: -silence on|off\n");
        return FALSE;
}

static int
cmd_repair(struct mbus *m, char *addr, int argc, char *argv[])
{
        char *repair;
        assert(argc == 1);
        repair = mbus_encode_str(argv[0]);
        mbus_qmsgf(m, addr, TRUE, "audio.channel.repair", repair);
        xfree(repair);
        UNUSED(argc);
        return TRUE;
}

static int
cmd_primary(struct mbus *m, char *addr, int argc, char *argv[])
{
        /* Set primary codec: "-f codec". You cannot set the   */
        /* redundant codec with this option, use "-r" instead. */
        char *firstname, *realname, *name, *freq, *chan;

        assert(argc == 1);
        /* Break at trailing / in case user attempting old syntax */
        firstname = (char*)strtok(argv[0], "/");

        /* The codec should be of the form "pcmu-8k-mono".     */
        realname = xstrdup(codec_get_compatible_name(firstname));
        name     = (char*)strtok(realname, "-");
        freq     = (char*)strtok(NULL, "-");
        chan     = (char*)strtok(NULL, "");
        if (freq != NULL && chan != NULL) {
                debug_msg("codec: %s %s %s\n", name, chan, freq);
                name    = mbus_encode_str(name);
                freq    = mbus_encode_str(freq);
                chan    = mbus_encode_str(chan);
                mbus_qmsgf(m, addr, TRUE, "tool.rat.codec", "%s %s %s", name, chan, freq);
                xfree(name);
                xfree(freq);
                xfree(chan);
        }
        xfree(realname);
        return TRUE;
}

static int
cmd_redundancy(struct mbus *m, char *addr, int argc, char *argv[])
{
        const char *compat;
        char       *redundancy, *codec;
        int         offset;

        assert(argc == 1);
        /* Set channel coding to redundancy: "-r codec/offset" */
        compat = codec_get_compatible_name((const char*)strtok(argv[0], "/"));
        offset = atoi((char*)strtok(NULL, ""));

        if (offset > 0) {
                redundancy = mbus_encode_str("redundancy");
                codec      = mbus_encode_str(compat);
                mbus_qmsgf(m, addr, TRUE, "audio.channel.coding", "%s %s %d", redundancy, codec, offset);
                xfree(redundancy);
                xfree(codec);
                return TRUE;
        }
        UNUSED(argc);
        usage("Usage: -r <codec>/<offset>");
        return FALSE;
}

static void
cmd_sdes(struct mbus *m, char *addr, const char *msg_name, const char *value)
{
        char *sdes_value, *local_user;
        local_user = mbus_encode_str("localuser");
        sdes_value = mbus_encode_str(value);
        mbus_qmsgf(m, addr, TRUE, msg_name, "%s %s", local_user, sdes_value);
        xfree(local_user);
        xfree(sdes_value);
}

static int
cmd_sdes_name(struct mbus *m, char *addr, int argc, char *argv[]) {
        assert(argc == 1);
        UNUSED(argc);
        cmd_sdes(m, addr, "rtp.source.name", argv[0]);
        return TRUE;
}

static int
cmd_sdes_email(struct mbus *m, char *addr, int argc, char *argv[]) {
        assert(argc == 1);
        UNUSED(argc);
        cmd_sdes(m, addr, "rtp.source.email", argv[0]);
        return TRUE;
}

static int
cmd_sdes_phone(struct mbus *m, char *addr, int argc, char *argv[]) {
        assert(argc == 1);
        UNUSED(argc);
        cmd_sdes(m, addr, "rtp.source.phone", argv[0]);
        return TRUE;
}

static int
cmd_sdes_loc(struct mbus *m, char *addr, int argc, char *argv[]) {
        assert(argc == 1);
        UNUSED(argc);
        cmd_sdes(m, addr, "rtp.source.loc", argv[0]);
        return TRUE;
}

static int
cmd_app_site(struct mbus *m, char *addr, int argc, char *argv[]) {
        char *app_value, *local_user;
	assert(argc == 1);
	UNUSED(argc);
        local_user = mbus_encode_str("localuser");
        app_value = mbus_encode_str(argv[0]);
        mbus_qmsgf(m,
		   addr,
		   TRUE,
		   "rtp.source.app.site",
		   "%s %s",
		   local_user,
		   app_value);
        xfree(local_user);
        xfree(app_value);
        return TRUE;
}
  
static args_handler late_args[] = {
	{ "-logstats",       cmd_logstats,     0 },
	{ "-logdebug",       cmd_logdebug,     1 },
        { "-l",              cmd_layers,       1 },
        { "-allowloopback",  cmd_allowloop,    0 },
        { "-allow_loopback", cmd_allowloop,    0 },
        { "-C",              cmd_session_name, 1 },
        { "-E",              cmd_sdes_email,   1 },
        { "-pt",             cmd_payload_map,  1 },
        { "-crypt",          cmd_crypt,        1 },
        { "-K",              cmd_crypt,        1 },
        { "-L",              cmd_sdes_loc,     1 },
        { "-N",              cmd_sdes_name,    1 },
        { "-P",              cmd_sdes_phone,   1 },
        { "-S",              cmd_app_site,     1 },
        { "-agc",            cmd_agc,          1 },
        { "-silence",        cmd_silence,      1 },
        { "-repair",         cmd_repair,       1 },
        { "-f",              cmd_primary,      1 },
        { "-r",              cmd_redundancy,   1 },
        { "-t",              NULL,             1 }, /* handled in parse early args  */
	{ "-noui",           NULL,             0 }, /* handled in parse early args  */
	{ "-T",	             NULL,             0 }, /* transcoder: special handling */
	{ "-X",	             NULL,             1 }, /* tcl resource handling */
};

static uint32_t late_args_cnt = sizeof(late_args)/sizeof(late_args[0]);

const args_handler *
cmd_args_handler(char *cmdname)
{
        uint32_t j;
        for (j = 0; j < late_args_cnt; j++) {
                if (strcmp(cmdname, late_args[j].cmdname) == 0) {
                        return late_args + j;
                }
        }
        return NULL;
}


