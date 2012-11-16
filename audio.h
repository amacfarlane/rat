/*
 * FILE:    audio.h
 * PROGRAM: RAT
 * AUTHOR:  Orion Hodson / Colin Perkins / Isidor Kouvelas
 *
 * Copyright (c) 1995-2001 University College London
 * All rights reserved.
 *
 * $Id: audio.h 3642 2004-01-12 17:14:56Z ucacoxh $
 */

#ifndef _RAT_AUDIO_H_
#define _RAT_AUDIO_H_

#include "audio_types.h"
#include "codec_types.h"
#include "auddev.h"

/* Structures used in function declarations below */
struct s_cushion_struct;
struct s_session;
struct s_mixer;

/* Structure used for reconfiguration processing */

struct s_audio_config;

/* General audio processing functions */
int     audio_rw_process (struct s_session *spi, struct s_session *spo, struct s_mixer *ms);

/* audio_device_take_initial takes safe config of null audio device.  All
 * further devices used in rat accessed through audio_device_reconfigure.
 * It is a nasty hack, but seemingly necessary in this kludge layer that
 * should not exist.
 */

int     audio_device_take_initial(struct s_session *sp, audio_desc_t ad);
int	audio_device_release     (struct s_session *sp, audio_desc_t ad);

/* Functions used for changing the device set up.  Since the time it
 * takes to reconfigure the device is non-deterministic and often
 * longer than the mbus can tolerate we have to store requests to
 * change device config.  In the main process loop we can check if
 * device config change is pending, if so process all outstanding mbus
 * messages, then do device reconfig.
 */

void    audio_device_register_change_device(struct s_session *sp,
                                            audio_desc_t ad);

void    audio_device_register_change_primary(struct s_session *sp,
                                             codec_id_t  cid);

void    audio_device_register_change_render_3d(struct s_session *sp,
                                               int enabled);

int     audio_device_reconfigure (struct s_session *sp);
int     audio_device_get_safe_config(struct s_audio_config **config);

#endif /* _RAT_AUDIO_H_ */
