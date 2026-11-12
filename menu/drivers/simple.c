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
 * the normal menu buttons. Sizes are in dp, on a 360 dp wide design.
 * Gamepad mode draws the menu in a 4:3 box at the top, over the touch
 * controls. */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <compat/strl.h>
#include <file/file_path.h>
#include <string/stdstring.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../menu_driver.h"
#include "../../gfx/gfx_display.h"
#include "../../gfx/font_driver.h"
#include "../../configuration.h"
#include "../../runloop.h"
#include "../../command.h"

#define SIMPLE_DESIGN_WIDTH   360.0f
#define SIMPLE_GUTTER         22.0f

#define SIMPLE_COL_BG         0x15171CFF
#define SIMPLE_COL_BELOW      0x0E1014FF
#define SIMPLE_COL_FG         0xEEF0F4FF
#define SIMPLE_COL_MUTED      0x9AA0ADFF
#define SIMPLE_COL_DIM        0x5E6472FF
#define SIMPLE_COL_LINE       0x2F333DFF
#define SIMPLE_COL_ACCENT     0xF2D64BFF
#define SIMPLE_COL_SHEET      0x20232BFF

enum simple_font
{
   SIMPLE_FONT_WORD = 0,
   SIMPLE_FONT_HOME,
   SIMPLE_FONT_NOTE,
   SIMPLE_FONT_LAST
};

/* All in dp */
typedef struct simple_metrics
{
   float font[SIMPLE_FONT_LAST];
   float header_y;
   float home_top;
   float list_top;
   float home_row;
   float list_row;
   float note_gap;
} simple_metrics_t;

static const simple_metrics_t simple_touch = {
   { 24.0f, 40.0f, 11.0f }, 46.0f, 150.0f, 72.0f, 72.0f, 48.0f, 22.0f };
static const simple_metrics_t simple_gamepad = {
   { 18.0f, 24.0f, 10.0f }, 20.0f, 44.0f, 40.0f, 36.0f, 32.0f, 14.0f };

typedef struct simple_handle
{
   menu_input_pointer_t pointer;    /* int64_t alignment */
   font_data_impl_t fonts[SIMPLE_FONT_LAST];
   menu_entry_t entry;
   const simple_metrics_t *m;
   size_t first;
   unsigned dims;
   float scale;
   bool is_home;
   bool gamepad;
   char msgbox[1024];
} simple_handle_t;

static void simple_color(float *c, uint32_t rgba)
{
   int i;
   for (i = 0; i < 4; i++)
   {
      c[i * 4 + 0] = ((rgba >> 24) & 0xFF) / 255.0f;
      c[i * 4 + 1] = ((rgba >> 16) & 0xFF) / 255.0f;
      c[i * 4 + 2] = ((rgba >>  8) & 0xFF) / 255.0f;
      c[i * 4 + 3] = ( rgba        & 0xFF) / 255.0f;
   }
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
   if (sm->gamepad && w * 0.75f < h)
      return w * 0.75f;
   return h;
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
      "WorkSans-Light.ttf", "WorkSans-Light.ttf", "IBMPlexMono-Medium.ttf" };
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

static void simple_quad(gfx_display_t *p_disp, void *userdata,
      unsigned dims, float x, float y, float w, float h, uint32_t rgba)
{
   float c[16];
   simple_color(c, rgba);
   gfx_display_draw_quad(p_disp, userdata, dims,
         (int)(x + 0.5f), (int)(y + 0.5f),
         VIDEO_SCALE_PACK((unsigned)(w + 0.5f), (unsigned)(h + 0.5f)),
         dims, c, NULL);
}

static float simple_list_top(const simple_handle_t *sm)
{
   return (sm->is_home ? sm->m->home_top : sm->m->list_top) * sm->scale;
}

static float simple_row_height(const simple_handle_t *sm)
{
   return (sm->is_home ? sm->m->home_row : sm->m->list_row) * sm->scale;
}

/* Bottom of the rows: gamepad mode keeps a line for button hints */
static float simple_rows_bottom(const simple_handle_t *sm, unsigned dims)
{
   return simple_box_bottom(sm, dims)
      - (sm->gamepad ? 28.0f * sm->scale : 0.0f);
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
   sm->gamepad = settings->bools.menu_simple_gamepad_mode;
   sm->m       = sm->gamepad ? &simple_gamepad : &simple_touch;
   sm->dims    = video_driver_get_output_dims();
   sm->scale   = simple_scale_for(sm->dims);
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
      strlcpy(sm->msgbox, msg, sizeof(sm->msgbox));
}

static void simple_render(void *data, unsigned dims, bool is_idle)
{
   size_t visible;
   float top, row_h, bottom;
   simple_handle_t *sm        = (simple_handle_t*)data;
   struct menu_state *menu_st = menu_state_get_ptr();
   menu_list_t *menu_list     = menu_st->entries.list;
   file_list_t *list          = menu_list ? MENU_LIST_GET_SELECTION(menu_list, 0) : NULL;
   size_t selection           = menu_st->selection_ptr;
   size_t size                = list ? list->size : 0;
   gfx_display_t *p_disp      = disp_get_ptr();
   settings_t *settings       = config_get_ptr();
   bool gamepad               = settings->bools.menu_simple_gamepad_mode;

   if (!sm)
      return;

   if (dims != sm->dims || gamepad != sm->gamepad)
   {
      if (gamepad != sm->gamepad)
      {
         sm->gamepad = gamepad;
         sm->m       = gamepad ? &simple_gamepad : &simple_touch;
#ifdef HAVE_OVERLAY
         command_event(CMD_EVENT_OVERLAY_UNLOAD, NULL);
         if (!menu_driver_overlay_hidden(settings))
            command_event(CMD_EVENT_OVERLAY_INIT, NULL);
#endif
      }
      sm->dims  = dims;
      sm->scale = simple_scale_for(dims);
      simple_fonts_load(sm, video_driver_is_threaded());
   }

   {
      const char *label = NULL;
      menu_entries_get_last_stack(NULL, &label, NULL, NULL, NULL);
      sm->is_home = string_is_equal(label,
            msg_hash_to_str(MENU_ENUM_LABEL_MAIN_MENU));
   }
   top                   = simple_list_top(sm);
   row_h                 = simple_row_height(sm);
   bottom                = simple_rows_bottom(sm, dims);
   p_disp->header_height = (unsigned)top;
   visible               = (size_t)((bottom - top) / row_h);
   if (visible < 1)
      visible = 1;

   if (selection < sm->first)
      sm->first = selection;
   else if (selection >= sm->first + visible)
      sm->first = selection - visible + 1;
   if (sm->first >= size)
      sm->first = 0;
   menu_st->entries.begin = sm->first;

   if (     settings->bools.menu_mouse_enable
         || settings->bools.menu_pointer_enable)
   {
      menu_input_get_pointer_state(&sm->pointer);
      if (     (sm->pointer.type != MENU_POINTER_DISABLED)
            && (sm->pointer.flags & MENU_INP_PTR_FLG_ACTIVE))
      {
         int y = VIDEO_POS_Y(sm->pointer.pos);
         menu_st->input_state.ptr = (y >= (int)top && y < (int)bottom)
            ? (unsigned)(((float)y - top) / row_h) + (unsigned)sm->first
            : (unsigned)size;
      }
   }
}

static void simple_draw_header(simple_handle_t *sm, unsigned dims)
{
   char title[NAME_MAX_LENGTH];
   char crumb[NAME_MAX_LENGTH + 16];
   float s     = sm->scale;
   float w     = (float)VIDEO_SCALE_W(dims);
   float y     = sm->m->header_y * s;
   bool runs   = (runloop_get_flags() & RUNLOOP_FLAG_CORE_RUNNING) ? true : false;

   if (sm->is_home)
   {
      simple_text(sm, SIMPLE_FONT_NOTE, "RETROARCH",
            SIMPLE_GUTTER * s, y, dims, SIMPLE_COL_DIM, TEXT_ALIGN_LEFT);
      if (runs)
         simple_text(sm, SIMPLE_FONT_NOTE, "PAUSED",
               w - SIMPLE_GUTTER * s, y, dims, SIMPLE_COL_MUTED,
               TEXT_ALIGN_RIGHT);
      return;
   }

   title[0] = '\0';
   menu_entries_get_title(title, sizeof(title));
   strlcpy(crumb, "< HOME / ", sizeof(crumb));
   simple_upper(crumb + 9, title, sizeof(crumb) - 9);
   simple_text(sm, SIMPLE_FONT_NOTE, crumb,
         SIMPLE_GUTTER * s, y, dims, SIMPLE_COL_MUTED, TEXT_ALIGN_LEFT);
   simple_text(sm, SIMPLE_FONT_NOTE, "RETROARCH",
         w - SIMPLE_GUTTER * s, y, dims, SIMPLE_COL_DIM, TEXT_ALIGN_RIGHT);
}

static void simple_draw_rows(simple_handle_t *sm, gfx_display_t *p_disp,
      void *userdata, unsigned dims, file_list_t *list, size_t selection)
{
   size_t i;
   char note[NAME_MAX_LENGTH];
   float s      = sm->scale;
   float w      = (float)VIDEO_SCALE_W(dims);
   float x      = SIMPLE_GUTTER * s;
   float top    = simple_list_top(sm);
   float row_h  = simple_row_height(sm);
   float bottom = simple_rows_bottom(sm, dims);
   float mark_h = row_h * 0.5f;
   enum simple_font word_font = sm->is_home
      ? SIMPLE_FONT_HOME : SIMPLE_FONT_WORD;

   for (i = sm->first; i < list->size; i++)
   {
      const char *word, *split;
      menu_entry_t *entry = &sm->entry;
      float y             = top + (float)(i - sm->first) * row_h;
      float centre        = y + row_h * 0.5f;
      bool selected       = (i == selection);

      if (y + row_h > bottom)
         break;

      MENU_ENTRY_INITIALIZE((*entry));
      entry->flags |= MENU_ENTRY_FLAG_RICH_LABEL_ENABLED
                    | MENU_ENTRY_FLAG_VALUE_ENABLED;
      menu_entry_get(entry, 0, i, NULL, true);
      word = entry->rich_label[0] ? entry->rich_label : entry->path;

      if (!sm->is_home && i > sm->first)
         simple_quad(p_disp, userdata, dims, x, y, w - 2.0f * x,
               s < 1.0f ? 1.0f : s, SIMPLE_COL_LINE);

      if (selected)
         simple_quad(p_disp, userdata, dims, x - 12.0f * s,
               centre - mark_h * 0.5f, 3.0f * s, mark_h, SIMPLE_COL_ACCENT);

      /* "Continue: <game>" shows the game as a note under the word */
      if (sm->is_home && (split = strstr(word, ": ")))
      {
         char head[NAME_MAX_LENGTH];
         size_t n = (size_t)(split - word) + 1;
         strlcpy(head, word, n < sizeof(head) ? n : sizeof(head));
         simple_upper(note, split + 2, sizeof(note));
         if (sm->gamepad)
         {
            simple_text(sm, word_font, head, x, centre, dims,
                  SIMPLE_COL_FG, TEXT_ALIGN_LEFT);
            simple_text(sm, SIMPLE_FONT_NOTE, note, w - x, centre,
                  dims, SIMPLE_COL_MUTED, TEXT_ALIGN_RIGHT);
         }
         else
         {
            simple_text(sm, word_font, head, x, centre - 8.0f * s, dims,
                  SIMPLE_COL_FG, TEXT_ALIGN_LEFT);
            simple_text(sm, SIMPLE_FONT_NOTE, note, x,
                  centre + sm->m->note_gap * s,
                  dims, SIMPLE_COL_MUTED, TEXT_ALIGN_LEFT);
         }
         continue;
      }

      simple_text(sm, word_font, word, x, centre, dims,
            SIMPLE_COL_FG, TEXT_ALIGN_LEFT);

      if (entry->value[0] && !string_is_equal(entry->value, "..."))
      {
         simple_upper(note, entry->value, sizeof(note));
         simple_text(sm, SIMPLE_FONT_NOTE, note, w - x, centre, dims,
               SIMPLE_COL_DIM, TEXT_ALIGN_RIGHT);
      }
   }
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
   simple_text(sm, SIMPLE_FONT_NOTE, "A OPEN   B BACK",
         SIMPLE_GUTTER * s, y, dims, SIMPLE_COL_MUTED, TEXT_ALIGN_LEFT);
   simple_text(sm, SIMPLE_FONT_NOTE, "GAMEPAD MODE",
         w - SIMPLE_GUTTER * s, y, dims, SIMPLE_COL_DIM, TEXT_ALIGN_RIGHT);
}

static void simple_draw_msgbox(simple_handle_t *sm, gfx_display_t *p_disp,
      void *userdata, unsigned dims)
{
   char line[256];
   const char *p   = sm->msgbox;
   float s         = sm->scale;
   float w         = (float)VIDEO_SCALE_W(dims);
   float h         = simple_box_bottom(sm, dims);
   float line_h    = 18.0f * s;
   float card_h;
   float y;
   unsigned lines  = 1;
   const char *q;

   for (q = p; *q; q++)
      if (*q == '\n')
         lines++;
   card_h = (float)lines * line_h + 40.0f * s;
   y      = (h - card_h) * 0.5f;

   simple_quad(p_disp, userdata, dims, 0.0f, 0.0f, w, h, 0x000000A6);
   simple_quad(p_disp, userdata, dims, SIMPLE_GUTTER * s, y,
         w - 2.0f * SIMPLE_GUTTER * s, card_h, SIMPLE_COL_SHEET);

   y += 20.0f * s + line_h * 0.5f;
   while (*p)
   {
      size_t n = strcspn(p, "\n");
      strlcpy(line, p, n + 1 < sizeof(line) ? n + 1 : sizeof(line));
      simple_text(sm, SIMPLE_FONT_NOTE, line, 2.0f * SIMPLE_GUTTER * s,
            y, dims, SIMPLE_COL_FG, TEXT_ALIGN_LEFT);
      y += line_h;
      p += n;
      if (*p == '\n')
         p++;
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

   simple_draw_header(sm, dims);
   if (list && list->size)
      simple_draw_rows(sm, p_disp, userdata, dims, list,
            menu_st->selection_ptr);
   if (sm->gamepad)
      simple_draw_hints(sm, p_disp, userdata, dims);

   for (i = 0; i < SIMPLE_FONT_LAST; i++)
      font_flush(dims, &sm->fonts[i]);

   if (sm->msgbox[0])
   {
      simple_draw_msgbox(sm, p_disp, userdata, dims);
      font_flush(dims, &sm->fonts[SIMPLE_FONT_NOTE]);
      sm->msgbox[0] = '\0';
   }

   for (i = 0; i < SIMPLE_FONT_LAST; i++)
      font_driver_bind_block(sm->fonts[i].font, NULL);

   if (video_st->current_video && video_st->current_video->set_viewport)
      video_st->current_video->set_viewport(video_st->data, dims, false, true);
}

static int simple_pointer_up(void *data,
      unsigned x, unsigned y, unsigned ptr,
      enum menu_input_pointer_gesture gesture,
      menu_file_list_cbs_t *cbs,
      menu_entry_t *entry, unsigned action)
{
   simple_handle_t *sm        = (simple_handle_t*)data;
   struct menu_state *menu_st = menu_state_get_ptr();
   menu_list_t *menu_list     = menu_st->entries.list;
   size_t size                = menu_list ? MENU_LIST_GET_SELECTION(menu_list, 0)->size : 0;

   if (!sm)
      return -1;
   if (     gesture != MENU_INPUT_GESTURE_TAP
         && gesture != MENU_INPUT_GESTURE_SHORT_PRESS)
      return 0;
   if (y >= (unsigned)simple_box_bottom(sm, sm->dims))
      return 0;

   /* The breadcrumb line is the back button */
   if (y < (unsigned)simple_list_top(sm))
      return sm->is_home ? 0
         : generic_menu_entry_action(sm, entry, menu_st->selection_ptr,
               MENU_ACTION_CANCEL);
   if (ptr < size)
   {
      menu_st->selection_ptr = ptr;
      return generic_menu_entry_action(sm, entry, ptr, MENU_ACTION_SELECT);
   }
   return 0;
}

static void simple_navigation_clear(void *data, bool pending_push)
{
   simple_handle_t *sm = (simple_handle_t*)data;
   if (sm)
      sm->first = 0;
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
   NULL,                         /* osk_ptr_at_pos */
   NULL,                         /* osk_pointer_over_textbox */
   NULL,                         /* update_savestate_thumbnail_path */
   NULL,                         /* update_savestate_thumbnail_image */
   NULL,                         /* pointer_down */
   simple_pointer_up,
   generic_menu_entry_action,
   NULL                          /* wheel_scroll */
};
