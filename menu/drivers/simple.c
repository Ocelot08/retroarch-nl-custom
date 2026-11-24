/*  RetroArch - A frontend for libretro.
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

/* Phone-first menu driver for the Simple layout. A style test: it draws
 * the current list with the Simple type and spacing, and takes taps and
 * the normal menu buttons. Sizes are in dp, on a 360 x 780 dp design.
 * Gamepad mode draws the menu in a 4:3 box at the top, over the touch
 * controls. */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <compat/strl.h>
#include <file/file_path.h>
#include <lists/dir_list.h>
#include <string/stdstring.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../menu_driver.h"
#include "../menu_cbs.h"
#include "../../gfx/gfx_display.h"
#include "../../gfx/font_driver.h"
#include "../../configuration.h"
#include "../../core_info.h"
#include "../../defaults.h"
#include "../../playlist.h"
#include "../../runloop.h"
#include "../../command.h"
#include "../../input/input_driver.h"
#include "../../input/input_osk.h"
#include "../../audio/audio_driver.h"
#include "../menu_setting.h"
#include "../../setting_list.h"
#include "../../msg_hash_lbl_str.h"

#ifdef ANDROID
/* As in menu_setting.c: platform_unix.h brings in JNI */
unsigned android_app_get_cutout_top(void);
void android_app_update_viewport_inset(void);
#endif

/* Test builds log pages, taps and actions to logcat for scripted runs */
#if defined(SIMPLE_MENU_TEST) && defined(ANDROID)
#include <stdarg.h>
#include <android/log.h>
#define SIMPLE_DEBUG 1
#else
#define SIMPLE_DEBUG 0
#endif

#define SIMPLE_CRUMB_PARTS    12
#define SIMPLE_CRUMB_PART     48
#define SIMPLE_DESIGN_WIDTH   360.0f
#define SIMPLE_GUTTER         22.0f
#define SIMPLE_MORE_GAP       16.0f
/* Edge bounce, as Material UI's: drag resistance past an end, the
 * furthest it may go as a share of the view, and the spring back */
#define SIMPLE_OVER_RESIST    0.35f
#define SIMPLE_OVER_MAX       0.25f
#define SIMPLE_SPRING_K       70.0f
#define SIMPLE_SPRING_DAMP    14.0f
/* Small caps letter-spacing, as a share of the size */
#define SIMPLE_TRACKING       0.08f

#define SIMPLE_COL_BG         0x15171CFF
#define SIMPLE_COL_BELOW      0x0E1014FF
#define SIMPLE_COL_FG         0xEEF0F4FF
#define SIMPLE_COL_MUTED      0x9AA0ADFF
#define SIMPLE_COL_DIM        0x7B818FFF   /* 4.5:1 on the page */
#define SIMPLE_COL_LINE       0x2F333DFF
#define SIMPLE_COL_ACCENT     0xF2D64BFF
#define SIMPLE_COL_SHEET      0x20232BFF
#define SIMPLE_COL_SELECT     0x4A5060FF   /* selected text */

enum simple_font
{
   SIMPLE_FONT_WORD = 0,
   SIMPLE_FONT_HOME,
   SIMPLE_FONT_NOTE,
   SIMPLE_FONT_TITLE,
   SIMPLE_FONT_FULL,       /* RetroArch's own lists */
   SIMPLE_FONT_LAST
};

/* All in dp */
typedef struct simple_metrics
{
   float font[SIMPLE_FONT_LAST];
   float header_y;
   float list_top;
   float list_row;
   float home_top;
   float home_row;
   float band_h;
   float cont_label_y;
   float cont_title_y;
   float cont_bottom;
} simple_metrics_t;

static const simple_metrics_t simple_touch = {
   { 24.0f, 30.0f, 11.0f, 48.0f, 18.0f },
   50.0f, 80.0f, 50.0f, 110.0f, 60.0f,
   360.0f, 262.0f, 304.0f, 352.0f };
static const simple_metrics_t simple_gamepad = {
   { 17.0f, 20.0f, 9.5f, 22.0f, 14.0f },
   20.0f, 40.0f, 30.0f, 40.0f, 34.0f,
   96.0f, 46.0f, 66.0f, 88.0f };

/* Art band colours, picked by the game's name until box art is drawn */
static const uint32_t simple_band[][2] = {
   { 0x3B5BDBFF, 0x7048E8FF },
   { 0x0CA678FF, 0x1971C2FF },
   { 0xE8590CFF, 0xC2255CFF },
   { 0x5F3DC4FF, 0x2B8A3EFF },
   { 0x1098ADFF, 0x364FC7FF },
   { 0x862E9CFF, 0xD6336CFF }
};

typedef struct simple_handle
{
   menu_input_pointer_t pointer;    /* int64_t alignment */
   font_data_impl_t fonts[SIMPLE_FONT_LAST];
   menu_entry_t entry;
   /* Main thread's own, for taps; entry above is the drawing thread's */
   menu_entry_t hit_entry;
   menu_entry_t log_entry;          /* descriptions, the action log */
   const simple_metrics_t *m;
   simple_metrics_t metrics;        /* m, moved clear of a camera cutout */
   const file_list_t *last_list;
   size_t first;
   size_t more_idx;
   size_t last_selection;
   /* A number row opened to its slider, or (size_t)-1 */
   size_t open_idx;
   size_t open_depth;               /* stack depth it was opened at */
   float scroll_y;
   float desc_h;                    /* height of the description */
   float shift;                     /* dp the cutout moves the page */
   float drag_start_y;
   float drag_start_scroll;
   float spring_v;
   unsigned dims;
   unsigned n_systems;
   unsigned n_cores;
   float scale;
   bool is_home;
   bool full;               /* one of RetroArch's own lists: smaller type */
   bool has_continue;
   bool gamepad;
   bool dragging;
   bool bouncing;
   bool sliding;
   bool was_pressed;
   bool opts;               /* the option list opt_path names */
   char core_note[64];
   char msgbox[1024];
   /* Of the setting whose option list was last opened */
   char opt_path[16];
   char opt_title[NAME_MAX_LENGTH];
   char opt_desc[MENU_LABEL_MAX_LENGTH];
   char desc_text[MENU_LABEL_MAX_LENGTH];
   /* The prompt card, laid out on the main thread for the drawing one */
   char card_text[1024];
   float card_y;
   float card_h;
   float card_rows_y;
   unsigned card_lines;
   unsigned desc_lines;
   /* Text being typed, laid out as the card is; kb_open as it began */
   char kb_text[1024];
   char kb_open[1024];
   char kb_label[NAME_MAX_LENGTH];
   float kb_cursor_x;
   unsigned kb_lines;
   unsigned kb_cursor_line;
   bool kb_on;
   bool kb_untouched;
   bool kb_selected;
   bool card_on;
   bool card_confirm;
   bool msg_seen;
   /* Test builds: what was last logged */
   size_t log_sel;
   size_t log_size;
   size_t log_depth;
   unsigned log_dims;
   float log_scroll;
   bool log_gamepad;
   bool log_rows;
   char log_title[NAME_MAX_LENGTH];
   char crumb[SIMPLE_CRUMB_PARTS][SIMPLE_CRUMB_PART];
} simple_handle_t;

static void simple_color(float *c, int vert, uint32_t rgba)
{
   c[vert * 4 + 0] = ((rgba >> 24) & 0xFF) / 255.0f;
   c[vert * 4 + 1] = ((rgba >> 16) & 0xFF) / 255.0f;
   c[vert * 4 + 2] = ((rgba >>  8) & 0xFF) / 255.0f;
   c[vert * 4 + 3] = ( rgba        & 0xFF) / 255.0f;
}

static void simple_upper(char *dst, const char *src, size_t len)
{
   size_t i;
   for (i = 0; i + 1 < len && src[i]; i++)
      dst[i] = (src[i] >= 'a' && src[i] <= 'z') ? src[i] - 32 : src[i];
   dst[i] = '\0';
}

static float simple_scale_for(unsigned dims)
{
   unsigned w = VIDEO_SCALE_W(dims);
   unsigned h = VIDEO_SCALE_H(dims);
   float s    = (float)(w < h ? w : h) / SIMPLE_DESIGN_WIDTH;
   return s > 0.5f ? s : 0.5f;
}

/* Bottom of the area the menu draws in, in pixels */
static float simple_box_bottom(const simple_handle_t *sm, unsigned dims)
{
   float w = (float)VIDEO_SCALE_W(dims);
   float h = (float)VIDEO_SCALE_H(dims);
   if (sm->gamepad && w * 0.75f + sm->shift * sm->scale < h)
      return w * 0.75f + sm->shift * sm->scale;
   return h;
}

#define SIMPLE_CUTOUT_GAP 12.0f

/* The mode's metrics, moved down so the header is below a cutout the
 * window reaches behind */
static void simple_set_metrics(simple_handle_t *sm)
{
   float inset = 0.0f;
   simple_metrics_t *m = &sm->metrics;

   *m = sm->gamepad ? simple_gamepad : simple_touch;
#ifdef ANDROID
   inset = (float)android_app_get_cutout_top() / sm->scale;
   android_app_update_viewport_inset();
#endif
   sm->shift = inset > 0.0f ? inset + SIMPLE_CUTOUT_GAP - m->header_y : 0.0f;
   if (sm->shift < 0.0f)
      sm->shift = 0.0f;
   m->header_y     += sm->shift;
   m->list_top     += sm->shift;
   m->home_top     += sm->shift;
   m->band_h       += sm->shift;
   m->cont_label_y += sm->shift;
   m->cont_title_y += sm->shift;
   m->cont_bottom  += sm->shift;
   sm->m            = m;
}

static void simple_fonts_free(simple_handle_t *sm)
{
   int i;
   for (i = 0; i < SIMPLE_FONT_LAST; i++)
   {
      if (sm->fonts[i].font)
         font_driver_free(sm->fonts[i].font);
      sm->fonts[i].font = NULL;
   }
}

static void simple_fonts_load(simple_handle_t *sm, bool is_threaded)
{
   static const char *files[SIMPLE_FONT_LAST] = {
      "WorkSans-Light.ttf", "WorkSans-Light.ttf",
      "IBMPlexMono-Medium.ttf", "WorkSans-Light.ttf",
      "WorkSans-Light.ttf" };
   char dir[DIR_MAX_LENGTH];
   char path[PATH_MAX_LENGTH];
   settings_t *settings  = config_get_ptr();
   gfx_display_t *p_disp = disp_get_ptr();
   int i;

   simple_fonts_free(sm);
   fill_pathname_join_special(dir, settings->paths.directory_assets,
         "simple", sizeof(dir));

   for (i = 0; i < SIMPLE_FONT_LAST; i++)
   {
      font_data_impl_t *f = &sm->fonts[i];
      fill_pathname_join_special(path, dir, files[i], sizeof(path));
      /* Missing files fall back to the default font */
      f->font = gfx_display_font_file(p_disp,
            path_is_valid(path) ? path : NULL,
            sm->m->font[i] * sm->scale, is_threaded);
      if (f->font)
      {
         f->line_height        = (int)(f->font->metrics.height + 0.5f);
         f->line_ascender      = (int)(f->font->metrics.ascender + 0.5f);
         f->line_centre_offset = (int)((f->font->metrics.ascender
               - f->font->metrics.descender) * 0.5f + 0.5f);
      }
   }
}

static void simple_text(simple_handle_t *sm, enum simple_font fi,
      const char *s, float x, float y_centre, unsigned dims,
      uint32_t color, enum text_alignment align)
{
   font_data_impl_t *f = &sm->fonts[fi];
   if (!f->font || !s || !*s)
      return;
   gfx_display_draw_text(f->font, s, x,
         y_centre + (float)f->line_centre_offset, dims,
         color, align, 1.0f, false, 0.0f, false);
}

static size_t simple_utf8_len(unsigned char c)
{
   if (c < 0x80)
      return 1;
   if ((c & 0xE0) == 0xC0)
      return 2;
   if ((c & 0xF0) == 0xE0)
      return 3;
   return 4;
}

static float simple_note_width(simple_handle_t *sm, const char *s)
{
   const char *p;
   float width         = 0.0f;
   font_data_impl_t *f = &sm->fonts[SIMPLE_FONT_NOTE];
   float track         = sm->m->font[SIMPLE_FONT_NOTE] * sm->scale
      * SIMPLE_TRACKING;
   if (!f->font || !s || !*s)
      return 0.0f;
   for (p = s; *p; )
   {
      size_t n = simple_utf8_len((unsigned char)*p);
      width   += font_driver_get_message_width(f->font, p, n, 1.0f) + track;
      p       += n;
   }
   return width - track;
}

/* Copies @src, cut short with an ellipsis if it is wider than @max_w */
static void simple_fit(simple_handle_t *sm, enum simple_font fi,
      const char *src, float max_w, char *dst, size_t len)
{
   size_t n;
   font_data_t *font = sm->fonts[fi].font;

   n = strlcpy(dst, src, len);
   if (!font || n >= len
         || font_driver_get_message_width(font, dst, n, 1.0f) <= max_w)
      return;
   while (n > 0)
   {
      do
         n--;
      while (n > 0 && ((unsigned char)dst[n] & 0xC0) == 0x80);
      while (n > 0 && dst[n - 1] == ' ')
         n--;
      if (n + 4 > len)
         continue;
      memcpy(dst + n, "\xE2\x80\xA6", 4);
      if (font_driver_get_message_width(font, dst, n + 3, 1.0f) <= max_w)
         return;
   }
}

/* As simple_fit, for notes: their width has the letter-spacing */
static void simple_fit_note(simple_handle_t *sm, const char *src,
      float max_w, char *dst, size_t len)
{
   size_t n = strlcpy(dst, src, len);

   if (n >= len)
      n = len - 1;
   if (simple_note_width(sm, dst) <= max_w)
      return;
   while (n > 0)
   {
      do
         n--;
      while (n > 0 && ((unsigned char)dst[n] & 0xC0) == 0x80);
      while (n > 0 && dst[n - 1] == ' ')
         n--;
      if (n + 4 > len)
         continue;
      memcpy(dst + n, "\xE2\x80\xA6", 4);
      if (simple_note_width(sm, dst) <= max_w)
         return;
   }
}

/* Small caps notes, drawn a glyph at a time for the letter-spacing */
static void simple_note(simple_handle_t *sm, const char *s,
      float x, float y_centre, unsigned dims, uint32_t color,
      enum text_alignment align)
{
   char glyph[8];
   const char *p;
   float width          = 0.0f;
   font_data_impl_t *f  = &sm->fonts[SIMPLE_FONT_NOTE];
   float track          = sm->m->font[SIMPLE_FONT_NOTE] * sm->scale
      * SIMPLE_TRACKING;

   if (!f->font || !s || !*s)
      return;

   for (p = s; *p; )
   {
      size_t n = simple_utf8_len((unsigned char)*p);
      width   += font_driver_get_message_width(f->font, p, n, 1.0f) + track;
      p       += n;
   }
   width -= track;
   if (align == TEXT_ALIGN_RIGHT)
      x -= width;
   else if (align == TEXT_ALIGN_CENTER)
      x -= width * 0.5f;

   for (p = s; *p; )
   {
      size_t n = simple_utf8_len((unsigned char)*p);
      if (n >= sizeof(glyph))
         break;
      memcpy(glyph, p, n);
      glyph[n] = '\0';
      if (*p != ' ')
         simple_text(sm, SIMPLE_FONT_NOTE, glyph, x, y_centre, dims,
               color, TEXT_ALIGN_LEFT);
      x += font_driver_get_message_width(f->font, p, n, 1.0f) + track;
      p += n;
   }
}

static void simple_quad(gfx_display_t *p_disp, void *userdata,
      unsigned dims, float x, float y, float w, float h, uint32_t rgba)
{
   float c[16];
   int i;
   for (i = 0; i < 4; i++)
      simple_color(c, i, rgba);
   gfx_display_draw_quad(p_disp, userdata, dims,
         (int)(x + 0.5f), (int)(y + 0.5f),
         VIDEO_SCALE_PACK((unsigned)(w + 0.5f), (unsigned)(h + 0.5f)),
         dims, c, NULL);
}

static uint32_t simple_mix(uint32_t a, uint32_t b, float t)
{
   uint32_t out = 0xFF;
   int shift;
   for (shift = 8; shift <= 24; shift += 8)
   {
      float ca = (float)((a >> shift) & 0xFF);
      float cb = (float)((b >> shift) & 0xFF);
      out     |= (uint32_t)(ca + (cb - ca) * t + 0.5f) << shift;
   }
   return out;
}

/* Vertices run bottom-left, bottom-right, top-left, top-right */
static void simple_grad_quad(gfx_display_t *p_disp, void *userdata,
      unsigned dims, float y, float h, uint32_t top_l, uint32_t top_r,
      uint32_t bot_l, uint32_t bot_r)
{
   float c[16];
   simple_color(c, 0, bot_l);
   simple_color(c, 1, bot_r);
   simple_color(c, 2, top_l);
   simple_color(c, 3, top_r);
   gfx_display_draw_quad(p_disp, userdata, dims, 0, (int)(y + 0.5f),
         VIDEO_SCALE_PACK(VIDEO_SCALE_W(dims), (unsigned)(h + 0.5f)),
         dims, c, NULL);
}

/* Colour at the top, eased into the background so it has no edge */
static void simple_band_quad(gfx_display_t *p_disp, void *userdata,
      unsigned dims, float h, uint32_t left, uint32_t right)
{
   float h1      = h * 0.45f;
   uint32_t ml   = simple_mix(left,  SIMPLE_COL_BG, 0.75f);
   uint32_t mr   = simple_mix(right, SIMPLE_COL_BG, 0.75f);
   simple_grad_quad(p_disp, userdata, dims, 0.0f, h1, left, right, ml, mr);
   simple_grad_quad(p_disp, userdata, dims, h1, h - h1, ml, mr,
         SIMPLE_COL_BG, SIMPLE_COL_BG);
}

static bool simple_is_more(const char *label)
{
   return label && (
         string_is_equal(label, MENU_ENUM_LABEL_SIMPLE_ALL_SETTINGS_STR)
      || string_is_equal(label, MENU_ENUM_LABEL_SIMPLE_MORE_STR)
      || string_is_equal(label, MENU_ENUM_LABEL_SIMPLE_GAMES_MORE_STR)
      || string_is_equal(label, MENU_ENUM_LABEL_SIMPLE_CONTROLS_MORE_STR)
      || string_is_equal(label, MENU_ENUM_LABEL_SIMPLE_SCREEN_MORE_STR)
      || string_is_equal(label, MENU_ENUM_LABEL_SIMPLE_SOUND_MORE_STR)
      || string_is_equal(label, MENU_ENUM_LABEL_SIMPLE_SAVING_MORE_STR)
      || string_is_equal(label, MENU_ENUM_LABEL_SIMPLE_LIBRARY_MORE_STR)
      || string_is_equal(label, MENU_ENUM_LABEL_SIMPLE_ACCOUNTS_MORE_STR)
      || string_is_equal(label, MENU_ENUM_LABEL_SIMPLE_ACCESSIBILITY_MORE_STR)
      || string_is_equal(label, MENU_ENUM_LABEL_SIMPLE_INTERFACE_MORE_STR)
      || string_is_equal(label, MENU_ENUM_LABEL_SIMPLE_DOWNLOADS_MORE_STR));
}

static rarch_setting_t *simple_row_setting(const file_list_t *list,
      size_t i)
{
   menu_file_list_cbs_t *cbs = (list && i < list->size)
      ? (menu_file_list_cbs_t*)list->list[i].actiondata : NULL;
   return cbs ? cbs->setting : NULL;
}

static void simple_log(const char *fmt, ...)
{
#if SIMPLE_DEBUG
   va_list ap;
   va_start(ap, fmt);
   __android_log_vprint(ANDROID_LOG_INFO, "RASimple", fmt, ap);
   va_end(ap);
#else
   (void)fmt;
#endif
}

static const char *simple_action_name(enum menu_action action)
{
   switch (action)
   {
      case MENU_ACTION_UP:     return "up";
      case MENU_ACTION_DOWN:   return "down";
      case MENU_ACTION_LEFT:   return "left";
      case MENU_ACTION_RIGHT:  return "right";
      case MENU_ACTION_OK:     return "ok";
      case MENU_ACTION_CANCEL: return "cancel";
      case MENU_ACTION_INFO:   return "info";
      case MENU_ACTION_SELECT: return "select";
      case MENU_ACTION_START:  return "start";
      default:                 break;
   }
   return "other";
}

/* Keeps the name and description of row @i for its option list */
static void simple_keep_options(simple_handle_t *sm, size_t i)
{
   struct menu_state *menu_st = menu_state_get_ptr();
   menu_entry_t *e            = &sm->log_entry;
   const file_list_t *list    = menu_st->entries.list
      ? MENU_LIST_GET_SELECTION(menu_st->entries.list, 0) : NULL;
   rarch_setting_t *setting   = simple_row_setting(list, i);

   sm->kb_label[0] = '\0';
   if (sm->is_home || !setting)
      return;
   MENU_ENTRY_INITIALIZE((*e));
   e->flags |= MENU_ENTRY_FLAG_RICH_LABEL_ENABLED
             | MENU_ENTRY_FLAG_SUBLABEL_ENABLED;
   menu_entry_get(e, 0, i, NULL, true);
   snprintf(sm->opt_path, sizeof(sm->opt_path), "%d",
         (int)setting->enum_idx);
   strlcpy(sm->opt_title, e->rich_label[0] ? e->rich_label : e->path,
         sizeof(sm->opt_title));
   strlcpy(sm->opt_desc, e->sublabel, sizeof(sm->opt_desc));
   /* Rows without one bound: the setting's own, kept between its id and
    * its name */
   if (     !sm->opt_desc[0] && setting->short_description
         && setting->enum_idx != MSG_UNKNOWN
         && string_is_equal(setting->short_description, msg_hash_to_str(
               (enum msg_hash_enums)(setting->enum_idx + 2))))
   {
      const char *desc = msg_hash_to_str(
            (enum msg_hash_enums)(setting->enum_idx + 1));
      if (desc && !string_is_equal(desc, "null"))
         strlcpy(sm->opt_desc, desc, sizeof(sm->opt_desc));
   }
   simple_upper(sm->kb_label, sm->opt_title, sizeof(sm->kb_label));
}

static int simple_entry_action(void *userdata, menu_entry_t *entry,
      size_t i, enum menu_action action)
{
   simple_handle_t *sm = (simple_handle_t*)userdata;

   if (sm && (action == MENU_ACTION_OK || action == MENU_ACTION_SELECT))
      simple_keep_options(sm, i);

   /* NOOP comes every frame */
   if (SIMPLE_DEBUG && sm && action != MENU_ACTION_NOOP)
   {
      menu_entry_t *e = &sm->log_entry;
      MENU_ENTRY_INITIALIZE((*e));
      e->flags |= MENU_ENTRY_FLAG_RICH_LABEL_ENABLED;
      menu_entry_get(e, 0, i, NULL, true);
      simple_log("action %s(%d) %u \"%s\"", simple_action_name(action),
            (int)action, (unsigned)i,
            e->rich_label[0] ? e->rich_label : e->path);
   }
   return generic_menu_entry_action(userdata, entry, i, action);
}

static float simple_number_get(const rarch_setting_t *setting)
{
   switch (setting->type)
   {
      case ST_FLOAT:
         return *setting->value.target.fraction;
      case ST_INT:
         return (float)*setting->value.target.integer;
      default:
         break;
   }
   return (float)setting_uint_get(setting);
}

/* A number with a range opens to a slider; other values step. Named
 * options are ranged integers too, and can start with a digit ("90 deg",
 * "4:3"), so an integer's text has to start with its value. */
static bool simple_is_number(const rarch_setting_t *setting,
      const char *value)
{
   const char *v = value;
   if (v && (*v == '-' || *v == '+'))
      v++;
   if (!v || *v < '0' || *v > '9')
      return false;
   if (!setting || !(setting->flags & SD_FLAG_HAS_RANGE)
         || setting->step <= 0.0f || setting->max <= setting->min)
      return false;
   if (setting->type == ST_FLOAT)
      return true;
   if (setting->type != ST_UINT && setting->type != ST_INT)
      return false;
   return     setting->ui_type != ST_UI_TYPE_UINT_COMBOBOX
           && setting->ui_type != ST_UI_TYPE_UINT_RADIO_BUTTONS
           && (setting->max - setting->min) / setting->step <= 400.0f
           && (float)strtol(value, NULL, 10) == simple_number_get(setting);
}

static bool simple_is_choice(const rarch_setting_t *setting,
      const char *value)
{
   if (!setting || simple_is_number(setting, value))
      return false;
   return    setting->type == ST_UINT || setting->type == ST_INT
          || setting->type == ST_FLOAT
          || setting->type == ST_STRING_OPTIONS;
}

#define SIMPLE_OPTIONS_STEP 5      /* as many step with a tap */
#define SIMPLE_OPTIONS_MAX  200

/* How many options a choice has; 0 is more than a list can show */
static unsigned simple_options(const rarch_setting_t *setting)
{
   float n;
   if (setting->type == ST_STRING_OPTIONS)
   {
      unsigned c    = 1;
      const char *p = setting->aux.values;
      if (!p)
         return 0;
      for (; *p; p++)
         if (*p == '|')
            c++;
      return c;
   }
   if (     (setting->type != ST_UINT && setting->type != ST_INT)
         || !(setting->flags & SD_FLAG_ENFORCE_MAXRANGE))
      return 0;
   n = (setting->max - ((setting->flags & SD_FLAG_ENFORCE_MINRANGE)
            ? setting->min : 0.0f))
      / (setting->step < 1.0f ? 1.0f : setting->step) + 1.0f;
   return (n < 1.0f || n > (float)SIMPLE_OPTIONS_MAX) ? 0 : (unsigned)n;
}

/* Moves a number to @frac of its range a step at a time, so every
 * handler the setting has runs as it would for left and right */
static void simple_number_set(rarch_setting_t *setting, unsigned type,
      float frac)
{
   int n;
   float target;
   if (frac < 0.0f)
      frac = 0.0f;
   if (frac > 1.0f)
      frac = 1.0f;
   target = setting->min + frac * (setting->max - setting->min);
   n      = (int)((target - simple_number_get(setting)) / setting->step
         + (target >= simple_number_get(setting) ? 0.5f : -0.5f));
   while (n > 0)
   {
      menu_action_handle_setting(setting, type, MENU_ACTION_RIGHT, false);
      n--;
   }
   while (n < 0)
   {
      menu_action_handle_setting(setting, type, MENU_ACTION_LEFT, false);
      n++;
   }
}

/* Top and height of entry @i, in pixels */
static void simple_entry_rect(const simple_handle_t *sm, size_t i,
      float *top, float *h)
{
   const simple_metrics_t *m = sm->m;
   float s                   = sm->scale;

   if (sm->is_home && sm->has_continue)
   {
      if (i == 0)
      {
         *top = (m->cont_label_y - 14.0f) * s;
         *h   = (m->cont_bottom - m->cont_label_y + 14.0f) * s;
      }
      else
      {
         *top = (m->cont_bottom + (float)(i - 1) * m->home_row) * s;
         *h   = m->home_row * s;
      }
      return;
   }
   if (sm->is_home)
   {
      *top = (m->home_top + (float)i * m->home_row) * s;
      *h   = m->home_row * s;
      return;
   }
   *top = (m->list_top + (float)i * m->list_row) * s - sm->scroll_y
      + sm->desc_h;
   *h   = m->list_row * s;
   if (i >= sm->more_idx)
      *top += SIMPLE_MORE_GAP * s;
   if (i > sm->open_idx)
      *top += m->list_row * s;
}

/* Pages scroll under the header: a drag follows the finger, a flick
 * carries on with the menu's own decaying acceleration, and a move by
 * buttons brings the selection into view. */
static void simple_scroll(simple_handle_t *sm, const file_list_t *list,
      size_t selection, float bottom)
{
   float top, h, max;
   float s      = sm->scale;
   float view_t = sm->m->list_top * s + sm->desc_h;
   size_t size  = list ? list->size : 0;
   float extent = (float)size * sm->m->list_row * s
      + (sm->more_idx < size ? SIMPLE_MORE_GAP * s : 0.0f)
      + (sm->open_idx < size ? sm->m->list_row * s : 0.0f);

   max = extent - (bottom - view_t);
   if (max < 0.0f)
      max = 0.0f;

   if (list != sm->last_list)
   {
      sm->last_list      = list;
      sm->scroll_y       = 0.0f;
      sm->last_selection = (size_t)-1;
      sm->open_idx       = (size_t)-1;
      sm->dragging       = false;
      sm->bouncing       = false;
   }

   if (     (sm->pointer.flags & MENU_INP_PTR_FLG_PRESSED)
         && (sm->pointer.flags & MENU_INP_PTR_FLG_DRAGGED)
         && !sm->sliding)
   {
      float y = (float)VIDEO_POS_Y(sm->pointer.pos);
      if (!sm->dragging)
      {
         sm->dragging          = true;
         sm->bouncing          = false;
         sm->drag_start_y      = y;
         sm->drag_start_scroll = sm->scroll_y;
      }
      sm->scroll_y = sm->drag_start_scroll - (y - sm->drag_start_y);
      if (sm->scroll_y < 0.0f)
         sm->scroll_y *= SIMPLE_OVER_RESIST;
      else if (sm->scroll_y > max)
         sm->scroll_y = max + (sm->scroll_y - max) * SIMPLE_OVER_RESIST;
   }
   else
   {
      struct menu_state *menu_st = menu_state_get_ptr();
      float frame_ms             = anim_get_ptr()->delta_time;
      if (frame_ms < 1.0f || frame_ms > 33.3f)
         frame_ms = 16.667f;
      sm->dragging = false;

      if (!sm->bouncing)
      {
         sm->scroll_y -= sm->pointer.y_accel;
         /* Past an end: hand what is left of a flick to the spring */
         if (sm->scroll_y < 0.0f || sm->scroll_y > max)
         {
            sm->bouncing  = true;
            sm->spring_v  = -sm->pointer.y_accel * (1000.0f / frame_ms);
            menu_st->input_state.pointer.y_accel = 0.0f;
         }
      }
      if (sm->bouncing)
      {
         float target = sm->scroll_y < 0.0f ? 0.0f
            : (sm->scroll_y > max ? max : sm->scroll_y);
         float dt     = frame_ms / 1000.0f;
         float disp   = sm->scroll_y - target;
         sm->spring_v += (-SIMPLE_SPRING_K * disp
               - SIMPLE_SPRING_DAMP * sm->spring_v) * dt;
         sm->scroll_y += sm->spring_v * dt;
         if (     disp < 0.5f && disp > -0.5f
               && sm->spring_v < 5.0f && sm->spring_v > -5.0f)
         {
            sm->scroll_y = target;
            sm->bouncing = false;
         }
      }
   }

   if (     selection != sm->last_selection
         && !sm->dragging && !sm->bouncing && size)
   {
      simple_entry_rect(sm, selection, &top, &h);
      if (top < view_t)
         sm->scroll_y -= view_t - top;
      else if (top + h > bottom)
         sm->scroll_y += top + h - bottom;
      sm->last_selection = selection;
   }

   {
      float over = (bottom - view_t) * SIMPLE_OVER_MAX;
      if (sm->dragging || sm->bouncing)
      {
         if (sm->scroll_y < -over)
            sm->scroll_y = -over;
         if (sm->scroll_y > max + over)
            sm->scroll_y = max + over;
      }
      else
      {
         if (sm->scroll_y > max)
            sm->scroll_y = max;
         if (sm->scroll_y < 0.0f)
            sm->scroll_y = 0.0f;
      }
   }
   sm->first = sm->scroll_y > 0.0f
      ? (size_t)(sm->scroll_y / (sm->m->list_row * s)) : 0;
}

/* Top of the scrolling rows; above it is the header */
static float simple_view_top(const simple_handle_t *sm)
{
   float top, h;
   if (sm->is_home)
   {
      simple_entry_rect(sm, 0, &top, &h);
      return top;
   }
   return sm->m->list_top * sm->scale + sm->desc_h;
}

/* Bottom of the rows: gamepad mode keeps a line for button hints */
static float simple_rows_bottom(const simple_handle_t *sm, unsigned dims)
{
   return simple_box_bottom(sm, dims)
      - (sm->gamepad ? 28.0f * sm->scale : 0.0f);
}

#define SIMPLE_ARROW_GAP 10.0f

/* The open number's slider row: its top and the ends of its track */
static bool simple_slider_rect(const simple_handle_t *sm, unsigned dims,
      float *top, float *x0, float *x1)
{
   float h;
   float s = sm->scale;
   float x = SIMPLE_GUTTER * s;
   if (sm->is_home || sm->open_idx == (size_t)-1)
      return false;
   simple_entry_rect(sm, sm->open_idx, top, &h);
   *top += h;
   *x0   = x + 36.0f * s;
   *x1   = (float)VIDEO_SCALE_W(dims) - x - 36.0f * s;
   return true;
}

/* Centres of a choice's arrows, for a value @value_w wide */
static void simple_arrows(simple_handle_t *sm, unsigned dims,
      float value_w, float *left_c, float *right_c, float *value_r)
{
   float s  = sm->scale;
   float aw = simple_note_width(sm, "\xE2\x80\xBA");
   float r  = (float)VIDEO_SCALE_W(dims) - SIMPLE_GUTTER * s;
   *right_c = r - aw * 0.5f;
   *value_r = r - aw - SIMPLE_ARROW_GAP * s;
   *left_c  = *value_r - value_w - SIMPLE_ARROW_GAP * s - aw * 0.5f;
}

static void simple_home_counts(simple_handle_t *sm)
{
   settings_t *settings     = config_get_ptr();
   core_info_list_t *cores  = NULL;
   struct string_list *lpls = dir_list_new(
         settings->paths.directory_playlist, "lpl",
         false, false, false, false);
   const struct playlist_entry *entry = NULL;

   sm->n_systems = lpls ? (unsigned)lpls->size : 0;
   if (lpls)
      dir_list_free(lpls);
   sm->n_cores   = core_info_get_list(&cores) && cores
      ? (unsigned)cores->count : 0;

   sm->core_note[0] = '\0';
   if (     g_defaults.content_history
         && playlist_size(g_defaults.content_history) > 0)
   {
      playlist_get_index(g_defaults.content_history, 0, &entry);
      if (     entry && entry->core_name && *entry->core_name
            && !string_is_equal(entry->core_name, "DETECT"))
      {
         /* "Maker - System (Core)": the maker is the part to lose */
         const char *name = strstr(entry->core_name, " - ");
         const char *core = strchr(entry->core_name, '(');
         name = (name && (!core || name < core))
            ? name + 3 : entry->core_name;
         simple_upper(sm->core_note, name, sizeof(sm->core_note));
      }
   }
}

static void *simple_init(void **userdata, bool video_is_threaded)
{
   simple_handle_t *sm  = NULL;
   settings_t *settings = config_get_ptr();
   menu_handle_t *menu  = (menu_handle_t*)calloc(1, sizeof(*menu));

   if (!menu)
      return NULL;
   if (!(sm = (simple_handle_t*)calloc(1, sizeof(*sm))))
   {
      free(menu);
      return NULL;
   }
   sm->gamepad  = settings->bools.menu_simple_gamepad_mode;
   sm->open_idx = (size_t)-1;
   sm->dims     = video_driver_get_output_dims();
   sm->scale   = simple_scale_for(sm->dims);
   simple_set_metrics(sm);
   *userdata   = sm;
   return menu;
}

static void simple_free(void *data)
{
   simple_handle_t *sm = (simple_handle_t*)data;
   int i;
   if (!sm)
      return;
   for (i = 0; i < SIMPLE_FONT_LAST; i++)
      video_coord_array_free(&sm->fonts[i].raster_block.carr);
}

static void simple_context_reset(void *data, bool is_threaded)
{
   simple_handle_t *sm = (simple_handle_t*)data;
   if (!sm)
      return;
   sm->dims  = video_driver_get_output_dims();
   sm->scale = simple_scale_for(sm->dims);
   simple_fonts_load(sm, is_threaded);
   video_driver_monitor_reset();
}

static void simple_context_destroy(void *data)
{
   simple_handle_t *sm = (simple_handle_t*)data;
   if (sm)
      simple_fonts_free(sm);
}

static void simple_render_messagebox(void *data, const char *msg)
{
   simple_handle_t *sm = (simple_handle_t*)data;
   if (sm && msg)
   {
      strlcpy(sm->msgbox, msg, sizeof(sm->msgbox));
      sm->msg_seen = true;
   }
}

#define SIMPLE_CARD_PAD    22.0f
#define SIMPLE_CARD_LINE   30.0f
#define SIMPLE_CARD_GAP    14.0f
#define SIMPLE_CARD_MAX    10

/* Wraps @src to @max_w, a line per '\n' in @dst */
static unsigned simple_wrap(simple_handle_t *sm, enum simple_font fi,
      const char *src, float max_w, char *dst, size_t len)
{
   size_t n          = 0;
   size_t line_start = 0;
   unsigned lines    = 1;
   font_data_t *font = sm->fonts[fi].font;

   dst[0] = '\0';
   while (*src && n + 2 < len && lines <= SIMPLE_CARD_MAX)
   {
      size_t w = strcspn(src, " \n");
      bool nl  = src[w] == '\n';
      size_t at = n;
      if (n > line_start)
         dst[n++] = ' ';
      if (n + w + 1 >= len)
         break;
      memcpy(dst + n, src, w);
      dst[n + w] = '\0';
      if (     n > line_start && font
            && font_driver_get_message_width(font, dst + line_start,
               n + w - line_start, 1.0f) > max_w)
      {
         dst[at]    = '\n';
         n          = at + 1;
         line_start = n;
         lines++;
         memcpy(dst + n, src, w);
         dst[n + w] = '\0';
      }
      n   += w;
      src += w;
      if (*src)
         src++;
      if (nl && *src)
      {
         dst[n++]   = '\n';
         line_start = n;
         lines++;
      }
   }
   dst[n] = '\0';
   return lines > SIMPLE_CARD_MAX ? SIMPLE_CARD_MAX : lines;
}

#define SIMPLE_KB_LINES   5
#define SIMPLE_KB_LINE    1.3f
#define SIMPLE_KB_PAD     16.0f

/* Lays out the text being typed, broken anywhere: an address has no
 * spaces. A password shows as stars. */
static void simple_kb_layout(simple_handle_t *sm, unsigned dims)
{
   size_t i, n, cursor;
   size_t pos          = 0;
   size_t line_start   = 0;
   bool seen           = false;
   bool secret         = input_driver_text_entry_type()
      == INPUT_TEXT_TYPE_PASSWORD;
   const char *text    = NULL;
   font_data_t *font   = sm->fonts[SIMPLE_FONT_FULL].font;
   float max_w         = (float)VIDEO_SCALE_W(dims)
      - 4.0f * SIMPLE_GUTTER * sm->scale;

   if (!menu_input_dialog_get_display_kb())
   {
      if (sm->kb_on)
         sm->kb_label[0] = '\0';
      sm->kb_on = false;
      return;
   }
   cursor = 0;
   if (!(text = input_driver_keyboard_line_view(&cursor)))
      text = "";
   if (!sm->kb_on)
   {
      const char *label = menu_input_dialog_get_kb_label();
      strlcpy(sm->kb_open, text, sizeof(sm->kb_open));
      /* The row's own name, if it was a setting's */
      if (!sm->kb_label[0])
         simple_upper(sm->kb_label, label ? label : "",
               sizeof(sm->kb_label));
      sm->kb_untouched = true;
   }
   else if (sm->kb_untouched && !string_is_equal(text, sm->kb_open))
      sm->kb_untouched = false;
   /* The system's keyboard starts with all of it selected */
   sm->kb_selected    = *text && (input_driver_keyboard_line_selected()
         || (sm->kb_untouched && input_osk_native_active()));
   sm->kb_lines       = 1;
   sm->kb_cursor_line = 0;
   sm->kb_cursor_x    = 0.0f;

   for (i = 0; text[i]; i += n)
   {
      char glyph[8];
      n = simple_utf8_len((unsigned char)text[i]);
      if (n > strlen(text + i))
         break;
      if (secret)
         strlcpy(glyph, "*", sizeof(glyph));
      else
      {
         memcpy(glyph, text + i, n);
         glyph[n] = '\0';
      }
      if (pos + strlen(glyph) + 2 >= sizeof(sm->kb_text))
         break;
      if (!seen && i >= cursor)
      {
         seen               = true;
         sm->kb_cursor_line = sm->kb_lines - 1;
         sm->kb_cursor_x    = font ? font_driver_get_message_width(font,
               sm->kb_text + line_start, pos - line_start, 1.0f) : 0.0f;
      }
      strlcpy(sm->kb_text + pos, glyph, sizeof(sm->kb_text) - pos);
      if (     font && pos > line_start
            && font_driver_get_message_width(font, sm->kb_text + line_start,
               pos + strlen(glyph) - line_start, 1.0f) > max_w)
      {
         sm->kb_text[pos++] = '\n';
         line_start         = pos;
         sm->kb_lines++;
         strlcpy(sm->kb_text + pos, glyph, sizeof(sm->kb_text) - pos);
      }
      pos += strlen(glyph);
   }
   sm->kb_text[pos] = '\0';
   if (!seen)
   {
      sm->kb_cursor_line = sm->kb_lines - 1;
      sm->kb_cursor_x    = font ? font_driver_get_message_width(font,
            sm->kb_text + line_start, pos - line_start, 1.0f) : 0.0f;
   }
   sm->kb_on = true;
}

#define SIMPLE_DESC_LINE  1.3f
#define SIMPLE_DESC_GAP   12.0f

/* The description above a setting's options */
static void simple_desc_layout(simple_handle_t *sm, unsigned dims)
{
   unsigned lines = 0;
   if (sm->opts && sm->opt_desc[0])
      lines = simple_wrap(sm, SIMPLE_FONT_FULL, sm->opt_desc,
            (float)VIDEO_SCALE_W(dims) - 2.0f * SIMPLE_GUTTER * sm->scale,
            sm->desc_text, sizeof(sm->desc_text));
   if (lines > (sm->gamepad ? 2u : 6u))
      lines = sm->gamepad ? 2 : 6;
   sm->desc_lines = lines;
   sm->desc_h     = lines ? ((float)lines * sm->m->font[SIMPLE_FONT_FULL]
         * SIMPLE_DESC_LINE + SIMPLE_DESC_GAP) * sm->scale : 0.0f;
}

/* RetroArch's prompts ask for a second press; the card asks a question */
static const char *simple_card_question(const simple_handle_t *sm,
      unsigned msg)
{
   size_t i;
   static const enum msg_hash_enums asks[][2] = {
      { MSG_PRESS_AGAIN_TO_QUIT,
         MENU_ENUM_LABEL_VALUE_SIMPLE_QUIT_ASK },
      { MSG_PRESS_AGAIN_TO_CLOSE_CONTENT,
         MENU_ENUM_LABEL_VALUE_SIMPLE_CLOSE_ASK },
      { MSG_PRESS_AGAIN_TO_RESET,
         MENU_ENUM_LABEL_VALUE_SIMPLE_RESTART_ASK },
      { MSG_OVERLAY_GAMEPAD_IMPORT_CONFIRM,
         MENU_ENUM_LABEL_VALUE_SIMPLE_GAMEPAD_IMPORT_ASK },
      { MSG_OVERLAY_LAYOUT_RESET_CONFIRM,
         MENU_ENUM_LABEL_VALUE_SIMPLE_GAMEPAD_RESET_ASK },
      { MSG_OVERLAY_LAYOUT_PROFILE_DELETE_CONFIRM,
         MENU_ENUM_LABEL_VALUE_SIMPLE_GAMEPAD_DELETE_ASK }
   };
   for (i = 0; i < ARRAY_SIZE(asks); i++)
      if (msg == (unsigned)asks[i][0])
         return msg_hash_to_str(asks[i][1]);
   return sm->msgbox;
}

static void simple_card_layout(simple_handle_t *sm, unsigned dims)
{
   struct menu_state *menu_st = menu_state_get_ptr();
   float s     = sm->scale;
   float w     = (float)VIDEO_SCALE_W(dims);
   float box_b = simple_box_bottom(sm, dims);
   unsigned rows;

   sm->card_on      = sm->msg_seen;
   sm->msg_seen     = false;
   if (!sm->card_on)
      return;
   sm->card_confirm = menu_st->dialog_st.confirm_cmd != 0;
   rows             = sm->card_confirm ? 2 : 1;
   sm->card_lines   = simple_wrap(sm, SIMPLE_FONT_WORD,
         simple_card_question(sm, menu_st->dialog_st.confirm_msg),
         w - 4.0f * SIMPLE_GUTTER * s, sm->card_text,
         sizeof(sm->card_text));
   sm->card_h       = (SIMPLE_CARD_PAD + SIMPLE_CARD_GAP + 6.0f
         + (float)sm->card_lines * SIMPLE_CARD_LINE
         + (float)rows * sm->m->list_row) * s;
   sm->card_y       = (box_b - sm->card_h) * 0.5f;
   if (sm->card_y < 0.0f)
      sm->card_y = 0.0f;
   sm->card_rows_y  = sm->card_y + (SIMPLE_CARD_PAD + SIMPLE_CARD_GAP
         + (float)sm->card_lines * SIMPLE_CARD_LINE) * s;

   /* A tap confirms only on the first answer; anywhere else cancels */
   if (sm->card_confirm)
   {
      float px = (float)VIDEO_POS_X(sm->pointer.pos);
      float py = (float)VIDEO_POS_Y(sm->pointer.pos);
      menu_st->dialog_st.confirm_hover_ok =
            px >= SIMPLE_GUTTER * s && px < w - SIMPLE_GUTTER * s
         && py >= sm->card_rows_y
         && py <  sm->card_rows_y + sm->m->list_row * s;
   }
}

/* Simple's own lists keep the large type; every other list is one of
 * RetroArch's, drawn smaller. Ids, not titles, so renames don't matter.
 * In the Classic layout every list is RetroArch's, the Main Menu too. */
static void simple_set_page(simple_handle_t *sm, bool simple_layout)
{
   size_t i;
   const char *path  = NULL;
   const char *label = NULL;
   static const enum msg_hash_enums lists[] = {
      MENU_ENUM_LABEL_CONTENT_SETTINGS,    /* the Game menu */
      MENU_ENUM_LABEL_DEFERRED_SIMPLE_GAMES_LIST,
      MENU_ENUM_LABEL_DEFERRED_SIMPLE_GAMES_MORE_LIST,
      MENU_ENUM_LABEL_DEFERRED_SIMPLE_SETTINGS_LIST,
      MENU_ENUM_LABEL_DEFERRED_SIMPLE_HELP_LIST,
      MENU_ENUM_LABEL_DEFERRED_SIMPLE_CONTROLS_LIST,
      MENU_ENUM_LABEL_DEFERRED_SIMPLE_SCREEN_LIST,
      MENU_ENUM_LABEL_DEFERRED_SIMPLE_SOUND_LIST,
      MENU_ENUM_LABEL_DEFERRED_SIMPLE_SAVING_LIST,
      MENU_ENUM_LABEL_DEFERRED_SIMPLE_LIBRARY_LIST,
      MENU_ENUM_LABEL_DEFERRED_SIMPLE_ACCOUNTS_LIST,
      MENU_ENUM_LABEL_DEFERRED_SIMPLE_ACCESSIBILITY_LIST,
      MENU_ENUM_LABEL_DEFERRED_SIMPLE_INTERFACE_LIST,
      MENU_ENUM_LABEL_DEFERRED_SIMPLE_GAME_CONTROLS_LIST,
      MENU_ENUM_LABEL_DEFERRED_SIMPLE_GAME_SCREEN_LIST,
      MENU_ENUM_LABEL_DEFERRED_SIMPLE_DOWNLOADS_LIST,
      MENU_ENUM_LABEL_DEFERRED_SIMPLE_BOX_ART_LIST
   };

   menu_entries_get_last_stack(&path, &label, NULL, NULL, NULL);
   /* A prompt keeps the page it covers */
   if (string_is_equal(label, MENU_ENUM_LABEL_INFO_SCREEN_STR))
      return;
   sm->opts    = sm->opt_path[0] && string_is_equal(path, sm->opt_path)
      && string_is_equal(label,
            MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_STR);
   sm->is_home = simple_layout && string_is_equal(label,
         msg_hash_to_str(MENU_ENUM_LABEL_MAIN_MENU));
   sm->full    = !sm->is_home;
   if (!simple_layout || sm->is_home)
      return;
   for (i = 0; i < ARRAY_SIZE(lists); i++)
      if (string_is_equal(label, msg_hash_to_str(lists[i])))
      {
         sm->full = false;
         return;
      }
}

/* Test builds: a page line when the page changes, a sel line when the
 * selection moves, the rows again once the list is at rest somewhere new.
 * Rows are in window pixels, as the touch reader sees them. */
static void simple_log_state(simple_handle_t *sm, menu_list_t *menu_list,
      size_t selection, size_t size, unsigned dims)
{
   char title[NAME_MAX_LENGTH];
   size_t depth = menu_list ? MENU_LIST_GET_STACK_SIZE(menu_list, 0) : 0;

   title[0]     = '\0';
   if (sm->is_home)
      strlcpy(title, "Home", sizeof(title));
   else
      menu_entries_get_title(title, sizeof(title));

   if (     !string_is_equal(title, sm->log_title)
         || depth       != sm->log_depth
         || size        != sm->log_size
         || dims        != sm->log_dims
         || sm->gamepad != sm->log_gamepad)
   {
      strlcpy(sm->log_title, title, sizeof(sm->log_title));
      sm->log_depth   = depth;
      sm->log_size    = size;
      sm->log_dims    = dims;
      sm->log_gamepad = sm->gamepad;
      sm->log_sel     = selection;
      sm->log_scroll  = sm->scroll_y;
      sm->log_rows    = true;
      simple_log("page \"%s\" depth=%u mode=%s style=%s dims=%ux%u size=%u "
            "sel=%u", title, (unsigned)depth,
            sm->gamepad ? "gamepad" : "touch",
            sm->is_home ? "home" : (sm->full ? "full" : "list"),
            (unsigned)VIDEO_SCALE_W(dims), (unsigned)VIDEO_SCALE_H(dims),
            (unsigned)size, (unsigned)selection);
   }
   else if (selection != sm->log_sel)
   {
      sm->log_sel = selection;
      simple_log("sel %u", (unsigned)selection);
   }

   if (     !sm->dragging && !sm->bouncing
         && sm->pointer.y_accel > -0.5f && sm->pointer.y_accel < 0.5f
         && (     sm->scroll_y - sm->log_scroll >= 1.0f
               || sm->log_scroll - sm->scroll_y >= 1.0f))
   {
      sm->log_scroll = sm->scroll_y;
      sm->log_rows   = true;
      simple_log("scroll %d", (int)sm->scroll_y);
   }
}

/* Closes an open slider when its page is left (pages share one list) or
 * its value stops being a number */
static void simple_check_open(simple_handle_t *sm, menu_list_t *menu_list,
      const file_list_t *list)
{
   size_t depth = menu_list ? MENU_LIST_GET_STACK_SIZE(menu_list, 0) : 0;

   if (depth != sm->open_depth)
   {
      sm->open_depth = depth;
      sm->open_idx   = (size_t)-1;
   }
   if (     sm->open_idx == (size_t)-1
         || (sm->pointer.flags & MENU_INP_PTR_FLG_PRESSED))
      return;
   if (!list || sm->open_idx >= list->size)
   {
      sm->open_idx = (size_t)-1;
      return;
   }
   MENU_ENTRY_INITIALIZE(sm->hit_entry);
   sm->hit_entry.flags |= MENU_ENTRY_FLAG_VALUE_ENABLED;
   menu_entry_get(&sm->hit_entry, 0, sm->open_idx, NULL, true);
   if (!simple_is_number(simple_row_setting(list, sm->open_idx),
            sm->hit_entry.value))
      sm->open_idx = (size_t)-1;
}

static void simple_render(void *data, unsigned dims, bool is_idle)
{
   size_t i;
   float bottom;
   simple_handle_t *sm        = (simple_handle_t*)data;
   struct menu_state *menu_st = menu_state_get_ptr();
   menu_list_t *menu_list     = menu_st->entries.list;
   file_list_t *list          = menu_list ? MENU_LIST_GET_SELECTION(menu_list, 0) : NULL;
   size_t selection           = menu_st->selection_ptr;
   size_t size                = list ? list->size : 0;
   gfx_display_t *p_disp      = disp_get_ptr();
   settings_t *settings       = config_get_ptr();
   bool gamepad               = settings->bools.menu_simple_gamepad_mode;
   bool was_home;

   if (!sm)
      return;

   if (dims != sm->dims || gamepad != sm->gamepad)
   {
      if (gamepad != sm->gamepad)
      {
         sm->gamepad = gamepad;
#ifdef HAVE_OVERLAY
         command_event(CMD_EVENT_OVERLAY_UNLOAD, NULL);
         if (!menu_driver_overlay_hidden(settings))
            command_event(CMD_EVENT_OVERLAY_INIT, NULL);
#endif
      }
      sm->dims  = dims;
      sm->scale = simple_scale_for(dims);
      simple_set_metrics(sm);
      simple_fonts_load(sm, video_driver_is_threaded());
   }

   was_home = sm->is_home;
   simple_set_page(sm, settings->uints.menu_layout == MENU_LAYOUT_SIMPLE);
   simple_check_open(sm, menu_list, list);
   simple_desc_layout(sm, dims);
   if (sm->is_home && !was_home)
      simple_home_counts(sm);

   sm->has_continue = sm->is_home && size > 0
      && string_is_equal(list->list[0].label,
            MENU_ENUM_LABEL_SIMPLE_CONTINUE_STR);
   sm->more_idx = size;
   for (i = 0; i < size; i++)
      if (simple_is_more(list->list[i].label))
      {
         sm->more_idx = i;
         break;
      }

   bottom = simple_rows_bottom(sm, dims);
   if (     settings->bools.menu_mouse_enable
         || settings->bools.menu_pointer_enable)
      menu_input_get_pointer_state(&sm->pointer);
   else
      memset(&sm->pointer, 0, sizeof(sm->pointer));
   {
      bool pressed = (sm->pointer.flags & MENU_INP_PTR_FLG_PRESSED) ? true : false;
      float st, x0, x1;
      if (!pressed)
         sm->sliding = false;
      else if (!sm->was_pressed
            && simple_slider_rect(sm, dims, &st, &x0, &x1))
      {
         float px = (float)VIDEO_POS_X(sm->pointer.pos);
         float py = (float)VIDEO_POS_Y(sm->pointer.pos);
         sm->sliding = py >= st && py < st + sm->m->list_row * sm->scale
            && px >= x0 - 12.0f * sm->scale && px <= x1 + 12.0f * sm->scale;
      }
      if (     sm->sliding && list && sm->open_idx < size
            && simple_slider_rect(sm, dims, &st, &x0, &x1))
         simple_number_set(simple_row_setting(list, sm->open_idx),
               list->list[sm->open_idx].type,
               ((float)VIDEO_POS_X(sm->pointer.pos) - x0) / (x1 - x0));
      sm->was_pressed = pressed;
   }
   if (sm->is_home)
   {
      sm->first     = 0;
      sm->scroll_y  = 0.0f;
      sm->last_list = NULL;
   }
   else
      simple_scroll(sm, list, selection, bottom);
   menu_st->entries.begin = sm->first;
   p_disp->header_height  = (unsigned)simple_view_top(sm);
   /* The touch reader scales touches by this; unset, every tap is 0,0 */
   p_disp->framebuf_dims  = dims;
   simple_card_layout(sm, dims);
   simple_kb_layout(sm, dims);
   if (SIMPLE_DEBUG)
      simple_log_state(sm, menu_list, selection, size, dims);

   if (     (sm->pointer.type != MENU_POINTER_DISABLED)
         && (sm->pointer.flags & MENU_INP_PTR_FLG_ACTIVE))
   {
      float y = (float)VIDEO_POS_Y(sm->pointer.pos);
      menu_st->input_state.ptr = (unsigned)size;
      if (y >= simple_view_top(sm) && y < bottom)
         for (i = sm->first; i < size; i++)
         {
            float top, h;
            simple_entry_rect(sm, i, &top, &h);
            if (y >= top && y < top + h)
            {
               menu_st->input_state.ptr = (unsigned)i;
               break;
            }
         }
   }
}

/* Titles of the open levels, root first, upper case. A deep stack keeps
 * the root and the last levels; *gap says some were left out. */
static size_t simple_crumb_parts(simple_handle_t *sm, bool *gap)
{
   size_t i, first, size;
   size_t n                   = 0;
   char title[NAME_MAX_LENGTH];
   struct menu_state *menu_st = menu_state_get_ptr();
   const file_list_t *stack   = menu_st->entries.list
      ? MENU_LIST_GET(menu_st->entries.list, 0) : NULL;
   bool simple_layout         = config_get_ptr()->uints.menu_layout
      == MENU_LAYOUT_SIMPLE;

   *gap = false;
   if (!stack || !stack->size)
      return 0;
   size  = stack->size;
   if (     size > 1
         && string_is_equal(stack->list[size - 1].label,
            MENU_ENUM_LABEL_INFO_SCREEN_STR))
      size--;
   first = (size > SIMPLE_CRUMB_PARTS)
      ? size - (SIMPLE_CRUMB_PARTS - 1) : 1;
   *gap  = first > 1;

   for (i = 0; i < size; i++)
   {
      const struct item_file *item = &stack->list[i];
      menu_file_list_cbs_t *cbs    = (menu_file_list_cbs_t*)item->actiondata;

      if (i > 0 && i < first)
         continue;
      title[0] = '\0';
      if (i == 0 && simple_layout)
         strlcpy(title, "Home", sizeof(title));
      else if (i == stack->size - 1 && sm->opts)
         strlcpy(title, sm->opt_title, sizeof(title));
      else if (i == stack->size - 1)
         menu_entries_get_title(title, sizeof(title));
      else if (cbs && cbs->action_get_title)
         cbs->action_get_title(item->path, item->label, item->type,
               title, sizeof(title));
      simple_upper(sm->crumb[n++], title, SIMPLE_CRUMB_PART);
   }
   return n;
}

/* "<- A / B / C", or with the middle levels as an ellipsis */
static void simple_crumb_join(simple_handle_t *sm, char *s, size_t len,
      size_t n, size_t keep_from, bool gap)
{
   size_t i;
   strlcpy(s, n > 1 ? "\xE2\x86\x90 " : "", len);
   strlcat(s, sm->crumb[0], len);
   if (gap || keep_from > 1)
      strlcat(s, " / \xE2\x80\xA6", len);
   for (i = keep_from; i < n; i++)
   {
      strlcat(s, " / ", len);
      strlcat(s, sm->crumb[i], len);
   }
}

static void simple_crumb(simple_handle_t *sm, char *s, size_t len,
      float max_w)
{
   size_t keep_from;
   bool gap;
   size_t n = simple_crumb_parts(sm, &gap);

   s[0] = '\0';
   if (!n)
      return;
   for (keep_from = 1; keep_from < n; keep_from++)
   {
      simple_crumb_join(sm, s, len, n, keep_from, gap);
      if (simple_note_width(sm, s) <= max_w)
         return;
   }
   /* Only the current level fits, if that */
   {
      char full[SIMPLE_CRUMB_PART + 16];
      strlcpy(full, "\xE2\x86\x90 \xE2\x80\xA6 / ", sizeof(full));
      strlcat(full, sm->crumb[n - 1], sizeof(full));
      simple_fit_note(sm, full, max_w, s, len);
   }
}

static void simple_draw_header(simple_handle_t *sm, unsigned dims)
{
   char crumb[SIMPLE_CRUMB_PARTS * (SIMPLE_CRUMB_PART + 3) + 8];
   float s   = sm->scale;
   float w   = (float)VIDEO_SCALE_W(dims);
   float x   = SIMPLE_GUTTER * s;
   float y   = sm->m->header_y * s;

   if (sm->is_home)
   {
      bool art = sm->has_continue;
      simple_note(sm, "HOME", x, y, dims,
            art ? SIMPLE_COL_FG : SIMPLE_COL_MUTED, TEXT_ALIGN_LEFT);
      simple_note(sm, "RETROARCH", w - x, y, dims,
            art ? SIMPLE_COL_FG : SIMPLE_COL_DIM, TEXT_ALIGN_RIGHT);
      return;
   }

   /* Room left of RETROARCH, with a gap */
   simple_crumb(sm, crumb, sizeof(crumb),
         w - 2.0f * x - simple_note_width(sm, "RETROARCH") - 16.0f * s);
   simple_note(sm, crumb, x, y, dims, SIMPLE_COL_MUTED, TEXT_ALIGN_LEFT);
   simple_note(sm, "RETROARCH", w - x, y, dims, SIMPLE_COL_DIM,
         TEXT_ALIGN_RIGHT);
}

static void simple_draw_desc(simple_handle_t *sm, unsigned dims)
{
   char line[256];
   unsigned r;
   const char *p = sm->desc_text;
   float line_h  = sm->m->font[SIMPLE_FONT_FULL] * SIMPLE_DESC_LINE
      * sm->scale;
   float y       = sm->m->list_top * sm->scale;

   for (r = 0; r < sm->desc_lines && *p; r++)
   {
      size_t n = strcspn(p, "\n");
      if (n < sizeof(line))
      {
         memcpy(line, p, n);
         line[n] = '\0';
         simple_text(sm, SIMPLE_FONT_FULL, line, SIMPLE_GUTTER * sm->scale,
               y + ((float)r + 0.3f) * line_h, dims, SIMPLE_COL_MUTED,
               TEXT_ALIGN_LEFT);
      }
      p += n;
      if (*p)
         p++;
   }
}

/* A Home row's note on the right, and whether it needs the player */
static void simple_home_note(const simple_handle_t *sm, const char *label,
      char *s, size_t len)
{
   s[0] = '\0';
   if (string_is_equal(label, MENU_ENUM_LABEL_SIMPLE_GAMES_STR))
   {
      if (sm->n_systems)
         snprintf(s, len, "%u SYSTEM%s", sm->n_systems,
               sm->n_systems == 1 ? "" : "S");
      else
         strlcpy(s, "ADD GAMES", len);
   }
   else if (string_is_equal(label, MENU_ENUM_LABEL_SIMPLE_DOWNLOADS_STR))
   {
      if (sm->n_cores)
         snprintf(s, len, "%u EMULATOR%s", sm->n_cores,
               sm->n_cores == 1 ? "" : "S");
      else
         strlcpy(s, "GET EMULATORS", len);
   }
   else if (string_is_equal(label, MENU_ENUM_LABEL_SIMPLE_SETTINGS_STR))
      strlcpy(s, "CONTROLS \xC2\xB7 SCREEN", len);
   else if (string_is_equal(label, MENU_ENUM_LABEL_SIMPLE_HELP_STR))
      strlcpy(s, "ABOUT \xC2\xB7 QUIT", len);
   else if (string_is_equal(label, MENU_ENUM_LABEL_SIMPLE_GAME_MENU_STR))
      strlcpy(s, "PAUSED", len);
}

/* A Settings category's note: the setting that best sums it up */
static void simple_settings_note(const char *label, char *s, size_t len)
{
   settings_t *settings = config_get_ptr();
   s[0] = '\0';
   if (string_is_equal(label, MENU_ENUM_LABEL_SIMPLE_CONTROLS_STR))
      strlcpy(s, !settings->bools.input_overlay_enable ? "TOUCH OFF"
            : settings->bools.input_overlay_hide_when_gamepad_connected
            ? "TOUCH \xC2\xB7 AUTO" : "TOUCH ON", len);
   else if (string_is_equal(label, MENU_ENUM_LABEL_SIMPLE_SCREEN_STR))
      strlcpy(s, settings->bools.video_smooth
            ? "SMOOTH" : "SHARP PIXELS", len);
   else if (string_is_equal(label, MENU_ENUM_LABEL_SIMPLE_SOUND_STR))
      strlcpy(s, audio_state_get_ptr()->mute_enable ? "MUTED"
            : settings->bools.audio_enable ? "ON" : "OFF", len);
   else if (string_is_equal(label, MENU_ENUM_LABEL_SIMPLE_SAVING_STR))
      strlcpy(s, settings->bools.savestate_auto_load
            ? "RESUME ON" : "RESUME OFF", len);
   else if (string_is_equal(label, MENU_ENUM_LABEL_SIMPLE_LIBRARY_STR))
      strlcpy(s, settings->bools.network_on_demand_thumbnails
            ? "BOX ART ON" : "BOX ART OFF", len);
#ifdef HAVE_CHEEVOS
   else if (string_is_equal(label, MENU_ENUM_LABEL_SIMPLE_ACCOUNTS_STR))
   {
      if (*settings->arrays.cheevos_username)
         simple_upper(s, settings->arrays.cheevos_username, len);
      else
         strlcpy(s, "NOT SIGNED IN", len);
   }
#endif
   else if (string_is_equal(label, MENU_ENUM_LABEL_SIMPLE_ACCESSIBILITY_STR))
      strlcpy(s, settings->bools.accessibility_enable ? "ON" : "OFF", len);
   else if (string_is_equal(label, MENU_ENUM_LABEL_SIMPLE_INTERFACE_STR))
      strlcpy(s, settings->uints.menu_layout == MENU_LAYOUT_SIMPLE
            ? "SIMPLE" : "CLASSIC", len);
}

static void simple_draw_continue(simple_handle_t *sm, unsigned dims,
      const char *word, bool selected)
{
   char title[NAME_MAX_LENGTH];
   char fitted[NAME_MAX_LENGTH];
   char note[sizeof(sm->core_note)];
   char *tags;
   const char *split = strstr(word, ": ");
   font_data_t *font = sm->fonts[SIMPLE_FONT_TITLE].font;
   float s           = sm->scale;
   float w           = (float)VIDEO_SCALE_W(dims);
   float x           = SIMPLE_GUTTER * s;
   float room        = w - 2.0f * x;
   uint32_t label_c  = (!sm->gamepad || selected)
      ? SIMPLE_COL_ACCENT : SIMPLE_COL_MUTED;

   strlcpy(title, split ? split + 2 : word, sizeof(title));
   simple_note(sm, "CONTINUE", x, sm->m->cont_label_y * s, dims,
         label_c, TEXT_ALIGN_LEFT);
   if (sm->core_note[0])
   {
      simple_fit_note(sm, sm->core_note,
            room - simple_note_width(sm, "CONTINUE") - 16.0f * s,
            note, sizeof(note));
      simple_note(sm, note, w - x, sm->m->cont_label_y * s,
            dims, SIMPLE_COL_MUTED, TEXT_ALIGN_RIGHT);
   }
   /* Too long: the "(USA)" tags go before any of the name does */
   if (     font && (tags = strstr(title, " (")) && tags != title
         && font_driver_get_message_width(font, title, strlen(title), 1.0f)
            > (int)room)
      *tags = '\0';
   simple_fit(sm, SIMPLE_FONT_TITLE, title, room, fitted, sizeof(fitted));
   simple_text(sm, SIMPLE_FONT_TITLE, fitted, x, sm->m->cont_title_y * s,
         dims, SIMPLE_COL_FG, TEXT_ALIGN_LEFT);
}

/* A row's word. One too wide takes the smaller type, then a second
 * line where the row is tall enough, then an ellipsis. */
static void simple_draw_word(simple_handle_t *sm, enum simple_font fi,
      const char *word, float x, float centre, float h, float max_w,
      unsigned dims, uint32_t color)
{
   char fitted[NAME_MAX_LENGTH];
   float line_h      = sm->m->font[SIMPLE_FONT_FULL] * 1.2f * sm->scale;
   font_data_t *font = sm->fonts[fi].font;

   if (     fi == SIMPLE_FONT_WORD && font
         && font_driver_get_message_width(font, word, strlen(word), 1.0f)
            > max_w)
   {
      fi   = SIMPLE_FONT_FULL;
      font = sm->fonts[fi].font;
   }
   if (     fi == SIMPLE_FONT_FULL && font && 2.0f * line_h <= h
         && font_driver_get_message_width(font, word, strlen(word), 1.0f)
            > max_w)
   {
      /* After a space or, for a path, a slash; else anywhere */
      size_t i;
      size_t n   = 0;
      size_t any = 0;
      for (i = 1; word[i]; i++)
      {
         size_t to = i + (word[i] == '/' ? 1 : 0);
         if (((unsigned char)word[i] & 0xC0) == 0x80)
            continue;
         if (font_driver_get_message_width(font, word, to, 1.0f) > max_w)
            break;
         any = i;
         if (word[i] == ' ' || word[i] == '/')
            n = to;
      }
      if (!n)
         n = any;
      if (n > 0 && n < sizeof(fitted))
      {
         memcpy(fitted, word, n);
         fitted[n] = '\0';
         simple_text(sm, fi, fitted, x, centre - line_h * 0.5f, dims,
               color, TEXT_ALIGN_LEFT);
         word   += n + (word[n] == ' ' ? 1 : 0);
         centre += line_h * 0.5f;
      }
   }
   simple_fit(sm, fi, word, max_w, fitted, sizeof(fitted));
   simple_text(sm, fi, fitted, x, centre, dims, color, TEXT_ALIGN_LEFT);
}

static void simple_draw_rows(simple_handle_t *sm, gfx_display_t *p_disp,
      void *userdata, unsigned dims, file_list_t *list, size_t selection)
{
   size_t i;
   char note[NAME_MAX_LENGTH];
   float s      = sm->scale;
   float w      = (float)VIDEO_SCALE_W(dims);
   float x      = SIMPLE_GUTTER * s;
   float line   = s < 1.0f ? 1.0f : s;
   float bottom = simple_rows_bottom(sm, dims);
   float view_t = simple_view_top(sm);
   enum simple_font word_font = sm->is_home
      ? SIMPLE_FONT_HOME
      : (sm->full ? SIMPLE_FONT_FULL : SIMPLE_FONT_WORD);

   for (i = sm->first; i < list->size; i++)
   {
      const char *word;
      float top, h, centre;
      menu_entry_t *entry = &sm->entry;
      bool selected       = (i == selection);
      bool more           = !sm->is_home && i == sm->more_idx;

      simple_entry_rect(sm, i, &top, &h);
      if (top + h <= view_t)
         continue;
      if (top >= bottom || (sm->is_home && top + h > bottom))
         break;
      centre = top + h * 0.5f;

      MENU_ENTRY_INITIALIZE((*entry));
      entry->flags |= MENU_ENTRY_FLAG_RICH_LABEL_ENABLED
                    | MENU_ENTRY_FLAG_VALUE_ENABLED;
      menu_entry_get(entry, 0, i, NULL, true);
      word = entry->rich_label[0] ? entry->rich_label : entry->path;
      /* Whole rows only: a part row's centre can be under the header */
      if (sm->log_rows && top >= view_t - 0.5f && top + h <= bottom + 0.5f)
         simple_log("row %u y=%d-%d \"%s\"", (unsigned)i, (int)top,
               (int)(top + h), word);

      /* Yellow is the gamepad selection; touch gets a quiet mark */
      if (selected)
      {
         float mark_h = sm->has_continue && i == 0
            ? (sm->m->cont_title_y - sm->m->cont_label_y + 24.0f) * s
            : h * 0.5f;
         float mark_y = sm->has_continue && i == 0
            ? (sm->m->cont_label_y - 8.0f) * s
            : centre - mark_h * 0.5f;
         simple_quad(p_disp, userdata, dims, x - 12.0f * s, mark_y,
               3.0f * s, mark_h,
               sm->gamepad ? SIMPLE_COL_ACCENT : SIMPLE_COL_DIM);
      }

      if (sm->has_continue && i == 0)
      {
         simple_draw_continue(sm, dims, word, selected);
         continue;
      }

      if (!more && (i > 0 || !sm->is_home || sm->has_continue))
         simple_quad(p_disp, userdata, dims, x, top, w - 2.0f * x, line,
               SIMPLE_COL_LINE);

      if (sm->is_home)
         simple_home_note(sm, list->list[i].label, note, sizeof(note));
      else if (entry->value[0] && !string_is_equal(entry->value, "..."))
         simple_upper(note, entry->value, sizeof(note));
      else
         simple_settings_note(list->list[i].label, note, sizeof(note));

      {
         char fitted[NAME_MAX_LENGTH];
         rarch_setting_t *setting = sm->is_home ? NULL
            : simple_row_setting(list, i);
         float max_note = (w - 2.0f * x) * 0.5f;
         float note_w;
         bool choice    = note[0] && simple_is_choice(setting, entry->value);
         bool text      = setting && setting->type == ST_STRING;

         if (sm->opts && (entry->flags & MENU_ENTRY_FLAG_CHECKED))
            strlcpy(note, "CURRENT", sizeof(note));
         else if (text && !entry->value[0])
            strlcpy(note, "NOT SET", sizeof(note));
         /* A password is not put on screen */
         else if (text && setting->ui_type == ST_UI_TYPE_PASSWORD_LINE_EDIT)
            strlcpy(note, "SET", sizeof(note));
         note_w = simple_note_width(sm, note);
         if (!sm->is_home && note_w > max_note)
         {
            if (text)
               strlcpy(note, "SET", sizeof(note));
            else
            {
               simple_fit_note(sm, note, max_note, fitted, sizeof(fitted));
               strlcpy(note, fitted, sizeof(note));
            }
            note_w = simple_note_width(sm, note);
         }

         /* Values a tap changes are bright; open-ended ones have arrows */
         if (     (choice && simple_options(setting))
               || (note[0] && setting && setting->type == ST_BOOL))
         {
            simple_note(sm, note, w - x, centre, dims, SIMPLE_COL_FG,
                  TEXT_ALIGN_RIGHT);
            note[0] = '\0';
         }
         else if (choice)
         {
            float lc, rc, vr;
            simple_arrows(sm, dims, note_w, &lc, &rc, &vr);
            simple_note(sm, "\xE2\x80\xB9", lc, centre, dims,
                  SIMPLE_COL_MUTED, TEXT_ALIGN_CENTER);
            simple_note(sm, "\xE2\x80\xBA", rc, centre, dims,
                  SIMPLE_COL_MUTED, TEXT_ALIGN_CENTER);
            simple_note(sm, note, vr, centre, dims, SIMPLE_COL_FG,
                  TEXT_ALIGN_RIGHT);
            note_w = (w - x) - (lc - 24.0f * s);
            note[0] = '\0';
         }
         simple_draw_word(sm, more ? SIMPLE_FONT_FULL : word_font, word,
               x, centre, h,
               w - 2.0f * x - (note_w > 0.0f ? note_w + 16.0f * s : 0.0f),
               dims, more ? SIMPLE_COL_MUTED : SIMPLE_COL_FG);

         if (i == sm->open_idx && simple_is_number(setting, entry->value))
         {
            float st, x0, x1, frac;
            if (simple_slider_rect(sm, dims, &st, &x0, &x1))
            {
               float sc = st + h * 0.5f;
               frac = (simple_number_get(setting) - setting->min)
                  / (setting->max - setting->min);
               if (frac < 0.0f)
                  frac = 0.0f;
               if (frac > 1.0f)
                  frac = 1.0f;
               simple_note(sm, "\xE2\x88\x92", x + 8.0f * s, sc, dims,
                     SIMPLE_COL_MUTED, TEXT_ALIGN_CENTER);
               simple_note(sm, "+", w - x - 8.0f * s, sc, dims,
                     SIMPLE_COL_MUTED, TEXT_ALIGN_CENTER);
               simple_quad(p_disp, userdata, dims, x0, sc - s, x1 - x0,
                     2.0f * s, SIMPLE_COL_LINE);
               simple_quad(p_disp, userdata, dims, x0, sc - s,
                     (x1 - x0) * frac, 2.0f * s, SIMPLE_COL_FG);
               simple_quad(p_disp, userdata, dims,
                     x0 + (x1 - x0) * frac - 1.5f * s, sc - 9.0f * s,
                     3.0f * s, 18.0f * s, SIMPLE_COL_FG);
            }
         }
      }
      simple_note(sm, note, w - x, centre, dims, SIMPLE_COL_DIM,
            TEXT_ALIGN_RIGHT);
   }
   sm->log_rows = false;
}

static void simple_draw_hints(simple_handle_t *sm, gfx_display_t *p_disp,
      void *userdata, unsigned dims)
{
   float s      = sm->scale;
   float w      = (float)VIDEO_SCALE_W(dims);
   float box_b  = simple_box_bottom(sm, dims);
   float y      = box_b - 14.0f * s;

   simple_quad(p_disp, userdata, dims, 0.0f, box_b, w,
         s < 1.0f ? 1.0f : s, SIMPLE_COL_LINE);
   /* OK and Cancel can be swapped (the default here): name them so */
   simple_note(sm, config_get_ptr()->bools.input_menu_swap_ok_cancel_buttons
         ? "B OPEN   A BACK   \xE2\x86\x95 MOVE"
         : "A OPEN   B BACK   \xE2\x86\x95 MOVE",
         SIMPLE_GUTTER * s, y, dims, SIMPLE_COL_MUTED, TEXT_ALIGN_LEFT);
}

/* The text being typed, at the top: a keyboard covers the bottom */
static void simple_draw_kb(simple_handle_t *sm, gfx_display_t *p_disp,
      void *userdata, unsigned dims)
{
   char line[sizeof(sm->kb_text)];
   unsigned r;
   const char *p  = sm->kb_text;
   float s        = sm->scale;
   float w        = (float)VIDEO_SCALE_W(dims);
   float x        = SIMPLE_GUTTER * s;
   float tx       = 2.0f * x;
   float line_h   = sm->m->font[SIMPLE_FONT_FULL] * SIMPLE_KB_LINE * s;
   float note_h   = sm->m->font[SIMPLE_FONT_NOTE] * 2.0f * s;
   float pad      = SIMPLE_KB_PAD * s * (sm->gamepad ? 0.5f : 1.0f);
   float top      = sm->m->list_top * s;
   float y        = top + pad + note_h + pad * 0.5f;
   unsigned max   = sm->gamepad ? 3 : SIMPLE_KB_LINES;
   unsigned shown = sm->kb_lines > max ? max : sm->kb_lines;
   /* The lines around the cursor */
   unsigned first = sm->kb_cursor_line >= max
      ? sm->kb_cursor_line - max + 1 : 0;

   simple_quad(p_disp, userdata, dims, 0.0f, 0.0f, w,
         simple_box_bottom(sm, dims), 0x000000A6);
   simple_quad(p_disp, userdata, dims, x, top, w - 2.0f * x,
         y - top + (float)shown * line_h + pad * 0.5f + note_h + pad,
         SIMPLE_COL_SHEET);
   simple_note(sm, sm->kb_label, tx, top + pad + note_h * 0.5f, dims,
         SIMPLE_COL_MUTED, TEXT_ALIGN_LEFT);

   for (r = 0; r < first + shown; r++)
   {
      size_t n = strcspn(p, "\n");
      if (r >= first)
      {
         float ly = y + (float)(r - first) * line_h;
         memcpy(line, p, n);
         line[n] = '\0';
         if (sm->kb_selected && n)
            simple_quad(p_disp, userdata, dims, tx, ly,
                  (float)font_driver_get_message_width(
                     sm->fonts[SIMPLE_FONT_FULL].font, line, n, 1.0f),
                  line_h, SIMPLE_COL_SELECT);
         simple_text(sm, SIMPLE_FONT_FULL, line, tx, ly + line_h * 0.5f,
               dims, SIMPLE_COL_FG, TEXT_ALIGN_LEFT);
         if (     r == sm->kb_cursor_line && !sm->kb_selected
               && ((menu_driver_get_current_time() / 500000) & 1))
            simple_quad(p_disp, userdata, dims, tx + sm->kb_cursor_x,
                  ly + line_h * 0.15f, 2.0f * s, line_h * 0.7f,
                  SIMPLE_COL_ACCENT);
      }
      p += n;
      if (*p)
         p++;
   }
   simple_note(sm, "DONE SAVES \xC2\xB7 BACK CANCELS", tx,
         y + (float)shown * line_h + pad * 0.5f + note_h * 0.5f, dims,
         SIMPLE_COL_MUTED, TEXT_ALIGN_LEFT);

   /* Without the system's keyboard, RetroArch's own */
   gfx_display_draw_keyboard(p_disp, userdata, dims, 0,
         sm->fonts[SIMPLE_FONT_FULL].font, menu_state_get_ptr()->osk_grid,
         input_driver_keyboard_textbox_focus()
         ? 44 : menu_state_get_ptr()->osk_ptr, SIMPLE_COL_FG);
}

static void simple_draw_card(simple_handle_t *sm, gfx_display_t *p_disp,
      void *userdata, unsigned dims)
{
   char line[256];
   unsigned r;
   const char *p = sm->card_text;
   float s       = sm->scale;
   float w       = (float)VIDEO_SCALE_W(dims);
   float x       = SIMPLE_GUTTER * s;
   float tx      = 2.0f * SIMPLE_GUTTER * s;
   float row_h   = sm->m->list_row * s;
   float y       = sm->card_y + (SIMPLE_CARD_PAD + SIMPLE_CARD_LINE * 0.5f) * s;
   unsigned rows = sm->card_confirm ? 2 : 1;
   static const enum msg_hash_enums answers[2][2] = {
      { MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_OK, MSG_UNKNOWN },
      { MENU_ENUM_LABEL_VALUE_YES, MENU_ENUM_LABEL_VALUE_NO }
   };

   simple_quad(p_disp, userdata, dims, 0.0f, 0.0f, w,
         simple_box_bottom(sm, dims), 0x000000A6);
   simple_quad(p_disp, userdata, dims, x, sm->card_y, w - 2.0f * x,
         sm->card_h, SIMPLE_COL_SHEET);

   for (r = 0; r < sm->card_lines && *p; r++)
   {
      size_t n = strcspn(p, "\n");
      strlcpy(line, p, n + 1 < sizeof(line) ? n + 1 : sizeof(line));
      simple_text(sm, SIMPLE_FONT_WORD, line, tx, y, dims,
            SIMPLE_COL_FG, TEXT_ALIGN_LEFT);
      y += SIMPLE_CARD_LINE * s;
      p += n;
      if (*p == '\n')
         p++;
   }

   for (r = 0; r < rows; r++)
   {
      float ry = sm->card_rows_y + (float)r * row_h;
      simple_quad(p_disp, userdata, dims, tx, ry, w - 2.0f * tx,
            s < 1.0f ? 1.0f : s, SIMPLE_COL_LINE);
      simple_text(sm, SIMPLE_FONT_WORD,
            msg_hash_to_str(answers[sm->card_confirm ? 1 : 0][r]),
            tx, ry + row_h * 0.5f, dims,
            r == 0 ? SIMPLE_COL_FG : SIMPLE_COL_MUTED, TEXT_ALIGN_LEFT);
   }
}

static void simple_frame(void *data, video_frame_info_t *video_info)
{
   int i;
   simple_handle_t *sm            = (simple_handle_t*)data;
   gfx_display_t *p_disp          = disp_get_ptr();
   video_driver_state_t *video_st = video_state_get_ptr();
   struct menu_state *menu_st     = menu_state_get_ptr();
   menu_list_t *menu_list         = menu_st->entries.list;
   file_list_t *list              = menu_list ? MENU_LIST_GET_SELECTION(menu_list, 0) : NULL;
   void *userdata                 = video_info->userdata;
   unsigned dims                  = video_info->dims;
   float w                        = (float)VIDEO_SCALE_W(dims);
   float h                        = (float)VIDEO_SCALE_H(dims);
   float box_b;

   if (!sm || !sm->fonts[SIMPLE_FONT_WORD].font)
      return;

   box_b = simple_box_bottom(sm, dims);

   if (video_st->current_video && video_st->current_video->set_viewport)
      video_st->current_video->set_viewport(video_st->data, dims, true, false);

   for (i = 0; i < SIMPLE_FONT_LAST; i++)
   {
      font_driver_bind_block(sm->fonts[i].font, &sm->fonts[i].raster_block);
      sm->fonts[i].raster_block.carr.coords.vertices = 0;
   }

   simple_quad(p_disp, userdata, dims, 0.0f, 0.0f, w, box_b, SIMPLE_COL_BG);
   if (box_b < h)
      simple_quad(p_disp, userdata, dims, 0.0f, box_b, w, h - box_b,
            SIMPLE_COL_BELOW);

   if (sm->has_continue && list && list->size)
   {
      const char *name = list->list[0].path;
      unsigned hash    = 0;
      size_t pick;
      while (name && *name)
         hash = hash * 31 + (unsigned char)*name++;
      pick = hash % (sizeof(simple_band) / sizeof(simple_band[0]));
      simple_band_quad(p_disp, userdata, dims, sm->m->band_h * sm->scale,
            simple_band[pick][0], simple_band[pick][1]);
   }

   if (list && list->size)
      simple_draw_rows(sm, p_disp, userdata, dims, list,
            menu_st->selection_ptr);
   for (i = 0; i < SIMPLE_FONT_LAST; i++)
      font_flush(dims, &sm->fonts[i]);

   /* Rows scrolled out of view go under the header and the hints */
   if (!sm->is_home)
   {
      float view_t = simple_view_top(sm);
      float line   = sm->scale < 1.0f ? 1.0f : sm->scale;
      float x      = SIMPLE_GUTTER * sm->scale;
      float rows_b = simple_rows_bottom(sm, dims);
      simple_quad(p_disp, userdata, dims, 0.0f, 0.0f, w, view_t,
            SIMPLE_COL_BG);
      /* The first row's line, held here while it is scrolled past */
      if (sm->scroll_y > 0.0f || !list || !list->size)
         simple_quad(p_disp, userdata, dims, x, view_t, w - 2.0f * x, line,
               SIMPLE_COL_LINE);
      if (rows_b < box_b)
         simple_quad(p_disp, userdata, dims, 0.0f, rows_b, w,
               box_b - rows_b, SIMPLE_COL_BG);
   }
   simple_draw_header(sm, dims);
   simple_draw_desc(sm, dims);
   if (sm->gamepad)
      simple_draw_hints(sm, p_disp, userdata, dims);
   for (i = 0; i < SIMPLE_FONT_LAST; i++)
      font_flush(dims, &sm->fonts[i]);

   if (sm->kb_on)
   {
      int f;
      simple_draw_kb(sm, p_disp, userdata, dims);
      for (f = 0; f < SIMPLE_FONT_LAST; f++)
         font_flush(dims, &sm->fonts[f]);
   }

   if (sm->card_on)
   {
      int f;
      simple_draw_card(sm, p_disp, userdata, dims);
      for (f = 0; f < SIMPLE_FONT_LAST; f++)
         font_flush(dims, &sm->fonts[f]);
   }

   for (i = 0; i < SIMPLE_FONT_LAST; i++)
      font_driver_bind_block(sm->fonts[i].font, NULL);

   if (video_st->current_video && video_st->current_video->set_viewport)
      video_st->current_video->set_viewport(video_st->data, dims, false, true);
}

/* Left edge of the part of a row that changes its value */
static float simple_value_left(const simple_handle_t *sm)
{
   return (float)VIDEO_SCALE_W(sm->dims) * 0.6f;
}

static int simple_pointer_up(void *data,
      unsigned x, unsigned y, unsigned ptr,
      enum menu_input_pointer_gesture gesture,
      menu_file_list_cbs_t *cbs,
      menu_entry_t *entry, unsigned action)
{
   float st, x0, x1;
   rarch_setting_t *setting;
   simple_handle_t *sm        = (simple_handle_t*)data;
   struct menu_state *menu_st = menu_state_get_ptr();
   menu_list_t *menu_list     = menu_st->entries.list;
   file_list_t *list          = menu_list ? MENU_LIST_GET_SELECTION(menu_list, 0) : NULL;
   size_t size                = list ? list->size : 0;
   float s;
   float fx                   = (float)x;
   float fy                   = (float)y;

   if (!sm)
      return -1;
   s = sm->scale;
   simple_log("tap %u,%u gesture=%d ptr=%u", x, y, (int)gesture, ptr);
   if (     gesture != MENU_INPUT_GESTURE_TAP
         && gesture != MENU_INPUT_GESTURE_SHORT_PRESS)
      return 0;
   if (fy >= simple_box_bottom(sm, sm->dims))
      return 0;

   /* The breadcrumb line is the back button */
   if (!sm->is_home && fy < simple_view_top(sm) - sm->desc_h)
      return simple_entry_action(sm, entry, menu_st->selection_ptr,
            MENU_ACTION_CANCEL);
   if (!sm->is_home && fy < simple_view_top(sm))
      return 0;

   /* The open number's slider row: - and + step, the track sets */
   if (     simple_slider_rect(sm, sm->dims, &st, &x0, &x1)
         && sm->open_idx < size
         && fy >= st && fy < st + sm->m->list_row * s)
   {
      setting = simple_row_setting(list, sm->open_idx);
      if (fx < x0 - 12.0f * s)
         menu_action_handle_setting(setting, list->list[sm->open_idx].type,
               MENU_ACTION_LEFT, false);
      else if (fx > x1 + 12.0f * s)
         menu_action_handle_setting(setting, list->list[sm->open_idx].type,
               MENU_ACTION_RIGHT, false);
      else
         simple_number_set(setting, list->list[sm->open_idx].type,
               (fx - x0) / (x1 - x0));
      return 0;
   }

   if (ptr >= size)
      return 0;

   menu_st->selection_ptr = ptr;
   MENU_ENTRY_INITIALIZE(sm->hit_entry);
   sm->hit_entry.flags |= MENU_ENTRY_FLAG_RICH_LABEL_ENABLED
                       | MENU_ENTRY_FLAG_VALUE_ENABLED;
   menu_entry_get(&sm->hit_entry, 0, ptr, NULL, true);
   setting = sm->is_home ? NULL : simple_row_setting(list, ptr);

   /* A number's row opens and closes its slider */
   if (simple_is_number(setting, sm->hit_entry.value))
   {
      sm->open_idx = (sm->open_idx == ptr) ? (size_t)-1 : ptr;
      return 0;
   }

   /* An on/off row or a choice: the right side changes it, or opens the
    * list of a long one; the word opens its options */
   if (setting && setting->type == ST_BOOL)
   {
      if (     fx >= simple_value_left(sm)
            || menu_setting_find_enum(setting->enum_idx) != setting)
         return simple_entry_action(sm, &sm->hit_entry, ptr,
               MENU_ACTION_SELECT);
      simple_keep_options(sm, ptr);
      return generic_action_ok_displaylist_push(sm->opt_path, NULL, NULL,
            0, ptr, 0, ACTION_OK_DL_DROPDOWN_BOX_LIST);
   }
   if (simple_is_choice(setting, sm->hit_entry.value) && sm->hit_entry.value[0])
   {
      char note[NAME_MAX_LENGTH];
      float lc, rc, vr;
      unsigned options = simple_options(setting);
      if (options)
         return simple_entry_action(sm, &sm->hit_entry, ptr,
               (options <= SIMPLE_OPTIONS_STEP
                && fx >= simple_value_left(sm))
               ? MENU_ACTION_RIGHT : MENU_ACTION_SELECT);
      simple_upper(note, sm->hit_entry.value, sizeof(note));
      simple_arrows(sm, sm->dims, simple_note_width(sm, note),
            &lc, &rc, &vr);
      if (fx >= rc - 24.0f * s)
         return simple_entry_action(sm, &sm->hit_entry, ptr,
               MENU_ACTION_RIGHT);
      if (fx >= lc - 24.0f * s && fx <= lc + 24.0f * s)
         return simple_entry_action(sm, &sm->hit_entry, ptr,
               MENU_ACTION_LEFT);
      return 0;
   }
   return simple_entry_action(sm, &sm->hit_entry, ptr,
         MENU_ACTION_SELECT);
}

static void simple_navigation_clear(void *data, bool pending_push)
{
   simple_handle_t *sm = (simple_handle_t*)data;
   if (sm)
   {
      sm->first     = 0;
      sm->last_list = NULL;
      sm->open_idx  = (size_t)-1;
   }
}

static int simple_list_bind_init(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx)
{
   return 0;
}

menu_ctx_driver_t menu_ctx_simple = {
   NULL,                         /* set_texture */
   simple_render_messagebox,
   simple_render,
   simple_frame,
   simple_init,
   simple_free,
   simple_context_reset,
   simple_context_destroy,
   NULL,                         /* populate_entries */
   NULL,                         /* toggle */
   simple_navigation_clear,
   NULL,                         /* navigation_decrement */
   NULL,                         /* navigation_increment */
   NULL,                         /* navigation_set */
   NULL,                         /* navigation_set_last */
   NULL,                         /* navigation_descend_alphabet */
   NULL,                         /* navigation_ascend_alphabet */
   NULL,                         /* lists_init */
   NULL,                         /* list_insert */
   NULL,                         /* list_prepend */
   NULL,                         /* list_delete */
   NULL,                         /* list_clear */
   NULL,                         /* list_cache */
   NULL,                         /* list_push */
   NULL,                         /* list_get_selection */
   NULL,                         /* list_get_size */
   NULL,                         /* list_get_entry */
   NULL,                         /* list_set_selection */
   simple_list_bind_init,
   NULL,                         /* load_image */
   "simple",
   NULL,                         /* environ */
   NULL,                         /* update_thumbnail_path */
   NULL,                         /* update_thumbnail_image */
   NULL,                         /* refresh_thumbnail_image */
   NULL,                         /* set_thumbnail_content */
   gfx_display_osk_ptr_at_pos,
   NULL,                         /* osk_pointer_over_textbox */
   NULL,                         /* update_savestate_thumbnail_path */
   NULL,                         /* update_savestate_thumbnail_image */
   NULL,                         /* pointer_down */
   simple_pointer_up,
   simple_entry_action,
   NULL                          /* wheel_scroll */
};
