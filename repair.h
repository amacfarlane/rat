/*
 * FILE:    repair.h
 * PROGRAM: RAT
 * AUTHOR:  Orion Hodson
 *
 * Copyright (c) 1995-2001 University College London
 * All rights reserved.
 *
 * $Id: repair.h 3642 2004-01-12 17:14:56Z ucacoxh $
 */

#ifndef _REPAIR_H_
#define _REPAIR_H_

#include "codec_types.h"
#include "codec_state.h"
#include "repair_types.h"

int repair(repair_id_t                 r,
           int                         consec_lost,
           struct s_codec_state_store *states,
           media_data                  *prev,
           coded_unit                  *missing);


uint16_t                 repair_get_count   (void);
const repair_details_t *repair_get_details (uint16_t n);

void repair_set_codec_specific_allowed(int allowed);
int  repair_get_codec_specific_allowed(void);

#endif /* _REPAIR_H_ */

