/*
 * FILE:    auddev_netbsd.c - RAT driver for NetBSD audio devices
 * PROGRAM: RAT
 * AUTHOR:  Brook Milligan
 *
 * $Id: auddev_netbsd.c 3966 2007-02-09 17:26:21Z piers $
 *
 * Copyright (c) 2002-2006 Brook Milligan
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef HIDE_SOURCE_STRINGS
static const char cvsid[] =
"$Id: auddev_netbsd.c 3966 2007-02-09 17:26:21Z piers $";
#endif				/* HIDE_SOURCE_STRINGS */

#include <sys/types.h>
#include <sys/time.h>
#include <sys/audioio.h> 
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include "config_unix.h"
#include "audio_types.h"
#include "audio_fmt.h"
#include "auddev_netbsd.h"
#include "codec_g711.h"
#include "memory.h"
#include "debug.h"

#define DEBUG_MIXER 1

/*
 * Path to audio/mixer devices.  In addition to the base names given
 * here, a series of devices with numerical suffixes beginning with 0
 * are also probed.  This series includes at least min_devices-2
 * (e.g., /dev/sound0-/dev/sound7 for min_devices=9), but continues
 * beyond that until the first failure when opening a device.
 */
static char     audio_path[MAXPATHLEN] = "/dev/sound";
static char     mixer_path[MAXPATHLEN] = "/dev/mixer";
static int      min_devices = 1 + 8;

/*
 * Information on each audio device
 */
typedef struct audio_dinfo {
	char           *dev_name;	/* Audio device name */
	int             fd;	/* File descriptor */
	int             audio_props;	/* Audio device properties */
	audio_device_t  audio_dev;	/* Kernel device names */
	audio_info_t    audio_info;	/* Kernel audio info. */
	audio_encoding_t *audio_enc;	/* Supported kernel encodings */
	const audio_encoding_t *play_encoding;	/* Current encodings */
	const audio_encoding_t *record_encoding;
}               audio_dinfo_t;

/*
 * Information on each mixer device
 */
typedef struct mixer_dinfo {
	char           *dev_name;	/* Mixer device name */
	int             fd;	/* File descriptor */
}               mixer_dinfo_t;

/*
 * Information on each record volume control port.
 *
 * The mixer devices provide a rich interface to the input devices.
 * In many cases, a mute and preamp device are available in addition
 * to the gain control device itself.  Furthermore, selection of the
 * input device is done via the record.source mixer device.
 * Consequently, for each record volume control, this set of four
 * mixer devices is required.  Only the gain control is updated
 * regularly; the other three are constants used to select the
 * appropriate feature.
 *
 * Note that audio devices never provide such a rich interface, so
 * only one "mixer control", the gain field, is used to store the
 * required gain information.  XXX - This should really be handled by
 * a union.
 *
 * Unused mixer devices are flagged with a type field equal to
 * AUDIO_INVALID_PORT.
 */
typedef struct record_gain_ctrl {
	mixer_ctrl_t    gain;
	mixer_ctrl_t    source;
	mixer_ctrl_t    mute;
	mixer_ctrl_t    preamp;
}               record_gain_ctrl_t;

/*
 * Array of available record controls.
 */
typedef struct record_gain {
	int             port;	/* selected port */
	int             n;
	record_gain_ctrl_t *gain_ctrl;
}               record_gain_t;

/*
 * Array of available play controls.
 *
 * The interface for play controls is much simpler than that for record
 * controls, as only one is required even for mixer controls.
 */
typedef struct play_gain {
	int             port;	/* selected port */
	int             n;
	mixer_ctrl_t   *gain_ctrl;
}               play_gain_t;

/*
 * Table of all detected audio devices and their corresponding mixer devices
 */
typedef struct audio_devices {
	char           *full_name;
	audio_dinfo_t   audio_info;
	mixer_dinfo_t   mixer_info;
	play_gain_t     play;
	record_gain_t   record;
}               audio_devices_t;
static int      n_devices = 0;
audio_devices_t *audio_devices = NULL;

/*
 * Rat names for I/O ports.
 *
 * These tables map the I/O ports known to rat into the set of kernel
 * audio/mixer devices.  XXX - These tables should be included within
 * the device table.
 */
static int      n_rat_in_ports = 0;
static audio_port_details_t *rat_in_ports = NULL;

static int      n_rat_out_ports = 0;
static audio_port_details_t *rat_out_ports = NULL;


/* extra port types in addition to those defined in audio(4) */
#define AUDIO_INVALID_PORT (-1)
#define AUDIO_PORT (-2)

#define INVALID_FD (-1)


/*
 * Potential kernel audio I/O ports
 *
 * The AUDIO_GETINFO ioctl(2) returns masks of valid I/O ports in the
 * audio_info.play.avail_ports and audio_info.record.avail_ports
 * fields.  These tables map each of the potential ports to a label
 * that can be used as a rat device name.  This is unnecessary for
 * mixer devices, because the AUDIO_MIXER_DEVINFO ioctl(2) provides
 * access to the kernel-defined label for each mixer device.  See
 * audio(4) for more details.
 */
static audio_port_details_t netbsd_in_ports[] = {
	{AUDIO_MICROPHONE, AudioNmicrophone},
	{AUDIO_LINE_IN, AudioNline},
	{AUDIO_CD, AudioNcd},
	{0, AudioNmaster}
};
#define NETBSD_NUM_INPORTS                                                    \
	(sizeof(netbsd_in_ports) / sizeof(netbsd_in_ports[0]))

static audio_port_details_t netbsd_out_ports[] = {
	{AUDIO_SPEAKER, AudioNspeaker},
	{AUDIO_HEADPHONE, AudioNheadphone},
	{AUDIO_LINE_OUT, AudioNline},
	{0, AudioNmaster}
};
#define NETBSD_NUM_OUTPORTS                                                   \
	(sizeof(netbsd_out_ports) / sizeof(netbsd_out_ports[0]))


/*
 * Encoding mappings between the kernel and rat
 */
struct audio_encoding_match {
	int             netbsd_encoding;
	deve_e          rat_encoding;
};
static struct audio_encoding_match audio_encoding_match[] = {
	{AUDIO_ENCODING_ULAW, DEV_PCMU},
	{AUDIO_ENCODING_ALAW, DEV_PCMA},
	{AUDIO_ENCODING_ULINEAR_LE, DEV_U8},
	{AUDIO_ENCODING_ULINEAR_BE, DEV_U8},
	{AUDIO_ENCODING_ULINEAR, DEV_U8},
	{AUDIO_ENCODING_SLINEAR_LE, DEV_S8},
	{AUDIO_ENCODING_SLINEAR_BE, DEV_S8},
	{AUDIO_ENCODING_SLINEAR, DEV_S8},
	{AUDIO_ENCODING_SLINEAR_LE, DEV_S16},
	{AUDIO_ENCODING_SLINEAR_BE, DEV_S16},
	{AUDIO_ENCODING_SLINEAR, DEV_S16},
	{AUDIO_ENCODING_NONE, 0}
};

/*
 * Gain control mixer devices
 */
typedef struct mixer_devices {
	const char     *class;
	const char     *device;
}               mixer_devices_t;

/*
 * Possible mixer play gain devices.
 *
 * Unlike the record devices which are listed by the kernel as enums
 * in the record.source mixer device, the gain devices that
 * potentially act to control the output level are not defined within
 * the kernel.  This table serves as a list of devices to consider for
 * the purpose of identifying output gain controls.
 */
static mixer_devices_t mixer_play_gain[] = {
	{AudioCoutputs, AudioNmaster},
	{AudioCinputs, AudioNdac},
	{NULL, NULL}
};

/*
 * Possible loopback gain devices.
 *
 * XXX - Loopback handling is not yet supported.  However, this table
 * plays a role similar to the previous one for output gain controls.
 * It lists the devices to consider for the purpose of identifying
 * loopback gain controls.
 */
#if 0				/* XXX */
static struct mixer_devices mixer_loopback_gain[] = {
	{AudioCinputs, AudioNmixerout},
	{AudioCinputs, AudioNspeaker},
	{NULL, NULL}
};
#endif


static int      probe_device(const char *, const char *);
static int      probe_audio_device(const char *, audio_dinfo_t *);
static int      probe_mixer_device(const char *, mixer_dinfo_t *);
static audio_info_t set_audio_info(audio_format * ifmt, audio_format * ofmt);
static const audio_encoding_t *match_encoding
                (audio_format * fmt, const audio_encoding_t * encodings);
static int      audio_select(audio_desc_t ad, int delay_us);
static void     update_audio_info(audio_desc_t ad);
static u_char   average_mixer_level(mixer_level_t);

static void     find_gain_ctrl(audio_desc_t);
static void     find_audio_play_ctrl(audio_desc_t);
static void     find_audio_record_ctrl(audio_desc_t);
static void     find_mixer_play_ctrl(audio_desc_t, mixer_devinfo_t *);
static void     find_mixer_record_ctrl(audio_desc_t, mixer_devinfo_t *);

static void     map_netbsd_ports(audio_desc_t ad);
static void     fix_rat_names(void);

static mixer_devinfo_t *mixer_devices(audio_desc_t ad);

static void     append_record_gain_ctrl(record_gain_t *);
static void
copy_record_gain_ctrl(record_gain_t * gains,
		      record_gain_ctrl_t * src);
static void     copy_rat_in_port(audio_port_details_t * src);

#if DEBUG_MIXER > 0
static void     print_audio_properties(audio_desc_t ad);
static void
print_audio_formats(audio_desc_t ad,
		    audio_format * ifmt, audio_format * ofmt);
static void     print_gain_ctrl(audio_desc_t, mixer_devinfo_t *);
#endif				/* DEBUG_MIXER */

/*
 * Conversion between rat and kernel device gains.
 *
 * Rat uses a scale of 0-100 for gain controls, whereas the kernel
 * uses a scale of AUDIO_MIN_GAIN-AUDIO_MAX_GAIN.  Consequently, gains
 * must be mapped from one scheme to the other.
 */

#define netbsd_rat_to_device(gain)                                            \
	((gain) * (AUDIO_MAX_GAIN - AUDIO_MIN_GAIN) / MAX_AMP + AUDIO_MIN_GAIN)
#define netbsd_device_to_rat(gain)                                            \
	(((gain) - AUDIO_MIN_GAIN) * MAX_AMP / (AUDIO_MAX_GAIN - AUDIO_MIN_GAIN))

#define AUDIO_MID_GAIN ((AUDIO_MIN_GAIN + AUDIO_MAX_GAIN) / 2)


/*
 * netbsd_audio_init: initialize data structures
 *
 * Determine how many audio/mixer devices are available.  Return: TRUE
 * if number of devices is greater than 0, FALSE otherwise.
 */

int
netbsd_audio_init()
{
	int             i, found;
	char            audio_dev_name[MAXPATHLEN];
	char            mixer_dev_name[MAXPATHLEN];
	char            dev_index[MAXPATHLEN];

#if DEBUG_MIXER > 1
	warnx("netbsd_audio_init()");
#endif
	probe_device(audio_path, mixer_path);
	i = 0;
	found = TRUE;
	while (i < min_devices - 1 || found) {
		if (snprintf(dev_index, MAXPATHLEN, "%d", i) >= MAXPATHLEN)
			break;
		strcpy(audio_dev_name, audio_path);
		strcat(audio_dev_name, dev_index);
		strcpy(mixer_dev_name, mixer_path);
		strcat(mixer_dev_name, dev_index);
		found = probe_device(audio_dev_name, mixer_dev_name);
		++i;
	}
#if DEBUG_MIXER > 1
	warnx("netbsd_audio_init():  n_devices=%d", n_devices);
#endif
	return n_devices > 0;
}


/*
 * netbsd_audio_device_count: return number of available audio devices
 */

int
netbsd_audio_device_count()
{
	return n_devices;
}


/*
 * netbsd_audio_device_name: return the full audio device name
 */

char           *
netbsd_audio_device_name(audio_desc_t ad)
{
	assert(audio_devices && n_devices > ad);
	return audio_devices[ad].full_name;
}


/*
 * netbsd_audio_open: try to open the audio and mixer devices
 *
 * Return: valid file descriptor if ok, INVALID_FD otherwise.
 */

int
netbsd_audio_open(audio_desc_t ad, audio_format * ifmt, audio_format * ofmt)
{
	int             audio_fd;
	int             mixer_fd;
	int             full_duplex = 1;

	assert(audio_devices && n_devices > ad
	       && audio_devices[ad].audio_info.fd == INVALID_FD);

	debug_msg("Opening %s (audio device %s, mixer device %s)\n",
		  audio_devices[ad].full_name,
		  audio_devices[ad].audio_info.dev_name,
		  audio_devices[ad].mixer_info.dev_name);
#if DEBUG_MIXER > 0
	warnx("opening %s", audio_devices[ad].full_name);
	warnx("  audio device: %s", audio_devices[ad].audio_info.dev_name);
	if (audio_devices[ad].mixer_info.dev_name)
		warnx("  mixer device: %s",
		      audio_devices[ad].mixer_info.dev_name);
	print_audio_properties(ad);
#endif

	/* open audio device */
	audio_fd = open(audio_devices[ad].audio_info.dev_name,
			O_RDWR | O_NONBLOCK, 0);
	if (audio_fd < 0) {
		perror("opening audio device");
		return INVALID_FD;
	}
	audio_devices[ad].audio_info.fd = audio_fd;

	/* set full duplex */
	if (ioctl(audio_fd, AUDIO_SETFD, &full_duplex) < 0) {
		perror("setting full duplex");
		close(audio_fd);
		audio_devices[ad].audio_info.fd = INVALID_FD;
		return INVALID_FD;
	}
	/* set audio formats */
	audio_devices[ad].audio_info.audio_info = set_audio_info(ifmt, ofmt);

	/* find requested encoding */
	audio_devices[ad].audio_info.play_encoding
		= match_encoding(ofmt, audio_devices[ad].audio_info.audio_enc);
	audio_devices[ad].audio_info.record_encoding
		= match_encoding(ifmt, audio_devices[ad].audio_info.audio_enc);
	audio_devices[ad].audio_info.audio_info.play.encoding
		= audio_devices[ad].audio_info.play_encoding->encoding;
	audio_devices[ad].audio_info.audio_info.record.encoding
		= audio_devices[ad].audio_info.record_encoding->encoding;

	/* notify kernel */
	if (ioctl(audio_fd, AUDIO_SETINFO,
		  &audio_devices[ad].audio_info.audio_info) < 0) {
		perror("setting audio info");
	}
#if DEBUG_MIXER > 0
	print_audio_formats(ad, ifmt, ofmt);
#endif

	/* flush audio output */
	if (ioctl(audio_fd, AUDIO_FLUSH, NULL) < 0) {
		perror("flushing audio device");
		close(audio_fd);
		audio_devices[ad].audio_info.fd = INVALID_FD;
		return INVALID_FD;
	}
	/* open mixer device (if any) */
	audio_devices[ad].mixer_info.fd = INVALID_FD;
	if (audio_devices[ad].mixer_info.dev_name) {
		mixer_fd = open(audio_devices[ad].mixer_info.dev_name,
				O_RDONLY | O_NONBLOCK, 0);
		if (mixer_fd < 0) {
			perror("ignoring mixer device");
		}
		audio_devices[ad].mixer_info.fd = mixer_fd;
	}
	find_gain_ctrl(ad);

	return audio_devices[ad].audio_info.fd;
}


/*
 * netbsd_audio_close: close audio and mixer devices (if open)
 */

void
netbsd_audio_close(audio_desc_t ad)
{
	assert(audio_devices && n_devices > ad);

	if (audio_devices[ad].audio_info.fd >= 0) {
		/* flush audio device first */
		if (ioctl(audio_devices[ad].audio_info.fd, AUDIO_FLUSH,
			  NULL) < 0)
			perror("netbsd_audio_close: flushing device");
		(void) close(audio_devices[ad].audio_info.fd);
	}
	if (audio_devices[ad].mixer_info.fd >= 0) {
		(void) close(audio_devices[ad].mixer_info.fd);
	}
	free(audio_devices[ad].play.gain_ctrl);
	audio_devices[ad].play.gain_ctrl = NULL;
	audio_devices[ad].play.n = 0;
	free(audio_devices[ad].record.gain_ctrl);
	audio_devices[ad].record.gain_ctrl = NULL;
	audio_devices[ad].record.n = 0;

	free(rat_in_ports);
	rat_in_ports = NULL;
	n_rat_in_ports = 0;
	free(rat_in_ports);
	rat_out_ports = NULL;
	n_rat_out_ports = 0;

#if DEBUG_MIXER > 0
	warnx("closing %s", audio_devices[ad].full_name);
#endif
	debug_msg("Closing %s (audio device %s, mixer device %s)\n",
		  audio_devices[ad].full_name,
		  audio_devices[ad].audio_info.dev_name,
		  audio_devices[ad].mixer_info.dev_name);

	audio_devices[ad].audio_info.fd = INVALID_FD;
	audio_devices[ad].mixer_info.fd = INVALID_FD;
}


/*
 * netbsd_audio_drain: drain audio buffers
 */

void
netbsd_audio_drain(audio_desc_t ad)
{
	assert(audio_devices && n_devices > ad);
	if (audio_devices[ad].audio_info.fd < 0)
		return;

	if (ioctl(audio_devices[ad].audio_info.fd, AUDIO_DRAIN, NULL) < 0)
		perror("netbsd_audio_drain");
}


/*
 * netbsd_audio_duplex: check duplex flag for audio device
 *
 * Return: duplex flag, or 0 on failure (which shouldn't happen)
 */

int
netbsd_audio_duplex(audio_desc_t ad)
{
	int             duplex;

	assert(audio_devices && n_devices > ad);
	if (audio_devices[ad].audio_info.fd < 0)
		return 0;

	if (ioctl(audio_devices[ad].audio_info.fd, AUDIO_GETFD, &duplex) < 0) {
		perror("netbsd_audio_duplex: cannot get duplex mode");
		return 0;
	}
	return duplex;
}


/*
 * netbsd_audio_read: read data from audio device
 *
 * Return: number of bytes read
 */

int
netbsd_audio_read(audio_desc_t ad, u_char * buf, int buf_bytes)
{
	int             fd;
	int             bytes_read = 0;
	int             this_read;

        errno = 0;

	assert(audio_devices && n_devices > ad);
	fd = audio_devices[ad].audio_info.fd;
	if (fd < 0)
		return 0;

	while (buf_bytes > 0) {
		this_read = read(audio_devices[ad].audio_info.fd,
				 (char *) buf, buf_bytes);
		if (this_read < 0) {
			if (errno != EAGAIN && errno != EINTR)
				perror("netbsd_audio_read");
			return bytes_read;
		}
		bytes_read += this_read;
		buf += this_read;
		buf_bytes -= this_read;
	}
	return bytes_read;
}


/*
 * netbsd_audio_write: write data to audio device
 *
 * Return: number of bytes written
 */

int
netbsd_audio_write(audio_desc_t ad, u_char * buf, int buf_bytes)
{
	int             fd;
	int             bytes_written = 0;
	int             this_write;
        
	errno = 0;

	assert(audio_devices && n_devices > ad);
	fd = audio_devices[ad].audio_info.fd;
	if (fd < 0)
		return 0;

	while (buf_bytes > 0) {
		this_write = write(fd, buf, buf_bytes);
		if (this_write < 0) {
			if (errno != EAGAIN && errno != EINTR)
				perror("netbsd_audio_write");
			return bytes_written;
		}
		bytes_written += this_write;
		buf += this_write;
		buf_bytes -= this_write;
	}

	return bytes_written;
}


/*
 * netbsd_audio_non_block: set audio device to non-blocking I/O
 */

void
netbsd_audio_non_block(audio_desc_t ad)
{
	int             on = 1;	/* enable non-blocking I/O */

	assert(audio_devices && n_devices > ad);
	if (audio_devices[ad].audio_info.fd < 0)
		return;

	if (ioctl(audio_devices[ad].audio_info.fd, FIONBIO, &on) < 0)
		perror("netbsd_audio_non_block");
}


/*
 * netbsd_audio_block: set audio device to blocking I/O
 */

void
netbsd_audio_block(audio_desc_t ad)
{
	int             on = 0;	/* disable non-blocking I/O */

	assert(audio_devices && n_devices > ad);
	if (audio_devices[ad].audio_info.fd < 0)
		return;

	if (ioctl(audio_devices[ad].audio_info.fd, FIONBIO, &on) < 0)
		perror("netbsd_audio_block");
}


/*
 * netbsd_audio_iport_count: get number of available input ports.
 */

int
netbsd_audio_iport_count(audio_desc_t ad)
{
#if DEBUG_MIXER > 1
	warnx("netbsd_audio_iport_count(%d):  "
	      "n_rat_in_ports=%d, record_devices=%d",
	      ad, n_rat_in_ports, audio_devices[ad].record.n);
#endif

	assert(audio_devices && n_devices > ad);
	if (audio_devices[ad].audio_info.fd < 0)
		return 0;

	return n_rat_in_ports;
}


/*
 * netbsd_audio_oport_count: get number of available output ports
 */

int
netbsd_audio_oport_count(audio_desc_t ad)
{
#if DEBUG_MIXER > 1
	warnx("netbsd_audio_oport_count(%d):  "
	      "n_rat_out_ports=%d, play_devices=%d",
	      ad, n_rat_out_ports, audio_devices[ad].play.n);
#endif

	assert(audio_devices && n_devices > ad);
	if (audio_devices[ad].audio_info.fd < 0)
		return 0;

	return n_rat_out_ports;
}


/*
 * netbsd_audio_iport_details: get kernel-rat device mappings for input port
 */

const audio_port_details_t *
netbsd_audio_iport_details(audio_desc_t ad, int idx)
{
	assert(audio_devices && n_devices > ad);
	if (audio_devices[ad].audio_info.fd < 0)
		return NULL;
	assert(0 <= idx && idx < n_rat_in_ports);

	return &rat_in_ports[idx];
}


/*
 * netbsd_audio_oport_details: get kernel-rat device mappings for output port
 */

const audio_port_details_t *
netbsd_audio_oport_details(audio_desc_t ad, int idx)
{
	assert(audio_devices && n_devices > ad);
	if (audio_devices[ad].audio_info.fd < 0)
		return NULL;
	assert(0 <= idx && idx < n_rat_out_ports);

	return &rat_out_ports[idx];
}


/*
 * netbsd_audio_get_iport: get information on input port
 */

audio_port_t
netbsd_audio_get_iport(audio_desc_t ad)
{
	audio_port_t    port;

	assert(audio_devices && n_devices > ad);
	if (audio_devices[ad].audio_info.fd < 0)
		return AUDIO_INVALID_PORT;

	update_audio_info(ad);

	port = audio_devices[ad].record.port;
#if DEBUG_MIXER > 1
	warnx("netbsd_audio_get_iport(%d): iport=%d", ad, port);
#endif
	return port;
}


/*
 * netbsd_audio_get_oport: get information on output port
 */

audio_port_t
netbsd_audio_get_oport(audio_desc_t ad)
{
	audio_port_t    port;

	assert(audio_devices && n_devices > ad);
	if (audio_devices[ad].audio_info.fd < 0)
		return AUDIO_INVALID_PORT;

	update_audio_info(ad);

	port = audio_devices[ad].play.port;
#if DEBUG_MIXER > 1
	warnx("netbsd_audio_get_oport(%d):  oport=%d", ad, port);
#endif
	return port;
}


/*
 * netbsd_audio_set_iport: set input port
 */

void
netbsd_audio_set_iport(audio_desc_t ad, audio_port_t port)
{
	int             fd;
	record_gain_ctrl_t *record_ctrl;
	audio_info_t    audio_info;
	mixer_ctrl_t    mixer_info;

#if DEBUG_MIXER > 1
	warnx("netbsd_audio_set_iport(%d,%d)", ad, port);
#endif

	assert(audio_devices && n_devices > ad);
	if (audio_devices[ad].audio_info.fd < 0)
		return;

	update_audio_info(ad);
	record_ctrl = &audio_devices[ad].record.gain_ctrl[port];

	/* audio control */
	if (record_ctrl->gain.type == AUDIO_PORT) {
		fd = audio_devices[ad].audio_info.fd;
		AUDIO_INITINFO(&audio_info);
		audio_info.record.port = record_ctrl->gain.dev;
		audio_info.record.gain
			= average_mixer_level(record_ctrl->gain.un.value);
		if (ioctl(fd, AUDIO_SETINFO, &audio_info) < 0) {
			perror("netbsd_audio_set_iport: "
			       "setting audio parameters");
			return;
		}
	}
	/* mixer controls */
	if (record_ctrl->gain.type != AUDIO_PORT) {
		fd = audio_devices[ad].mixer_info.fd;
		/* source */
		mixer_info = record_ctrl->source;
		if (ioctl(fd, AUDIO_MIXER_WRITE, &mixer_info) < 0) {
			perror("netbsd_audio_set_iport: "
			       "setting mixer record source");
			return;
		}
		/* mute */
		if (record_ctrl->mute.dev != AUDIO_INVALID_PORT) {
			mixer_info = record_ctrl->mute;
			if (ioctl(fd, AUDIO_MIXER_WRITE, &mixer_info) < 0) {
				perror("netbsd_audio_set_iport: "
				       "setting mixer mute");
				return;
			}
		}
		/* preamp */
		if (record_ctrl->preamp.dev != AUDIO_INVALID_PORT) {
			mixer_info = record_ctrl->preamp;
			if (ioctl(fd, AUDIO_MIXER_WRITE, &mixer_info) < 0) {
				perror("netbsd_audio_set_iport: "
				       "setting mixer preamp");
				return;
			}
		}
	}
	audio_devices[ad].record.port = port;
}


/*
 * netbsd_audio_set_oport: set output port
 */

void
netbsd_audio_set_oport(audio_desc_t ad, audio_port_t port)
{
	int             fd;
	mixer_ctrl_t   *play_ctrl;
	audio_info_t    audio_info;

#if DEBUG_MIXER > 1
	warnx("netbsd_audio_set_oport(%d, %d)", ad, port);
#endif

	assert(audio_devices && n_devices > ad);
	if (audio_devices[ad].audio_info.fd < 0)
		return;

	update_audio_info(ad);
	play_ctrl = &audio_devices[ad].play.gain_ctrl[port];

	/* inform kernel only if audio port */
	if (play_ctrl->type == AUDIO_PORT) {
		fd = audio_devices[ad].audio_info.fd;
		AUDIO_INITINFO(&audio_info);
		audio_info.play.port = play_ctrl->dev;
		audio_info.play.gain = play_ctrl->un.value.level[0];
		if (ioctl(fd, AUDIO_SETINFO, &audio_info) < 0) {
			perror("netbsd_audio_set_oport: "
			       "setting audio parameters");
			return;
		}
	}
	audio_devices[ad].play.port = port;
}


/*
 * netbsd_audio_get_igain: get record gain (percent (%) of maximum)
 */

int
netbsd_audio_get_igain(audio_desc_t ad)
{
	int             port;
	record_gain_ctrl_t *record_ctrl;
	int             gain;

	assert(audio_devices && n_devices > ad);
	if (audio_devices[ad].audio_info.fd < 0)
		return 0;

	update_audio_info(ad);

	port = audio_devices[ad].record.port;
	record_ctrl = &audio_devices[ad].record.gain_ctrl[port];
	gain = average_mixer_level(record_ctrl->gain.un.value);
#if DEBUG_MIXER > 1
	warnx("netbsd_audio_get_igain(%d):  average igain=%d", ad, gain);
#endif
	gain = netbsd_device_to_rat(gain);
	return gain;
}


/*
 * netbsd_audio_get_ogain: get play (output) gain (percent (%) of maximum)
 */

int
netbsd_audio_get_ogain(audio_desc_t ad)
{
	int             port;
	mixer_ctrl_t   *play_ctrl;
	int             gain;

	assert(audio_devices && n_devices > ad);
	if (audio_devices[ad].audio_info.fd < 0)
		return 0;

	update_audio_info(ad);

	port = audio_devices[ad].play.port;
	play_ctrl = &audio_devices[ad].play.gain_ctrl[port];
	gain = average_mixer_level(play_ctrl->un.value);
#if DEBUG_MIXER > 1
	warnx("netbsd_audio_get_ogain(%d):  ogain=%d", ad, gain);
#endif
	gain = netbsd_device_to_rat(gain);
	return gain;
}


/*
 * netbsd_audio_set_igain: set record gain (percent (%) of maximum)
 */

void
netbsd_audio_set_igain(audio_desc_t ad, int gain)
{
	int             fd;
	audio_info_t    audio_info;
	mixer_ctrl_t    mixer_info;
	int             port;
	record_gain_ctrl_t *record_ctrl;
	int             i;

#if DEBUG_MIXER > 1
	warnx("netbsd_audio_set_igain(%d,%d)", ad, gain);
#endif

	assert(audio_devices && n_devices > ad);
	if (audio_devices[ad].audio_info.fd < 0)
		return;

	update_audio_info(ad);

	gain = netbsd_rat_to_device(gain);
	port = audio_devices[ad].record.port;
	record_ctrl = &audio_devices[ad].record.gain_ctrl[port];

	/* audio control */
	if (record_ctrl->gain.type == AUDIO_PORT) {
		fd = audio_devices[ad].audio_info.fd;
		AUDIO_INITINFO(&audio_info);
		audio_info.record.gain = gain;
		if (ioctl(fd, AUDIO_SETINFO, &audio_info) < 0) {
			perror("netbsd_audio_set_igain: "
			       "setting audio parameters");
			return;
		}
	}
	/* mixer control */
	if (record_ctrl->gain.type != AUDIO_PORT) {
		fd = audio_devices[ad].mixer_info.fd;
		mixer_info = record_ctrl->gain;
		for (i = 0; i < record_ctrl->gain.un.value.num_channels; ++i)
			mixer_info.un.value.level[i] = gain;
		if (ioctl(fd, AUDIO_MIXER_WRITE, &mixer_info) < 0) {
			perror("netbsd_audio_set_igain: "
			       "setting mixer parameters");
			return;
		}
	}
	/* update record gain */
	for (i = 0; i < record_ctrl->gain.un.value.num_channels; ++i)
		record_ctrl->gain.un.value.level[i] = gain;
}


/*
 * netbsd_audio_set_ogain: set play (output) gain (percent (%) of maximum)
 */

void
netbsd_audio_set_ogain(audio_desc_t ad, int gain)
{
	int             fd;
	audio_info_t    audio_info;
	mixer_ctrl_t    mixer_info;
	int             port;
	mixer_ctrl_t   *play_ctrl;
	int             i;

#if DEBUG_MIXER > 1
	warnx("netbsd_audio_set_ogain(%d, %d)", ad, gain);
#endif

	assert(audio_devices && n_devices > ad);
	if (audio_devices[ad].audio_info.fd < 0)
		return;

	update_audio_info(ad);

	gain = netbsd_rat_to_device(gain);
	port = audio_devices[ad].play.port;
	play_ctrl = &audio_devices[ad].play.gain_ctrl[port];

	/* audio control */
	if (play_ctrl->type == AUDIO_PORT) {
		fd = audio_devices[ad].audio_info.fd;
		AUDIO_INITINFO(&audio_info);
		audio_info.play.gain = gain;
		if (ioctl(fd, AUDIO_SETINFO, &audio_info) < 0) {
			perror("netbsd_audio_set_ogain: "
			       "setting audio parameters");
			return;
		}
	}
	/* mixer control */
	if (play_ctrl->type != AUDIO_PORT) {
		fd = audio_devices[ad].mixer_info.fd;
		mixer_info = *play_ctrl;
		for (i = 0; i < play_ctrl->un.value.num_channels; ++i)
			mixer_info.un.value.level[i] = gain;
		if (ioctl(fd, AUDIO_MIXER_WRITE, &mixer_info) < 0) {
			perror("netbsd_audio_set_ogain: "
			       "setting mixer parameters");
			return;
		}
	}
	/* update play gain */
	for (i = 0; i < play_ctrl->un.value.num_channels; ++i)
		play_ctrl->un.value.level[i] = gain;
}


/*
 * netbsd_audio_loopback: set loopback gain (percent (%) of maximum)
 *
 * Note:  nothing to do; loopback device is not yet supported.
 */

void
netbsd_audio_loopback(audio_desc_t ad, int gain)
{
	int             unused_gain;

#if DEBUG_MIXER > 1
	warnx("netbsd_audio_loopback(%d, %d)", ad, gain);
#endif

	assert(audio_devices && n_devices > ad);
	if (audio_devices[ad].audio_info.fd < 0)
		return;

	unused_gain = gain;	/* XXX - avoid compiler complaints */
}


/*
 * netbsd_audio_is_ready: is audio device ready for reading?
 */

int
netbsd_audio_is_ready(audio_desc_t ad)
{
	assert(audio_devices && n_devices > ad);
	if (audio_devices[ad].audio_info.fd < 0)
		return FALSE;

	return audio_select(ad, 0);
}


/*
 * netbsd_audio_wait_for: delay the process
 */

void
netbsd_audio_wait_for(audio_desc_t ad, int delay_ms)
{
	assert(audio_devices && n_devices > ad);
	if (audio_devices[ad].audio_info.fd < 0)
		return;

	audio_select(ad, delay_ms * 1000);
}


/*
 * netbsd_audio_supports: does the driver support format fmt?
 */

int
netbsd_audio_supports(audio_desc_t ad, audio_format * fmt)
{
	int             fd;
	const audio_encoding_t *encodings;

	assert(audio_devices && n_devices > ad);
	fd = audio_devices[ad].audio_info.fd;
	if (fd < 0)
		return FALSE;

	/*
	 * Note that the fmt argument is not necessarily correctly
	 * filled by rat.  This function is called by
	 * audio_device_supports() in the file auddev.c, which does
	 * not initialize the bits_per_sample field in fmt.  Calling
	 * audio_format_change_encoding() ensures that all fields are
	 * initialized.
	 */
	audio_format_change_encoding(fmt, fmt->encoding);

	encodings = audio_devices[ad].audio_info.audio_enc;
	return match_encoding(fmt, encodings) != NULL;
}


/*
 * probe_device: probe audio and mixer devices
 */

int
probe_device(const char *audio_dev_name, const char *mixer_dev_name)
{
	audio_dinfo_t   audio_info;
	mixer_dinfo_t   mixer_info;

	if (!probe_audio_device(audio_dev_name, &audio_info))
		return FALSE;
	probe_mixer_device(mixer_dev_name, &mixer_info);

#if DEBUG_MIXER > 1
	warnx("  %s", audio_dev_name);
#endif

	audio_devices = (audio_devices_t *) realloc
		(audio_devices, (n_devices + 1) * sizeof(audio_devices_t));
	audio_devices[n_devices].full_name
		= (char *) malloc(strlen(audio_info.audio_dev.name)
				  + strlen(audio_info.dev_name)
				  + 4 + 1);
	strcpy(audio_devices[n_devices].full_name,
	       audio_info.audio_dev.name);
	strcat(audio_devices[n_devices].full_name, " -- ");
	strcat(audio_devices[n_devices].full_name, audio_info.dev_name);
	audio_devices[n_devices].audio_info = audio_info;
	audio_devices[n_devices].mixer_info = mixer_info;
	++n_devices;

	return TRUE;
}


/*
 * probe_audio_device: probe audio device
 */

int
probe_audio_device(const char *dev_name, audio_dinfo_t * audio_info)
{
	int             fd;
	audio_encoding_t encoding;
	int             n;
	int             i;

	debug_msg("probe_audio_device(%s)\n", dev_name);

	if ((fd = open(dev_name, O_RDWR | O_NONBLOCK, 0)) < 0)
		return FALSE;

	if (ioctl(fd, AUDIO_GETDEV, &audio_info->audio_dev) < 0) {
		perror("getting audio device info");
		close(fd);
		return FALSE;
	}
	if (ioctl(fd, AUDIO_GETPROPS, &audio_info->audio_props) < 0) {
		perror("getting audio device properties");
		close(fd);
		return FALSE;
	}
	if (!(audio_info->audio_props & AUDIO_PROP_FULLDUPLEX)) {
		warnx("skipping %s; only full duplex devices supported.",
		      audio_info->dev_name);
		close(fd);
		return FALSE;
	}
	/* get supported encodings */
	for (n = 0;; n++) {
		encoding.index = n;
		if (ioctl(fd, AUDIO_GETENC, &encoding) < 0)
			break;
	}
	audio_info->audio_enc = calloc(n + 1, sizeof *audio_info->audio_enc);
	for (i = 0; i < n; i++) {
		audio_info->audio_enc[i].index = i;
		ioctl(fd, AUDIO_GETENC, &audio_info->audio_enc[i]);
	}

	close(fd);

	audio_info->dev_name = strdup(dev_name);
	audio_info->fd = INVALID_FD;
	AUDIO_INITINFO(&audio_info->audio_info);
	return TRUE;
}


/*
 * probe_mixer_device: probe device mixer
 */

int
probe_mixer_device(const char *dev_name, mixer_dinfo_t * mixer_info)
{
	int             fd;

	debug_msg("probe_mixer_device(%s)\n", dev_name);

	if ((fd = open(dev_name, O_RDONLY | O_NONBLOCK, 0)) < 0) {
		return FALSE;
	}
	close(fd);

	mixer_info->dev_name = strdup(dev_name);
	mixer_info->fd = INVALID_FD;
	return TRUE;
}

/*
 * set_audio_info: set kernel data structure
 *
 * Return: initialized kernel data structure
 */

audio_info_t
set_audio_info(audio_format * ifmt, audio_format * ofmt)
{
	audio_info_t    audio_info;
	int             blocksize;
	int             i;

	AUDIO_INITINFO(&audio_info);

	/*
         * Note that AUMODE_PLAY_ALL is critical here to ensure continuous
         * audio output.  Rat attempts to manage the timing of the output
         * audio stream; it is important that the kernel not attempt to do the
         * same.
         */
	audio_info.mode = AUMODE_PLAY_ALL | AUMODE_RECORD;

	audio_info.play.sample_rate = ofmt->sample_rate;
	audio_info.play.channels = ofmt->channels;
	audio_info.play.precision = ofmt->bits_per_sample;
	audio_info.play.encoding = ofmt->encoding;

	audio_info.record.sample_rate = ifmt->sample_rate;
	audio_info.record.channels = ifmt->channels;
	audio_info.record.precision = ifmt->bits_per_sample;
	audio_info.record.encoding = ifmt->encoding;

	blocksize = max(ifmt->bytes_per_block, ofmt->bytes_per_block);
	i = 1;
	while (blocksize) {
		blocksize >>= 1;
		i <<= 1;
	}
	audio_info.blocksize = i;	/* next power of 2 larger than
					 * blocksize */

	return audio_info;
}

/*
 * match_encoding: get audio device encoding matching fmt
 *
 * Find the best match among supported encodings.  Prefer encodings
 * that are not emulated in the kernel.
 */

const audio_encoding_t *
match_encoding(audio_format * fmt, const audio_encoding_t * encodings)
{
	const audio_encoding_t *best_encp = NULL;
	struct audio_encoding_match *match_encp;
	const audio_encoding_t *encp;

	for (match_encp = audio_encoding_match;
	     match_encp->netbsd_encoding != AUDIO_ENCODING_NONE;
	     ++match_encp) {

		if (match_encp->rat_encoding != fmt->encoding)
			continue;	/* skip mismatched encodings */

		/* scan supported encodings */
		for (encp = encodings; encp->precision != 0; ++encp) {
			if (encp->encoding == match_encp->netbsd_encoding
			    && encp->precision == fmt->bits_per_sample
			    && (best_encp == NULL
			 || (best_encp->flags & AUDIO_ENCODINGFLAG_EMULATED)
			 || !(encp->flags & AUDIO_ENCODINGFLAG_EMULATED))) {
				best_encp = encp;
			}
		}
	}
	return best_encp;
}


/*
 * audio_select: wait for delay_us microseconds
 */

int
audio_select(audio_desc_t ad, int delay_us)
{
	int             fd;
	fd_set          rfds;
	struct timeval  tv;

	fd = audio_devices[ad].audio_info.fd;
	tv.tv_sec = 0;
	tv.tv_usec = delay_us;

	FD_ZERO(&rfds);
	FD_SET(fd, &rfds);

	select(fd + 1, &rfds, NULL, NULL, &tv);

	return FD_ISSET(fd, &rfds) ? TRUE : FALSE;
}

/*
 * update_audio_info
 *
 * Update the in-memory data structures from the kernel.  This enables
 * rat to respond to any audio or mixer device changes occuring via
 * any other mechanism.  Always update all audio/mixer devices so they
 * are accurate when new ports are selected.
 */

void
update_audio_info(audio_desc_t ad)
{
	int             fd;
	audio_info_t    audio_info;
	mixer_ctrl_t    mixer_info;
	mixer_ctrl_t   *play_ctrl;
	record_gain_ctrl_t *record_ctrl;
	int             i;
	int             j;

	/* audio device info */
	fd = audio_devices[ad].audio_info.fd;
	if (ioctl(fd, AUDIO_GETINFO, &audio_info) < 0)
		err(1, "getting audio parameters");

	/* play info */
	for (i = 0; i < audio_devices[ad].play.n; ++i) {
		play_ctrl = &audio_devices[ad].play.gain_ctrl[i];
		/* audio control */
		if (play_ctrl->type == AUDIO_PORT
		    && play_ctrl->dev == (int) audio_info.play.port) {
			play_ctrl->un.value.level[0] = audio_info.play.gain;
		}
		/* mixer control */
		if (play_ctrl->type != AUDIO_PORT) {
			fd = audio_devices[ad].mixer_info.fd;
			mixer_info = *play_ctrl;
			if (ioctl(fd, AUDIO_MIXER_READ, &mixer_info) < 0)
				err(1, "getting mixer parameters (%d)", i);
			play_ctrl->un.value = mixer_info.un.value;
		}
	}

	/* record info */
	for (i = 0; i < audio_devices[ad].record.n; ++i) {
		record_ctrl = &audio_devices[ad].record.gain_ctrl[i];
		if (record_ctrl->gain.type == AUDIO_PORT
		 && record_ctrl->gain.dev == (int) audio_info.record.port) {
			for (j = 0; j < record_ctrl->gain.un.value.num_channels;
			     ++j) {
				record_ctrl->gain.un.value.level[j]
					= audio_info.record.gain;
			}
		}
		/* mixer control */
		if (record_ctrl->gain.type != AUDIO_PORT) {
			fd = audio_devices[ad].mixer_info.fd;
			mixer_info = record_ctrl->gain;
			if (ioctl(fd, AUDIO_MIXER_READ, &mixer_info) < 0)
				err(1, "getting mixer gain parameters (%d)", i);
			record_ctrl->gain = mixer_info;
		}
	}
}

/*
 * average_mixer_level: calculate the average across channels
 */

u_char
average_mixer_level(mixer_level_t mixer_level)
{
	int             sum = 0;
	int             i;

	for (i = 0; i < mixer_level.num_channels; ++i)
		sum += mixer_level.level[i];
	return sum / mixer_level.num_channels;
}

/*
 * find_gain_ctrl: construct dynamic lists of gain controls
 */

void
find_gain_ctrl(audio_desc_t ad)
{
	mixer_devinfo_t *mixers;

	mixers = mixer_devices(ad);

	audio_devices[ad].play.port = AUDIO_INVALID_PORT;
	audio_devices[ad].play.n = 0;
	audio_devices[ad].play.gain_ctrl = NULL;
	audio_devices[ad].record.port = AUDIO_INVALID_PORT;
	audio_devices[ad].record.n = 0;
	audio_devices[ad].record.gain_ctrl = NULL;

	find_audio_play_ctrl(ad);
	find_audio_record_ctrl(ad);
	find_mixer_play_ctrl(ad, mixers);
	find_mixer_record_ctrl(ad, mixers);

	if (audio_devices[ad].play.port == AUDIO_INVALID_PORT)
		audio_devices[ad].play.port = 0;
	if (audio_devices[ad].record.port == AUDIO_INVALID_PORT)
		audio_devices[ad].record.port = 0;

	map_netbsd_ports(ad);
	fix_rat_names();

#if DEBUG_MIXER > 0
	print_gain_ctrl(ad, mixers);
#endif

	free(mixers);
}

/*
 * find_audio_play_ctrl: append audio output gain controls to list
 */

void
find_audio_play_ctrl(audio_desc_t ad)
{
	int             fd;
	audio_info_t    audio_devinfo;
	mixer_ctrl_t   *mp;
	audio_port_details_t *ap;
	int             n = 0;

	fd = audio_devices[ad].audio_info.fd;
	if (ioctl(fd, AUDIO_GETINFO, &audio_devinfo) < 0) {
		perror("reading audio device information");
		return;
	}
	for (ap = netbsd_out_ports;
	     ap < netbsd_out_ports + NETBSD_NUM_OUTPORTS;
	     ++ap) {
		if (audio_devinfo.play.avail_ports & ap->port) {
			++n;
			++audio_devices[ad].play.n;
			audio_devices[ad].play.gain_ctrl
				= (mixer_ctrl_t *) realloc
				(audio_devices[ad].play.gain_ctrl,
			   audio_devices[ad].play.n * sizeof(mixer_ctrl_t));
			mp = audio_devices[ad].play.gain_ctrl
				+ audio_devices[ad].play.n - 1;
			mp->dev = ap->port;
			mp->type = AUDIO_PORT;
			mp->un.value.num_channels = 1;
			mp->un.value.level[0]
				= (audio_devinfo.play.port == ap->port ?
				   audio_devinfo.play.gain : AUDIO_MID_GAIN);

			/* set default audio play port */
			if (ap->port == AUDIO_SPEAKER)
				audio_devices[ad].play.port
					= audio_devices[ad].play.n - 1;
		}
	}

	if (n == 0) {		/* construct default output port */
		++n;
		++audio_devices[ad].play.n;
		audio_devices[ad].play.gain_ctrl = (mixer_ctrl_t *) realloc
			(audio_devices[ad].play.gain_ctrl,
			 audio_devices[ad].play.n * sizeof(mixer_ctrl_t));
		mp = audio_devices[ad].play.gain_ctrl
			+ audio_devices[ad].play.n - 1;
		mp->dev = 0;
		mp->type = AUDIO_PORT;
		mp->un.value.num_channels = 1;
		mp->un.value.level[0] = audio_devinfo.play.gain;

		/* set default audio play port */
		audio_devices[ad].play.port = audio_devices[ad].play.n - 1;
	}
}

/*
 * find_audio_record_ctrl: append audio input gain controls to list
 */

void
find_audio_record_ctrl(audio_desc_t ad)
{
	int             fd;
	audio_info_t    audio_devinfo;
	record_gain_ctrl_t *gp;
	audio_port_details_t *ap;
	int             n = 0;

	fd = audio_devices[ad].audio_info.fd;
	if (ioctl(fd, AUDIO_GETINFO, &audio_devinfo) < 0) {
		perror("reading audio device information");
		return;
	}
	for (ap = netbsd_in_ports; ap < netbsd_in_ports + NETBSD_NUM_INPORTS;
	     ++ap) {
		if (audio_devinfo.record.avail_ports & ap->port) {
			++n;
			append_record_gain_ctrl(&audio_devices[ad].record);
			gp = audio_devices[ad].record.gain_ctrl
				+ audio_devices[ad].record.n - 1;
			gp->gain.dev = ap->port;
			gp->gain.type = AUDIO_PORT;
			gp->gain.un.value.num_channels = 1;
			gp->gain.un.value.level[0]
				= (audio_devinfo.record.port == ap->port ?
				audio_devinfo.record.gain : AUDIO_MID_GAIN);
		}
		/* set default audio record port */
		if (ap->port == AUDIO_MICROPHONE)
			audio_devices[ad].record.port
				= audio_devices[ad].record.n - 1;
	}
	if (n == 0) {		/* construct default output port */
		append_record_gain_ctrl(&audio_devices[ad].record);
		gp = audio_devices[ad].record.gain_ctrl
			+ audio_devices[ad].record.n - 1;
		gp->gain.dev = 0;
		gp->gain.type = AUDIO_PORT;
		gp->gain.un.value.num_channels = 1;
		gp->gain.un.value.level[0] = audio_devinfo.record.gain;

		/* set default audio record port */
		audio_devices[ad].record.port = audio_devices[ad].record.n - 1;
	}
}

/*
 * find_mixer_play_ctrl: append mixer output gain controls to list
 */

void
find_mixer_play_ctrl(audio_desc_t ad, mixer_devinfo_t * mixers)
{
	const mixer_devinfo_t *mixer;
	audio_devices_t *ap;
	mixer_ctrl_t   *mp;
	int             i;
	mixer_devices_t *dp;

	ap = audio_devices + ad;
	if (ap->mixer_info.fd < 0)
		return;

	/* find outputs.master or inputs.dac */
	for (mixer = mixers, i = 0;
	     mixer->type != AUDIO_INVALID_PORT; ++mixer, ++i) {
		for (dp = mixer_play_gain; dp->class; ++dp) {
			if (strcmp(dp->class, mixers[mixer->mixer_class].
				   label.name) == 0
			    && strcmp(dp->device, mixer->label.name) == 0
			    && mixer->type == AUDIO_MIXER_VALUE) {
				++ap->play.n;
				ap->play.gain_ctrl = (mixer_ctrl_t *) realloc
					(ap->play.gain_ctrl, ap->play.n
					 * sizeof(mixer_ctrl_t));
				mp = ap->play.gain_ctrl + ap->play.n - 1;
				mp->dev = i;
				mp->type = mixer->type;
				mp->un.value.num_channels
					= mixer->un.v.num_channels;

				/* set default mixer play port */
				audio_devices[ad].play.port = ap->play.n - 1;
				break;
			}
		}
	}
}

/*
 * find_mixer_record_ctrl: append mixer input gain controls to list
 */

void
find_mixer_record_ctrl(audio_desc_t ad, mixer_devinfo_t * mixers)
{
	const mixer_devinfo_t *rec_src;
	int             rec_src_class;
	const mixer_devinfo_t *rec_volume;
	int             rec_volume_class;
	const mixer_devinfo_t *gain;
	int             gain_class;
	const mixer_devinfo_t *first_child;
	const mixer_devinfo_t *child;

	audio_devices_t *ap;
	record_gain_ctrl_t *mp;
	int             src;
	int             ord;

	ap = audio_devices + ad;
	if (ap->mixer_info.fd < 0)
		return;

	/* find record.source */
	for (rec_src = mixers; rec_src->type != AUDIO_INVALID_PORT; ++rec_src) {
		rec_src_class = rec_src->mixer_class;
		if (strcmp(mixers[rec_src_class].label.name, AudioCrecord) == 0
		    && strcmp(rec_src->label.name, AudioNsource) == 0
		    && rec_src->type == AUDIO_MIXER_ENUM)
			break;
	}
	if (rec_src->type == AUDIO_INVALID_PORT)	/* no record.source
							 * found */
		return;

	/* find record.volume */
	for (rec_volume = mixers; rec_volume->type != AUDIO_INVALID_PORT;
	     ++rec_volume) {
		rec_volume_class = rec_volume->mixer_class;
		if (strcmp(mixers[rec_volume_class].label.name, AudioCrecord)
		    == 0
		    && strcmp(rec_volume->label.name, AudioNvolume) == 0
		    && rec_volume->type == AUDIO_MIXER_VALUE)
			break;
	}

	/* for each possible source */
	for (src = 0; src < rec_src->un.e.num_mem; ++src) {
		/* is a gain port available? */
		for (gain = mixers; gain->type != AUDIO_INVALID_PORT; ++gain) {
			gain_class = gain->mixer_class;
			if (strcmp(mixers[gain_class].label.name,
				   AudioCinputs) == 0
			    && strcmp(gain->label.name,
				      rec_src->un.e.member[src].label.name)
			    == 0
			    && gain->type == AUDIO_MIXER_VALUE)
				break;
		}
		if (gain->type == AUDIO_INVALID_PORT) {	/* no gain port */
			if (rec_volume->type != AUDIO_INVALID_PORT)
				gain = rec_volume;	/* use record.volume
							 * instead */
			else
				continue;	/* no gain port anywhere! */
		}
		/* extend table */
		append_record_gain_ctrl(&ap->record);
		mp = &ap->record.gain_ctrl[ap->record.n - 1];

		/* gain info. */
		mp->gain.dev = gain->index;
		mp->gain.type = gain->type;
		mp->gain.un.value.num_channels = gain->un.v.num_channels;

		/* source info. */
		mp->source.dev = rec_src->index;
		mp->source.type = rec_src->type;
		mp->source.un.ord = rec_src->un.e.member[src].ord;

		/* find first in list */
		first_child = gain;
		while (first_child->prev != AUDIO_MIXER_LAST)
			first_child = &mixers[first_child->prev];

		/* mute info. */
		for (child = first_child; child;
		     child = child->next != AUDIO_MIXER_LAST ?
		     &mixers[child->next] : NULL) {
			if (strcmp(child->label.name, AudioNmute) == 0
			    && child->type == AUDIO_MIXER_ENUM) {
				for (ord = 0; ord < child->un.e.num_mem; ++ord) {
					if (strcmp(child->un.e.member[ord].
					      label.name, AudioNoff) == 0) {
						mp->mute.dev = child->index;
						mp->mute.type = child->type;
						mp->mute.un.ord
							= child->un.e.member[ord].ord;
						break;
					}
				}
				break;
			}
		}

		/*
		 * preamp info. XXX - this must come last, because it
		 * duplicates the current record for each preamp state
		 */
		for (child = first_child; child;
		     child = child->next != AUDIO_MIXER_LAST ?
		     &mixers[child->next] : NULL) {
			if (strcmp(child->label.name, AudioNpreamp) == 0
			    && child->type == AUDIO_MIXER_ENUM) {
				assert(child->un.e.num_mem == 2);	/* off, on */
				mp->preamp.dev = child->index;
				mp->preamp.type = child->type;
				mp->preamp.un.ord = child->un.e.member[0].ord;
				break;
			}
		}

		/* set default mixer record port */
		if (strcmp(mixers[gain->mixer_class].label.name,
			   AudioCinputs) == 0
		    && strcmp(gain->label.name, AudioNmicrophone) == 0)
			audio_devices[ad].record.port
				= audio_devices[ad].record.n - 1;
	}
}

/*
 * map_netbsd_ports
 *
 * Map a selected subset of the kernel audio and mixer ports to
 * entries in the rat device mapping table.  Whenever the mixer device
 * is available, it will duplicate ports that are also available via
 * the audio devices much simpler interface.  Consequently, we prefer
 * to provide access to the mixer devices if they are available.
 * Otherwise, map the audio devices for rat.
 */

void
map_netbsd_ports(audio_desc_t ad)
{
	int             mixer_fd;
	mixer_devinfo_t mixer_info;
	mixer_ctrl_t   *source;
	int             i;
	int             j;
	int             len;
	int             m;
	int             n;
	char            name[MAX_AUDIO_DEV_LEN + 3 + 1];

	/* give priority to mixer devices */
	mixer_fd = audio_devices[ad].mixer_info.fd;
	if (mixer_fd > 0) {
		/* mixer output ports */
		for (i = 0; i < audio_devices[ad].play.n; ++i) {
			if (audio_devices[ad].play.gain_ctrl[i].type
			    == AUDIO_PORT)
				continue;
			mixer_info.index = audio_devices[ad].play.gain_ctrl[i].
				dev;
			if (ioctl(mixer_fd, AUDIO_MIXER_DEVINFO, &mixer_info) < 0) {
				perror("map_netbsd_ports(): "
				       "getting play mixer device info");
				continue;
			}
			++n_rat_out_ports;
			rat_out_ports = (audio_port_details_t *) realloc
				(rat_out_ports, n_rat_out_ports
				 * sizeof(audio_port_details_t));
			rat_out_ports[n_rat_out_ports - 1].port = i;
			len = AUDIO_PORT_NAME_LENGTH;
			rat_out_ports[n_rat_out_ports - 1].name[0] = '\0';
			strncat(rat_out_ports[n_rat_out_ports - 1].name,
				mixer_info.label.name, len);
		}

		/* mixer input ports */
		for (i = 0; i < audio_devices[ad].record.n; ++i) {
			if (audio_devices[ad].record.gain_ctrl[i].gain.type
			    == AUDIO_PORT)
				continue;
			source = &audio_devices[ad].record.gain_ctrl[i].source;
			mixer_info.index = source->dev;
			if (ioctl(mixer_fd, AUDIO_MIXER_DEVINFO, &mixer_info)
			    < 0) {
				perror("map_netbsd_ports(): "
				       "getting record mixer device info");
				continue;
			}
			++n_rat_in_ports;
			rat_in_ports = (audio_port_details_t *) realloc
				(rat_in_ports, n_rat_in_ports
				 * sizeof(audio_port_details_t));
			rat_in_ports[n_rat_in_ports - 1].port = i;
			len = AUDIO_PORT_NAME_LENGTH;
			rat_in_ports[n_rat_in_ports - 1].name[0] = '\0';
			strncat(rat_in_ports[n_rat_in_ports - 1].name,
				mixer_info.un.e.member[source->un.ord].label.name, len);
		}

		/* handle input ports with preamps */
		n = audio_devices[ad].record.n;
		for (i = 0; i < n; ++i) {
			if (audio_devices[ad].record.gain_ctrl[i].gain.type
			    == AUDIO_PORT)
				continue;
			if (audio_devices[ad].record.gain_ctrl[i].preamp.dev < 0)
				continue;
			mixer_info.index = audio_devices[ad].record.gain_ctrl[i].
				preamp.dev;
			if (ioctl(mixer_fd, AUDIO_MIXER_DEVINFO, &mixer_info)
			    < 0) {
				perror("map_netbsd_ports(): "
				 "getting record mixer preamp device info");
				continue;
			}
			if (strcmp(mixer_info.label.name, AudioNpreamp) != 0
			    || mixer_info.type != AUDIO_MIXER_ENUM)
				continue;
			assert(mixer_info.un.e.num_mem == 2);	/* off, on */

			/* update audio_devices */
			copy_record_gain_ctrl(&audio_devices[ad].record,
				    &audio_devices[ad].record.gain_ctrl[i]);
			m = audio_devices[ad].record.n - 1;
			audio_devices[ad].record.gain_ctrl[i].preamp.un.ord =
				mixer_info.un.e.member[0].ord;
			audio_devices[ad].record.gain_ctrl[m].preamp.un.ord =
				mixer_info.un.e.member[1].ord;
			/* update rat port maps */
			for (j = 0; j < n_rat_in_ports; ++j) {
				if ((int) rat_in_ports[j].port != i)
					continue;
				copy_rat_in_port(&rat_in_ports[j]);
				m = n_rat_in_ports - 1;
				name[0] = '\0';
				strcat(name, " (");
				strcat(name, mixer_info.un.e.member[0].
				       label.name);
				strcat(name, ")");
				strncat(rat_in_ports[j].name, name,
					AUDIO_PORT_NAME_LENGTH);
				rat_in_ports[m].port
					= audio_devices[ad].record.n - 1;
				name[0] = '\0';
				strcat(name, " (");
				strcat(name, mixer_info.un.e.member[1].
				       label.name);
				strcat(name, ")");
				strncat(rat_in_ports[m].name, name,
					AUDIO_PORT_NAME_LENGTH);
				break;
			}
		}
	} else {		/* otherwise settle for audio devices */
		/* audio output ports */
		for (i = 0; i < audio_devices[ad].play.n; ++i) {
			if (audio_devices[ad].play.gain_ctrl[i].type
			    != AUDIO_PORT)
				continue;
			++n_rat_out_ports;
			rat_out_ports = (audio_port_details_t *) realloc
				(rat_out_ports, n_rat_out_ports
				 * sizeof(audio_port_details_t));
			rat_out_ports[n_rat_out_ports - 1].port = i;
			len = AUDIO_PORT_NAME_LENGTH;
			rat_out_ports[n_rat_out_ports - 1].name[0] = '\0';
			strncat(rat_out_ports[n_rat_out_ports - 1].name,
				netbsd_out_ports[i].name, len);
		}

		/* audio input ports */
		for (i = 0; i < audio_devices[ad].record.n; ++i) {
			if (audio_devices[ad].record.gain_ctrl[i].gain.type
			    != AUDIO_PORT)
				continue;
			++n_rat_in_ports;
			rat_in_ports = (audio_port_details_t *) realloc
				(rat_in_ports, n_rat_in_ports
				 * sizeof(audio_port_details_t));
			rat_in_ports[n_rat_in_ports - 1].port = i;
			len = AUDIO_PORT_NAME_LENGTH;
			rat_in_ports[n_rat_in_ports - 1].name[0] = '\0';
			strncat(rat_in_ports[n_rat_in_ports - 1].name,
				netbsd_in_ports[i].name, len);
		}
	}
}

/*
 * fix_rat_names
 *
 * Appropriately capitalize names in the rat device mapping table for
 * the rat GUI
 */

void
fix_rat_names()
{
	int             i;
	char           *k;

	/* play port names */
	for (i = 0; i < n_rat_out_ports; ++i) {
		k = rat_out_ports[i].name;
		if (strcmp(k, "cd") == 0 || strcmp(k, "dac") == 0) {
			while (*k) {
				*k = toupper((int) *k);
				++k;
			}
		} else
			*k = toupper((int) *k);
	}

	/* record port names */
	for (i = 0; i < n_rat_in_ports; ++i) {
		k = rat_in_ports[i].name;
		if (strcmp(k, "cd") == 0 || strcmp(k, "dac") == 0) {
			while (*k) {
				*k = toupper((int) *k);
				++k;
			}
		} else
			*k = toupper((int) *k);
	}
}

/*
 * mixer_devices: construct table of all mixer devices
 */

mixer_devinfo_t *
mixer_devices(audio_desc_t ad)
{
	int             fd;
	mixer_devinfo_t devinfo;
	mixer_devinfo_t *mixers;
	int             n;
	int             i;

	fd = audio_devices[ad].mixer_info.fd;
	if (fd < 0)
		return NULL;

	/* count number of devices */
	n = 0;
	while (1) {
		devinfo.index = n;
		if (ioctl(fd, AUDIO_MIXER_DEVINFO, &devinfo) < 0)
			break;
		++n;
	}

	mixers = (mixer_devinfo_t *) malloc((n + 1) * sizeof(mixer_devinfo_t));

	/* get device info. */
	for (i = 0; i < n; ++i) {
		mixers[i].index = i;
		if (ioctl(fd, AUDIO_MIXER_DEVINFO, &mixers[i]) < 0)
			break;
	}

	mixers[n].type = AUDIO_INVALID_PORT;

	return mixers;
}

/*
 * append_record_gain_ctrl: append an empty record to a record_gain_t[]
 */

void
append_record_gain_ctrl(record_gain_t * gp)
{
	record_gain_ctrl_t *cp;

	/* append a new record */
	++gp->n;
	gp->gain_ctrl = (record_gain_ctrl_t *) realloc
		(gp->gain_ctrl, gp->n * sizeof(record_gain_ctrl_t));

	/* initialize new record */
	cp = gp->gain_ctrl + gp->n - 1;
	cp->source.dev = AUDIO_INVALID_PORT;
	cp->gain.dev = AUDIO_INVALID_PORT;
	cp->mute.dev = AUDIO_INVALID_PORT;
	cp->preamp.dev = AUDIO_INVALID_PORT;
}

/*
 * copy_record_gain_ctrl
 *
 * Append a duplicate record (src) to a record_gain_t[].  This
 * function is for creating duplicate entries for mixer controls that
 * have preamps.  The paired records will be used to correspond to the
 * preamp being off and on.
 */

void
copy_record_gain_ctrl(record_gain_t * gains, record_gain_ctrl_t * src)
{
	/* append a new record */
	++gains->n;
	gains->gain_ctrl = (record_gain_ctrl_t *) realloc
		(gains->gain_ctrl, gains->n * sizeof(*gains->gain_ctrl));

	/* duplicate contents */
	memcpy(gains->gain_ctrl + gains->n - 1, src,
	       sizeof(*gains->gain_ctrl));
}

/*
 * copy_rat_in_port
 *
 * Append a duplicate of src to the rat_in_ports array.  This function
 * is for creating duplicate entries in the rat device mapping table
 * for mixer controls that have preamps.  The paired records will be
 * used to correspond to the preamp being off and on.
 */

void
copy_rat_in_port(audio_port_details_t * src)
{
	/* append a new record */
	++n_rat_in_ports;
	rat_in_ports = (audio_port_details_t *) realloc
		(rat_in_ports, n_rat_in_ports * sizeof(*rat_in_ports));

	/* duplicate contents */
	memcpy(rat_in_ports + n_rat_in_ports - 1, src, sizeof(*rat_in_ports));
}

#if DEBUG_MIXER > 0

/*
 * print_audio_properties: report encodings and audio properties
 */

void
print_audio_properties(audio_desc_t ad)
{
	const audio_encoding_t *encp;

	/* report supported encodings */
	warnx("Supported encodings:");
	encp = audio_devices[ad].audio_info.audio_enc;
	while (encp->precision != 0) {
		warnx("  %s: %d bits/sample %s",
		      encp->name, encp->precision,
		      encp->flags & AUDIO_ENCODINGFLAG_EMULATED ?
		      "(emulated)" : "");
		++encp;
	}

	/* report device properties */
	warnx("Device properties: ");
	if (audio_devices[ad].audio_info.audio_props & AUDIO_PROP_INDEPENDENT)
		warnx("  independent");
	if (audio_devices[ad].audio_info.audio_props & AUDIO_PROP_FULLDUPLEX)
		warnx("  full duplex");
	if (audio_devices[ad].audio_info.audio_props & AUDIO_PROP_MMAP)
		warnx("  mmap");
}

/*
 * print_audio_formats: report audio format types
 */

void
print_audio_formats(audio_desc_t ad, audio_format * ifmt, audio_format * ofmt)
{
	warnx("NetBSD audio input format:");
	warnx("  %d Hz, %d bits/sample, %d %s",
	      audio_devices[ad].audio_info.audio_info.record.sample_rate,
	      audio_devices[ad].audio_info.audio_info.record.precision,
	      audio_devices[ad].audio_info.audio_info.record.channels,
	      audio_devices[ad].audio_info.audio_info.record.channels == 1 ?
	      "channel" : "channels");
	warnx("  %d byte blocks",
	      audio_devices[ad].audio_info.audio_info.blocksize);
	warnx("  requested rat encoding %d", ifmt->encoding);
	warnx("  mapped kernel encoding %d: %s",
	      audio_devices[ad].audio_info.record_encoding->encoding,
	      audio_devices[ad].audio_info.record_encoding->name);
	if (audio_devices[ad].audio_info.record_encoding->flags
	    & AUDIO_ENCODINGFLAG_EMULATED)
		warnx("  NetBSD %s support is emulated",
		      audio_devices[ad].audio_info.play_encoding->name);

	warnx("NetBSD audio output format:");
	warnx("  %d Hz, %d bits/sample, %d %s",
	      audio_devices[ad].audio_info.audio_info.play.sample_rate,
	      audio_devices[ad].audio_info.audio_info.play.precision,
	      audio_devices[ad].audio_info.audio_info.play.channels,
	      audio_devices[ad].audio_info.audio_info.play.channels == 1 ?
	      "channel" : "channels");
	warnx("  %d byte blocks",
	      audio_devices[ad].audio_info.audio_info.blocksize);
	warnx("  requested rat encoding %d", ofmt->encoding);
	warnx("  mapped kernel encoding %d: %s",
	      audio_devices[ad].audio_info.play_encoding->encoding,
	      audio_devices[ad].audio_info.play_encoding->name);
	if (audio_devices[ad].audio_info.play_encoding->flags
	    & AUDIO_ENCODINGFLAG_EMULATED)
		warnx("  NetBSD %s support is emulated",
		      audio_devices[ad].audio_info.play_encoding->name);
}

/*
 * print_gain_ctrl: report array of gain controls
 */

void
print_gain_ctrl(audio_desc_t ad, mixer_devinfo_t * mixers)
{
	audio_port_details_t *ap = NULL;
	int             i;
	int             port;
	record_gain_ctrl_t *iport;
	mixer_ctrl_t   *oport;
	int             gain_class;
	int             source_class;
	int             source;
	int             gain;
	int             mute;
	int             preamp;
	int             ord;


	/* report audio/mixer play controls */
	warnx("NetBSD play gain controls: %s", audio_devices[ad].full_name);
	for (i = 0; i < n_rat_out_ports; ++i) {
		port = rat_out_ports[i].port;
		oport = &audio_devices[ad].play.gain_ctrl[port];
		if (oport->type == AUDIO_PORT) {
			for (ap = netbsd_out_ports;
			     ap < netbsd_out_ports + NETBSD_NUM_OUTPORTS;
			     ++ap) {
				if ((int) ap->port == oport->dev)
					warnx("  %2d. audio: %s%s (%d) ==> %s",
					      port, ap->name,
					      (port ==
					       audio_devices[ad].play.port ?
					       "*" : ""),
					      oport->dev,
					      rat_out_ports[i].name);
			}
		} else {	/* mixer control */
			gain = oport->dev;
			gain_class = mixers[gain].mixer_class;
			warnx("  %2d. mixer: %s.%s%s (%d) ==> %s", port,
			      mixers[gain_class].label.name,
			      mixers[gain].label.name,
			   (port == audio_devices[ad].play.port ? "*" : ""),
			      gain,
			      rat_out_ports[i].name);
		}
	}

	/* report audio/mixer record controls */
	warnx("NetBSD record gain controls: %s", audio_devices[ad].full_name);
	for (i = 0; i < n_rat_in_ports; ++i) {
		port = rat_in_ports[i].port;
		iport = &audio_devices[ad].record.gain_ctrl[port];
		if (iport->gain.type == AUDIO_PORT) {
			for (ap = netbsd_in_ports;
			     ap < netbsd_in_ports + NETBSD_NUM_INPORTS;
			     ++ap) {
				if ((int) ap->port
				    == iport->gain.dev)
					warnx("  %2d. audio: %s%s (%d) ==> %s",
					      port, ap->name,
					      (port ==
					     audio_devices[ad].record.port ?
					       "*" : ""),
					      iport->gain.dev,
					      rat_in_ports[i].name);
			}
		} else {	/* mixer control */
			gain = iport->gain.dev;
			gain_class = mixers[gain].mixer_class;
			source = iport->source.dev;
			source_class = mixers[source].mixer_class;
			ord = iport->source.un.ord;
			mute = iport->mute.dev;
			preamp = iport->preamp.dev;
			warnx("  %2d. mixer: gain=%s.%s%s (%d) ==> %s",
			      port,
			      mixers[gain_class].label.name,
			      mixers[gain].label.name,
			 (port == audio_devices[ad].record.port ? "*" : ""),
			      gain,
			      rat_in_ports[i].name);
			warnx("             %s.%s=%s (%d: %d)",
			      mixers[source_class].label.name,
			      mixers[source].label.name,
			      mixers[source].un.e.member[ord].label.name,
			      source, ord);
			if (mute != AUDIO_INVALID_PORT)
				warnx("             mute=%s.%s.%s (%d)",
				      mixers[gain_class].label.name,
				      mixers[gain].label.name,
				      mixers[mute].label.name,
				      mute);
			if (preamp != AUDIO_INVALID_PORT)
				warnx("             preamp=%s.%s.%s (%d)",
				      mixers[gain_class].label.name,
				      mixers[gain].label.name,
				      mixers[preamp].label.name,
				      preamp);
			if (preamp != AUDIO_INVALID_PORT) {
				switch (iport->preamp.un.ord) {
				case 0:
					warnx("             preamp=%s (%d)",
					      AudioNoff,
					      iport->preamp.un.ord);
					break;
				case 1:
					warnx("             preamp=%s (%d)",
					      AudioNon,
					      iport->preamp.un.ord);
					break;
				default:
					warnx("             preamp=%s: (%d)",
					      "unknown state",
					      iport->preamp.un.ord);
					break;
				}
			}
		}
	}
}

#endif				/* DEBUG_MIXER */
