/*
 * FILE:    render_3D.h
 * PROGRAM: RAT
 * AUTHORS: Marcus Iken
 *
 * Copyright (c) 1998-2001 University College London
 * All rights reserved.
 *
 * $Id: render_3D.h 3642 2004-01-12 17:14:56Z ucacoxh $
 */

#ifndef __RENDER_3D_H__
#define __RENDER_3D_H__

struct s_render_3D_dbentry;

int   render_3D_filter_get_count(void);
char *render_3D_filter_get_name(int id);
int   render_3D_filter_get_by_name(char *name);
int   render_3D_filter_get_lengths_count(void);
int   render_3D_filter_get_length(int idx);
int   render_3D_filter_get_lower_azimuth(void);
int   render_3D_filter_get_upper_azimuth(void);

struct s_render_3D_dbentry*
      render_3D_init (int sampling_rate);

void  render_3D_free (struct s_render_3D_dbentry **data);
void  render_3D      (struct s_render_3D_dbentry *data,
                      coded_unit *in_native,
                      coded_unit *out_native);

void render_3D_set_parameters (struct s_render_3D_dbentry *p_3D_data,
                               int sampling_rate,
                               int azimuth,
                               int filter_number,
                               int length);

void render_3D_get_parameters (struct s_render_3D_dbentry *p_3D_data,
                               int *azimuth,
                               int *filter_type,
                               int *filter_length);

#endif /* __RENDER_3D_H__ */
