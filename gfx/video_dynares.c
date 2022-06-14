/*  DynaRes
 *  Copyright (C) 2021 rTomas.
 *  Available retroarch.cfg parameters:
 *  dynares_mode          = disabled, native, superx, custom, fixed
 *  dynares_crt_type      = "generic_15"
 *  dynares_video_info    = "true"
 *  dynares_handheld_full = "true"
 *  dynares_fast_mode     = "true"
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
#include <stdio.h>
#include <stdlib.h>
#include "../verbosity.h"
#include "../retroarch.h"
#include "../driver.h"
#include "../deps/switchres/switchres_wrapper.h"
#include "video_dynares.h"
#include <string/stdstring.h>
#include <time.h>
#include <math.h>

/* DynaRes params */
char *dynares;
char *sys_name;
unsigned width = 0;
unsigned height = 0;
unsigned rotation = 0;
bool handheld_full;
char *crt_type;
char fst_load = 1;
timing srtiming;
char sys_timing[256];
char gam_timing[256];
FILE *pf;

void dynares_print_time(void)
{
   long ms;  // Milliseconds
   time_t s; // Seconds
   struct timespec spec;

   clock_gettime(CLOCK_BOOTTIME, &spec);

   s = spec.tv_sec;
   ms = round(spec.tv_nsec / 1.0e6); // Convert nanoseconds to milliseconds
   if (ms > 999)
   {
      s++;
      ms = 0;
   }

   RARCH_LOG("[DynaRes]: Current time: %" PRIdMAX ".%03ld seconds since the Epoch\n", (intmax_t)s, ms);
}

void dynares_check_rotation(unsigned *width, unsigned *height)
{
   RARCH_LOG("[DynaRes]: Check rotation: System: %s, resolution: %dx%d, rotation: %d\n", sys_name, *width, *height, rotation);

   if (string_is_equal(sys_name, "MAME"))
   {
      if (rotation == 1 || rotation == 3)
      {
         unsigned tmp = *width;
         *width = *height;
         *height = tmp;
         RARCH_LOG("[DynaRes]: Check rotation: New resolution: %dx%d, rotation: %d\n", *width, *height, rotation);
      }
   }
}

void dynares_video_show_info(int width, int height, int interlaced, float hz)
{
   char msg[128];
   settings_t *settings = config_get_ptr();

   char* mode = "p";

   if(interlaced)
      mode = "i";

   snprintf(msg, sizeof(msg),
            "%ux%u%s %.3fHz.", width, height, mode, hz);
   if (settings->bools.dynares_video_info)
      runloop_msg_queue_push(msg, 1, 90, false, NULL,
                             MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   RARCH_LOG("[DynaRes]: Info: %s\n", msg);
}

void dynares_init(unsigned *width, unsigned *height, unsigned base_width, unsigned base_height, double fps, int region, float *font_size)
{
   settings_t *settings = config_get_ptr();
   dynares = settings->arrays.dynares_mode;
   crt_type = settings->arrays.dynares_crt_type;
   handheld_full = settings->bools.dynares_handheld_full;

   int v_width = 0;
   int v_height = 0;
   int overscan_x = 0;
   int overscan_y = 0;

   /* Get core name, rotation and make mame rotation if required */
   runloop_state_t *runloop_st = runloop_state_get_ptr();
   rarch_system_info_t *sys_info = &runloop_st->system;
   sys_name = sys_info->info.library_name;
   rotation = settings->uints.video_rotation;
   *width = base_width;
   *height = base_height;
   dynares_check_rotation(width, height);
   base_width = *width;
   base_height = *height;

   if (string_is_equal(dynares, "native"))
   {
      /* Set screen width and height */
      *width = base_width;
      *height = base_height;
      /* Support for DOSBox, GBA, NGP, X68K, GBC on 15KHz TV */
      if (*width == 640 && *height == 400) // DOS
         *height = 480;
      else if (*width == 800 && *height == 600) // DOS
         *height = 480;
      else if (*width == 320 && *height == 350) // X68K
      {
         *width = 640;
         *height = 480;
      }
      else if (*width == 240 && *height == 160) // GBA
      {
         if (handheld_full)
         {
            *width = 720;
            *height = 480;
         }
         else
         {
            *width = 256;
            *height = 224;
         }
      }
      else if (*width == 160 && *height == 152) // NGP
      {
         if (handheld_full)
         {
            *width = 480;
            *height = 456;
         }
         else
         {
            *width = 256;
            *height = 224;
         }
      }
      else if (*width == 160 && *height == 144) // GBC
      {
         /* Cannot make full screen due to impossible refresh rate generation */
         *width = 256;
         *height = 224;
      }
      /* Set viewport */
      v_width = *width;
      v_height = *height;
      /* Set font size */
      if (*height <= 300)
         *font_size = 6;
      else if (*height > 300)
         *font_size = 12;
      settings->floats.video_font_size = *font_size;
   }
   else if (string_is_equal(dynares, "superx"))
   {
      /* Set screen width - (x) = horizontal overscan */
      if (base_width == 384 || base_width == 448)
         *width = 2744; /* 384(8) (Capcom arcade games), 448(9) (SGI arcade games) */
      else if (base_width == 292)
         *width = 2400; /* 292(8) (Williams arcade games) */
      else if (base_width == 160 || base_width == 240 || base_width == 304 || base_width == 400 || base_width == 800)
         *width = 2464; /* 160(8), 240(8), 304(4), 400(6), 800(6) */
      else if (base_width == 720)
         *width = 2880; /* 720(0) (Amiga) */
      else if (base_width == 256 || base_width == 320 || base_width == 368 || base_width == 512 || base_width == 640)
         *width = 2624; /* 256(8), 320(8), 368(6), 512(8), 640(8); (x)=overscan, Intended for all systems except Capcom Arcade games */
      else
      {
         /* Choose the best available super res */
         int resolutions[5] = {2400, 2464, 2624, 2744, 2880};
         int lst_size = sizeof(resolutions) / sizeof(resolutions[0]);
         int best_mark, mark;
         for (int i = 0; i < lst_size; i++)
         {
            if (i == 0)
            {
               best_mark = resolutions[i] % base_width;
               *width = resolutions[i];
            }
            else
            {
               mark = resolutions[i] % base_width;
               if (mark < best_mark)
               {
                  best_mark = mark;
                  *width = resolutions[i];
               }
            }
         }
      }
      settings->uints.video_fullscreen_x = *width;
      /* Set screen height */
      *height = base_height;
      /* Support for DOSBox, GBA, NGP, X68K, GBC on 15KHz TV */
      if (*height == 350 || *height == 400 || *height == 600) // DOS & X68K
         *height = 480;
      else if (*height == 432) // PSX
         *height = 480;
      else if (*height == 160) // GBA
      {
         if (handheld_full)
         {
            *height = 480;
            overscan_x = 32;
            overscan_y = 40;
         }
         else
         {
            *height = 224;
            overscan_x = base_width * 2;
         }
      }
      else if (*height == 152) // NGP
      {
         if (handheld_full)
            *height = 456;
         else
         {
            *height = 224;
            overscan_x = base_width * 5;
         }
      }
      else if (*height == 144) // GBC
      {
         if (handheld_full)
         {
            *height = 448;
            overscan_x = base_width;
         }
         else
         {
            *height = 224;
            overscan_x = base_width * 5;
         }
      }
      settings->uints.video_fullscreen_y = *height;
      /* Set viewport */
      v_width = (*width / base_width) * base_width - overscan_x;
      v_height = (*height / base_height) * base_height - overscan_y;
      /* Set font size */
      *font_size = 48;
      settings->floats.video_font_size = *font_size;
   }
   else if (string_is_equal(dynares, "fixed"))
   {
      /* Set screen width */
      *width = 1968; /* 1920(8) */
      settings->uints.video_fullscreen_x = *width;
      /* Set screen height */
      if (region == 0) /* NTSC */
         *height = 480;
      else if (region == 1) /* PAL */
         *height = 512;
      else
         *height = 480;
      settings->uints.video_fullscreen_y = *height;
      /* Set viewport */
      v_width = *width;
      v_height = *height;
      /* Set font size */
      *font_size = 48;
      settings->floats.video_font_size = *font_size;
   }
   else if (string_is_equal(dynares, "custom"))
   {
      *width = settings->uints.video_fullscreen_x;
      *height = settings->uints.video_fullscreen_y;
      v_width = settings->video_viewport_custom.width;
      v_height = settings->video_viewport_custom.height;
   }
   RARCH_LOG("[DynaRes]: Init: Loading DynaRes... \n");
   /* Gen system timing */
   sr_init();
   sr_set_log_level(0);
   sr_set_monitor(crt_type);
   sr_set_interlace_force_even(1);
   sr_set_v_shift_correct(0);
   sr_set_user_mode(320, 240, 60.0);
   sr_init_disp("auto");
   sr_get_timing(320, 240, 60.0, crt_type, 0, &srtiming);
   snprintf(sys_timing, sizeof(sys_timing), "%d %d %d %d %d %d %d %d %d %d %d %d %d %f %d %ld %d",
            srtiming.hhh, srtiming.hsync, srtiming.h_f_porch, srtiming.h_b_porch, srtiming.h_total,
            srtiming.vvv, srtiming.vsync, srtiming.v_f_porch, srtiming.v_b_porch, srtiming.v_total,
            srtiming.vsync_offset_a, srtiming.vsync_offset_b, srtiming.pixel_rep,
            srtiming.fps, srtiming.interlaced, srtiming.pixel_freq, srtiming.aspect);
   // RARCH_LOG("[DynaRes]: Init: System timing requested: %s\n", sys_timing);
   sr_deinit();
   /* Gen game timing */
   sr_init();
   sr_set_log_level(0); // 0-3
   sr_set_monitor(crt_type);
   sr_set_interlace_force_even(1);
   sr_set_user_mode(*width, *height, fps);
   /*if (string_is_equal(crt_type, "generic_15") && (int)fps == 54)
      sr_set_v_shift_correct(1);
   else
      sr_set_v_shift_correct(0);*/
   sr_init_disp("auto");
   if(sr_get_timing(*width, *height, fps, crt_type, 0, &srtiming) == 1)
   {
      snprintf(gam_timing, sizeof(gam_timing), "%d %d %d %d %d %d %d %d %d %d %d %d %d %f %d %ld %d",
               srtiming.hhh, srtiming.hsync, srtiming.h_f_porch, srtiming.h_b_porch, srtiming.h_total,
               srtiming.vvv, srtiming.vsync, srtiming.v_f_porch, srtiming.v_b_porch, srtiming.v_total,
               srtiming.vsync_offset_a, srtiming.vsync_offset_b, srtiming.pixel_rep,
               srtiming.fps, srtiming.interlaced, srtiming.pixel_freq, srtiming.aspect);
      RARCH_LOG("[DynaRes]: Init: %s(%s): Game timing requested: %s\n", dynares, crt_type, gam_timing);
      sr_deinit();
   } else {
      /* 15K/25/31Khz transformation */
      sr_deinit();
      sr_init();
      sr_set_log_level(0); // 0-3
      sr_set_monitor(crt_type);
      sr_set_interlace_force_even(1);
      /*if (string_is_equal(crt_type, "generic_15") && (int)fps == 54)
         sr_set_v_shift_correct(1);
      else
         sr_set_v_shift_correct(0);*/
      sr_init_disp("auto");
      sr_get_timing(*width, *height, fps, crt_type, 0, &srtiming);
      snprintf(gam_timing, sizeof(gam_timing), "%d %d %d %d %d %d %d %d %d %d %d %d %d %f %d %ld %d",
               srtiming.hhh, srtiming.hsync, srtiming.h_f_porch, srtiming.h_b_porch, srtiming.h_total,
               srtiming.vvv, srtiming.vsync, srtiming.v_f_porch, srtiming.v_b_porch, srtiming.v_total,
               srtiming.vsync_offset_a, srtiming.vsync_offset_b, srtiming.pixel_rep,
               srtiming.fps, srtiming.interlaced, srtiming.pixel_freq, srtiming.aspect);
      RARCH_LOG("[DynaRes]: Init: %s(%s): Game timing requested: %s\n", dynares, crt_type, gam_timing);
      RARCH_LOG("[DynaRes]: Init: Game timing transformation (15K/25/31Khz): %u %u %f\n", srtiming.hhh, srtiming.vvv, fps);
      sr_deinit();
      if(srtiming.vvv != *height)
      {
         *width = srtiming.hhh;
         *height = srtiming.vvv;
         v_width = *width;
         v_height = *height;
         settings->uints.video_fullscreen_x = *width;
         settings->uints.video_fullscreen_y = *height;
         if (string_is_equal(dynares, "superx"))
            settings->bools.video_smooth = 1;
      }
   }
   /* Write timings to file */
   pf = fopen("/home/pi/RGB-Pi/data/timings.dat", "w");
   fputs(sys_timing, pf);
   fputc('\n', pf);
   fputs(gam_timing, pf);
   fputc('\n', pf);
   fclose(pf);
   RARCH_LOG("[DynaRes]: Init: %s: Setting resolution %ux%u@%f\n", dynares, *width, *height, fps);
   /* Set viewport */
   settings->video_viewport_custom.width = v_width;
   settings->video_viewport_custom.height = v_height;
   settings->video_viewport_custom.x = (settings->uints.video_fullscreen_x - v_width) / 2;
   settings->video_viewport_custom.y = (settings->uints.video_fullscreen_y - v_height) / 2;
   RARCH_LOG("[DynaRes]: Init: %s: Viewport resolution changed: %ux%u\n", dynares, v_width, v_height);
   dynares_video_show_info(base_width, base_height, srtiming.interlaced, fps);
}

void dynares_loop_check(unsigned base_width, unsigned base_height, double fps)
{
   if (string_is_equal(dynares, "native"))
   {
      if (fst_load)
      {
         settings_t *settings = config_get_ptr();
         RARCH_LOG("[DynaRes]: Loop: %s: Resolution init: %dx%d@%f\n", dynares, base_width, base_height, fps);
         fst_load = 0;
         width = base_width;
         height = base_height;
         settings->uints.video_aspect_ratio_idx = 21; /* 1:1 (fixed aspect ratio) */
      }
      else
      {
         if (width != base_width || height != base_height)
         {
            width = base_width;
            height = base_height;
            RARCH_LOG("[DynaRes]: Loop: %s: Resolution changed: %dx%d@%f\n", dynares, base_width, base_height, fps);
            settings_t *settings = config_get_ptr();
            if (settings->bools.dynares_fast_mode)
               dynares_video_driver_reinit();
            else
               video_driver_reinit(DRIVERS_CMD_ALL);
         }
      }
   }
   else if (string_is_equal(dynares, "superx"))
   {
      if (fst_load)
      {
         settings_t *settings = config_get_ptr();
         RARCH_LOG("[DynaRes]: Loop: %s: Resolution init: %dx%d@%f\n", dynares, base_width, base_height, fps);
         fst_load = 0;
         settings->uints.video_aspect_ratio_idx = 23; /* Custom (non-fixed aspect ratio) */
      }
   }
   else if (string_is_equal(dynares, "fixed"))
   {
      if (fst_load)
      {
         settings_t *settings = config_get_ptr();
         RARCH_LOG("[DynaRes]: Loop: %s: Resolution changed: %dx%d@%f\n", dynares, base_width, base_height, fps);
         fst_load = 0;
         settings->uints.video_aspect_ratio_idx = 23; /* Custom (non-fixed aspect ratio) */
      }
   }
}

void dynares_set_geometry(unsigned base_width, unsigned base_height, double fps)
{
   if (string_is_equal(dynares, "superx"))
   {
      settings_t *settings = config_get_ptr();
      int overscan_x = 0;
      int overscan_y = 0;
      int v_height = 0;
      int v_width = 0;
      /* Dynamic height resolution */
      if (height != base_height)
      {
         height = base_height;
         RARCH_LOG("[DynaRes]: Geom: %s: Resolution changed: %dx%d\n", dynares, settings->uints.video_fullscreen_x, base_height);
         if (settings->bools.dynares_fast_mode)
            dynares_video_driver_reinit();
         else
            video_driver_reinit(DRIVERS_CMD_ALL);
      }
      /* Dynamic super integer scale (viewport custom) */
      handheld_full = settings->bools.dynares_handheld_full;
      if (base_height == 160)
      {
         if (handheld_full)
         {
            overscan_x = 32;
            overscan_y = 40;
         }
         else
            overscan_x = base_width * 2;
      }
      else if (height == 152) // NGP
         overscan_x = base_width * 5;
      else if (height == 144) // GBC
      {
         if (handheld_full)
            overscan_x = base_width;
         else
            overscan_x = base_width * 5;
      }
      v_width = (settings->uints.video_fullscreen_x / base_width) * base_width - overscan_x;
      v_height = (settings->uints.video_fullscreen_y / base_height) * base_height - overscan_y;
      settings->video_viewport_custom.width = v_width;
      settings->video_viewport_custom.height = v_height;
      settings->video_viewport_custom.x = (settings->uints.video_fullscreen_x - v_width) / 2;
      settings->video_viewport_custom.y = (settings->uints.video_fullscreen_y - v_height) / 2;
      RARCH_LOG("[DynaRes]: Geom: %s: Viewport resolution changed: %ux%u\n", dynares, v_width, v_height);
      dynares_video_show_info(base_width, base_height, srtiming.interlaced, fps);
   }
}