/*  DynaRes
 *  Copyright (C) 2021 rTomas.
 *
 *  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2021 - Daniel De Matteis
 *  Copyright (C) 2012-2015 - Michael Lelli
 *  Copyright (C) 2014-2017 - Jean-Andr� Santoni
 *  Copyright (C) 2016-2019 - Brad Parker
 *  Copyright (C) 2016-2019 - Andr�s Su�rez (input mapper code)
 *  Copyright (C) 2016-2017 - Gregor Richards (network code)
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __VIDEO_DYNARES_H__
#define __VIDEO_DYNARES_H__

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

void dynares_print_time(void);

void dynares_check_rotation(
   unsigned *width,
   unsigned *height);

int dynares_is_mame_rotated(void);

void dynares_video_show_info(
   int width,
   int height,
   int interlaced,
   float hz);

void dynares_init(
   unsigned *width,
   unsigned *height,
   unsigned base_width,
   unsigned base_height,
   double fps,
   int region,
   float *font_size);

void dynares_loop_check(
   unsigned base_width,
   unsigned base_height,
   double fps);

void dynares_set_geometry(
   unsigned base_width,
   unsigned base_height,
   double fps);

RETRO_END_DECLS

#endif