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
 * the normal menu buttons. Sizes are in dp, on a 360 dp wide design. */

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

#define SIMPLE_DESIGN_WIDTH   360.0f
#define SIMPLE_GUTTER         22.0f
#define SIMPLE_HEADER_TOP     40.0f
#define SIMPLE_HEADER_HEIGHT  64.0f
#define SIMPLE_HOME_TOP       150.0f
#define SIMPLE_HOME_ROW       72.0f
#define SIMPLE_LIST_ROW       48.0f
#define SIMPLE_WORD_SIZE      24.0f
#define SIMPLE_HOME_SIZE      40.0f
#define SIMPLE_NOTE_SIZE      11.0f

#define SIMPLE_COL_BG         0x15171CFF
#define SIMPLE_COL_FG         0xEEF0F4FF
#define SIMPLE_COL_MUTED      0x9AA0ADFF
#define SIMPLE_COL_DIM        0x5E6472FF
#define SIMPLE_COL_LINE       0x2F333DFF
#define SIMPLE_COL_ACCENT     0xF2D64BFF

enum simple_font
{
   SIMPLE_FONT_WORD = 0,
   SIMPLE_FONT_HOME,
   SIMPLE_FONT_NOTE,
   SIMPLE_FONT_LAST
};

typedef struct simple_handle
{
   menu_input_pointer_t pointer;    /* int64_t alignment */
   font_data_impl_t fonts[SIMPLE_FONT_LAST];
   menu_entry_t entry;
   size_t first;
   unsigned dims;
   float scale;
   bool is_home;
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
   static const float sizes[SIMPLE_FONT_LAST] = {
      SIMPLE_WORD_SIZE, SIMPLE_HOME_SIZE, SIMPLE_NOTE_SIZE };
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
            sizes[i] * sm->scale, is_threaded);
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
   return (sm->is_home ? SIMPLE_HOME_TOP : SIMPLE_HEADER_HEIGHT + 8.0f)
      * sm->scale;
}

static float simple_row_height(const simple_handle_t *sm)
{
   return (sm->is_home ? SIMPLE_HOME_ROW : SIMPLE_LIST_ROW) * sm->scale;
}

static void *simple_init(void **userdata, bool video_is_threaded)
{
   simple_handle_t *sm = NULL;
   menu_handle_t *menu = (menu_handle_t*)calloc(1, sizeof(*menu));

   if (!menu)
      return NULL;
   if (!(sm = (simple_handle_t*)calloc(1, sizeof(*sm))))
   {
      free(menu);
      return NULL;
   }
   sm->dims  = video_driver_get_output_dims();
   sm->scale = simple_scale_for(sm->dims);
   *userdata = sm;
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

static void simple_render(void *data, unsigned dims, bool is_idle)
{
   size_t visible;
   float top, row_h;
   simple_handle_t *sm        = (simple_handle_t*)data;
   struct menu_state *menu_st = menu_state_get_ptr();
   menu_list_t *menu_list     = menu_st->entries.list;
   file_list_t *list          = menu_list ? MENU_LIST_GET_SELECTION(menu_list, 0) : NULL;
   size_t selection           = menu_st->selection_ptr;
   size_t size                = list ? list->size : 0;
   gfx_display_t *p_disp      = disp_get_ptr();
   settings_t *settings       = config_get_ptr();

   if (!sm)
      return;

   if (dims != sm->dims)
   {
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
   p_disp->header_height = (unsigned)top;
   visible               = (size_t)(((float)VIDEO_SCALE_H(dims) - top) / row_h);
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
         menu_st->input_state.ptr = (y >= (int)top)
            ? (unsigned)(((float)y - top) / row_h) + (unsigned)sm->first
            : (unsigned)size;
      }
   }
}

static void simple_draw_header(simple_handle_t *sm, gfx_display_t *p_disp,
      void *userdata, unsigned dims, struct menu_state *menu_st)
{
   char title[NAME_MAX_LENGTH];
   char crumb[NAME_MAX_LENGTH + 16];
   float s     = sm->scale;
   float w     = (float)VIDEO_SCALE_W(dims);
   float y     = (SIMPLE_HEADER_TOP + 6.0f) * s;
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
   (void)p_disp;
   (void)userdata;
   (void)menu_st;
}

static void simple_draw_rows(simple_handle_t *sm, gfx_display_t *p_disp,
      void *userdata, unsigned dims, file_list_t *list, size_t selection)
{
   size_t i;
   char note[NAME_MAX_LENGTH];
   float s     = sm->scale;
   float w     = (float)VIDEO_SCALE_W(dims);
   float h     = (float)VIDEO_SCALE_H(dims);
   float x     = SIMPLE_GUTTER * s;
   float top   = simple_list_top(sm);
   float row_h = simple_row_height(sm);
   enum simple_font word_font = sm->is_home
      ? SIMPLE_FONT_HOME : SIMPLE_FONT_WORD;

   for (i = sm->first; i < list->size; i++)
   {
      const char *word, *split;
      menu_entry_t *entry = &sm->entry;
      float y             = top + (float)(i - sm->first) * row_h;
      float centre        = y + row_h * 0.5f;
      bool selected       = (i == selection);

      if (y + row_h > h)
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
               centre - 12.0f * s, 3.0f * s, 24.0f * s, SIMPLE_COL_ACCENT);

      /* "Continue: <game>" shows the game as a note under the word */
      if (sm->is_home && (split = strstr(word, ": ")))
      {
         char head[NAME_MAX_LENGTH];
         size_t n = (size_t)(split - word) + 1;
         strlcpy(head, word, n < sizeof(head) ? n : sizeof(head));
         simple_text(sm, word_font, head, x, centre - 8.0f * s, dims,
               SIMPLE_COL_FG, TEXT_ALIGN_LEFT);
         simple_upper(note, split + 2, sizeof(note));
         simple_text(sm, SIMPLE_FONT_NOTE, note, x, centre + 22.0f * s,
               dims, SIMPLE_COL_MUTED, TEXT_ALIGN_LEFT);
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

   if (!sm || !sm->fonts[SIMPLE_FONT_WORD].font)
      return;

   if (video_st->current_video && video_st->current_video->set_viewport)
      video_st->current_video->set_viewport(video_st->data, dims, true, false);

   for (i = 0; i < SIMPLE_FONT_LAST; i++)
   {
      font_driver_bind_block(sm->fonts[i].font, &sm->fonts[i].raster_block);
      sm->fonts[i].raster_block.carr.coords.vertices = 0;
   }

   simple_quad(p_disp, userdata, dims, 0.0f, 0.0f,
         (float)VIDEO_SCALE_W(dims), (float)VIDEO_SCALE_H(dims),
         SIMPLE_COL_BG);

   simple_draw_header(sm, p_disp, userdata, dims, menu_st);
   if (list && list->size)
      simple_draw_rows(sm, p_disp, userdata, dims, list,
            menu_st->selection_ptr);

   for (i = 0; i < SIMPLE_FONT_LAST; i++)
   {
      font_flush(dims, &sm->fonts[i]);
      font_driver_bind_block(sm->fonts[i].font, NULL);
   }

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
   NULL,                         /* render_messagebox */
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
