/*
 * FILE:    auddev_alsa.c
 * PROGRAM: RAT ALSA 0.9+/final audio driver.
 * AUTHOR:  Steve Smith  (ALSA 0.9+/final)
 *          Robert Olson (ALSA 0.5)
 *
 * Copyright (c) 2003 University of Sydney
 * Copyright (c) 2000 Argonne National Laboratory
 * All rights reserved.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * $Revision: 4395 $
 * $Date: 2009-02-20 09:20:48 -0800 (Fri, 20 Feb 2009) $
 *
 */

#define ALSA_PCM_NEW_HW_PARAMS_API
#define ALSA_PCM_NEW_SW_PARAMS_API
#include <alsa/asoundlib.h>
#include <stdio.h>
#include <stdarg.h>

#include "config_unix.h"
#include "audio_types.h"
#include "auddev_alsa.h"
#include "debug.h"


/*
 * Structure that keeps track of the cards we know about. Rat wants a linear
 * list of cards, so we'll give it that.
 *
 * This is filled in during the init step.
 */

typedef struct RatCardInfo_t
{
    char *name;
    int card_number;
    char *pcm_device;
    char *pcm_mixer;
} RatCardInfo;

#define MAX_RAT_CARDS 128

static RatCardInfo ratCards[MAX_RAT_CARDS];
static int nRatCards = 0;
static int mixer_state_change = 0;

/* Define ALSA mixer identifiers.  We use these to lookup the volume
 * controls on the mixer.  This feels like a bit of a hack, but
 * appears to be what everybody else uses. */
#define RAT_ALSA_MIXER_PCM_NAME "PCM"
#define RAT_ALSA_MIXER_MASTER_NAME "Master"
#define RAT_ALSA_MIXER_CAPTURE_NAME "Capture"
#define RAT_ALSA_MIXER_LINE_NAME "Line"
#define RAT_ALSA_MIXER_MIC_NAME "Mic"
#define RAT_ALSA_MIXER_CD_NAME "CD"

/*
 * The list of input ports to choose between.  These are scanned from the
 * mixer device.
 */
#define MAX_RAT_DEVICES 32

typedef struct _port_t {
    audio_port_details_t details;
    snd_mixer_selem_id_t *sid;
    snd_mixer_elem_t* smixer_elem;
    int priority;
} port_t;

static port_t iports[MAX_RAT_DEVICES];
static unsigned num_iports;

/*
 * The list of output ports to choose between.
 */
static audio_port_details_t out_ports[] = {
    {0, RAT_ALSA_MIXER_PCM_NAME},
    {1, RAT_ALSA_MIXER_MASTER_NAME}
};

static audio_port_t oport = 0;

#define NUM_OUT_PORTS (sizeof(out_ports)/(sizeof(out_ports[0])))

/*
 * Current open audio device
 */

typedef struct _pcm_stream_t {
    snd_pcm_t *handle;
    snd_pcm_uframes_t buffer_size;
    snd_pcm_uframes_t period_size;
    int channels;
} pcm_stream_t;

static struct current_t {
    int index;
    RatCardInfo *info;
    unsigned bytes_per_block;
    pcm_stream_t tx;
    pcm_stream_t rx;
    audio_port_t iport;
    snd_mixer_t *mixer;
    snd_mixer_elem_t *txgain;
    snd_mixer_elem_t *rxgain;
    audio_port_t txport;
} current;

static void clear_current()
{
    current.tx.handle = NULL;
    current.rx.handle = NULL;
    current.mixer =  NULL;
    current.index = -1;
    current.info = NULL;
    current.iport = (audio_port_t)NULL;
}


/*
 * Utility funcs
 */
static char *encodingToString[] = {
    "PCMU",
    "PCMA",
    "S8",
    "U8",
    "S16"
};

// Trimmed down Error macros 
#define CHECKERR(msg) \
{ \
  if (err < 0) \
  { \
    debug_msg(msg); \
    debug_msg("snd_strerror : %s\n", snd_strerror(err)); \
    return FALSE; \
  } \
}
#define CHECKOPENERR(msg) \
{ \
  if (err < 0) \
  { \
    debug_msg(msg); \
    debug_msg("snd_strerror : %s\n", snd_strerror(err)); \
    snd_pcm_close(stream->handle); \
    stream->handle=NULL; \
    return FALSE; \
  } \
}



static int mapformat(deve_e encoding)
{
    int format = -1;
    switch (encoding)
    {
    case DEV_PCMU:
        format = SND_PCM_FORMAT_MU_LAW;
        break;
      case DEV_PCMA:
        format = SND_PCM_FORMAT_A_LAW;
	break;
    case DEV_S8:
        format = SND_PCM_FORMAT_S8;
	break;
    case DEV_U8:
        format = SND_PCM_FORMAT_U8;
	break;
    case DEV_S16:
        format = SND_PCM_FORMAT_S16;
	break;
    }
    return format;
}

__attribute__((unused)) static char* mapstate(snd_pcm_state_t s)
{
    switch (s) {
      case SND_PCM_STATE_OPEN:
        return "SND_PCM_STATE_OPEN";
      case SND_PCM_STATE_SETUP:
        return "SND_PCM_STATE_SETUP";
      case SND_PCM_STATE_PREPARED:
        return "SND_PCM_STATE_PREPARED";
      case SND_PCM_STATE_RUNNING:
        return "SND_PCM_STATE_RUNNING";
      case SND_PCM_STATE_XRUN:
        return "SND_PCM_STATE_XRUN";
      case SND_PCM_STATE_DRAINING:
        return "SND_PCM_STATE_DRAINING";
      case SND_PCM_STATE_PAUSED:
        return "SND_PCM_STATE_PAUSED";
      case SND_PCM_STATE_SUSPENDED:
        return "SND_PCM_STATE_SUSPENDED";
      default:
        return "UNKNOWN!!!";
    }
}

static void dump_audio_format(audio_format *f)
{
    if (f == 0)
	debug_msg("    <null>\n");
    else
        debug_msg("encoding=%s sample_rate=%d bits_per_sample=%d "
                  "channels=%d bytes_per_block=%d\n",
                  encodingToString[f->encoding],
                  f->sample_rate, f->bits_per_sample,
		  f->channels, f->bytes_per_block);
}


static void  __attribute__((unused)) dump_alsa_current(snd_pcm_t *handle)
{
    int err;
    snd_output_t *out;

    err = snd_output_stdio_attach(&out, stderr, 0);
    snd_output_printf(out, "--- MY IO\n");

    err = snd_pcm_dump_setup(handle, out);

    snd_output_printf(out, "--- SW\n");
    err = snd_pcm_dump_sw_setup(handle, out);

    snd_output_printf(out, "--- HW\n");
    err = snd_pcm_dump_hw_setup(handle, out);

    snd_output_printf(out, "--- DONE\n");
    snd_output_close(out);
    }


/* *** Alsa driver implementation. *** */

static int open_stream(RatCardInfo *info, pcm_stream_t *stream,
                       snd_pcm_stream_t type, audio_format *fmt)
    {
    int err;
    int dir=0;
    size_t bsize;
    snd_pcm_uframes_t frames;
    unsigned int rrate;
    snd_pcm_hw_params_t *hw_params;
    snd_pcm_sw_params_t *sw_params;
    /* Set avail min inside a software configuration container */
    /* Can we tweak this up/down?*/
    //snd_pcm_uframes_t    avail_min=100; //4	
    snd_pcm_uframes_t    avail_min=4;	
    snd_pcm_uframes_t    xfer_align=1;	

    // Open type (Capture or Playback)
    err = snd_pcm_open(&stream->handle, info->pcm_device,
                       type, SND_PCM_NONBLOCK);
    if (err<0) { 
      debug_msg("snd_pcm_open failed for: %s\n",info->pcm_device);
      return FALSE;
    } 
    debug_msg("ALSA:snd_pcm_open ok for: %s\n",info->pcm_device);

    snd_pcm_hw_params_alloca (&hw_params);

    err = snd_pcm_hw_params_any (stream->handle, hw_params);
    CHECKOPENERR("Failed to initialise HW parameters");

    err = snd_pcm_hw_params_set_access(stream->handle, hw_params,
                                       SND_PCM_ACCESS_RW_INTERLEAVED);
    CHECKOPENERR("Failed to set interleaved access");

    err = snd_pcm_hw_params_set_format (stream->handle, hw_params,
                                        mapformat(fmt->encoding));
    CHECKOPENERR("Failed to set encoding");

    err = snd_pcm_hw_params_set_channels (stream->handle, hw_params,
                                          fmt->channels);
    CHECKOPENERR("Failed to set channels");
    stream->channels = fmt->channels;

    rrate = fmt->sample_rate;
    err = snd_pcm_hw_params_set_rate_near (stream->handle, hw_params,
                                           &rrate, &dir);
    CHECKOPENERR("Failed to set sample rate");
    if (rrate != fmt->sample_rate) {
        debug_msg("Error: ALSA rate set to %d when we wanted %d\n",
                rrate, fmt->sample_rate);
        snd_pcm_close(stream->handle);
        stream->handle=NULL;
        return FALSE;
    }

    // Setup the buffer size. This stuff's all in frames (one frame 
    // is number of channel * sample - e.g one stereo frame is two samples).
    // We can't convert with the helper functions (snd_pcm_bytes_to_frames())
    // at this point as they require a working handle, and ours isn't setup
    // yet. We use a factor RAT_ALSA_BUFFER_DIVISOR which is buffer size in
    // millisec(10^-3) - it seems we need 
    // We don't actually do anything with these values anyway.
    bsize = snd_pcm_format_size (mapformat(fmt->encoding),
                                 fmt->sample_rate / RAT_ALSA_BUFFER_DIVISOR);
    debug_msg("Bsize1 == %d\n", bsize);
    bsize = (fmt->sample_rate * (fmt->bits_per_sample/8) * fmt->channels* RAT_ALSA_BUFFER_DIVISOR)/ (1000);
    debug_msg("Bsize2 == %d\n", bsize);
    bsize = (fmt->sample_rate*RAT_ALSA_BUFFER_DIVISOR)/1000;

    frames = bsize;
    debug_msg("sample_rate, bytes_per_block == %d, %d\n", fmt->sample_rate, fmt->bytes_per_block);
    debug_msg("Bsize == %d\n", bsize);
    err = snd_pcm_hw_params_set_buffer_size_near(stream->handle, hw_params,
                                                 &frames);
    CHECKOPENERR("Failed to set buffer size");

    stream->buffer_size = frames;
    debug_msg("Return Bsize == %d\n", stream->buffer_size);

    frames=  fmt->bytes_per_block / ((fmt->bits_per_sample/8)* fmt->channels);
    frames= bsize /(RAT_ALSA_BUFFER_DIVISOR/10);
    err = snd_pcm_hw_params_set_period_size_near(stream->handle, hw_params,
                                                 &frames, &dir);
    CHECKOPENERR("Failed to set period size");

    stream->period_size = frames;
    debug_msg("Returned Period == %d\n", stream->period_size);

    err = snd_pcm_hw_params (stream->handle, hw_params);
    CHECKOPENERR("Failed to install HW parameters");


    // ALSA software settings
    snd_pcm_sw_params_alloca(&sw_params);
    err = snd_pcm_sw_params_current(stream->handle, sw_params);
    CHECKOPENERR("Failed to initialise SW params");

    err = snd_pcm_sw_params_set_start_threshold(stream->handle, sw_params,
                                                stream->period_size);
    CHECKOPENERR("Failed to set threshold value");

    err = snd_pcm_sw_params_set_avail_min(stream->handle, sw_params, stream->period_size);
    CHECKOPENERR("Failed to set min available value");

    err = snd_pcm_sw_params_set_xfer_align(stream->handle,sw_params,xfer_align);
    CHECKOPENERR("Failed to set xfer align value");

    err = snd_pcm_sw_params(stream->handle, sw_params);
    CHECKOPENERR("Failed to set SW params");
    
    snd_pcm_sw_params_get_boundary(sw_params, &avail_min);
    debug_msg("snd_pcm_sw_params_get_boundary %d\n",avail_min);
    snd_pcm_sw_params_set_stop_threshold(stream->handle,sw_params, avail_min);
    snd_pcm_sw_params_get_stop_threshold(sw_params, &avail_min);
    debug_msg("snd_pcm_sw_params_get_stop_threshold %d\n",avail_min);
    snd_pcm_sw_params_get_silence_size(sw_params, &avail_min);
    debug_msg("snd_pcm_sw_params_get_silence_size %d\n",avail_min);
    snd_pcm_sw_params_get_avail_min(sw_params, &avail_min);
    debug_msg("min available value %d\n",avail_min);

    snd_output_t *output = NULL;
    snd_output_stdio_attach(&output, stdout, 0);
    //snd_pcm_dump_setup(stream->handle, output);
    //snd_pcm_dump(stream->handle, output);
    return TRUE;
}



// Open a named mixer
static int open_volume_ctl(char *name, snd_mixer_elem_t **ctl)
{
  snd_mixer_selem_id_t *sid;
  int err=0;

  snd_mixer_selem_id_alloca(&sid);

  // FIXME? Find the appropriate mixer element.  This feels really wrong,
  // there has to be another way to do this.
  snd_mixer_selem_id_set_index (sid, 0);
  snd_mixer_selem_id_set_name (sid, name);

  *ctl = snd_mixer_find_selem(current.mixer, sid);
  if (*ctl == NULL ) {
   debug_msg("Couldn't find mixer control element (name:%s)\n",name);
   return FALSE;
  }

  if (snd_mixer_selem_has_playback_volume(*ctl)) {
    debug_msg("Got volume control %s of type PLAY\n", name);
    // FIXME: Does this always work?
    snd_mixer_selem_set_playback_volume_range (*ctl, 0, 100);

    if (snd_mixer_selem_has_playback_switch( *ctl ) ) {
      if (snd_mixer_selem_set_playback_switch_all(*ctl, 1) < 0) {
        debug_msg("Failed to switch on playback volume");
        return FALSE;
      }
    } 

  } else if (snd_mixer_selem_has_capture_volume(*ctl)) {
    debug_msg("Got volume control %s of type CAPTURE\n", name);
    snd_mixer_selem_set_capture_volume_range (*ctl, 0, 100);

    if (snd_mixer_selem_has_capture_switch(*ctl)) {
      err = snd_mixer_selem_set_capture_switch_all(*ctl, 1);
      if (err<0) {
	debug_msg("Failed to switch on capture volume");
	return FALSE;
      }
    }

  } else {
    debug_msg("Unknown mixer type %s set\n", name);
    return FALSE;
  }

  return TRUE;
}


// Open and initialise the system mixer

static int porder(const void *a, const void *b)
{
    return (((port_t*)a)->priority - ((port_t*)b)->priority);
}

/* Not used for now. It seems to cause problems in mbus, however it
 * may be useful for handling mixer updates from external sources
 *
static int
mixer_callback (snd_mixer_t *mixer, unsigned int mask, snd_mixer_elem_t *elem)
{
  mixer_state_change = 1;
  debug_msg("mixer_callback\n");
  return 0;
} 
*/

static int setup_mixers()
{
    snd_mixer_elem_t *elem, *selem;
    snd_mixer_selem_id_t *sid;
    int err;
    unsigned i;
    int need_cap_switch=1;
    
    snd_mixer_selem_id_alloca(&sid);

    err = snd_mixer_open (&current.mixer, 0);
    if (err < 0) { 
      debug_msg("Failed to open the mixer(snd err: %s)",snd_strerror(err));
      current.mixer=NULL; 
      return FALSE; 
    } 

    err = snd_mixer_attach(current.mixer, current.info->pcm_mixer);
    if (err < 0) { 
      debug_msg("Failed to attach the mixer(snd err: %s)",snd_strerror(err));
      snd_mixer_close(current.mixer); 
      current.mixer=NULL; 
      return FALSE; 
    } 

    err = snd_mixer_selem_register(current.mixer, NULL, NULL);
    if (err < 0) { 
      debug_msg("Failed to register the mixer(snd err: %s)",snd_strerror(err));
      snd_mixer_close(current.mixer); 
      current.mixer=NULL; 
      return FALSE; 
    } 
    
    /* Not used for now - see above */
    /* snd_mixer_set_callback (current.mixer, mixer_callback); */

    err = snd_mixer_load(current.mixer);
    if (err < 0) { 
      debug_msg("Failed to load the mixer(snd err: %s)",snd_strerror(err));
      snd_mixer_close(current.mixer); 
      current.mixer=NULL; 
      return FALSE; 
    } 

    /* Get the playback and capture volume controls
     * If Capture open fails it ptobably means we have a capture card
     * which does not support capture_switch - in which case we set the
     * capture level of the selected input device.
     */
    oport = 0;
    if (!open_volume_ctl(RAT_ALSA_MIXER_PCM_NAME, &current.txgain)) {
      oport = 1;
      if (!open_volume_ctl(RAT_ALSA_MIXER_MASTER_NAME, &current.txgain)) {
        oport = 0;
        snd_mixer_close(current.mixer); 
        current.mixer=NULL; 
        return FALSE;
      }
    }
    if (!open_volume_ctl(RAT_ALSA_MIXER_CAPTURE_NAME, &current.rxgain)) {
      need_cap_switch=0;
      current.rxgain=0;
    }

    num_iports = 0;

    /* We now scan the mixer for recording controls.  We're interested in
     * the controls that are part of a group (i.e. radio-button types) that
     * can be flipped between. 
     */
    for (elem = snd_mixer_first_elem (current.mixer);
        elem && (num_iports < MAX_RAT_DEVICES);
        elem = snd_mixer_elem_next (elem))
    {
      if (!snd_mixer_selem_is_active(elem) || 
           snd_mixer_selem_is_enumerated(elem)) 
         continue;

      sid = (snd_mixer_selem_id_t*)malloc(snd_mixer_selem_id_sizeof());
      snd_mixer_selem_get_id( elem, sid );
      /* Find the Simple mixer element */
      selem = snd_mixer_find_selem(current.mixer, sid );

      int gid = snd_mixer_selem_get_capture_group(selem);
      const char *name = snd_mixer_selem_get_name(selem);

      debug_msg("Trying CAPTURE element '%s' of group %d \n", name, gid);

      /* The code below is minimal so it works for card without capture switch */
      if (snd_mixer_selem_has_capture_volume(selem) ||
          snd_mixer_selem_has_capture_switch(selem) )
      //    snd_mixer_selem_is_active (elem) &&
      //    (snd_mixer_selem_has_capture_switch(elem)) &&
      //    snd_mixer_selem_has_capture_switch_exclusive(elem))
      {
        // FIXME: It's theoretically possible that there would be more
        // than one capture group, but RAT isn't really equipped to handle
        // the case so we'll just ignore it for now.

        debug_msg("Got CAPTURE element '%s' of group %d\n", name, gid);

        snprintf(iports[num_iports].details.name, AUDIO_PORT_NAME_LENGTH,
            "%s", name);
        iports[num_iports].smixer_elem = selem;
        //iports[num_iports].sid = sid;
    debug_msg("Found %s selem:%d (sid:%d)\n",iports[num_iports].details.name,selem,iports[num_iports].sid );

        // The principle of least-surprise means that we should present
        // the ports in the same order as the other drivers.  As we're
        // more flexible about retrieving the mixer ports we need to
        // attempt to reorder the list, so we assign a priority and
        // sort the list at the end.
        if (strstr(name, RAT_ALSA_MIXER_MIC_NAME) == name) {
          iports[num_iports].priority = 100;
        } else if (strstr(name, RAT_ALSA_MIXER_LINE_NAME) == name) {
          iports[num_iports].priority = 50;
        } else if (strstr(name, RAT_ALSA_MIXER_CD_NAME) == name) {
          iports[num_iports].priority = 30;
        } else {
          iports[num_iports].priority = 10;
        }
        num_iports++;
      }
    }

    qsort(iports, num_iports, sizeof(port_t), porder);

    // Now it's sorted we need to set the port ID to the index, allowing us
    // a fast lookup for port-IDs
    for (i=0; i<num_iports; i++) {
        iports[i].details.port = i;
    }

    return TRUE;
}


int alsa_audio_open(audio_desc_t ad, audio_format *infmt, audio_format *outfmt)
{
    int err;

    debug_msg("Audio open ad == %d\n", ad);
    debug_msg("Input format:\n");
    dump_audio_format(infmt);
    debug_msg("Output format:\n");
    dump_audio_format(outfmt);
    
    //clear_current();
    if (current.tx.handle != NULL) {
        debug_msg( "Attempt to open a device while another is open\n");
        return FALSE;
    }
    current.index = ad;
    current.info = ratCards + ad;
    current.bytes_per_block = infmt->bytes_per_block;

    if (!open_stream(current.info, &current.tx,
                     SND_PCM_STREAM_PLAYBACK, outfmt)) {
        alsa_audio_close(ad); 
        debug_msg( "Failed to open device for playback\n");
        return FALSE;
    }
    debug_msg( "Opened device for playback\n");

    if (!open_stream(current.info, &current.rx,
                     SND_PCM_STREAM_CAPTURE, infmt)) {
        alsa_audio_close(ad); 
        debug_msg( "Failed to open device for capture\n");
        return FALSE;
    }
    debug_msg( "Opened device for capture\n");

    if (setup_mixers() == FALSE) {
      alsa_audio_close(ad); 
        debug_msg( "Failed to set mixer levels \n");
      return FALSE;
    }

    err = snd_pcm_prepare(current.tx.handle);
    if (err<0) {
      debug_msg("Failed to prepare playback");
      alsa_audio_close(ad); 
      return FALSE;
    }


    err = snd_pcm_start(current.rx.handle);
    if (err<0) {
      debug_msg("Failed to start PCM capture");
      alsa_audio_close(ad); 
      return FALSE;
    }

    return TRUE;
}

/*
 * Shutdown.
 */
void alsa_audio_close(audio_desc_t ad)
{
    int err;
    
    if (current.index != ad) {
        fprintf(stderr, "Index to close (%d) doesn't match current(%d)\n",
                ad, current.index);
        return;
    }

    debug_msg("Closing device \"%s\"\n", current.info->name);

    if (current.tx.handle != NULL ) {
	    err = snd_pcm_close(current.tx.handle);
	    if (err<0) debug_msg("Error closing playback PCM");
    }

    if (current.rx.handle != NULL ) {
	    err = snd_pcm_close(current.rx.handle);
            if (err<0) debug_msg("Error closing capture PCM");
    }

    // Close mixer 
    if (current.mixer !=  NULL ) {
    	err = snd_mixer_close(current.mixer);
    	if (err<0) debug_msg("Error closing mixer");
    }

    clear_current();
}

/*
 * Flush input buffer.
 */
void alsa_audio_drain(audio_desc_t ad __attribute__((unused)))
{
    int err;

    debug_msg("audio_drain\n");
    err = snd_pcm_drain(current.rx.handle);
    if (err<0) debug_msg("Problem draining input\n");
}

static void handle_mixer_events(snd_mixer_t *mixer_handle)
{
  int count, err;
  struct pollfd *fds;
  int num_revents = 0;
  unsigned short revents;

  /* Get count of poll descriptors for mixer handle */
  if ((count = snd_mixer_poll_descriptors_count(mixer_handle)) < 0) {
    debug_msg("Error in snd_mixer_poll_descriptors_count(%d)\n", count);
    return;
  }

  fds =(struct pollfd*)calloc(count, sizeof(struct pollfd));
  if (fds == NULL) {
    debug_msg("snd_mixer fds calloc err\n");
    return;
  }

  if ((err = snd_mixer_poll_descriptors(mixer_handle, fds, count)) < 0){
    debug_msg ("snd_mixer_poll_descriptors err=%d\n", err);
  free(fds);
    return;
  }

  if (err != count){
    debug_msg ("snd_mixer_poll_descriptors (err(%d) != count(%d))\n",err,count);
  free(fds);
    return;
  }

  num_revents = poll(fds, count, 1);

  /* Graceful handling of signals recvd in poll() */
  if (num_revents < 0 && errno == EINTR)
    num_revents = 0;

  if (num_revents > 0) {
    if (snd_mixer_poll_descriptors_revents(mixer_handle, fds, count, &revents) >= 0) {
      if (revents & POLLNVAL)
        debug_msg ("snd_mixer_poll_descriptors (POLLNVAL)\n");
      if (revents & POLLERR)
        debug_msg ("snd_mixer_poll_descriptors (POLLERR)\n");
      if (revents & POLLIN)
        snd_mixer_handle_events(mixer_handle);
    }
  }
  free(fds);
}
   


/*
 * Set capture gain.
 */
void alsa_audio_set_igain(audio_desc_t ad, int gain)
{
    int err;
    debug_msg("Set igain %d %d\n", ad, gain);

    if (current.rxgain) {
      if (snd_mixer_selem_has_capture_switch(current.rxgain)) {
	err = snd_mixer_selem_set_capture_volume_all(current.rxgain, gain);
	if (err<0) debug_msg("Failed to set capture volume\n");
      } else {
	// Or try setting capture level of selected input
	debug_msg("Attempting to set input element capture volume\n");
	err = snd_mixer_selem_set_capture_volume_all(iports[current.iport].smixer_elem, gain);
	if(err<0) debug_msg("Failed to set capture volume\n");
      }
    } else {
	// Or try setting capture level of selected input
	debug_msg("current.rxgain=0 - Attempting to set input element capture volume\n");
	err = snd_mixer_selem_set_capture_volume_all(iports[current.iport].smixer_elem, gain);
	if(err<0) debug_msg("Failed to set capture volume\n");
    }
handle_mixer_events(current.mixer);
//    mixer_state_change = 1;
}


/*
 * Get capture gain.
 */
int alsa_audio_get_igain(audio_desc_t ad)
{
    long igain=0;
    int err;
    debug_msg("Get igain %d\n", ad);

    if (current.rxgain) {
      if (snd_mixer_selem_has_capture_switch(current.rxgain)) {
	err = snd_mixer_selem_get_capture_volume(current.rxgain,
						 SND_MIXER_SCHN_MONO, &igain);
	if(err<0) debug_msg("Failed to get capture volume");
      } else {
	// Or try getting capture level of selected input
	err = snd_mixer_selem_get_capture_volume(iports[current.iport].smixer_elem, SND_MIXER_SCHN_MONO, &igain);
	if(err<0) debug_msg("Failed to get capture volume");
      }
    } else {
	// Or try getting capture level of selected input
	err = snd_mixer_selem_get_capture_volume(iports[current.iport].smixer_elem, SND_MIXER_SCHN_MONO, &igain);
	if(err<0) debug_msg("Failed to get capture volume");
    }

    return (int)igain;
}

int alsa_audio_duplex(audio_desc_t ad __attribute__((unused)))
{
    return TRUE; // FIXME: ALSA always duplex?
}

/*
 * Set play gain.
 */
void alsa_audio_set_ogain(audio_desc_t ad, int vol)
{
    int err;

    debug_msg("Set ogain %d %d\n", ad, vol);

    err = snd_mixer_selem_set_playback_switch_all(current.txgain, 1);
    if(err<0) debug_msg("Failed to switch on playback volume");

    err = snd_mixer_selem_set_playback_volume_all(current.txgain, vol);
    if(err<0) debug_msg("Couldn't set mixer playback volume");

handle_mixer_events(current.mixer);
//    mixer_state_change = 1;
}

/*
 * Get play gain.
 */
int
alsa_audio_get_ogain(audio_desc_t ad)
{
    long ogain=0;
    int err;

    debug_msg("Get ogain %d\n", ad);
    err = snd_mixer_selem_get_playback_volume(current.txgain,
                                             SND_MIXER_SCHN_MONO, &ogain);
    if(err<0) debug_msg("Failed to get capture volume");

    return (int)ogain;
}


/*
 * Record audio data.
 */

int alsa_audio_read(audio_desc_t ad __attribute__((unused)),
                u_char *buf, int bytes)
{
    snd_pcm_sframes_t fread, bread, avail;
    int err;
    long read_interval;
    struct timeval          curr_time;
    static struct timeval          last_read_time;

    gettimeofday(&curr_time, NULL);
    if (mixer_state_change) {
      mixer_state_change=0;
      handle_mixer_events (current.mixer);
        debug_msg("Handling mixer event\n");
    }

    snd_pcm_sframes_t frames = snd_pcm_bytes_to_frames(current.rx.handle,current.bytes_per_block);
    read_interval = (curr_time.tv_sec  - last_read_time.tv_sec) * 1000000 + (curr_time.tv_usec - last_read_time.tv_usec);
    avail = snd_pcm_avail_update(current.rx.handle);
    //debug_msg("Frames avail to be read=%d, time diff=%ld, want: %d frames, %d bytes (bytepb %d\n",avail, read_interval, frames, bytes, current.bytes_per_block);
    last_read_time.tv_sec=curr_time.tv_sec;
    last_read_time.tv_usec=curr_time.tv_usec;

    if (avail < frames)
      return 0;

    frames = snd_pcm_bytes_to_frames(current.rx.handle,bytes);
    fread = snd_pcm_readi(current.rx.handle, buf, frames);

    if (fread >= 0) {
        // Normal case
        bread = snd_pcm_frames_to_bytes(current.rx.handle, fread);
        gettimeofday(&curr_time, NULL);
        read_interval = (curr_time.tv_sec  - last_read_time.tv_sec) * 1000000 + (curr_time.tv_usec - last_read_time.tv_usec);
        //debug_msg("Read %d bytes (%d frames) took %ld usecs\n", bread, fread, read_interval);
        return bread;
    }

   // Something happened
    switch (fread)
	{
	case -EAGAIN:
          // Normal when non-blocking
    gettimeofday(&curr_time, NULL);
    read_interval = (curr_time.tv_sec  - last_read_time.tv_sec) * 1000000 + (curr_time.tv_usec - last_read_time.tv_usec);
    debug_msg("failed %d Read %d bytes (%d frames) took %ld usecs\n", fread, bread, fread, read_interval);
 	  return 0;

	case -EPIPE:
          debug_msg("Got capture overrun XRUN (EPIPE)\n");
          err = snd_pcm_prepare(current.rx.handle);
          if (err<0) debug_msg("Failed snd_pcm_prepare from capture overrun");
          err = snd_pcm_start(current.rx.handle);
          if (err<0) debug_msg("Failed to re-start PCM capture from XRUN");
          return FALSE;

        case -ESTRPIPE:
          debug_msg("Got capture ESTRPIPE\n");
          while ((err = snd_pcm_resume(current.rx.handle)) == -EAGAIN)
              sleep(1);       /* wait until the suspend flag is released */
          if (err < 0) {
              err = snd_pcm_prepare(current.rx.handle);
              if (err<0) debug_msg("Failed snd_pcm_prepare from capture suspend");
          }
          return FALSE;

	default:
          debug_msg("Read failed status= %s\n", snd_strerror(fread));
          return 0;
    }
}

/*
 * Playback audio data.
 */


int alsa_audio_write(audio_desc_t ad __attribute__((unused)),
                     u_char *buf, int bytes)
{
    int fwritten, err, num_bytes=0 ,read_interval;
    struct timeval          curr_time;
    static struct timeval          last_read_time;
    snd_pcm_sframes_t delay;
    snd_pcm_sframes_t frames =
        snd_pcm_bytes_to_frames(current.tx.handle,bytes);
    snd_output_t *output = NULL;
    snd_output_stdio_attach(&output, stdout, 0);

    snd_pcm_status_t *status;
    snd_pcm_status_alloca(&status);
    err = snd_pcm_status(current.tx.handle, status);
    if (err<0) {
      debug_msg("Can't get status of tx");
      return FALSE;
    }

    gettimeofday(&curr_time, NULL);
    ///debug_msg("Audio write %d\n", bytes);
    switch (snd_pcm_state(current.tx.handle)) {
    case SND_PCM_STATE_RUNNING:
      //debug_msg("In SND_PCM_STATE_RUNNING \n");
      break;
    case SND_PCM_STATE_XRUN:
      debug_msg("In SND_PCM_STATE_XRUN  - preparing audio \n");
        err = snd_pcm_prepare(current.tx.handle);
      break;
    default:
      break;
    }
    read_interval = (curr_time.tv_sec  - last_read_time.tv_sec) * 1000000 + (curr_time.tv_usec - last_read_time.tv_usec);
    snd_pcm_delay(current.tx.handle, &delay);

    //debug_msg("Frames avail to be written=%d, time diff=%d, Trying to write %d frames, Curr delay %d \n",snd_pcm_avail_update(current.tx.handle), read_interval, frames, delay);
    //snd_pcm_status_dump(status, output);
    last_read_time.tv_sec=curr_time.tv_sec;
    last_read_time.tv_usec=curr_time.tv_usec;

    fwritten = snd_pcm_writei(current.tx.handle, buf, frames);
    if (fwritten >= 0) {
        // Normal case
        num_bytes = snd_pcm_frames_to_bytes(current.tx.handle, fwritten);
        //debug_msg("Wrote %d bytes, frames: %d\n", num_bytes, fwritten);
        err = snd_pcm_status(current.tx.handle, status);
        //snd_pcm_status_dump(status, output);
        return num_bytes;
    }
    debug_msg("Err: Tried to Write %d bytes, frames: %d, but got: %d\n", bytes,frames, fwritten);

    // Something happened
    switch (fwritten)
    {
      case -EAGAIN:
        // Normal when non-blocking
        return 0;

      case -EPIPE:
        debug_msg("Got transmit underun XRUN(EPIPE) when trying to write audio\n");
        err = snd_pcm_prepare(current.tx.handle);
        if (err<0) debug_msg("Failed snd_pcm_prepare from Transmit underrun\n");
        debug_msg("Attempting Recovery from transmit underrun: Bytes:%d Frames:%d\n",num_bytes,fwritten);
        fwritten = snd_pcm_writei(current.tx.handle, buf, frames);
        if (fwritten<0) {
          debug_msg("Can't recover from transmit underrun\n");
          return 0;
        } else {
          num_bytes = snd_pcm_frames_to_bytes(current.tx.handle, fwritten);
          debug_msg("Recovered from transmit underrun: Bytes:%d Frames:%d\n",num_bytes,fwritten);
          err = snd_pcm_status(current.tx.handle, status);
          //snd_pcm_status_dump(status, output);
          return num_bytes;
        }

      case -ESTRPIPE:
        debug_msg("Got transmit ESTRPIPE\n");
        while ((err = snd_pcm_resume(current.tx.handle)) == -EAGAIN)
            sleep(1);       /* wait until the suspend flag is released */
        if (err < 0) {
            err = snd_pcm_prepare(current.tx.handle);
            if (err<0) debug_msg("Can't recovery from transmit suspend\n");
        }
        return 0;

      default:
        //debug_msg("Write failed status=%d: %s\n", fwritten, snd_strerror(fwritten));
        return 0;
    }


    num_bytes = snd_pcm_frames_to_bytes(current.tx.handle, fwritten);
    debug_msg("Audio wrote %d\n", num_bytes);
    return num_bytes;
}


/*
 * Set options on audio device to be non-blocking.
 */
void alsa_audio_non_block(audio_desc_t ad __attribute__((unused)))
    {
    int err;
    debug_msg("Set nonblocking\n");

    err = snd_pcm_nonblock(current.tx.handle, TRUE);
    if(err<0) debug_msg("Error setting TX non-blocking");

    err = snd_pcm_nonblock(current.rx.handle, TRUE);
    if(err<0) debug_msg("Error setting RX non-blocking");
}

/*
 * Set options on audio device to be blocking.
 */
void alsa_audio_block(audio_desc_t ad)
    {
    int err;
    debug_msg("[%d] set blocking\n", ad);
    if ((err = snd_pcm_nonblock(current.tx.handle, FALSE)) < 0) {
        debug_msg ("Cannot set device to be blocking: %s\n", snd_strerror(err));
    }
}



/*
 * Output port controls.  In our case there is only one output port, the
 * PCM control (or Master if PCM fails), so this is a dummy.
 */
void
alsa_audio_oport_set(audio_desc_t ad, audio_port_t port)
{
    debug_msg("oport_set %d %d\n", ad, port);
}

audio_port_t
alsa_audio_oport_get(audio_desc_t ad)
{
    debug_msg("oport_get %d\n", ad);
    return oport;
}

int
alsa_audio_oport_count(audio_desc_t ad)
{
    debug_msg("Get oport count for %d (num=%d)\n", ad, NUM_OUT_PORTS);
    return NUM_OUT_PORTS;
}

const audio_port_details_t*
alsa_audio_oport_details(audio_desc_t ad, int idx)
{
    debug_msg("oport details ad=%d idx=%d\n", ad, idx);
    if (idx >= 0 && idx < NUM_OUT_PORTS) {
        return &out_ports[idx];
    }
    return NULL;
}

/*
 * Set input port.
 */
void
alsa_audio_iport_set(audio_desc_t ad, audio_port_t port)
{
    int err = 0;
    audio_port_t i=port;
    snd_mixer_elem_t *selem;

    ad=ad; // Silence compiler warning
    current.iport = port;
    //The following does not seems necessary - though it does work
    //selem = snd_mixer_find_selem(current.mixer, iports[port].sid);
    selem = iports[port].smixer_elem;
    debug_msg("Selecting input port: %s\n",iports[i].details.name);

    if(selem)
      if (snd_mixer_selem_has_capture_switch(selem)) {
        if (snd_mixer_selem_has_capture_switch_joined(selem)){
          snd_mixer_selem_get_capture_switch(selem,SND_MIXER_SCHN_FRONT_LEFT,&err);
          debug_msg("Found snd_mixer_selem_has_capture_switch_joined\n");
          err=snd_mixer_selem_set_capture_switch_all(selem,1);
          if (err<0) {
            debug_msg("Error encountered setting joined capture switch\n" );
          }
        } else  {
          debug_msg("Found separate L+R captures levels\n" );
          err=snd_mixer_selem_set_capture_switch(selem,SND_MIXER_SCHN_FRONT_LEFT,1);
          if (err<0) {
            debug_msg("Error setting Left capture switch \n" );
          }
          err=snd_mixer_selem_set_capture_switch(selem,SND_MIXER_SCHN_FRONT_RIGHT,1);
          if (err<0) {
            debug_msg("Error setting Right capture switch \n" );
          }
        }
    }
    // Make sure Capture switch is on.
    if (current.rxgain) {
      if (snd_mixer_selem_has_capture_switch(current.rxgain)) {
	err=snd_mixer_selem_set_capture_switch_all(current.rxgain,1);
	if (err<0) {
	  debug_msg("Error setting Capture switch\n" );
	}
      }
    }
handle_mixer_events(current.mixer);
//    mixer_state_change = 1;
}



/*
 * Get input port.
 */
audio_port_t
alsa_audio_iport_get(audio_desc_t ad)
{
    debug_msg("iport_get %d\n", ad);
    return current.iport;
}

int
alsa_audio_iport_count(audio_desc_t ad)
{
    debug_msg("Get iport count for %d (num=%d)\n", ad, num_iports);
    return num_iports;
}

const audio_port_details_t* alsa_audio_iport_details(audio_desc_t ad, int idx)
{
    int err;
    snd_mixer_selem_get_capture_switch(iports[idx].smixer_elem,SND_MIXER_SCHN_FRONT_LEFT,&err);
    debug_msg("iport details ad=%d idx=%d : %s  (Enabled=%d)\n", ad, idx, iports[idx].details.name,err);
    return &iports[idx].details;
}


/*
 * For external purposes this function returns non-zero
 * if audio is ready.
 */
int alsa_audio_is_ready(audio_desc_t ad __attribute__((unused)))
{
    snd_pcm_status_t *status;
    snd_pcm_uframes_t avail;
    int err;
    snd_pcm_sframes_t frames;
    snd_output_t *output = NULL;

    snd_output_stdio_attach(&output, stdout, 0);
    snd_pcm_status_alloca(&status);

    err = snd_pcm_status(current.rx.handle, status);
    if (err<0) {
      debug_msg("Can't get status of rx");
      return FALSE;
    }

    avail = snd_pcm_frames_to_bytes(current.rx.handle, //frames
                                    snd_pcm_status_get_avail(status));

    if (!snd_pcm_status_get_avail_max(status)) {
      debug_msg("Resetting audio as statusmaxframes is zero\n");
      err = snd_pcm_prepare(current.rx.handle);
      if (err<0) debug_msg("Failed snd_pcm_prepare in audio_ready");
      err = snd_pcm_start(current.rx.handle);
      if (err<0) debug_msg("Failed to re-start in audio_ready");
      //dump status
      //snd_pcm_status_dump(status, output);
      //frames= snd_pcm_avail_update(current.rx.handle);
      debug_msg("audio_is_ready: snd_pcm_avail_update(current.rx.handle);: %d\n",  snd_pcm_avail_update(current.rx.handle));
      debug_msg("audio_is_ready: %d bytes (bytes per blk %d)\n", avail, current.bytes_per_block);
    }
  
    return (avail >= current.bytes_per_block);
}


void alsa_audio_wait_for(audio_desc_t ad __attribute__((unused)), int delay_ms)
{
    ///debug_msg("Audio wait %d\n", delay_ms);
    snd_pcm_wait(current.rx.handle, delay_ms);
}


char* alsa_get_device_name(audio_desc_t idx)
{
    debug_msg("Get name for card %d: \"%s\"\n", idx, ratCards[idx].name);
    return ratCards[idx].name;
}


int alsa_audio_init()
{
    int fd;
    char buf[4096];
    char *version;
    size_t count;
    int result =  FALSE;

    // Based on xmms-alsa

    fd = open("/proc/asound/version", O_RDONLY, 0);
    if (fd < 0) {
        result = FALSE;
	}  

    count = read(fd, buf, sizeof(buf) - 1);
    buf[count] = 0;
    close(fd);

    debug_msg("ALSA version identifier == %s\n", buf);

    version = strstr(buf, " Version ");

    if (version == NULL) {
        result = FALSE;
    }
	    
    version += 9; /* strlen(" Version ") */

    /* The successor to 0.9 might be 0.10, not 1.0.... */
    if (strcmp(version, "0.9") > 0  ||  isdigit(version[3])) {
        result = TRUE;
    } else {
        result = FALSE;
	    }

    debug_msg("ALSA init result == %d\n", result);

    clear_current();
    return result;
}

int alsa_get_device_count()
{
    snd_ctl_t *ctl_handle;
    snd_ctl_card_info_t *ctl_info;
    int err, cindex = 0, alsaDevNo=0;
    char card[128];
    // Ptr to ratCards array
    RatCardInfo *ratCard;

    debug_msg("ALSA get device count\n");

    snd_ctl_card_info_alloca(&ctl_info);
    
    // Get ALSA "default" card - so things like dsnoop etc can work
    if (!snd_ctl_open(&ctl_handle, "default", SND_CTL_NONBLOCK)) {
      if ((err = snd_ctl_card_info (ctl_handle, ctl_info) < 0)) {
        debug_msg("ALSA default card query failed: %s\n", snd_strerror(err));
        snd_ctl_close(ctl_handle);
      } else {
        ratCard = ratCards + cindex;
        ratCard->card_number = cindex;
        ratCard->pcm_device = strdup("default");
        ratCard->pcm_mixer = strdup("default");
        snprintf(card, sizeof(card), "ALSA default: %s", 
                                  snd_ctl_card_info_get_name (ctl_info));
        ratCard->name = strdup (card);
        debug_msg("Got \"default\" card %s\n", ratCard->name);
        snd_ctl_close(ctl_handle);
        cindex++;
      }
    } 

    do {
        sprintf (card , "hw:%d", alsaDevNo);
        err = snd_ctl_open (&ctl_handle, card, SND_CTL_NONBLOCK);

        if (err == 0) {
            debug_msg("Opened ALSA dev:%s\n",card);
            // Grab the card info
            ratCard = ratCards + cindex;
            ratCard->card_number = cindex;
            ratCard->pcm_mixer = strdup(card);
            sprintf (card , "plughw:%d", alsaDevNo);
            ratCard->pcm_device = strdup(card);

            if ((err = snd_ctl_card_info (ctl_handle, ctl_info) < 0)) {
                snprintf(card, sizeof(card), "ALSA %d: Not Available", cindex);
                ratCard->name = strdup (card);
                debug_msg("Got failed ALSA card %s (err:%s)\n", ratCard->name,snd_strerror(err));
                err=0;
            } else {
                snprintf(card, sizeof(card), "ALSA %d: %s", cindex,
                         snd_ctl_card_info_get_name (ctl_info));
                ratCard->name = strdup (card);
                debug_msg("Got card %s\n", ratCard->name);
            } 
            snd_ctl_close(ctl_handle);
            
        }
        cindex++;
        alsaDevNo++;

    } while (err == 0);

    nRatCards = cindex - 1;
    debug_msg("Got %d ALSA devices\n", nRatCards);

    return nRatCards;
}

int alsa_audio_supports(audio_desc_t ad, audio_format *fmt)
{
    snd_pcm_hw_params_t *hw_params;
    unsigned rmin, rmax, cmin, cmax;
    int err, dir;

    debug_msg("ALSA support  %d\n", ad);
    dump_audio_format(fmt);

    snd_pcm_hw_params_alloca (&hw_params);
    err = snd_pcm_hw_params_any (current.tx.handle, hw_params);

    err = snd_pcm_hw_params_get_rate_min (hw_params, &rmin, &dir);
    CHECKERR("Failed to get min rate");
    err = snd_pcm_hw_params_get_rate_max (hw_params, &rmax, &dir);
    CHECKERR("Failed to get max rate");

    err = snd_pcm_hw_params_get_channels_min (hw_params, &cmin);
    CHECKERR("Failed to get min channels");
    err = snd_pcm_hw_params_get_channels_max (hw_params, &cmax);
    CHECKERR("Failed to get max channels");

    if ((fmt->sample_rate >= rmin) && (fmt->sample_rate <= rmax) &&
        (fmt->channels >= (int)cmin) && (fmt->channels <= (int)cmax))
{
        debug_msg("Config is supported\n");
        return TRUE;
    }
    return FALSE;
}

