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

/* The touch layout editor, and gamepads: overlay configs of the user's
 * own, laid out in it, with custom buttons. input_driver.c calls in
 * from where the overlay is loaded, polled and drawn. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <boolean.h>
#include <retro_miscellaneous.h>
#include <compat/strl.h>
#include <string/stdstring.h>
#include <file/file_path.h>
#include <file/config_file.h>
#include <lists/dir_list.h>
#include <lists/string_list.h>
#include <streams/file_stream.h>
#include <string/rstrtod.h>

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include "input_driver.h"
#include "input_overlay.h"
#include "input_remapping.h"
#include "input_overlay_layout.h"
#include "../configuration.h"
#include "../file_path_special.h"
#include "../msg_hash.h"
#include "../paths.h"
#include "../retroarch.h"
#include "../runloop.h"
#include "../verbosity.h"
#ifdef HAVE_MENU
#include "../menu/menu_driver.h"
#endif
#ifdef HAVE_GFX_WIDGETS
#include "../gfx/gfx_widgets.h"
#endif

#ifdef HAVE_OVERLAY

#define input_driver_st (*input_state_get_ptr())

/* Layout editor. A touch grabs the control under it - the descs that
 * overlap where the pack puts them (a D-pad is a dozen), or that the
 * user layout added together - and drags it; the same descs on the
 * other pages of the same shape move along. What is laid out is saved
 * to the layout of the scope chosen (input_overlay_layout_path). */
typedef struct
{
   unsigned page;
   unsigned desc;
   float x;
   float y;
} input_overlay_edit_item_t;

enum input_overlay_edit_flags
{
   OVERLAY_EDIT_ACTIVE   = (1 << 0),
   /* The editor paused the core, and resumes it */
   OVERLAY_EDIT_PAUSED   = (1 << 1),
   /* A finger is down, whether or not it grabbed anything */
   OVERLAY_EDIT_HELD     = (1 << 2),
   OVERLAY_EDIT_DRAGGING = (1 << 3),
   OVERLAY_EDIT_MOVED    = (1 << 4),
   /* Moves not yet saved */
   OVERLAY_EDIT_CHANGED  = (1 << 5),
   /* Back is down, and went down while editing: its release finishes */
   OVERLAY_EDIT_BACK     = (1 << 6),
   OVERLAY_EDIT_BACK_NEW = (1 << 7),
   /* The bar's panel of controls is open */
   OVERLAY_EDIT_PANEL    = (1 << 8),
   /* The bar is being moved by its grip */
   OVERLAY_EDIT_GRIP     = (1 << 9),
   /* The panel's list is being scrolled */
   OVERLAY_EDIT_SCROLL   = (1 << 10),
   /* A control (sel_group of the page shown) is selected, or the game's
    * screen is */
   OVERLAY_EDIT_SELECTED = (1 << 11),
   OVERLAY_EDIT_SEL_SCREEN = (1 << 12),
   /* A finger went down on the game's screen, and has moved it */
   OVERLAY_EDIT_SCREEN_HELD  = (1 << 13),
   OVERLAY_EDIT_SCREEN_MOVED = (1 << 14),
   /* A control was sized while the finger was down: drawn buttons are
    * drawn again at the new size once it lifts */
   OVERLAY_EDIT_RESIZED      = (1 << 15)
};

/* Grid cells to the longer side of the screen, square, whose corners
 * the middle of what is dragged snaps to */
#define OVERLAY_EDIT_GRID 50

static struct
{
   input_overlay_edit_item_t *items;
   const input_overlay_t *ol;
   size_t count;
   size_t cap;
   float start_x;
   float start_y;
   /* Middle of what is drawn of the grabbed control, as fractions of
    * the screen */
   float mid_x;
   float mid_y;
   /* Where the finger last was, in pixels, and where on the bar's grip
    * it took hold */
   float ptr_x;
   float ptr_y;
   float grip_x;
   float grip_y;
   unsigned page;
   /* First control the panel shows, and the first and finger height
    * a scroll of it started from */
   unsigned panel_first;
   unsigned scroll_first;
   float scroll_y;
   /* gfx_overlay_bar_item held down, or -1 */
   int bar_item;
   /* Where the game's screen was when the finger went down on it, as
    * fractions of the screen */
   float screen_x;
   float screen_y;
   uint16_t sel_group;
   uint16_t flags;
} input_overlay_edit;

/* Where the bar is, as fractions of the screen (negative before it was
 * first shown), and whether the grid is halved: kept from one editing
 * to the next while the app runs */
static struct
{
   float x;
   float y;
   bool half;
} input_overlay_edit_bar = { -1.0f, -1.0f, false };

/* The scope the layout is edited for, and the content it was chosen
 * with: other content starts from the most specific layout it has. */
static struct
{
   uint32_t key;
   enum overlay_layout_scope scope;
   bool chosen;
} input_overlay_layout_sel;

/* The names the core and content layouts are filed under; empty when
 * no core, or no content, is running. */
static void input_overlay_layout_names(char *core, size_t core_len,
      char *content, size_t content_len)
{
   runloop_state_t *runloop_st = runloop_state_get_ptr();
   const char *core_name       = runloop_st->system.info.library_name;
   const char *base            = path_get(RARCH_PATH_BASENAME);

   *core    = '\0';
   *content = '\0';
   if (     runloop_st->current_core_type == CORE_TYPE_DUMMY
         || !core_name || !*core_name)
      return;
   strlcpy(core, core_name, core_len);
   if (base && *base)
      strlcpy(content, path_basename_nocompression(base), content_len);
}

void input_overlay_layout_path(char *s, size_t len,
      const char *overlay_path, enum overlay_layout_scope scope)
{
   char dir[DIR_MAX_LENGTH];
   char core[NAME_MAX_LENGTH];
   char content[NAME_MAX_LENGTH];
   const char *name = overlay_path ? path_basename(overlay_path) : NULL;

   *s = '\0';
   if (!name || !*name)
      return;

   fill_pathname_application_special(dir, sizeof(dir),
         APPLICATION_SPECIAL_DIRECTORY_CONFIG);
   if (!*dir)
      return;
   fill_pathname_join_special(dir, dir, "overlay_layouts", sizeof(dir));

   if (scope != OVERLAY_LAYOUT_SCOPE_ALL)
   {
      input_overlay_layout_names(core, sizeof(core),
            content, sizeof(content));
      if (!*core || (scope == OVERLAY_LAYOUT_SCOPE_CONTENT && !*content))
         return;
      fill_pathname_join_special(dir, dir, core, sizeof(dir));
      if (scope == OVERLAY_LAYOUT_SCOPE_CONTENT)
         fill_pathname_join_special(dir, dir, content, sizeof(dir));
   }

   fill_pathname_join_special(s, dir, name, len);
}

uint32_t input_overlay_layout_key(void)
{
   char core[NAME_MAX_LENGTH];
   char content[NAME_MAX_LENGTH];
   const char *s;
   uint32_t hash = 5381;

   input_overlay_layout_names(core, sizeof(core), content, sizeof(content));
   for (s = core; *s; s++)
      hash = hash * 33 + (uint8_t)*s;
   hash = hash * 33;
   for (s = content; *s; s++)
      hash = hash * 33 + (uint8_t)*s;
   return hash;
}

/* The gamepad overlay being laid out, shown or not, if loaded */
static input_overlay_t *input_overlay_layout_target(void)
{
   input_overlay_t *ol = input_driver_st.overlay_ptr;
   if (!ol || (ol->flags & INPUT_OVERLAY_IS_OSK))
      ol = input_driver_st.overlay_cache_ptr;
   if (!ol || (ol->flags & INPUT_OVERLAY_IS_OSK) || !ol->active)
      return NULL;
   return ol;
}

/* The most specific scope the running content has */
static enum overlay_layout_scope input_overlay_layout_scope_max(void)
{
   char core[NAME_MAX_LENGTH];
   char content[NAME_MAX_LENGTH];

   input_overlay_layout_names(core, sizeof(core), content, sizeof(content));
   if (!*core)
      return OVERLAY_LAYOUT_SCOPE_ALL;
   if (!*content)
      return OVERLAY_LAYOUT_SCOPE_CORE;
   return OVERLAY_LAYOUT_SCOPE_CONTENT;
}

enum overlay_layout_scope input_overlay_layout_scope(void)
{
   uint32_t key                  = input_overlay_layout_key();
   enum overlay_layout_scope max = input_overlay_layout_scope_max();

   if (     !input_overlay_layout_sel.chosen
         || input_overlay_layout_sel.key != key)
   {
      char path[PATH_MAX_LENGTH];
      input_overlay_t *ol = input_overlay_layout_target();
      int scope           = (int)max;

      /* The most specific layout there is, or controller chosen */
      for (; ol && scope > OVERLAY_LAYOUT_SCOPE_ALL; scope--)
      {
         input_overlay_layout_path(path, sizeof(path), ol->path,
               (enum overlay_layout_scope)scope);
         if (*path && path_is_valid(path))
            break;
         input_overlay_layout_path(path, sizeof(path), "controller.cfg",
               (enum overlay_layout_scope)scope);
         if (*path && path_is_valid(path))
            break;
      }
      if (!ol)
         scope = OVERLAY_LAYOUT_SCOPE_ALL;

      input_overlay_layout_sel.key    = key;
      input_overlay_layout_sel.scope  = (enum overlay_layout_scope)scope;
      input_overlay_layout_sel.chosen = true;
   }

   if (input_overlay_layout_sel.scope > max)
      input_overlay_layout_sel.scope = max;
   return input_overlay_layout_sel.scope;
}

void input_overlay_layout_scope_step(int dir)
{
   int max   = (int)input_overlay_layout_scope_max();
   int scope = (int)input_overlay_layout_scope() + dir;

   if (scope < 0)
      scope = max;
   else if (scope > max)
      scope = 0;
   input_overlay_layout_sel.scope = (enum overlay_layout_scope)scope;
}

size_t input_overlay_layout_scope_name(char *s, size_t len)
{
   char core[NAME_MAX_LENGTH];
   char content[NAME_MAX_LENGTH];

   input_overlay_layout_names(core, sizeof(core), content, sizeof(content));
   switch (input_overlay_layout_scope())
   {
      case OVERLAY_LAYOUT_SCOPE_CORE:
         return snprintf(s, len,
               msg_hash_to_str(MSG_OVERLAY_LAYOUT_SCOPE_CORE), core);
      case OVERLAY_LAYOUT_SCOPE_CONTENT:
         return snprintf(s, len,
               msg_hash_to_str(MSG_OVERLAY_LAYOUT_SCOPE_CONTENT), content);
      default:
         break;
   }
   return strlcpy(s, msg_hash_to_str(MSG_OVERLAY_LAYOUT_SCOPE_ALL), len);
}

static void input_overlay_layout_file(char *s, size_t len,
      const char *overlay_path);
static bool input_overlay_profile_normalize_file(const char *path);
static const char *input_overlay_profile_for(const char *pack);
static int input_overlay_builtin_profile_find(const char *name);
static void input_overlay_profile_file(char *s, size_t len, const char *name);
static void input_overlay_profiles_ensure(void);
static bool input_overlay_builtin_profile_reset(unsigned i);

/* The gamepads every build has, made from the overlays
 * input_overlay_builtin_pack() names */
static const char *input_overlay_builtin_profiles[] = {
   "Main", "Nintendo DS", "Nintendo 3DS" };

static bool input_overlay_layout_save(const input_overlay_t *ol)
{
   size_t i, j;
   char key[64];
   char val[32];
   char path[PATH_MAX_LENGTH];
   char dir[DIR_MAX_LENGTH];
   config_file_t *conf = NULL;
   bool ret            = false;

   input_overlay_layout_file(path, sizeof(path), ol->path);
   if (!*path)
      return false;

   /* Keep what earlier sessions saved */
   if (     !path_is_valid(path)
         || !(conf = config_file_new_from_path_to_string(path)))
      conf = config_file_new_alloc();
   if (!conf)
      return false;

   for (i = 0; i < ol->size; i++)
   {
      const struct overlay *page = &ol->overlays[i];
      if (page->flags & OVERLAY_LAYOUT_VIEWPORT_EDITED)
      {
         char vp[96];
         snprintf(key, sizeof(key), "overlay%u_viewport", (unsigned)i);
         if (page->flags & OVERLAY_HAS_VIEWPORT)
            snprintf(vp, sizeof(vp), "%.6f,%.6f,%.6f,%.6f",
                  page->viewport.x, page->viewport.y,
                  page->viewport.w, page->viewport.h);
         else
            strlcpy(vp, "none", sizeof(vp));
         config_set_string(conf, key, vp);
      }
      for (j = 0; j < page->size; j++)
      {
         const struct overlay_desc *desc = &page->descs[j];
         if (desc->flags & OVERLAY_DESC_LAYOUT_SIZE_EDITED)
         {
            snprintf(key, sizeof(key), "overlay%u_desc%u_layout_w",
                  (unsigned)i, (unsigned)j);
            snprintf(val, sizeof(val), "%.6f", desc->range_x);
            config_set_string(conf, key, val);
            snprintf(key, sizeof(key), "overlay%u_desc%u_layout_h",
                  (unsigned)i, (unsigned)j);
            snprintf(val, sizeof(val), "%.6f", desc->range_y);
            config_set_string(conf, key, val);
         }
         if (desc->flags & OVERLAY_DESC_LAYOUT_EDITED)
         {
            snprintf(key, sizeof(key), "overlay%u_desc%u_layout_x",
                  (unsigned)i, (unsigned)j);
            snprintf(val, sizeof(val), "%.6f", desc->x);
            config_set_string(conf, key, val);
            snprintf(key, sizeof(key), "overlay%u_desc%u_layout_y",
                  (unsigned)i, (unsigned)j);
            snprintf(val, sizeof(val), "%.6f", desc->y);
            config_set_string(conf, key, val);
         }
         if (desc->flags & OVERLAY_DESC_LAYOUT_HIDE_EDITED)
         {
            snprintf(key, sizeof(key), "overlay%u_desc%u_layout_hidden",
                  (unsigned)i, (unsigned)j);
            config_set_string(conf, key,
                  (desc->flags & OVERLAY_DESC_LAYOUT_HIDDEN)
                  ? "true" : "false");
         }
      }
   }

   fill_pathname_basedir(dir, path, sizeof(dir));
   if (!path_is_directory(dir))
      path_mkdir(dir);

   if ((ret = config_file_write(conf, path, true)))
   {
      for (i = 0; i < ol->size; i++)
      {
         ol->overlays[i].flags &= ~OVERLAY_LAYOUT_VIEWPORT_EDITED;
         for (j = 0; j < ol->overlays[i].size; j++)
            ol->overlays[i].descs[j].flags &= ~(OVERLAY_DESC_LAYOUT_EDITED
                  | OVERLAY_DESC_LAYOUT_HIDE_EDITED
                  | OVERLAY_DESC_LAYOUT_SIZE_EDITED);
      }
      /* In a profile, in the descs, for any loader */
      if (input_overlay_is_profile(path))
         input_overlay_profile_normalize_file(path);
      RARCH_LOG("[Overlay] Saved layout: \"%s\".\n", path);
   }
   else
      RARCH_ERR("[Overlay] Failed to save layout: \"%s\".\n", path);

   config_file_free(conf);
   return ret;
}

static bool input_overlay_pack_overlap(const struct overlay_desc *a,
      const struct overlay_desc *b)
{
   return   fabs(a->pack_x - b->pack_x) < a->pack_range_x + b->pack_range_x
         && fabs(a->pack_y - b->pack_y) < a->pack_range_y + b->pack_range_y;
}

/* Numbers the controls of @page (overlay_desc::layout_group). Where the
 * pack puts them, and not where the user did: a control dropped on
 * another stays apart from it. */
static void input_overlay_layout_group(struct overlay *page)
{
   size_t i, j, k;
   uint16_t group = 0;
   bool grew;

   if (page->flags & OVERLAY_LAYOUT_GROUPED)
      return;

   for (i = 0; i < page->size; i++)
      page->descs[i].layout_group = 0;

   for (i = 0; i < page->size; i++)
   {
      if (page->descs[i].layout_group)
         continue;
      page->descs[i].layout_group = ++group;
      do
      {
         grew = false;
         for (j = 0; j < page->size; j++)
         {
            struct overlay_desc *desc = &page->descs[j];
            if (desc->layout_group)
               continue;
            for (k = 0; k < page->size; k++)
            {
               const struct overlay_desc *in = &page->descs[k];
               if (in->layout_group != group)
                  continue;
               if (     (desc->layout_added || in->layout_added)
                     ? (desc->layout_added == in->layout_added)
                     : input_overlay_pack_overlap(desc, in))
               {
                  desc->layout_group = group;
                  grew               = true;
                  break;
               }
            }
         }
      } while (grew);
   }

   page->flags |= OVERLAY_LAYOUT_GROUPED;
}

/* The same control on another page: what input_overlay_next_move_touch_masks
 * matches, held to the binds as well. */
static bool input_overlay_edit_same(const struct overlay_desc *a,
      const struct overlay_desc *b)
{
   return   a->type          == b->type
         && a->retro_key_idx == b->retro_key_idx
         && fabs(a->x - b->x) < 0.001f
         && fabs(a->y - b->y) < 0.001f
         && !memcmp(&a->button_mask, &b->button_mask,
               sizeof(a->button_mask));
}

static bool input_overlay_page_has(const struct overlay *page,
      const struct overlay_desc *desc)
{
   size_t i;
   for (i = 0; i < page->size; i++)
      if (input_overlay_edit_same(&page->descs[i], desc))
         return true;
   return false;
}

static bool input_overlay_page_alike(const struct overlay *a,
      const struct overlay *b)
{
   return fabs(a->aspect_ratio - b->aspect_ratio) <= 0.01f;
}

/* Hides or shows @group of the active page, and the same descs on the
 * pages of its shape */
static void input_overlay_layout_set_hidden(input_overlay_t *ol,
      uint16_t group, bool hidden)
{
   size_t i, j, k;
   size_t index           = (size_t)(ol->active - ol->overlays);
   struct overlay *active = &ol->overlays[index];

   for (i = 0; i < active->size; i++)
   {
      struct overlay_desc *desc = &active->descs[i];
      if (desc->layout_group != group)
         continue;
      for (j = 0; j < ol->size; j++)
      {
         struct overlay *page = &ol->overlays[j];
         if (j != index && !input_overlay_page_alike(page, active))
            continue;
         for (k = 0; k < page->size; k++)
         {
            struct overlay_desc *d = &page->descs[k];
            if (d != desc && (j == index || !input_overlay_edit_same(d, desc)))
               continue;
            if (hidden)
               d->flags |=  OVERLAY_DESC_LAYOUT_HIDDEN;
            else
               d->flags &= ~OVERLAY_DESC_LAYOUT_HIDDEN;
            d->flags |= OVERLAY_DESC_LAYOUT_HIDE_EDITED;
         }
      }
   }
}

static void input_overlay_edit_drop(input_overlay_t *ol)
{
   size_t i;
   if (ol && ol->active)
      for (i = 0; i < ol->active->size; i++)
         ol->active->descs[i].touch_mask = 0;
   input_overlay_edit.count  = 0;
   input_overlay_edit.flags &= ~(OVERLAY_EDIT_DRAGGING | OVERLAY_EDIT_MOVED);
}

static void input_overlay_layout_reload(const input_overlay_t *ol);

static size_t input_overlay_layout_name(const struct overlay *page,
      uint16_t group, char *s, size_t len);
/* The page shown when the layout changed, to show again once it has
 * loaded */
static int input_overlay_layout_reload_page = -1;

/* The layout file of the scope edited as it was before the editor
 * first added a control to it, for Cancel to put back */
static struct
{
   void *buf;
   int64_t len;
   char path[PATH_MAX_LENGTH];
   bool taken;
   bool existed;
} input_overlay_edit_undo;

static void input_overlay_edit_undo_drop(void)
{
   free(input_overlay_edit_undo.buf);
   memset(&input_overlay_edit_undo, 0, sizeof(input_overlay_edit_undo));
}

#ifdef HAVE_GFX_WIDGETS
static void input_overlay_edit_undo_take(const input_overlay_t *ol)
{
   if (input_overlay_edit_undo.taken)
      return;
   input_overlay_layout_file(input_overlay_edit_undo.path,
         sizeof(input_overlay_edit_undo.path), ol->path);
   input_overlay_edit_undo.taken   = true;
   input_overlay_edit_undo.existed =
         *input_overlay_edit_undo.path
      && path_is_valid(input_overlay_edit_undo.path)
      && filestream_read_file(input_overlay_edit_undo.path,
            &input_overlay_edit_undo.buf, &input_overlay_edit_undo.len);
}

static void input_overlay_edit_undo_put_back(void)
{
   if (!input_overlay_edit_undo.taken || !*input_overlay_edit_undo.path)
      return;
   if (input_overlay_edit_undo.existed)
      filestream_write_file(input_overlay_edit_undo.path,
            input_overlay_edit_undo.buf, input_overlay_edit_undo.len);
   else if (path_is_valid(input_overlay_edit_undo.path))
      filestream_delete(input_overlay_edit_undo.path);
}
#endif

static void input_overlay_edit_finish(input_overlay_t *ol, bool save)
{
   uint16_t flags = input_overlay_edit.flags;

   if (!(flags & OVERLAY_EDIT_ACTIVE))
      return;

   input_overlay_edit_drop(ol);
   free(input_overlay_edit.items);
   memset(&input_overlay_edit, 0, sizeof(input_overlay_edit));
   input_overlay_edit_undo_drop();
#ifdef HAVE_GFX_WIDGETS
   gfx_widgets_set_overlay_bar(NULL);
#endif

   if (save && ol && (flags & OVERLAY_EDIT_CHANGED))
   {
      bool saved       = input_overlay_layout_save(ol);
      const char *_msg = msg_hash_to_str(saved
            ? MSG_OVERLAY_LAYOUT_SAVED : MSG_OVERLAY_LAYOUT_SAVE_FAILED);
      runloop_msg_queue_push(_msg, strlen(_msg), 1, 120, true, NULL,
            MESSAGE_QUEUE_ICON_DEFAULT, saved
            ? MESSAGE_QUEUE_CATEGORY_INFO : MESSAGE_QUEUE_CATEGORY_ERROR);
   }

   /* The finger that finished must not press anything */
   if (ol)
      ol->flags |= INPUT_OVERLAY_BLOCKED;

   if (flags & OVERLAY_EDIT_PAUSED)
      command_event(CMD_EVENT_UNPAUSE, NULL);
}

static bool input_overlay_edit_push(unsigned page, unsigned desc,
      const struct overlay_desc *d)
{
   input_overlay_edit_item_t *item;

   if (input_overlay_edit.count == input_overlay_edit.cap)
   {
      size_t cap = input_overlay_edit.cap ? input_overlay_edit.cap * 2 : 64;
      input_overlay_edit_item_t *items = (input_overlay_edit_item_t*)
         realloc(input_overlay_edit.items, cap * sizeof(*items));
      if (!items)
         return false;
      input_overlay_edit.items = items;
      input_overlay_edit.cap   = cap;
   }

   item       = &input_overlay_edit.items[input_overlay_edit.count++];
   item->page = page;
   item->desc = desc;
   item->x    = d->x_shift;
   item->y    = d->y_shift;
   return true;
}

static void input_overlay_edit_grab(input_overlay_t *ol, float x, float y)
{
   size_t i, j, k;
   uint16_t group;
   size_t index           = (size_t)(ol->active - ol->overlays);
   struct overlay *active = &ol->overlays[index];
   size_t first           = active->size;
   bool first_drawn       = false;
   float first_area       = 0.0f;

   input_overlay_layout_group(active);

   /* The smallest drawn desc under the finger, else the smallest of a
    * drawn control (a D-pad's middle is only its area desc) */
   for (i = 0; i < active->size; i++)
   {
      const struct overlay_desc *desc = &active->descs[i];
      float area = desc->range_x * desc->range_y;
      bool drawn = OVERLAY_HAS_IMAGE(&desc->image);
      if (     (desc->flags & OVERLAY_DESC_LAYOUT_HIDDEN)
            || fabs(x - desc->x_shift) > desc->range_x
            || fabs(y - desc->y_shift) > desc->range_y)
         continue;
      if (!drawn)
      {
         for (j = 0; j < active->size && !drawn; j++)
            drawn = active->descs[j].layout_group == desc->layout_group
               && OVERLAY_HAS_IMAGE(&active->descs[j].image)
               && !(active->descs[j].flags & OVERLAY_DESC_LAYOUT_HIDDEN);
         if (!drawn)
            continue;
         drawn = false;
      }
      else
         drawn = true;
      if (     first == active->size
            || (drawn && !first_drawn)
            || (drawn == first_drawn && area < first_area))
      {
         first       = i;
         first_drawn = drawn;
         first_area  = area;
      }
   }

   if (first == active->size)
      return;

   group = active->descs[first].layout_group;

   input_overlay_edit.count = 0;
   for (i = 0; i < active->size; i++)
   {
      struct overlay_desc *desc = &active->descs[i];

      if (desc->layout_group != group)
         continue;
      if (!input_overlay_edit_push((unsigned)index, (unsigned)i, desc))
         break;
      /* Lit as pressed while dragged */
      desc->touch_mask = 1;

      for (j = 0; j < ol->size; j++)
      {
         const struct overlay *page = &ol->overlays[j];
         if (j == index || !input_overlay_page_alike(page, active))
            continue;
         for (k = 0; k < page->size; k++)
            if (input_overlay_edit_same(&page->descs[k], desc))
               input_overlay_edit_push((unsigned)j, (unsigned)k,
                     &page->descs[k]);
      }
   }

   {
      float x0 = 1.0f, y0 = 1.0f, x1 = 0.0f, y1 = 0.0f;
      for (i = 0; i < active->size; i++)
      {
         const struct overlay_desc *desc = &active->descs[i];
         if (     desc->layout_group != group
               || !OVERLAY_HAS_IMAGE(&desc->image)
               || (desc->flags & OVERLAY_DESC_LAYOUT_HIDDEN))
            continue;
         x0 = MIN(x0, desc->mod_x);
         y0 = MIN(y0, desc->mod_y);
         x1 = MAX(x1, desc->mod_x + desc->mod_w);
         y1 = MAX(y1, desc->mod_y + desc->mod_h);
      }
      input_overlay_edit.mid_x = (x0 + x1) * 0.5f;
      input_overlay_edit.mid_y = (y0 + y1) * 0.5f;
   }

   input_overlay_edit.page     = (unsigned)index;
   input_overlay_edit.start_x  = x;
   input_overlay_edit.start_y  = y;
   input_overlay_edit.flags   |= OVERLAY_EDIT_DRAGGING;
}

static void input_overlay_edit_move(input_overlay_t *ol, float x, float y)
{
   size_t i;
   float dx = x - input_overlay_edit.start_x;
   float dy = y - input_overlay_edit.start_y;

   /* Short of this, a press is a tap */
   if (     !(input_overlay_edit.flags & OVERLAY_EDIT_MOVED)
         && fabs(dx) < 0.015f && fabs(dy) < 0.015f)
      return;
   input_overlay_edit.flags |= OVERLAY_EDIT_MOVED | OVERLAY_EDIT_CHANGED;

   /* The control's middle onto a grid corner */
   {
      unsigned dims = video_driver_get_output_dims();
      float w       = (float)VIDEO_SCALE_W(dims);
      float h       = (float)VIDEO_SCALE_H(dims);
      float cell    = MAX(w, h) / (OVERLAY_EDIT_GRID
            * (input_overlay_edit_bar.half ? 2 : 1));
      if (     cell >= 1.0f
            && ol->active->mod_w > 0.0f && ol->active->mod_h > 0.0f)
      {
         float mx = (input_overlay_edit.mid_x + dx * ol->active->mod_w) * w;
         float my = (input_overlay_edit.mid_y + dy * ol->active->mod_h) * h;
         mx = (float)floor(mx / cell + 0.5f) * cell;
         my = (float)floor(my / cell + 0.5f) * cell;
         dx = (mx / w - input_overlay_edit.mid_x) / ol->active->mod_w;
         dy = (my / h - input_overlay_edit.mid_y) / ol->active->mod_h;
      }
   }

   for (i = 0; i < input_overlay_edit.count; i++)
   {
      const input_overlay_edit_item_t *item = &input_overlay_edit.items[i];
      struct overlay *page;
      struct overlay_desc *desc;

      if (item->page >= ol->size)
         continue;
      page = &ol->overlays[item->page];
      if (item->desc >= page->size || page->mod_w <= 0.0f || page->mod_h <= 0.0f)
         continue;
      desc = &page->descs[item->desc];

      /* Where it is shown, separation and all, and from now on put
       * there as is: anywhere on screen, which a scaled page may not
       * cover */
      desc->x      = clamp_float(item->x + dx,
            -page->mod_x / page->mod_w, (1.0f - page->mod_x) / page->mod_w);
      desc->y      = clamp_float(item->y + dy,
            -page->mod_y / page->mod_h, (1.0f - page->mod_y) / page->mod_h);
      desc->flags |= OVERLAY_DESC_LAYOUT_EDITED | OVERLAY_DESC_LAYOUT_PLACED;
   }

   command_event(CMD_EVENT_OVERLAY_SET_SCALE_FACTOR, NULL);
}

/* No content runs behind the editor: the menu it came from comes back */
static void input_overlay_edit_return(void)
{
#ifdef HAVE_MENU
   if (     runloop_state_get_ptr()->current_core_type == CORE_TYPE_DUMMY
         && !(menu_state_get_ptr()->flags & MENU_ST_FLAG_ALIVE))
      retroarch_menu_running();
#endif
}


/* A control that can be added: control @group of page @page, or only
 * its desc @desc when that is not negative */
typedef struct
{
   size_t page;
   int desc;
   uint16_t group;
} input_overlay_addable_t;

/* A row of the list of controls of the page shown: control @have of
 * it, or when that is 0 one to add, as in input_overlay_addable_t */
typedef struct
{
   uint16_t page;
   int16_t desc;
   uint16_t group;
   uint16_t have;
} input_overlay_layout_entry_t;

static bool input_overlay_layout_entry(input_overlay_t *ol, unsigned i,
      input_overlay_layout_entry_t *out);
static bool input_overlay_layout_add_entry(input_overlay_t *ol,
      const input_overlay_layout_entry_t *e);
static bool input_overlay_layout_add_control(input_overlay_t *ol,
      const input_overlay_addable_t *add_in);

#ifdef HAVE_GFX_WIDGETS
/* Out of the editor into the menu: saved, or back to what was saved */
static void input_overlay_edit_leave(input_overlay_t *ol, bool save)
{
   bool changed = (input_overlay_edit.flags & OVERLAY_EDIT_CHANGED) != 0;

   if (!save)
      input_overlay_edit_undo_put_back();
   input_overlay_edit_finish(ol, save);
   if (!save && changed)
      input_overlay_layout_reload(ol);
#ifdef HAVE_MENU
   if (!(menu_state_get_ptr()->flags & MENU_ST_FLAG_ALIVE))
      retroarch_menu_running();
#endif
}

/* What the widgets draw of the bar, made again at every poll */
static gfx_overlay_bar_t input_overlay_edit_bar_drawn;

/* The box of what is drawn of @group of @page, in the page's units:
 * where the pack puts it and how big if @pack, else as it is shown */
static bool input_overlay_edit_group_box(const struct overlay *page,
      uint16_t group, bool pack, float *box)
{
   size_t i;
   bool any = false;

   box[0] = box[1] = box[2] = box[3] = 0.0f;
   for (i = 0; i < page->size; i++)
   {
      const struct overlay_desc *desc = &page->descs[i];
      float cx = pack ? desc->pack_x       : desc->x_shift;
      float cy = pack ? desc->pack_y       : desc->y_shift;
      float rx = pack ? desc->pack_range_x : desc->range_x;
      float ry = pack ? desc->pack_range_y : desc->range_y;
      if (     desc->layout_group != group
            || !OVERLAY_HAS_IMAGE(&desc->image))
         continue;
      if (!any)
      {
         box[0] = cx - rx;
         box[1] = cy - ry;
         box[2] = cx + rx;
         box[3] = cy + ry;
         any    = true;
         continue;
      }
      box[0] = MIN(box[0], cx - rx);
      box[1] = MIN(box[1], cy - ry);
      box[2] = MAX(box[2], cx + rx);
      box[3] = MAX(box[3], cy + ry);
   }
   return any;
}

/* How many times what the pack makes it @group is, across and down */
static void input_overlay_edit_group_scale(const struct overlay *page,
      uint16_t group, float *fx, float *fy)
{
   size_t i;
   *fx = *fy = 1.0f;
   for (i = 0; i < page->size; i++)
   {
      const struct overlay_desc *desc = &page->descs[i];
      if (     desc->layout_group == group
            && OVERLAY_HAS_IMAGE(&desc->image)
            && desc->pack_range_x > 0.0f && desc->pack_range_y > 0.0f)
      {
         *fx = desc->range_x / desc->pack_range_x;
         *fy = desc->range_y / desc->pack_range_y;
         return;
      }
   }
}

/* Sizes @group of the page shown to @fx and @fy times what the pack
 * makes it, spacing and all, about its middle; the same descs on the
 * pages of its shape along with it */
static void input_overlay_edit_resize(input_overlay_t *ol, uint16_t group,
      float fx, float fy)
{
   size_t i, j, k;
   float pack[4], now[4], pcx, pcy, ccx, ccy;
   size_t index         = (size_t)(ol->active - ol->overlays);
   struct overlay *page = &ol->overlays[index];

   if (     !input_overlay_edit_group_box(page, group, true, pack)
         || !input_overlay_edit_group_box(page, group, false, now))
      return;
   pcx = (pack[0] + pack[2]) * 0.5f;
   pcy = (pack[1] + pack[3]) * 0.5f;
   ccx = (now[0] + now[2]) * 0.5f;
   ccy = (now[1] + now[3]) * 0.5f;

   for (i = 0; i < page->size; i++)
   {
      struct overlay_desc *desc = &page->descs[i];
      float nx, ny;
      if (desc->layout_group != group)
         continue;
      nx = ccx + (desc->pack_x - pcx) * fx;
      ny = ccy + (desc->pack_y - pcy) * fy;

      for (j = 0; j < ol->size; j++)
      {
         struct overlay *other = &ol->overlays[j];
         if (j == index || !input_overlay_page_alike(other, page))
            continue;
         for (k = 0; k < other->size; k++)
         {
            struct overlay_desc *twin = &other->descs[k];
            if (!input_overlay_edit_same(twin, desc))
               continue;
            twin->x       = twin->x_shift + (nx - desc->x_shift);
            twin->y       = twin->y_shift + (ny - desc->y_shift);
            twin->range_x = twin->pack_range_x * fx;
            twin->range_y = twin->pack_range_y * fy;
            twin->flags  |= OVERLAY_DESC_LAYOUT_EDITED
               | OVERLAY_DESC_LAYOUT_PLACED | OVERLAY_DESC_LAYOUT_SIZE_EDITED;
         }
      }

      desc->x       = nx;
      desc->y       = ny;
      desc->range_x = desc->pack_range_x * fx;
      desc->range_y = desc->pack_range_y * fy;
      desc->flags  |= OVERLAY_DESC_LAYOUT_EDITED
         | OVERLAY_DESC_LAYOUT_PLACED | OVERLAY_DESC_LAYOUT_SIZE_EDITED;
   }

   input_overlay_edit.flags |= OVERLAY_EDIT_CHANGED | OVERLAY_EDIT_RESIZED;
   command_event(CMD_EVENT_OVERLAY_SET_SCALE_FACTOR, NULL);
}

/* The game's screen as the page shown places it, or where the video
 * driver has it, as x, y, w and h fractions of the screen */
static bool input_overlay_edit_screen(const input_overlay_t *ol, float *box)
{
   struct video_viewport vp;

   if (ol->active->flags & OVERLAY_HAS_VIEWPORT)
   {
      box[0] = ol->active->viewport.x;
      box[1] = ol->active->viewport.y;
      box[2] = ol->active->viewport.w;
      box[3] = ol->active->viewport.h;
      return true;
   }
   if (     !video_driver_get_viewport_info(&vp)
         || !VIDEO_SCALE_W(vp.full_dims) || !VIDEO_SCALE_H(vp.full_dims)
         || !VIDEO_SCALE_W(vp.dims) || !VIDEO_SCALE_H(vp.dims))
      return false;
   box[0] = (float)VIDEO_POS_X(vp.pos) / VIDEO_SCALE_W(vp.full_dims);
   box[1] = (float)VIDEO_POS_Y(vp.pos) / VIDEO_SCALE_H(vp.full_dims);
   box[2] = (float)VIDEO_SCALE_W(vp.dims) / VIDEO_SCALE_W(vp.full_dims);
   box[3] = (float)VIDEO_SCALE_H(vp.dims) / VIDEO_SCALE_H(vp.full_dims);
   return true;
}

/* Places the game's screen at @box on the pages of the shape of the
 * page shown, or, @box NULL, leaves it where the video driver puts it */
static void input_overlay_edit_set_screen(input_overlay_t *ol,
      const float *box)
{
   size_t j;
   for (j = 0; j < ol->size; j++)
   {
      struct overlay *page = &ol->overlays[j];
      if (!input_overlay_page_alike(page, ol->active))
         continue;
      if (box)
      {
         page->viewport.x = clamp_float(box[0], 0.0f, 1.0f - box[2]);
         page->viewport.y = clamp_float(box[1], 0.0f, 1.0f - box[3]);
         page->viewport.w = box[2];
         page->viewport.h = box[3];
         page->flags     |= OVERLAY_HAS_VIEWPORT;
      }
      else
         page->flags     &= ~OVERLAY_HAS_VIEWPORT;
      page->flags |= OVERLAY_LAYOUT_VIEWPORT_EDITED;
   }
   input_overlay_edit.flags |= OVERLAY_EDIT_CHANGED;
   input_overlay_viewport_publish();
   command_event(CMD_EVENT_VIDEO_SET_ASPECT_RATIO, NULL);
}

/* Grid cells, square, the longer side of the screen being
 * OVERLAY_EDIT_GRID of them, in pixels; and the screen's size */
static float input_overlay_edit_cell(float *w, float *h)
{
   unsigned dims = video_driver_get_output_dims();
   *w = (float)VIDEO_SCALE_W(dims);
   *h = (float)VIDEO_SCALE_H(dims);
   return MAX(*w, *h) / OVERLAY_EDIT_GRID;
}

#define OVERLAY_EDIT_SIZE_MIN    1.0f
#define OVERLAY_EDIT_SIZE_MAX   12.0f
#define OVERLAY_EDIT_SIZE_STEP   0.5f
#define OVERLAY_EDIT_SHAPE_STEPS 4
#define OVERLAY_EDIT_SCREEN_MIN  2

/* The size, in cells, of what is drawn of @group (its longer side),
 * and its shape: the steps it is wider (or, negative, taller) than the
 * pack makes it, 2^(1/OVERLAY_EDIT_SHAPE_STEPS) times a step */
static bool input_overlay_edit_group_measure(const struct overlay *page,
      uint16_t group, float *pack_w, float *pack_h, float *size,
      float *shape)
{
   float pack[4], fx, fy, w, h;
   float cell = input_overlay_edit_cell(&w, &h);

   if (     cell < 1.0f
         || !input_overlay_edit_group_box(page, group, true, pack))
      return false;
   input_overlay_edit_group_scale(page, group, &fx, &fy);
   *pack_w = (pack[2] - pack[0]) * page->mod_w * w;
   *pack_h = (pack[3] - pack[1]) * page->mod_h * h;
   *size   = MAX(*pack_w * fx, *pack_h * fy) / cell;
   *shape  = (fy > 0.0f) ? OVERLAY_EDIT_SHAPE_STEPS
      * (float)(log(fx / fy) / log(2.0)) : 0.0f;
   return *pack_w > 0.0f && *pack_h > 0.0f;
}

/* Sets slider @i of the selection to notch @notch */
static void input_overlay_edit_slide(input_overlay_t *ol, unsigned i,
      unsigned notch)
{
   float w, h;
   float cell = input_overlay_edit_cell(&w, &h);

   if (cell < 1.0f)
      return;

   if (input_overlay_edit.flags & OVERLAY_EDIT_SEL_SCREEN)
   {
      float box[4];
      if (!input_overlay_edit_screen(ol, box))
         return;
      if (i == 0)
         box[2] = MIN(1.0f, (OVERLAY_EDIT_SCREEN_MIN + notch) * cell / w);
      else
         box[3] = MIN(1.0f, (OVERLAY_EDIT_SCREEN_MIN + notch) * cell / h);
      input_overlay_edit_set_screen(ol, box);
   }
   else
   {
      float pack_w, pack_h, size, shape, ratio, f, fx, fy;
      uint16_t group = input_overlay_edit.sel_group;
      if (!input_overlay_edit_group_measure(ol->active, group,
               &pack_w, &pack_h, &size, &shape))
         return;
      if (i == 0)
         size  = OVERLAY_EDIT_SIZE_MIN + notch * OVERLAY_EDIT_SIZE_STEP;
      else
         shape = (float)notch - OVERLAY_EDIT_SHAPE_STEPS;
      ratio = (float)pow(2.0, shape / OVERLAY_EDIT_SHAPE_STEPS);
      /* Across ratio times what it is down, the longer side size */
      f     = size * cell / MAX(pack_w * ratio, pack_h);
      fx    = f * ratio;
      fy    = f;
      input_overlay_edit_resize(ol, group, fx, fy);
   }
}

/* Slider @i of the selection: its notches, where it is and its value */
static unsigned input_overlay_edit_slider(input_overlay_t *ol, unsigned i,
      char *label, size_t label_len, char *value, size_t value_len,
      float *pos)
{
   float w, h;
   char num[16];
   unsigned notches = 0;
   float cell       = input_overlay_edit_cell(&w, &h);

   *label = *value = '\0';
   *pos   = 0.0f;
   if (cell < 1.0f)
      return 0;

   if (input_overlay_edit.flags & OVERLAY_EDIT_SEL_SCREEN)
   {
      float box[4], cells;
      unsigned most = (unsigned)((i == 0 ? w : h) / cell);
      if (!input_overlay_edit_screen(ol, box) || most <= OVERLAY_EDIT_SCREEN_MIN)
         return 0;
      notches = most - OVERLAY_EDIT_SCREEN_MIN + 1;
      cells   = box[i == 0 ? 2 : 3] * (i == 0 ? w : h) / cell;
      *pos    = clamp_float((cells - OVERLAY_EDIT_SCREEN_MIN)
            / (most - OVERLAY_EDIT_SCREEN_MIN), 0.0f, 1.0f);
      strlcpy(label, msg_hash_to_str(i == 0
               ? MSG_OVERLAY_EDIT_WIDTH : MSG_OVERLAY_EDIT_HEIGHT), label_len);
      snprintf(num, sizeof(num), "%u", (unsigned)(cells + 0.5f));
      snprintf(value, value_len, msg_hash_to_str(MSG_OVERLAY_EDIT_CELLS), num);
   }
   else
   {
      float pack_w, pack_h, size, shape;
      if (!input_overlay_edit_group_measure(ol->active,
               input_overlay_edit.sel_group, &pack_w, &pack_h, &size, &shape))
         return 0;
      if (i == 0)
      {
         float half = (float)floor(size / OVERLAY_EDIT_SIZE_STEP + 0.5f)
            * OVERLAY_EDIT_SIZE_STEP;
         notches = (unsigned)((OVERLAY_EDIT_SIZE_MAX - OVERLAY_EDIT_SIZE_MIN)
               / OVERLAY_EDIT_SIZE_STEP) + 1;
         *pos    = clamp_float((size - OVERLAY_EDIT_SIZE_MIN)
               / (OVERLAY_EDIT_SIZE_MAX - OVERLAY_EDIT_SIZE_MIN), 0.0f, 1.0f);
         strlcpy(label, msg_hash_to_str(MSG_OVERLAY_EDIT_SIZE), label_len);
         if (half == (float)(int)half)
            snprintf(num, sizeof(num), "%d", (int)half);
         else
            snprintf(num, sizeof(num), "%.1f", half);
         snprintf(value, value_len, msg_hash_to_str(MSG_OVERLAY_EDIT_CELLS), num);
      }
      else
      {
         int step = (int)floor(shape + 0.5f);
         notches  = 2 * OVERLAY_EDIT_SHAPE_STEPS + 1;
         *pos     = clamp_float((shape + OVERLAY_EDIT_SHAPE_STEPS)
               / (2 * OVERLAY_EDIT_SHAPE_STEPS), 0.0f, 1.0f);
         strlcpy(label, msg_hash_to_str(MSG_OVERLAY_EDIT_SHAPE), label_len);
         if (step == 0)
            strlcpy(value, msg_hash_to_str(MSG_OVERLAY_EDIT_SHAPE_ORIGINAL),
                  value_len);
         else
            snprintf(value, value_len, msg_hash_to_str(step > 0
                     ? MSG_OVERLAY_EDIT_SHAPE_WIDER
                     : MSG_OVERLAY_EDIT_SHAPE_TALLER), step > 0 ? step : -step);
      }
   }
   return notches;
}

/* The notch of slider @i nearest the finger at @px */
static void input_overlay_edit_slide_to(input_overlay_t *ol, unsigned i,
      float px)
{
   int x, y, w, h;
   unsigned dims    = video_driver_get_output_dims();
   unsigned notches = input_overlay_edit_bar_drawn.sel_notches[i];
   float t;

   if (     notches < 2
         || !gfx_widgets_overlay_bar_rect(dims, &input_overlay_edit_bar_drawn,
            GFX_OVERLAY_BAR_SEL_TRACK0 + i, &x, &y, &w, &h)
         || w <= h / 2)
      return;
   /* As the widgets draw it: inset a quarter of its height each end */
   t = clamp_float((px - (x + h / 4)) / (float)(w - h / 2), 0.0f, 1.0f);
   input_overlay_edit_slide(ol, i,
         (unsigned)floor(t * (notches - 1) + 0.5f));
}

static void input_overlay_edit_deselect(void)
{
   input_overlay_edit.flags &= ~(OVERLAY_EDIT_SELECTED
         | OVERLAY_EDIT_SEL_SCREEN);
   input_overlay_edit.sel_group = 0;
}


/* Rows the panel shows, each @row_h high */
static unsigned input_overlay_edit_panel_rows(const gfx_overlay_bar_t *bar,
      int *row_h)
{
   int x, y, w, h;
   unsigned dims = video_driver_get_output_dims();

   *row_h = 0;
   if (     !gfx_widgets_overlay_bar_rect(dims, bar, GFX_OVERLAY_BAR_PANEL,
            &x, &y, &w, &h)
         || !gfx_widgets_overlay_bar_rect(dims, bar,
            GFX_OVERLAY_BAR_PANEL_DONE, &x, &y, &w, row_h)
         || *row_h <= 0)
      return 0;
   return (unsigned)(h / *row_h) - 1;
}

static bool input_overlay_page_has_analog(const struct overlay *page)
{
   size_t i;
   for (i = 0; i < page->size; i++)
      if (     page->descs[i].type == OVERLAY_TYPE_ANALOG_LEFT
            || page->descs[i].type == OVERLAY_TYPE_ANALOG_RIGHT)
         return true;
   return false;
}

/* The page the active one's own button switches to for the analog
 * stick, or back from it, or -1: of those that differ from the active
 * page in having a stick, the fullest, so not a page that only hides
 * the pad */
static int input_overlay_edit_analog_page(const input_overlay_t *ol)
{
   size_t i;
   int found      = -1;
   bool analog    = input_overlay_page_has_analog(ol->active);

   for (i = 0; i < ol->active->size; i++)
   {
      const struct overlay_desc *desc = &ol->active->descs[i];
      unsigned t                      = desc->next_index;
      if (     desc->type != OVERLAY_TYPE_BUTTONS
            || !BIT256_GET(desc->button_mask, RARCH_OVERLAY_NEXT)
            || t >= ol->size
            || &ol->overlays[t] == ol->active
            || input_overlay_page_has_analog(&ol->overlays[t]) == analog)
         continue;
      if (found < 0 || ol->overlays[t].size > ol->overlays[found].size)
         found = (int)t;
   }
   return found;
}

static gfx_overlay_bar_t *input_overlay_edit_bar_make(input_overlay_t *ol)
{
   unsigned i, rows;
   int row_h;
   gfx_overlay_bar_t *bar = &input_overlay_edit_bar_drawn;
   unsigned count         = input_overlay_layout_control_count();

   if (input_overlay_edit_bar.x < 0.0f)
   {
      /* Middle of the top, to start with */
      int x, y, w, h;
      unsigned dims = video_driver_get_output_dims();
      bar->x        = 0.0f;
      bar->y        = 0.0f;
      bar->flags    = GFX_OVERLAY_BAR_FLAG_SHOWN;
      if (     VIDEO_SCALE_W(dims)
            && gfx_widgets_overlay_bar_rect(dims, bar, GFX_OVERLAY_BAR_BAR,
               &x, &y, &w, &h))
      {
         input_overlay_edit_bar.x = (1.0f - (float)w / VIDEO_SCALE_W(dims)) * 0.5f;
         input_overlay_edit_bar.y = 0.04f;
      }
   }

   bar->x       = input_overlay_edit_bar.x;
   bar->y       = input_overlay_edit_bar.y;
   bar->pressed = input_overlay_edit.bar_item;
   bar->count   = MIN(count, GFX_OVERLAY_BAR_CONTROLS);
   bar->first   = 0;
   bar->flags   = GFX_OVERLAY_BAR_FLAG_SHOWN;
   if (input_overlay_edit_bar.half)
      bar->flags |= GFX_OVERLAY_BAR_FLAG_HALF_GRID;
   if (input_overlay_page_has_analog(ol->active))
      bar->flags |= GFX_OVERLAY_BAR_FLAG_ANALOG;
   if (input_overlay_edit_analog_page(ol) < 0)
      bar->flags |= GFX_OVERLAY_BAR_FLAG_ANALOG_OFF;
   if (input_overlay_edit.flags & OVERLAY_EDIT_PANEL)
   {
      bar->flags |= GFX_OVERLAY_BAR_FLAG_PANEL_OPEN;
      /* No further than the last row at the bottom */
      rows = input_overlay_edit_panel_rows(bar, &row_h);
      if (input_overlay_edit.panel_first + rows > bar->count)
         input_overlay_edit.panel_first = (bar->count > rows)
            ? bar->count - rows : 0;
      bar->first = input_overlay_edit.panel_first;
   }
   for (i = 0; i < bar->count; i++)
   {
      bool shown = false;
      input_overlay_layout_control_get(i, bar->names[i],
            sizeof(bar->names[i]), &shown);
      bar->shown[i] = shown;
   }

   if (!input_overlay_edit_screen(ol, bar->screen))
      bar->screen[2] = bar->screen[3] = 0.0f;
   bar->sel[2] = bar->sel[3] = 0.0f;
   if (input_overlay_edit.flags & OVERLAY_EDIT_SELECTED)
   {
      bar->flags |= GFX_OVERLAY_BAR_FLAG_SELECTED;
      if (input_overlay_edit.flags & OVERLAY_EDIT_SEL_SCREEN)
      {
         bar->flags |= GFX_OVERLAY_BAR_FLAG_SEL_SCREEN;
         strlcpy(bar->sel_name, msg_hash_to_str(MSG_OVERLAY_EDIT_SCREEN),
               sizeof(bar->sel_name));
      }
      else
      {
         float x0 = 1.0f, y0 = 1.0f, x1 = 0.0f, y1 = 0.0f;
         for (i = 0; i < ol->active->size; i++)
         {
            const struct overlay_desc *desc = &ol->active->descs[i];
            if (     desc->layout_group != input_overlay_edit.sel_group
                  || !OVERLAY_HAS_IMAGE(&desc->image))
               continue;
            x0 = MIN(x0, desc->mod_x);
            y0 = MIN(y0, desc->mod_y);
            x1 = MAX(x1, desc->mod_x + desc->mod_w);
            y1 = MAX(y1, desc->mod_y + desc->mod_h);
         }
         if (x1 > x0 && y1 > y0)
         {
            bar->sel[0] = x0;
            bar->sel[1] = y0;
            bar->sel[2] = x1 - x0;
            bar->sel[3] = y1 - y0;
         }
         input_overlay_layout_name(ol->active, input_overlay_edit.sel_group,
               bar->sel_name, sizeof(bar->sel_name));
      }
      for (i = 0; i < 2; i++)
         bar->sel_notches[i] = input_overlay_edit_slider(ol, i,
               bar->sel_label[i], sizeof(bar->sel_label[i]),
               bar->sel_value[i], sizeof(bar->sel_value[i]),
               &bar->sel_pos[i]);
   }
   return bar;
}

/* The part of the bar at @x, @y (pixels), or -1: the panel's over the
 * bar's, either one's own background last */
static int input_overlay_edit_bar_hit(float px, float py)
{
   static const unsigned order[] = {
      GFX_OVERLAY_BAR_PANEL_DONE, GFX_OVERLAY_BAR_PANEL,
      GFX_OVERLAY_BAR_SEL_RESET, GFX_OVERLAY_BAR_SEL_DONE,
      GFX_OVERLAY_BAR_SEL_ROW0, GFX_OVERLAY_BAR_SEL_ROW1,
      GFX_OVERLAY_BAR_SEL, GFX_OVERLAY_BAR_GRIP, GFX_OVERLAY_BAR_SAVE, GFX_OVERLAY_BAR_CANCEL,
      GFX_OVERLAY_BAR_CONTROLS_BUTTON, GFX_OVERLAY_BAR_HALF,
      GFX_OVERLAY_BAR_ANALOG, GFX_OVERLAY_BAR_BAR };
   unsigned i;
   int x, y, w, h;
   unsigned dims          = video_driver_get_output_dims();
   gfx_overlay_bar_t *bar = &input_overlay_edit_bar_drawn;

   if (!gfx_widgets_ready() || !(bar->flags & GFX_OVERLAY_BAR_FLAG_SHOWN))
      return -1;

   for (i = GFX_OVERLAY_BAR_ROW; i < GFX_OVERLAY_BAR_ITEMS; i++)
      if (     gfx_widgets_overlay_bar_rect(dims, bar, i, &x, &y, &w, &h)
            && px >= x && px < x + w && py >= y && py < y + h)
         return (int)i;
   for (i = 0; i < ARRAY_SIZE(order); i++)
      if (     gfx_widgets_overlay_bar_rect(dims, bar, order[i], &x, &y, &w, &h)
            && px >= x && px < x + w && py >= y && py < y + h)
         return (int)order[i];
   return -1;
}

/* Adds @e while editing: what was laid out so far is written first,
 * the overlay loads again with it and the editor goes on over it. The
 * layout as it was is kept for Cancel. True, as @ol is gone. */
static void input_overlay_edit_reload(input_overlay_t *ol)
{
   uint16_t flags = input_overlay_edit.flags;
   unsigned first = input_overlay_edit.panel_first;
   uint16_t group = input_overlay_edit.sel_group;
   unsigned page  = input_overlay_edit.page;

   /* Not editing while it reloads, so that nothing finishes */
   input_overlay_edit_drop(ol);
   free(input_overlay_edit.items);
   memset(&input_overlay_edit, 0, sizeof(input_overlay_edit));
   input_overlay_layout_reload(ol);

   input_overlay_edit.flags       = OVERLAY_EDIT_ACTIVE | OVERLAY_EDIT_CHANGED
         | (flags & (OVERLAY_EDIT_PAUSED | OVERLAY_EDIT_PANEL
                  | OVERLAY_EDIT_SELECTED | OVERLAY_EDIT_SEL_SCREEN));
   input_overlay_edit.panel_first = first;
   input_overlay_edit.sel_group   = group;
   /* The page it reloads on, so that the selection stays */
   input_overlay_edit.page        = page;
   input_overlay_edit.bar_item    = -1;
}

/* What was laid out so far is written to the layout, the layout as it
 * was kept for Cancel, and the overlay loaded again from it: so that a
 * control added or resized is made anew. @ol is gone after. */
static void input_overlay_edit_commit(input_overlay_t *ol)
{
   input_overlay_edit_undo_take(ol);
   input_overlay_layout_save(ol);
   input_overlay_edit_reload(ol);
}

static bool input_overlay_edit_add(input_overlay_t *ol,
      const input_overlay_layout_entry_t *e)
{
   input_overlay_edit_undo_take(ol);
   input_overlay_layout_save(ol);
   if (!input_overlay_layout_add_entry(ol, e))
      return false;
   input_overlay_edit_reload(ol);
   return true;
}

/* A tap on @item of the bar. True when it ended the editing, or @ol is
 * no more. */
static bool input_overlay_edit_bar_tap(input_overlay_t *ol, int item)
{
   switch (item)
   {
      case GFX_OVERLAY_BAR_SAVE:
         input_overlay_edit_leave(ol, true);
         return true;
      case GFX_OVERLAY_BAR_CANCEL:
         input_overlay_edit_leave(ol, false);
         return true;
      case GFX_OVERLAY_BAR_CONTROLS_BUTTON:
         input_overlay_edit.flags      ^= OVERLAY_EDIT_PANEL;
         input_overlay_edit.panel_first = 0;
         input_overlay_edit_deselect();
         break;
      case GFX_OVERLAY_BAR_SEL_DONE:
         input_overlay_edit_deselect();
         break;
      case GFX_OVERLAY_BAR_SEL_RESET:
         if (input_overlay_edit.flags & OVERLAY_EDIT_SEL_SCREEN)
         {
            /* The game's own shape at the width set: no bars in it */
            float w, h, box[4];
            float aspect = video_driver_get_aspect_ratio();
            input_overlay_edit_cell(&w, &h);
            if (     aspect > 0.0f && w > 0.0f && h > 0.0f
                  && input_overlay_edit_screen(ol, box))
            {
               box[3] = box[2] * w / aspect / h;
               if (box[3] > 1.0f)
               {
                  box[3] = 1.0f;
                  box[2] = aspect * h / w;
               }
               if (box[1] + box[3] > 1.0f)
                  box[1] = 1.0f - box[3];
               input_overlay_edit_set_screen(ol, box);
            }
         }
         else if (input_overlay_edit.flags & OVERLAY_EDIT_SELECTED)
         {
            input_overlay_edit_resize(ol, input_overlay_edit.sel_group,
                  1.0f, 1.0f);
            input_overlay_edit.flags &= ~OVERLAY_EDIT_RESIZED;
            input_overlay_edit_commit(ol);
            return true;
         }
         break;
      case GFX_OVERLAY_BAR_HALF:
         input_overlay_edit_bar.half = !input_overlay_edit_bar.half;
         break;
      case GFX_OVERLAY_BAR_PANEL_DONE:
         input_overlay_edit.flags &= ~OVERLAY_EDIT_PANEL;
         break;
      case GFX_OVERLAY_BAR_ANALOG:
         {
            int page = input_overlay_edit_analog_page(ol);
            if (page >= 0)
            {
               /* As the page's own button, which leaves the rotation be */
               bool rotate    = false;
               ol->next_index = (unsigned)page;
               command_event(CMD_EVENT_OVERLAY_NEXT, &rotate);
            }
         }
         break;
      default:
         /* A control on or off, saved or not with the rest */
         if (item >= GFX_OVERLAY_BAR_ROW && item < GFX_OVERLAY_BAR_ITEMS)
         {
            bool shown;
            input_overlay_layout_entry_t e;
            unsigned i = input_overlay_edit.panel_first
                  + (unsigned)(item - GFX_OVERLAY_BAR_ROW);
            if (     !input_overlay_layout_entry(ol, i, &e)
                  || !input_overlay_layout_control_get(i, NULL, 0, &shown))
               break;
            if (e.have)
            {
               input_overlay_layout_set_hidden(ol, e.have, shown);
               input_overlay_edit.flags |= OVERLAY_EDIT_CHANGED;
            }
            else
               return input_overlay_edit_add(ol, &e);
         }
         break;
   }
   return false;
}
#endif

void input_overlay_edit_poll(input_overlay_t *ol, float opacity)
{
   input_driver_state_t *input_st    = &input_driver_st;
   input_driver_t *input             = (input_driver_t*)input_st->current_driver;
   void *input_data                  = input_st->current_data;
   const video_views_layout_t *views = video_driver_get_views_layout();
   unsigned index                    = (unsigned)(ol->active - ol->overlays);
   bool pressed                      = false;
   float x                           = 0.0f;
   float y                           = 0.0f;
   float sx                          = 0.0f;
   float sy                          = 0.0f;
   float base                        = (opacity < 0.5f) ? 0.5f : opacity;
   unsigned dims                     = video_driver_get_output_dims();

   /* The pack was replaced under the drag */
   if (     input_overlay_edit.ol != ol
         || input_overlay_edit.page != index)
   {
      input_overlay_edit_drop(ol);
#ifdef HAVE_GFX_WIDGETS
      if (input_overlay_edit.page != index)
         input_overlay_edit_deselect();
#endif
      input_overlay_edit.ol   = ol;
      input_overlay_edit.page = index;
   }

   input_st->flags |= INP_FLAG_BLOCK_POINTER_INPUT;

   if (     input->input_state
         && input_data
         && ol->active->mod_w > 0.0f
         && ol->active->mod_h > 0.0f)
   {
      rarch_joypad_info_t joypad_info;
      unsigned device = ((ol->active->flags & OVERLAY_FULL_SCREEN) || views)
         ? RARCH_DEVICE_POINTER_SCREEN
         : RETRO_DEVICE_POINTER;

      joypad_info.joy_idx        = 0;
      joypad_info.auto_binds     = NULL;
      joypad_info.axis_threshold = 0.0f;

      if (input_frame_or_driver(input, input_data, input_st->primary_joypad,
               &joypad_info, NULL, true, 0, device, 0,
               RETRO_DEVICE_ID_POINTER_PRESSED))
      {
         int16_t px = input_frame_or_driver(input, input_data,
               input_st->primary_joypad, &joypad_info, NULL, true, 0,
               device, 0, RETRO_DEVICE_ID_POINTER_X);
         int16_t py = input_frame_or_driver(input, input_data,
               input_st->primary_joypad, &joypad_info, NULL, true, 0,
               device, 0, RETRO_DEVICE_ID_POINTER_Y);
         pressed    = true;
         sx         = (float)(px + 0x7fff) / 0xffff;
         sy         = (float)(py + 0x7fff) / 0xffff;
         x          = (sx - ol->active->mod_x) / ol->active->mod_w;
         y          = (sy - ol->active->mod_y) / ol->active->mod_h;
         sx        *= VIDEO_SCALE_W(dims);
         sy        *= VIDEO_SCALE_H(dims);
      }
   }

   if (pressed)
   {
      input_overlay_edit.ptr_x = sx;
      input_overlay_edit.ptr_y = sy;
      if (!(input_overlay_edit.flags & OVERLAY_EDIT_HELD))
      {
         int item = -1;
         input_overlay_edit.flags |= OVERLAY_EDIT_HELD;
#ifdef HAVE_GFX_WIDGETS
         item = input_overlay_edit_bar_hit(sx, sy);
#endif
         if (item < 0)
         {
            input_overlay_edit_grab(ol, x, y);
#ifdef HAVE_GFX_WIDGETS
            /* Not on a control: on the game's screen, perhaps */
            if (     !(input_overlay_edit.flags & OVERLAY_EDIT_DRAGGING)
                  && VIDEO_SCALE_W(dims) && VIDEO_SCALE_H(dims))
            {
               float box[4];
               float fx = sx / VIDEO_SCALE_W(dims);
               float fy = sy / VIDEO_SCALE_H(dims);
               if (     input_overlay_edit_screen(ol, box)
                     && fx >= box[0] && fx < box[0] + box[2]
                     && fy >= box[1] && fy < box[1] + box[3])
               {
                  input_overlay_edit.flags   |= OVERLAY_EDIT_SCREEN_HELD;
                  input_overlay_edit.screen_x = box[0];
                  input_overlay_edit.screen_y = box[1];
                  input_overlay_edit.grip_x   = sx;
                  input_overlay_edit.grip_y   = sy;
               }
            }
#endif
         }
#ifdef HAVE_GFX_WIDGETS
         else if (     item == GFX_OVERLAY_BAR_SEL_ROW0
                  || item == GFX_OVERLAY_BAR_SEL_ROW1)
         {
            input_overlay_edit.bar_item = item;
            input_overlay_edit_slide_to(ol,
                  (unsigned)(item - GFX_OVERLAY_BAR_SEL_ROW0), sx);
         }
         else if (item == GFX_OVERLAY_BAR_GRIP && VIDEO_SCALE_W(dims))
         {
            input_overlay_edit.flags |= OVERLAY_EDIT_GRIP;
            input_overlay_edit.grip_x = sx
               - input_overlay_edit_bar.x * VIDEO_SCALE_W(dims);
            input_overlay_edit.grip_y = sy
               - input_overlay_edit_bar.y * VIDEO_SCALE_H(dims);
         }
#endif
         else
         {
            input_overlay_edit.bar_item     = item;
            input_overlay_edit.scroll_y     = sy;
            input_overlay_edit.scroll_first = input_overlay_edit.panel_first;
         }
      }
      else if (input_overlay_edit.flags & OVERLAY_EDIT_GRIP)
      {
         input_overlay_edit_bar.x = (sx - input_overlay_edit.grip_x)
            / VIDEO_SCALE_W(dims);
         input_overlay_edit_bar.y = (sy - input_overlay_edit.grip_y)
            / VIDEO_SCALE_H(dims);
      }
#ifdef HAVE_GFX_WIDGETS
      else if (     input_overlay_edit.bar_item == GFX_OVERLAY_BAR_SEL_ROW0
               || input_overlay_edit.bar_item == GFX_OVERLAY_BAR_SEL_ROW1)
         input_overlay_edit_slide_to(ol, (unsigned)(input_overlay_edit.bar_item
                  - GFX_OVERLAY_BAR_SEL_ROW0), sx);
      else if (input_overlay_edit.flags & OVERLAY_EDIT_SCREEN_HELD)
      {
         /* Once selected, the screen moves with the finger, its top
          * left on a grid corner */
         float w, h, box[4];
         float cell = input_overlay_edit_cell(&w, &h)
            / (input_overlay_edit_bar.half ? 2 : 1);
         float dx   = sx - input_overlay_edit.grip_x;
         float dy   = sy - input_overlay_edit.grip_y;
         if (     (input_overlay_edit.flags & OVERLAY_EDIT_SEL_SCREEN)
               && cell >= 1.0f
               && (     (input_overlay_edit.flags & OVERLAY_EDIT_SCREEN_MOVED)
                     || fabs(dx) > 0.015f * w || fabs(dy) > 0.015f * h)
               && input_overlay_edit_screen(ol, box))
         {
            input_overlay_edit.flags |= OVERLAY_EDIT_SCREEN_MOVED;
            box[0] = (float)floor((input_overlay_edit.screen_x * w + dx)
                  / cell + 0.5f) * cell / w;
            box[1] = (float)floor((input_overlay_edit.screen_y * h + dy)
                  / cell + 0.5f) * cell / h;
            input_overlay_edit_set_screen(ol, box);
         }
      }
      else if (     (input_overlay_edit.flags & OVERLAY_EDIT_SCROLL)
               || input_overlay_edit.bar_item >= GFX_OVERLAY_BAR_ROW
               || input_overlay_edit.bar_item == GFX_OVERLAY_BAR_PANEL)
      {
         /* A drag up or down the panel scrolls it, a row a row height;
          * past a third of one it is no longer a tap */
         int row_h;
         unsigned rows = input_overlay_edit_panel_rows(
               &input_overlay_edit_bar_drawn, &row_h);
         float d       = sy - input_overlay_edit.scroll_y;
         if (     !(input_overlay_edit.flags & OVERLAY_EDIT_SCROLL)
               && row_h > 0
               && (d > row_h / 3.0f || d < -row_h / 3.0f))
         {
            input_overlay_edit.flags   |= OVERLAY_EDIT_SCROLL;
            input_overlay_edit.bar_item = -1;
         }
         if ((input_overlay_edit.flags & OVERLAY_EDIT_SCROLL) && row_h > 0)
         {
            unsigned count = input_overlay_edit_bar_drawn.count;
            int last       = (count > rows) ? (int)(count - rows) : 0;
            int first      = (int)input_overlay_edit.scroll_first
               - (int)floorf(d / row_h + 0.5f);
            input_overlay_edit.panel_first = (unsigned)MAX(0, MIN(first, last));
         }
      }
#endif
      else if (input_overlay_edit.flags & OVERLAY_EDIT_DRAGGING)
         input_overlay_edit_move(ol, x, y);
   }
   else if (input_overlay_edit.flags & OVERLAY_EDIT_HELD)
   {
      int item       = input_overlay_edit.bar_item;
      uint16_t flags = input_overlay_edit.flags;
      uint16_t group = 0;

      if (     (flags & OVERLAY_EDIT_DRAGGING)
            && input_overlay_edit.count
            && input_overlay_edit.items[0].page == index
            && input_overlay_edit.items[0].desc < ol->active->size)
         group = ol->active->descs[input_overlay_edit.items[0].desc]
            .layout_group;

      /* Only placed: a control does nothing while it is laid out */
      input_overlay_edit_drop(ol);
      input_overlay_edit.bar_item = -1;
      input_overlay_edit.flags   &= ~(OVERLAY_EDIT_HELD | OVERLAY_EDIT_GRIP
            | OVERLAY_EDIT_SCROLL | OVERLAY_EDIT_SCREEN_HELD
            | OVERLAY_EDIT_SCREEN_MOVED);

#ifdef HAVE_GFX_WIDGETS
      /* The bar's parts act on a lift over them */
      if (     item >= 0
            && input_overlay_edit_bar_hit(input_overlay_edit.ptr_x,
               input_overlay_edit.ptr_y) == item
            && input_overlay_edit_bar_tap(ol, item))
         return;

      /* Sized: drawn again at its new size */
      if (input_overlay_edit.flags & OVERLAY_EDIT_RESIZED)
      {
         input_overlay_edit.flags &= ~OVERLAY_EDIT_RESIZED;
         input_overlay_edit_commit(ol);
         return;
      }

      /* A tap selects the control or the screen it is on, and nothing
       * when on neither */
      if (item < 0 && !(flags & (OVERLAY_EDIT_MOVED
                  | OVERLAY_EDIT_SCREEN_MOVED)))
      {
         input_overlay_edit_deselect();
         if (group)
         {
            input_overlay_edit.flags    |= OVERLAY_EDIT_SELECTED;
            input_overlay_edit.sel_group = group;
         }
         else if (flags & OVERLAY_EDIT_SCREEN_HELD)
            input_overlay_edit.flags    |= OVERLAY_EDIT_SELECTED
               | OVERLAY_EDIT_SEL_SCREEN;
         if (input_overlay_edit.flags & OVERLAY_EDIT_SELECTED)
            input_overlay_edit.flags    &= ~OVERLAY_EDIT_PANEL;
      }
#else
      (void)item;
      (void)group;
#endif
   }

   /* Back finishes, as the hint says. Android sends it to the menu's
    * own back, not to the menu toggle the run loop finishes on. Taken
    * on release, so that nothing sees it held once editing is over. */
   if (input_driver_device_state(0, RARCH_DEVICE_POINTER_SCREEN, 0,
            RARCH_DEVICE_ID_POINTER_BACK))
   {
      if (!(input_overlay_edit.flags & OVERLAY_EDIT_BACK))
         input_overlay_edit.flags |= OVERLAY_EDIT_BACK | OVERLAY_EDIT_BACK_NEW;
   }
   else if (input_overlay_edit.flags & OVERLAY_EDIT_BACK)
   {
      bool done = (input_overlay_edit.flags & OVERLAY_EDIT_BACK_NEW) != 0;
      input_overlay_edit.flags &= ~(OVERLAY_EDIT_BACK | OVERLAY_EDIT_BACK_NEW);
      if (done)
      {
         input_overlay_edit_end();
         input_overlay_alpha_pass(ol, opacity, false, opacity,
               input_st->overlay_leds_lit, input_overlay_led_map());
         return;
      }
   }

   /* While dragging, the rest dims and the group lights up */
   if (input_overlay_edit.flags & OVERLAY_EDIT_DRAGGING)
      input_overlay_alpha_pass(ol, base * 0.5f, true, 1.0f,
            input_st->overlay_leds_lit, input_overlay_led_map());
   else
      input_overlay_alpha_pass(ol, base, false, 1.0f,
            input_st->overlay_leds_lit, input_overlay_led_map());

#ifdef HAVE_GFX_WIDGETS
   gfx_widgets_set_overlay_bar(input_overlay_edit_bar_make(ol));
#endif
}

void input_overlay_edit_begin(void)
{
   const char *_msg;
   runloop_state_t *runloop_st = runloop_state_get_ptr();

   if (input_overlay_edit.flags & OVERLAY_EDIT_ACTIVE)
      return;

   memset(&input_overlay_edit, 0, sizeof(input_overlay_edit));
   input_overlay_edit_undo_drop();
   /* Held: a finger still down from the menu grabs nothing, a Back
    * still down from it finishes nothing */
   input_overlay_edit.flags = OVERLAY_EDIT_ACTIVE | OVERLAY_EDIT_HELD
         | OVERLAY_EDIT_BACK;

   if (     runloop_st->current_core_type != CORE_TYPE_DUMMY
         && !(runloop_st->flags & RUNLOOP_FLAG_PAUSED))
   {
      command_event(CMD_EVENT_PAUSE, NULL);
      if (runloop_st->flags & RUNLOOP_FLAG_PAUSED)
         input_overlay_edit.flags |= OVERLAY_EDIT_PAUSED;
   }

   input_overlay_edit.bar_item = -1;

   _msg = msg_hash_to_str(MSG_OVERLAY_EDIT_LAYOUT_START);
   runloop_msg_queue_push(_msg, strlen(_msg), 2, 300, true, NULL,
         MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
}

/* @ol is going away or out of sight: its edits are saved and the
 * editor stops, if it was @ol being edited. */
void input_overlay_edit_detach(input_overlay_t *ol)
{
   if (     (input_overlay_edit.flags & OVERLAY_EDIT_ACTIVE)
         && (     input_overlay_edit.ol == ol
              || (!input_overlay_edit.ol && ol == input_driver_st.overlay_ptr)))
      input_overlay_edit_finish(ol, true);
}

bool input_overlay_edit_active(void)
{
   return (input_overlay_edit.flags & OVERLAY_EDIT_ACTIVE) != 0;
}

void input_overlay_edit_stop(void)
{
   input_overlay_edit_finish(input_driver_st.overlay_ptr, true);
}

void input_overlay_edit_end(void)
{
   if (!(input_overlay_edit.flags & OVERLAY_EDIT_ACTIVE))
      return;
   input_overlay_edit_finish(input_driver_st.overlay_ptr, true);
   input_overlay_edit_return();
}

/* The @i-th control of the active page that shows anything, or 0 */
static uint16_t input_overlay_layout_nth_control(input_overlay_t *ol,
      unsigned i)
{
   uint16_t group;
   struct overlay *page = &ol->overlays[ol->active - ol->overlays];

   input_overlay_layout_group(page);
   for (group = 1; ; group++)
   {
      size_t d;
      bool any   = false;
      bool shown = false;
      for (d = 0; d < page->size; d++)
      {
         if (page->descs[d].layout_group != group)
            continue;
         any = true;
         if (OVERLAY_HAS_IMAGE(&page->descs[d].image))
            shown = true;
      }
      if (!any)
         return 0;
      if (shown && i-- == 0)
         return group;
   }
}

/* What a drawn desc of buttons or a key does, as its name */
static size_t input_overlay_layout_desc_name(const struct overlay_desc *desc,
      char *s, size_t len)
{
   unsigned b;
   size_t _len = 0;

   *s = '\0';
   if (desc->custom)
      return snprintf(s, len, msg_hash_to_str(MSG_OVERLAY_CONTROL_CUSTOM),
            desc->custom_label);
   if (desc->type == OVERLAY_TYPE_KEYBOARD)
      return strlcpy(s, msg_hash_to_str(MSG_OVERLAY_CONTROL_KEY), len);
   if (     BIT256_GET(desc->button_mask, RARCH_OVERLAY_NEXT)
         && *desc->next_index_name)
      return snprintf(s, len, msg_hash_to_str(MSG_OVERLAY_CONTROL_PAGE),
            desc->next_index_name);

   for (b = 0; b < RARCH_BIND_LIST_END && _len < len; b++)
   {
      const char *bind;
      if (     !BIT256_GET(desc->button_mask, b)
            || !(bind = input_config_bind_map_get_desc(b)))
         continue;
      if (_len)
         _len += strlcpy(s + _len, " + ", len - _len);
      if (_len < len)
         _len += strlcpy(s + _len, bind, len - _len);
   }
   return _len;
}

static size_t input_overlay_layout_name(const struct overlay *page,
      uint16_t group, char *s, size_t len)
{
   size_t d;
   const struct overlay_desc *named = NULL;

   *s = '\0';
   for (d = 0; d < page->size; d++)
   {
      const struct overlay_desc *desc = &page->descs[d];
      if (desc->layout_group != group)
         continue;
      switch (desc->type)
      {
         case OVERLAY_TYPE_DPAD_AREA:
            return strlcpy(s, msg_hash_to_str(MSG_OVERLAY_CONTROL_DPAD), len);
         case OVERLAY_TYPE_ABXY_AREA:
            return strlcpy(s, msg_hash_to_str(MSG_OVERLAY_CONTROL_FACE_BUTTONS), len);
         case OVERLAY_TYPE_ANALOG_LEFT:
            return strlcpy(s, msg_hash_to_str(MSG_OVERLAY_CONTROL_LEFT_STICK), len);
         case OVERLAY_TYPE_ANALOG_RIGHT:
            return strlcpy(s, msg_hash_to_str(MSG_OVERLAY_CONTROL_RIGHT_STICK), len);
         default:
            break;
      }
      if (!named && OVERLAY_HAS_IMAGE(&desc->image))
         named = desc;
   }

   if (!named)
      return 0;
   return input_overlay_layout_desc_name(named, s, len);
}

/* The @i-th control the active page lacks of those on its other pages
 * of the same shape */
static bool input_overlay_layout_nth_other(input_overlay_t *ol,
      unsigned i, input_overlay_addable_t *out)
{
   size_t p;
   size_t index           = (size_t)(ol->active - ol->overlays);
   struct overlay *active = &ol->overlays[index];

   for (p = 0; p < ol->size; p++)
   {
      uint16_t group;
      struct overlay *page = &ol->overlays[p];

      if (p == index || !input_overlay_page_alike(page, active))
         continue;
      input_overlay_layout_group(page);

      for (group = 1; ; group++)
      {
         size_t d;
         bool any   = false;
         bool shown = false;
         bool have  = false;

         for (d = 0; d < page->size && !have; d++)
         {
            size_t q;
            const struct overlay_desc *desc = &page->descs[d];
            if (desc->layout_group != group)
               continue;
            any = true;
            if (OVERLAY_HAS_IMAGE(&desc->image))
               shown = true;
            /* On the active page, or listed from an earlier one */
            for (q = 0; q <= p && !have; q++)
               if (     (q == index || (q < p && q != index
                        && input_overlay_page_alike(&ol->overlays[q], active)))
                     && input_overlay_page_has(&ol->overlays[q], desc))
                  have = true;
         }
         if (!any)
            break;
         if (!shown || have)
            continue;
         if (i-- == 0)
         {
            out->page  = p;
            out->group = group;
            out->desc  = -1;
            return true;
         }
      }
   }
   return false;
}

static bool input_overlay_layout_same_buttons(const struct overlay_desc *a,
      const struct overlay_desc *b)
{
   return   a->type == b->type
         && !memcmp(&a->button_mask, &b->button_mask, sizeof(a->button_mask));
}

/* Whether @desc is a drawn button of a control of more than one, as a
 * D-pad's arrows and the face buttons are, and the first of its
 * buttons there */
static bool input_overlay_layout_in_cluster(const struct overlay *page,
      size_t d)
{
   size_t k;
   unsigned drawn                  = 0;
   const struct overlay_desc *desc = &page->descs[d];

   if (     desc->type != OVERLAY_TYPE_BUTTONS
         || desc->layout_added
         || !OVERLAY_HAS_IMAGE(&desc->image))
      return false;
   for (k = 0; k < page->size; k++)
   {
      const struct overlay_desc *other = &page->descs[k];
      if (     other->layout_group == desc->layout_group
            && OVERLAY_HAS_IMAGE(&other->image))
         drawn++;
      if (     k < d
            && !other->layout_added
            && other->layout_group == desc->layout_group
            && input_overlay_layout_same_buttons(other, desc))
         return false;
   }
   return drawn >= 2;
}

/* The control standing alone that has the buttons of @desc, or 0 */
static uint16_t input_overlay_layout_alone(const struct overlay *page,
      const struct overlay_desc *desc)
{
   size_t k;
   for (k = 0; k < page->size; k++)
   {
      const struct overlay_desc *other = &page->descs[k];
      if (     other->layout_added
            && OVERLAY_HAS_IMAGE(&other->image)
            && input_overlay_layout_same_buttons(other, desc))
         return other->layout_group;
   }
   return 0;
}

/* Buttons added alone before they were given reach of their own */
void input_overlay_layout_reach_singles(input_overlay_t *ol)
{
   size_t i, d, k;
   for (i = 0; i < ol->size; i++)
   {
      struct overlay *page = &ol->overlays[i];
      for (d = 0; d < page->size; d++)
      {
         struct overlay_desc *desc = &page->descs[d];
         bool alone                = desc->layout_added != 0;
         if (     !alone
               || desc->type != OVERLAY_TYPE_BUTTONS
               || (desc->reach_left > 0.0f && desc->reach_up > 0.0f))
            continue;
         for (k = 0; k < page->size && alone; k++)
            alone = k == d
               || page->descs[k].layout_added != desc->layout_added;
         if (!alone)
            continue;
         if (desc->reach_left <= 0.0f)
            desc->reach_left = desc->reach_right = 1.0f;
         if (desc->reach_up <= 0.0f)
            desc->reach_up   = desc->reach_down  = 1.0f;
         if (desc->hitbox == OVERLAY_HITBOX_NONE)
            desc->hitbox = OVERLAY_HITBOX_RECT;
      }
   }
}

#define OVERLAY_LAYOUT_ENTRIES 64

/* The list of controls of the page shown, made again only when the
 * overlay or its page changes */
static struct
{
   const struct overlay *page;
   uint32_t loads;
   unsigned count;
   input_overlay_layout_entry_t entries[OVERLAY_LAYOUT_ENTRIES];
} input_overlay_layout_list;

/* Overlays loaded so far, to tell one from an earlier at the same
 * address */
static uint32_t input_overlay_loads;

/* The controls of the page shown, then those of its other pages it
 * lacks, then each button of its D-pad and face buttons on its own */
static void input_overlay_layout_list_make(input_overlay_t *ol)
{
   unsigned i;
   size_t d;
   uint16_t group;
   input_overlay_addable_t add;
   unsigned n           = 0;
   struct overlay *page = &ol->overlays[ol->active - ol->overlays];
   input_overlay_layout_entry_t *out = input_overlay_layout_list.entries;

   input_overlay_layout_list.page  = ol->active;
   input_overlay_layout_list.loads = input_overlay_loads;

   for (i = 0; n < OVERLAY_LAYOUT_ENTRIES
         && (group = input_overlay_layout_nth_control(ol, i)); i++)
   {
      bool single = false;
      for (d = 0; d < page->size && !single; d++)
         single = input_overlay_layout_in_cluster(page, d)
            && input_overlay_layout_alone(page, &page->descs[d]) == group;
      if (single)
         continue;
      out[n].have  = group;
      out[n].group = group;
      out[n].page  = (uint16_t)(ol->active - ol->overlays);
      out[n].desc  = -1;
      n++;
   }

   for (i = 0; n < OVERLAY_LAYOUT_ENTRIES
         && input_overlay_layout_nth_other(ol, i, &add); i++)
   {
      out[n].have  = 0;
      out[n].group = add.group;
      out[n].page  = (uint16_t)add.page;
      out[n].desc  = -1;
      n++;
   }

   for (d = 0; n < OVERLAY_LAYOUT_ENTRIES && d < page->size; d++)
   {
      if (!input_overlay_layout_in_cluster(page, d))
         continue;
      out[n].have  = input_overlay_layout_alone(page, &page->descs[d]);
      out[n].group = page->descs[d].layout_group;
      out[n].page  = (uint16_t)(ol->active - ol->overlays);
      out[n].desc  = (int16_t)d;
      n++;
   }

   input_overlay_layout_list.count = n;
}

static unsigned input_overlay_layout_list_count(input_overlay_t *ol)
{
   if (     input_overlay_layout_list.page  != ol->active
         || input_overlay_layout_list.loads != input_overlay_loads)
      input_overlay_layout_list_make(ol);
   return input_overlay_layout_list.count;
}

static bool input_overlay_layout_entry(input_overlay_t *ol, unsigned i,
      input_overlay_layout_entry_t *out)
{
   if (i >= input_overlay_layout_list_count(ol))
      return false;
   *out = input_overlay_layout_list.entries[i];
   return true;
}

static bool input_overlay_layout_add_entry(input_overlay_t *ol,
      const input_overlay_layout_entry_t *e)
{
   input_overlay_addable_t add;
   add.page  = e->page;
   add.desc  = e->desc;
   add.group = e->group;
   return input_overlay_layout_add_control(ol, &add);
}

unsigned input_overlay_layout_control_count(void)
{
   input_overlay_t *ol = input_overlay_layout_target();
   return ol ? input_overlay_layout_list_count(ol) : 0;
}

bool input_overlay_layout_control_get(unsigned i, char *s, size_t len,
      bool *shown)
{
   size_t d;
   input_overlay_layout_entry_t e;
   const struct overlay *page;
   input_overlay_t *ol = input_overlay_layout_target();

   if (!ol || !input_overlay_layout_entry(ol, i, &e))
      return false;
   *shown = false;
   page   = ol->active;
   if (e.have)
      for (d = 0; d < page->size; d++)
         if (     page->descs[d].layout_group == e.have
               && OVERLAY_HAS_IMAGE(&page->descs[d].image)
               && !(page->descs[d].flags & OVERLAY_DESC_LAYOUT_HIDDEN))
            *shown = true;
   if (s)
   {
      if (e.have)
         input_overlay_layout_name(page, e.have, s, len);
      else if (e.desc >= 0)
         input_overlay_layout_desc_name(
               &ol->overlays[e.page].descs[e.desc], s, len);
      else
         input_overlay_layout_name(&ol->overlays[e.page], e.group, s, len);
   }
   return true;
}

/* Shows or hides a control of the page shown, saved at once, or adds
 * one to it */
bool input_overlay_layout_control_toggle(unsigned i)
{
   bool shown;
   input_overlay_layout_entry_t e;
   input_overlay_t *ol = input_overlay_layout_target();

   if (     !ol
         || !input_overlay_layout_entry(ol, i, &e)
         || !input_overlay_layout_control_get(i, NULL, 0, &shown))
      return false;
   if (!e.have)
   {
      input_overlay_edit_finish(input_driver_st.overlay_ptr, true);
      if (!input_overlay_layout_add_entry(ol, &e))
         return false;
      input_overlay_layout_reload(ol);
      return true;
   }
   input_overlay_layout_set_hidden(ol, e.have, shown);
   if (ol->iface && (ol->flags & INPUT_OVERLAY_ENABLE))
      command_event(CMD_EVENT_OVERLAY_SET_ALPHA_MOD, NULL);
   return input_overlay_layout_save(ol);
}

/* The gamepad the running content uses, from Load Gamepad or an
 * override, and the overlay preset to put back when it closes */
static char input_overlay_gamepad_chosen[PATH_MAX_LENGTH];
static char input_overlay_gamepad_prev[PATH_MAX_LENGTH];
static bool input_overlay_gamepad_temp;

void input_overlay_gamepad_override(const char *path)
{
   if (path && *path && input_overlay_is_profile(path))
      strlcpy(input_overlay_gamepad_chosen, path,
            sizeof(input_overlay_gamepad_chosen));
}

void input_overlay_gamepad_preset(const char *path)
{
   settings_t *settings = config_get_ptr();
   if (     runloop_state_get_ptr()->current_core_type == CORE_TYPE_DUMMY
         || !path || !input_overlay_is_profile(path))
      return;
   if (!input_overlay_gamepad_temp)
      strlcpy(input_overlay_gamepad_prev, settings->paths.path_overlay,
            sizeof(input_overlay_gamepad_prev));
   input_overlay_gamepad_temp = true;
   strlcpy(input_overlay_gamepad_chosen, path,
         sizeof(input_overlay_gamepad_chosen));
}

void input_overlay_gamepad_session_end(void)
{
   settings_t *settings = config_get_ptr();
   if (     input_overlay_gamepad_temp
         && string_is_equal(settings->paths.path_overlay,
            input_overlay_gamepad_chosen))
      configuration_set_string(settings, settings->paths.path_overlay,
            input_overlay_gamepad_prev);
   input_overlay_gamepad_temp      = false;
   *input_overlay_gamepad_chosen   = '\0';
}

/* The gamepad to show: the one chosen for the content (Load Gamepad, or
 * an override's overlay preset), else the one for all content, which is
 * the preset when that is a gamepad and otherwise the one made from
 * @pack, the overlay RetroArch would pick. A system's own overlay, as
 * Autoload Preferred Overlay picks, is not taken: a system's gamepad is
 * chosen by an override like any other. */
const char *input_overlay_gamepad_path(const char *pack)
{
   const char *preset = config_get_ptr()->paths.path_overlay;

   if (     *input_overlay_gamepad_chosen
         && path_is_valid(input_overlay_gamepad_chosen))
      return input_overlay_gamepad_chosen;
   if (input_overlay_is_profile(preset) && path_is_valid(preset))
      return preset;
   if (pack && strstr(pack, SYSTEM_OVERLAY_DIR))
      pack = preset;
   return input_overlay_profile_for(pack);
}

/* Loads the overlay again for a layout changed under it, on the page
 * it was on. The menu's lists of it are made again then. */
static void input_overlay_layout_reload(const input_overlay_t *ol)
{
   input_overlay_layout_reload_page = (ol && ol->active)
      ? (int)(ol->active - ol->overlays) : -1;
   input_overlay_deinit();
   input_overlay_init();
}

bool input_overlay_layout_drawn(void)
{
   input_overlay_t *ol = input_overlay_layout_target();
   return ol && ol->drawn;
}

/* Sets @key to @value in the layout of the scope chosen, and loads the
 * overlay again */
static bool input_overlay_layout_set(const char *key, const char *value)
{
   char path[PATH_MAX_LENGTH];
   char dir[DIR_MAX_LENGTH];
   bool ret;
   config_file_t *conf = NULL;
   input_overlay_t *ol = input_overlay_layout_target();

   if (!ol)
      return false;
   input_overlay_layout_file(path, sizeof(path), ol->path);
   if (!*path)
      return false;
   if (     !path_is_valid(path)
         || !(conf = config_file_new_from_path_to_string(path)))
      conf = config_file_new_alloc();
   if (!conf)
      return false;

   input_overlay_edit_finish(input_driver_st.overlay_ptr, true);
   config_set_string(conf, key, value);
   fill_pathname_basedir(dir, path, sizeof(dir));
   if (!path_is_directory(dir))
      path_mkdir(dir);
   ret = config_file_write(conf, path, true);
   config_file_free(conf);
   if (ret)
      input_overlay_layout_reload(ol);
   return ret;
}

/* Draws the buttons, or loads their images, from now on for the scope
 * chosen */
bool input_overlay_layout_drawn_toggle(void)
{
   input_overlay_t *ol = input_overlay_layout_target();
   return ol && input_overlay_layout_set("overlay_drawn_buttons",
         ol->drawn ? "false" : "true");
}

bool input_overlay_layout_filled(void)
{
   input_overlay_t *ol = input_overlay_layout_target();
   return ol && ol->filled;
}

/* The outline width, in pixels on the screen */
unsigned input_overlay_layout_stroke(void)
{
   input_overlay_t *ol = input_overlay_layout_target();
   return ol ? ol->stroke : 6;
}

bool input_overlay_layout_stroke_set(unsigned px)
{
   char val[8];
   snprintf(val, sizeof(val), "%u", MIN(px, 20));
   return input_overlay_layout_set("overlay_button_stroke_px", val);
}

/* A pixel thicker or thinner, from 0 to 20, wrapping round */
bool input_overlay_layout_stroke_step(int dir)
{
   input_overlay_t *ol = input_overlay_layout_target();
   int stroke;
   if (!ol)
      return false;
   stroke = (int)ol->stroke + dir;
   if (stroke < 0)
      stroke = 20;
   else if (stroke > 20)
      stroke = 0;
   return input_overlay_layout_stroke_set((unsigned)stroke);
}

/* The opacity of an outlined button's middle, in hundredths */
unsigned input_overlay_layout_fill_opacity(void)
{
   input_overlay_t *ol = input_overlay_layout_target();
   return ol ? ol->fill : 100;
}

/* A twentieth more or less opaque, from 0 to 1, wrapping round */
bool input_overlay_layout_fill_opacity_step(int dir)
{
   char val[8];
   input_overlay_t *ol = input_overlay_layout_target();
   int fill;
   if (!ol)
      return false;
   fill = ((int)ol->fill + 2) / 5 * 5 + dir * 5;
   if (fill < 0)
      fill = 100;
   else if (fill > 100)
      fill = 0;
   snprintf(val, sizeof(val), "%d", fill);
   return input_overlay_layout_set("overlay_button_fill_opacity", val);
}

/* Filled buttons, or outlined, from now on for the scope chosen */
bool input_overlay_layout_filled_toggle(void)
{
   input_overlay_t *ol = input_overlay_layout_target();
   return ol && input_overlay_layout_set("overlay_button_style",
         ol->filled ? "outline" : "filled");
}

void input_overlay_layout_reset(void)
{
   char path[PATH_MAX_LENGTH];
   const char *overlay = input_overlay_path(false);
   const char *_msg    = msg_hash_to_str(MSG_OVERLAY_LAYOUT_RESET);

   input_overlay_edit_finish(input_driver_st.overlay_ptr, false);

   /* The built-in gamepads deleted, back */
   {
      unsigned i;
      for (i = 0; i < ARRAY_SIZE(input_overlay_builtin_profiles); i++)
      {
         input_overlay_profile_file(path, sizeof(path),
               input_overlay_builtin_profiles[i]);
         if (*path && !path_is_valid(path))
            input_overlay_builtin_profile_reset(i);
      }
   }

   if (input_overlay_is_profile(overlay))
   {
      /* A built-in profile as it comes; one of the user's stays */
      char name[NAME_MAX_LENGTH];
      int builtin;
      strlcpy(name, path_basename(overlay), sizeof(name));
      path_remove_extension(name);
      if ((builtin = input_overlay_builtin_profile_find(name)) < 0)
         return;
      input_overlay_builtin_profile_reset((unsigned)builtin);
   }
   else
   {
      input_overlay_layout_path(path, sizeof(path), overlay,
            input_overlay_layout_scope());
      if (*path && path_is_valid(path))
         filestream_delete(path);
   }

   /* Loaded again, with what the other layouts and the pack say */
   input_overlay_layout_reload(input_overlay_layout_target());

   runloop_msg_queue_push(_msg, strlen(_msg), 1, 120, true, NULL,
         MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
}

/* Layout profiles, gamepads to the user: overlay configs of their own,
 * in overlays/gamepads/custom, that any RetroArch loads as it would
 * another. Their images are the packs' they were made from, by
 * relative path, but for this build's own (neo-custom), which are
 * copied to custom/img. Where the user
 * put a control is written in its desc, where the pack had it in
 * _pack; a hidden control's desc is off the screen, where it was in
 * _layout_x/_layout_y. */


static void input_overlay_profiles_dir(char *s, size_t len)
{
   char dir[DIR_MAX_LENGTH];
   fill_pathname_expand_special(dir, config_get_ptr()->paths.directory_overlay,
         sizeof(dir));
   fill_pathname_join_special(s, dir, "gamepads/custom", len);
}

bool input_overlay_is_profile(const char *path)
{
   char dir[DIR_MAX_LENGTH];
   char base[DIR_MAX_LENGTH];
   if (!path || !*path)
      return false;
   input_overlay_profiles_dir(dir, sizeof(dir));
   fill_pathname_basedir(base, path, sizeof(base));
   fill_pathname_slash(dir, sizeof(dir));
   return string_is_equal(base, dir);
}

/* custom/@name.cfg; empty when @name leaves nothing to name a file */
static void input_overlay_profile_file(char *s, size_t len, const char *name)
{
   char dir[DIR_MAX_LENGTH];
   char file[NAME_MAX_LENGTH];
   char *c;
   size_t _len;

   *s = '\0';
   while (*name == ' ' || *name == '.')
      name++;
   _len = strlcpy(file, name, sizeof(file) - STRLEN_CONST(".cfg"));
   if (_len >= sizeof(file) - STRLEN_CONST(".cfg"))
      _len = sizeof(file) - STRLEN_CONST(".cfg") - 1;
   while (_len && file[_len - 1] == ' ')
      file[--_len] = '\0';
   if (!_len)
      return;
   /* What file systems do not take in a name */
   for (c = file; *c; c++)
      if (strchr("/\\:*?\"<>|", *c) || (unsigned char)*c < 0x20)
         *c = '_';
   strlcpy(file + _len, ".cfg", sizeof(file) - _len);
   input_overlay_profiles_dir(dir, sizeof(dir));
   fill_pathname_join_special(s, dir, file, len);
}

static int input_overlay_builtin_profile_find(const char *name)
{
   unsigned i;
   for (i = 0; i < ARRAY_SIZE(input_overlay_builtin_profiles); i++)
      if (string_is_equal(name, input_overlay_builtin_profiles[i]))
         return (int)i;
   return -1;
}

/* The overlay built-in profile @i is made from */
static bool input_overlay_builtin_pack(unsigned i, char *s, size_t len)
{
   char dir[DIR_MAX_LENGTH];
   settings_t *settings = config_get_ptr();

   *s = '\0';
   /* The bundled pad, not the preset, which may be a gamepad itself */
   if (i == 0)
   {
      fill_pathname_expand_special(dir, settings->paths.directory_overlay,
            sizeof(dir));
      fill_pathname_join_special(s, dir,
            "gamepads/neo-retropad/neo-retropad.cfg", len);
   }
   else if (i < ARRAY_SIZE(input_overlay_builtin_profiles))
   {
      char name[NAME_MAX_LENGTH];
      fill_pathname_expand_special(dir, settings->paths.directory_overlay,
            sizeof(dir));
      snprintf(name, sizeof(name), "%s/Nintendo - %s.cfg",
            SYSTEM_OVERLAY_DIR, input_overlay_builtin_profiles[i]);
      fill_pathname_join_special(s, dir, name, len);
   }
   return *s && path_is_valid(s);
}

/* The keys of a desc whose place and size the layout gave, put in the
 * desc itself, as the user put it, so that any loader reads it there;
 * the pack's in _pack. A hidden one goes off the screen in its desc,
 * where it was staying in _layout_x and _layout_y. */
static void input_overlay_profile_normalize(config_file_t *conf)
{
   unsigned o, d, pages = 0;

   config_get_uint(conf, "overlays", &pages);
   for (o = 0; o < pages; o++)
   {
      unsigned descs = 0;
      bool normalized = false;
      char key[96];

      snprintf(key, sizeof(key), "overlay%u_normalized", o);
      config_get_bool(conf, key, &normalized);
      snprintf(key, sizeof(key), "overlay%u_descs", o);
      if (!normalized || !config_get_uint(conf, key, &descs))
         continue;

      for (d = 0; d < descs; d++)
      {
         char spec[256];
         char out[256];
         char base[64];
         char *tok[6];
         char *p;
         unsigned n = 0, k;
         float v[4], lay[4];
         bool has[4];
         bool hidden = false;
         static const char *suffix[] = {
            "_layout_x", "_layout_y", "_layout_w", "_layout_h" };

         snprintf(base, sizeof(base), "overlay%u_desc%u", o, d);
         if (!config_get_array(conf, base, spec, sizeof(spec)))
            continue;
         for (p = spec; n < 6; n++)
         {
            tok[n] = p;
            while (*p && *p != ',')
               p++;
            if (!*p)
            {
               n++;
               break;
            }
            *p++ = '\0';
         }
         if (n < 6)
            continue;
         v[0] = (float)rstrtod(tok[1], NULL);
         v[1] = (float)rstrtod(tok[2], NULL);
         v[2] = (float)rstrtod(tok[4], NULL);
         v[3] = (float)rstrtod(tok[5], NULL);

         for (k = 0; k < 4; k++)
         {
            snprintf(key, sizeof(key), "%s%s", base, suffix[k]);
            has[k] = config_get_float(conf, key, &lay[k]);
         }
         snprintf(key, sizeof(key), "%s_layout_hidden", base);
         config_get_bool(conf, key, &hidden);
         if (!has[0] && !has[1] && !has[2] && !has[3] && !hidden)
            continue;

         /* Where the pack had it, once */
         snprintf(key, sizeof(key), "%s_pack", base);
         if (!config_get_entry(conf, key))
         {
            char pack[128];
            snprintf(pack, sizeof(pack), "%.6f,%.6f,%.6f,%.6f",
                  v[0], v[1], v[2], v[3]);
            config_set_string(conf, key, pack);
         }

         /* A hidden one's place is in _layout_x/_y already, from when
          * it went; its desc only says off the screen */
         if (hidden && (v[0] > 2.0f || v[1] > 2.0f) && !has[0] && !has[1])
            continue;
         for (k = 0; k < 4; k++)
            if (has[k])
               v[k] = lay[k];
         for (k = 0; k < 4; k++)
         {
            snprintf(key, sizeof(key), "%s%s", base, suffix[k]);
            config_unset(conf, key);
         }
         if (hidden)
         {
            char val[32];
            snprintf(key, sizeof(key), "%s_layout_x", base);
            snprintf(val, sizeof(val), "%.6f", v[0]);
            config_set_string(conf, key, val);
            snprintf(key, sizeof(key), "%s_layout_y", base);
            snprintf(val, sizeof(val), "%.6f", v[1]);
            config_set_string(conf, key, val);
            v[0] = v[1] = 2.5f;
         }
         snprintf(out, sizeof(out), "%s,%.6f,%.6f,%s,%.6f,%.6f",
               tok[0], v[0], v[1], tok[3], v[2], v[3]);
         config_set_string(conf, base, out);
      }
   }
}

/* Writes @path's layout keys into its descs (input_overlay_profile_normalize) */
static bool input_overlay_profile_normalize_file(const char *path)
{
   bool ret;
   config_file_t *conf = config_file_new_from_path_to_string(path);
   if (!conf)
      return false;
   input_overlay_profile_normalize(conf);
   ret = config_file_write(conf, path, true);
   config_file_free(conf);
   return ret;
}

/* Profile @out made from overlay @pack, the layouts @layers over it
 * (those there are). Its images by relative path, this build's own
 * copied beside it. */
static bool input_overlay_profile_make(const char *out, const char *pack,
      const char * const *layers, unsigned num_layers)
{
   unsigned i;
   char dir[DIR_MAX_LENGTH];
   char img_dir[DIR_MAX_LENGTH];
   struct config_entry_list *entry;
   bool ret;
   config_file_t *conf = config_file_new_alloc();

   if (!conf)
      return false;
   input_overlay_profiles_dir(dir, sizeof(dir));
   fill_pathname_join_special(img_dir, dir, "img", sizeof(img_dir));
   if (!path_is_directory(dir))
      path_mkdir(dir);
   fill_pathname_slash(dir, sizeof(dir));

   for (i = 0; i <= num_layers; i++)
   {
      const char *from    = (i == 0) ? pack : layers[i - 1];
      config_file_t *part;
      if (     !from || !*from || !path_is_valid(from)
            || !(part = config_file_new_from_path_to_string(from)))
      {
         if (i == 0)
         {
            config_file_free(conf);
            return false;
         }
         continue;
      }
      for (entry = part->entries; entry; entry = entry->next)
      {
         size_t klen;
         if (!entry->key || !entry->value)
            continue;
         klen = strlen(entry->key);
         /* An image: the same file, from where the profile is */
         if (     klen > STRLEN_CONST("_overlay")
               && string_is_equal(entry->key + klen
                  - STRLEN_CONST("_overlay"), "_overlay")
               && *entry->value)
         {
            char abs_path[PATH_MAX_LENGTH];
            char rel[PATH_MAX_LENGTH];
            fill_pathname_resolve_relative(abs_path, from, entry->value,
                  sizeof(abs_path));
            path_resolve_realpath(abs_path, sizeof(abs_path), false);
            if (strstr(abs_path, "neo-custom"))
            {
               char copy[PATH_MAX_LENGTH];
               fill_pathname_join_special(copy, img_dir,
                     path_basename(abs_path), sizeof(copy));
               if (!path_is_valid(copy))
               {
                  void *buf    = NULL;
                  int64_t blen = 0;
                  if (!path_is_directory(img_dir))
                     path_mkdir(img_dir);
                  if (filestream_read_file(abs_path, &buf, &blen))
                  {
                     filestream_write_file(copy, buf, blen);
                     free(buf);
                  }
               }
               snprintf(rel, sizeof(rel), "img/%s", path_basename(abs_path));
            }
            else
               path_relative_to(rel, abs_path, dir, sizeof(rel));
            config_set_string(conf, entry->key, rel);
         }
         else
            config_set_string(conf, entry->key, entry->value);
      }
      config_file_free(part);
   }

   input_overlay_profile_normalize(conf);
   if ((ret = config_file_write(conf, out, true)))
      RARCH_LOG("[Overlay] Made layout profile \"%s\" from \"%s\".\n",
            out, pack);
   config_file_free(conf);
   return ret;
}

/* The built-in profiles, made the first time from their overlays with
 * the layouts the user had for them before profiles; and the layouts
 * the user had for a core or content, as profiles of their own chosen
 * for it */
static void input_overlay_profiles_ensure(void)
{
   unsigned i;
   char path[PATH_MAX_LENGTH];
   char pack[PATH_MAX_LENGTH];
   char dir[DIR_MAX_LENGTH];
   char layouts[DIR_MAX_LENGTH];
   static bool checked = false;

   if (checked)
      return;
   checked = true;
   input_overlay_profiles_dir(dir, sizeof(dir));
   fill_pathname_join_special(path, dir, ".made", sizeof(path));
   if (path_is_valid(path))
      return;

   fill_pathname_application_special(layouts, sizeof(layouts),
         APPLICATION_SPECIAL_DIRECTORY_CONFIG);
   fill_pathname_join_special(layouts, layouts, "overlay_layouts",
         sizeof(layouts));

   for (i = 0; i < ARRAY_SIZE(input_overlay_builtin_profiles); i++)
   {
      char out[PATH_MAX_LENGTH];
      char layer[PATH_MAX_LENGTH];
      const char *layers[1];
      struct string_list *cores;

      if (!input_overlay_builtin_pack(i, pack, sizeof(pack)))
         continue;
      input_overlay_profile_file(out, sizeof(out),
            input_overlay_builtin_profiles[i]);
      fill_pathname_join_special(layer, layouts, path_basename(pack),
            sizeof(layer));
      layers[0] = layer;
      if (!path_is_valid(out))
         input_overlay_profile_make(out, pack, layers, 1);

      /* A core's or a content's layout of it: a profile for it */
      if (!(cores = dir_list_new(layouts, NULL, true, false, false, false)))
         continue;
      {
         size_t c;
         for (c = 0; c < cores->size; c++)
         {
            char core_layer[PATH_MAX_LENGTH];
            const char *core = cores->elems[c].data;
            struct string_list *games;
            if (     !path_is_directory(core)
                  || string_is_equal(path_basename(core), "profiles"))
               continue;
            fill_pathname_join_special(core_layer, core,
                  path_basename(pack), sizeof(core_layer));
            if (path_is_valid(core_layer))
            {
               char name[NAME_MAX_LENGTH];
               char choice[PATH_MAX_LENGTH];
               char line[PATH_MAX_LENGTH + 32];
               const char *two[2];
               two[0] = layer;
               two[1] = core_layer;
               snprintf(name, sizeof(name), "%s (%s)",
                     input_overlay_builtin_profiles[i], path_basename(core));
               input_overlay_profile_file(out, sizeof(out), name);
               if (input_overlay_profile_make(out, pack, two, 2))
               {
                  fill_pathname_join_special(choice, core, "controller.cfg",
                        sizeof(choice));
                  snprintf(line, sizeof(line), "overlay = \"%s\"\n", out);
                  filestream_write_file(choice, line, strlen(line));
               }
            }
            if (!(games = dir_list_new(core, NULL, true, false, false, false)))
               continue;
            {
               size_t g;
               for (g = 0; g < games->size; g++)
               {
                  char game_layer[PATH_MAX_LENGTH];
                  const char *game = games->elems[g].data;
                  if (!path_is_directory(game))
                     continue;
                  fill_pathname_join_special(game_layer, game,
                        path_basename(pack), sizeof(game_layer));
                  if (path_is_valid(game_layer))
                  {
                     char name[NAME_MAX_LENGTH];
                     char choice[PATH_MAX_LENGTH];
                     char line[PATH_MAX_LENGTH + 32];
                     const char *three[3];
                     three[0] = layer;
                     three[1] = core_layer;
                     three[2] = game_layer;
                     snprintf(name, sizeof(name), "%s (%s)",
                           input_overlay_builtin_profiles[i],
                           path_basename(game));
                     input_overlay_profile_file(out, sizeof(out), name);
                     if (input_overlay_profile_make(out, pack, three, 3))
                     {
                        fill_pathname_join_special(choice, game,
                              "controller.cfg", sizeof(choice));
                        snprintf(line, sizeof(line), "overlay = \"%s\"\n", out);
                        filestream_write_file(choice, line, strlen(line));
                     }
                  }
               }
            }
            string_list_free(games);
         }
      }
      string_list_free(cores);
   }

   fill_pathname_join_special(path, dir, ".made", sizeof(path));
   filestream_write_file(path, "", 0);
}

/* The profile to use for overlay @pack where nothing chose one: its
 * built-in profile, if it has one */
static const char *input_overlay_profile_for(const char *pack)
{
   static char profile[PATH_MAX_LENGTH];
   unsigned i;

   if (!pack || !*pack)
      return pack;
   input_overlay_profiles_ensure();
   for (i = 0; i < ARRAY_SIZE(input_overlay_builtin_profiles); i++)
   {
      char from[PATH_MAX_LENGTH];
      if (     !input_overlay_builtin_pack(i, from, sizeof(from))
            || (  !string_is_equal(path_basename(from), path_basename(pack))
               && !(i == 1 && string_is_equal(path_basename(pack),
                     "Nintendo - Nintendo DSi.cfg"))))
         continue;
      input_overlay_profile_file(profile, sizeof(profile),
            input_overlay_builtin_profiles[i]);
      if (path_is_valid(profile))
         return profile;
   }
   return pack;
}

/* Where the layout shown is saved: its profile, or, for an overlay of
 * its own, the layout of the scope chosen */
static void input_overlay_layout_file(char *s, size_t len,
      const char *overlay_path)
{
   if (input_overlay_is_profile(overlay_path))
      strlcpy(s, overlay_path, len);
   else
      input_overlay_layout_path(s, len, overlay_path,
            input_overlay_layout_scope());
}

/* A copy of the profile shown, its edits in, as @name */
bool input_overlay_layout_profile_save(const char *name)
{
   char path[PATH_MAX_LENGTH];
   input_overlay_t *ol = input_overlay_layout_target();

   if (!ol || input_overlay_builtin_profile_find(name) >= 0)
      return false;
   input_overlay_profile_file(path, sizeof(path), name);
   if (!*path)
      return false;
   if (input_overlay_is_profile(ol->path))
   {
      void *buf    = NULL;
      int64_t blen = 0;
      bool ret;
      if (!filestream_read_file(ol->path, &buf, &blen))
         return false;
      ret = filestream_write_file(path, buf, blen);
      free(buf);
      return ret;
   }
   else
   {
      /* An overlay of its own, with its layouts */
      unsigned scope;
      char layer[OVERLAY_LAYOUT_SCOPE_LAST][PATH_MAX_LENGTH];
      const char *layers[OVERLAY_LAYOUT_SCOPE_LAST];
      for (scope = 0; scope < OVERLAY_LAYOUT_SCOPE_LAST; scope++)
      {
         input_overlay_layout_path(layer[scope], sizeof(layer[scope]),
               ol->path, (enum overlay_layout_scope)scope);
         layers[scope] = layer[scope];
      }
      return input_overlay_profile_make(path, ol->path, layers,
            OVERLAY_LAYOUT_SCOPE_LAST);
   }
}

/* Like the overlay preset: for the running content until it closes,
 * kept by an override; with none running, for all content */
bool input_overlay_layout_profile_load(const char *name)
{
   char profile[PATH_MAX_LENGTH];
   settings_t *settings = config_get_ptr();

   input_overlay_profiles_ensure();
   input_overlay_profile_file(profile, sizeof(profile), name);
   if (!*profile || !path_is_valid(profile))
      return false;

   input_overlay_edit_finish(input_driver_st.overlay_ptr, false);
   input_overlay_gamepad_preset(profile);
   configuration_set_string(settings, settings->paths.path_overlay, profile);
   RARCH_LOG("[Overlay] Using gamepad \"%s\".\n", profile);

   input_overlay_layout_reload_page = -1;
   input_overlay_deinit();
   input_overlay_init();
#ifdef HAVE_MENU
   menu_state_get_ptr()->flags |= MENU_ST_FLAG_ENTRIES_NEED_REFRESH;
#endif
   return true;
}

/* The gamepad Delete Gamepad asks about, until it is answered */
static char input_overlay_profile_asked[NAME_MAX_LENGTH];

void input_overlay_layout_profile_delete_ask(const char *name)
{
   strlcpy(input_overlay_profile_asked, name ? name : "",
         sizeof(input_overlay_profile_asked));
}

void input_overlay_layout_profile_delete_asked(void)
{
   char _msg[256];
   size_t _len;
   if (!*input_overlay_profile_asked)
      return;
   _len = snprintf(_msg, sizeof(_msg), msg_hash_to_str(
         input_overlay_layout_profile_delete(input_overlay_profile_asked)
         ? MSG_OVERLAY_LAYOUT_PROFILE_DELETED
         : MSG_OVERLAY_LAYOUT_PROFILE_FAILED), input_overlay_profile_asked);
   *input_overlay_profile_asked = '\0';
   runloop_msg_queue_push(_msg, MIN(_len, sizeof(_msg) - 1), 1, 180, true,
         NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
#ifdef HAVE_MENU
   menu_state_get_ptr()->flags |= MENU_ST_FLAG_ENTRIES_NEED_REFRESH;
#endif
}

/* Any gamepad, a built-in one too (Reset Gamepad brings that back);
 * the one shown is loaded again without it */
bool input_overlay_layout_profile_delete(const char *name)
{
   char path[PATH_MAX_LENGTH];
   input_overlay_t *ol = input_overlay_layout_target();

   input_overlay_profile_file(path, sizeof(path), name);
   if (     !*path || !path_is_valid(path)
         || filestream_delete(path) != 0)
      return false;
   if (ol && ol->path && string_is_equal(ol->path, path))
   {
      input_overlay_edit_finish(input_driver_st.overlay_ptr, false);
      input_overlay_layout_reload(ol);
   }
   return true;
}

/* Custom buttons: controls of their own label and input, added to each
 * page of the shape of the one shown, all with the same _custom id */
static unsigned input_overlay_custom_sel;

/* The gamepad shown, opened, with its path in @path; NULL when what is
 * shown is not a gamepad */
static config_file_t *input_overlay_custom_open(input_overlay_t **ol,
      char *path, size_t len)
{
   *ol = input_overlay_layout_target();
   if (!*ol || !input_overlay_is_profile((*ol)->path))
      return NULL;
   input_overlay_edit_finish(input_driver_st.overlay_ptr, true);
   strlcpy(path, (*ol)->path, len);
   return config_file_new_from_path_to_string(path);
}

static bool input_overlay_custom_write(config_file_t *conf,
      const char *path, input_overlay_t *ol)
{
   bool ret = config_file_write(conf, path, true);
   config_file_free(conf);
   if (ret)
      input_overlay_layout_reload(ol);
#ifdef HAVE_MENU
   menu_state_get_ptr()->flags |= MENU_ST_FLAG_ENTRIES_NEED_REFRESH;
#endif
   return ret;
}

/* @idx of @page taken out, the descs after it moved down one */
static void input_overlay_custom_drop(config_file_t *conf, unsigned page,
      unsigned idx)
{
   unsigned k, count = 0;
   char key[64];

   snprintf(key, sizeof(key), "overlay%u_descs", page);
   if (!config_get_uint(conf, key, &count) || idx >= count)
      return;
   for (k = idx; k < count; k++)
   {
      size_t i, _len;
      char prefix[32];
      struct config_entry_list *e;
      union string_list_elem_attr attr;
      struct string_list *keys = string_list_new();
      if (!keys)
         return;
      attr.i = 0;
      _len   = snprintf(prefix, sizeof(prefix), "overlay%u_desc%u", page, k);
      for (e = conf->entries; e; e = e->next)
         if (     e->key && !strncmp(e->key, prefix, _len)
               && (!e->key[_len] || e->key[_len] == '_'))
            string_list_append(keys, e->key, attr);
      for (i = 0; i < keys->size; i++)
         config_unset(conf, keys->elems[i].data);
      string_list_free(keys);

      if (k + 1 >= count || !(keys = string_list_new()))
         continue;
      _len = snprintf(prefix, sizeof(prefix), "overlay%u_desc%u", page, k + 1);
      for (e = conf->entries; e; e = e->next)
         if (     e->key && e->value && !strncmp(e->key, prefix, _len)
               && (!e->key[_len] || e->key[_len] == '_'))
         {
            snprintf(key, sizeof(key), "overlay%u_desc%u%s", page, k,
                  e->key + _len);
            attr.p = NULL;
            string_list_append(keys, key, attr);
            string_list_append(keys, e->value, attr);
         }
      for (i = 0; i + 1 < keys->size; i += 2)
         config_set_string(conf, keys->elems[i].data,
               keys->elems[i + 1].data);
      string_list_free(keys);
   }
   snprintf(key, sizeof(key), "overlay%u_descs", page);
   config_set_uint(conf, key, count - 1);
}

struct string_list *input_overlay_custom_list(void)
{
   size_t i;
   input_overlay_t *ol        = input_overlay_layout_target();
   struct string_list *list   = string_list_new();
   const struct overlay *page = ol ? ol->active : NULL;

   if (!list || !page)
      return list;
   for (i = 0; i < page->size; i++)
   {
      char text[128];
      unsigned b;
      union string_list_elem_attr attr;
      const struct overlay_desc *desc = &page->descs[i];
      const char *input               = NULL;
      if (!desc->custom)
         continue;
      for (b = 0; b < RARCH_BIND_LIST_END && !input; b++)
         if (BIT256_GET(desc->button_mask, b))
            input = input_config_bind_map_get_desc(b);
      if (*desc->custom_gamepad)
         snprintf(text, sizeof(text), "%s: %s",
               *desc->custom_label ? desc->custom_label : "-",
               desc->custom_gamepad);
      else
         snprintf(text, sizeof(text), "%s: %s",
               *desc->custom_label ? desc->custom_label : "-",
               input ? input : "");
      attr.i = desc->custom;
      string_list_append(list, text, attr);
   }
   return list;
}

void input_overlay_custom_select(unsigned id)
{
   input_overlay_custom_sel = id;
}

/* An input a custom button can send: a button of the pad or a hotkey,
 * not one that needs more than pressing */
bool input_overlay_custom_input(unsigned bind)
{
   if (bind >= RARCH_BIND_LIST_END)
      return false;
   if (bind >= RARCH_FIRST_CUSTOM_BIND && bind < RARCH_FIRST_META_KEY)
      return false;
   return   bind != RARCH_ENABLE_HOTKEY
         && bind != RARCH_OVERLAY_NEXT
         && input_config_bind_map_get_base(bind)
         && input_config_bind_map_get_desc(bind);
}

bool input_overlay_custom_new(unsigned bind, const char *gamepad,
      const char *label)
{
   size_t d;
   unsigned dst, id = 0;
   char key[64];
   char val[128];
   char path[PATH_MAX_LENGTH];
   char img[PATH_MAX_LENGTH];
   struct config_entry_list *e;
   input_overlay_t *ol;
   const struct overlay *active;
   config_file_t *conf;

   if (     (gamepad ? !*gamepad : !input_overlay_custom_input(bind))
         || !(conf = input_overlay_custom_open(&ol, path, sizeof(path))))
      return false;
   active = ol->active;

   for (e = conf->entries; e; e = e->next)
   {
      size_t klen = e->key ? strlen(e->key) : 0;
      if (     klen > STRLEN_CONST("_custom")
            && string_is_equal(e->key + klen - STRLEN_CONST("_custom"),
               "_custom"))
         id = MAX(id, (unsigned)strtoul(e->value, NULL, 10));
   }
   id++;

   /* Its image, for the images and other RetroArch, next to it */
   fill_pathname_basedir(img, path, sizeof(img));
   fill_pathname_join_special(val, img, "img", sizeof(val));
   fill_pathname_join_special(img, val, "button_custom.png", sizeof(img));
   if (!path_is_valid(img))
   {
      char from[PATH_MAX_LENGTH];
      char dir[DIR_MAX_LENGTH];
      void *buf    = NULL;
      int64_t blen = 0;
      fill_pathname_expand_special(dir,
            config_get_ptr()->paths.directory_overlay, sizeof(dir));
      fill_pathname_join_special(from, dir,
            "gamepads/neo-custom/img/button_custom.png", sizeof(from));
      if (!path_is_directory(val))
         path_mkdir(val);
      if (filestream_read_file(from, &buf, &blen))
      {
         filestream_write_file(img, buf, blen);
         free(buf);
      }
   }

   for (dst = 0; dst < ol->size; dst++)
   {
      unsigned to                = 0;
      uint16_t added             = 0;
      const struct overlay *page = &ol->overlays[dst];
      /* As big as a face button, round on the screen */
      float aspect               = page->aspect_ratio > 0.0f
         ? page->aspect_ratio : 1.0f;
      float rx                   = 0.0612f * MIN(1.0f, 1.0f / aspect);

      if (page != active
            && (  !input_overlay_page_alike(page, active)
               || !(page->flags & OVERLAY_TAKES_OTHER_INPUT)))
         continue;
      for (d = 0; d < page->size; d++)
         if (page->descs[d].layout_added > added)
            added = page->descs[d].layout_added;
      snprintf(key, sizeof(key), "overlay%u_descs", dst);
      config_get_uint(conf, key, &to);
      config_set_uint(conf, key, to + 1);

      snprintf(key, sizeof(key), "overlay%u_desc%u", dst, to);
      snprintf(val, sizeof(val), "%s,%.6f,%.6f,rect,%.6f,%.6f",
            gamepad ? "nul" : input_config_bind_map_get_base(bind),
            (0.5f - active->mod_x) / active->mod_w,
            (0.5f - active->mod_y) / active->mod_h, rx, rx * aspect);
      config_set_string(conf, key, val);
      snprintf(key, sizeof(key), "overlay%u_desc%u_overlay", dst, to);
      config_set_string(conf, key, "img/button_custom.png");
      snprintf(key, sizeof(key), "overlay%u_desc%u_label", dst, to);
      config_set_string(conf, key, label ? label : "");
      snprintf(key, sizeof(key), "overlay%u_desc%u_custom", dst, to);
      config_set_uint(conf, key, id);
      if (gamepad)
      {
         snprintf(key, sizeof(key), "overlay%u_desc%u_gamepad", dst, to);
         config_set_string(conf, key, gamepad);
      }
      snprintf(key, sizeof(key), "overlay%u_desc%u_layout_added", dst, to);
      config_set_uint(conf, key, (unsigned)added + 1);
   }
   return input_overlay_custom_write(conf, path, ol);
}

/* What the custom button selected sends, @bind or the @gamepad it
 * loads (unless RARCH_BIND_LIST_END and NULL), or its label (unless
 * NULL) */
bool input_overlay_custom_set(unsigned bind, const char *gamepad,
      const char *label)
{
   struct config_entry_list *e;
   char path[PATH_MAX_LENGTH];
   input_overlay_t *ol;
   config_file_t *conf;
   union string_list_elem_attr attr;
   struct string_list *descs;
   size_t i;

   if (     (bind != RARCH_BIND_LIST_END && !input_overlay_custom_input(bind))
         || !input_overlay_custom_sel
         || !(conf = input_overlay_custom_open(&ol, path, sizeof(path))))
      return false;
   if (!(descs = string_list_new()))
   {
      config_file_free(conf);
      return false;
   }
   attr.i = 0;
   for (e = conf->entries; e; e = e->next)
   {
      size_t klen = e->key ? strlen(e->key) : 0;
      if (     klen > STRLEN_CONST("_custom")
            && string_is_equal(e->key + klen - STRLEN_CONST("_custom"),
               "_custom")
            && strtoul(e->value, NULL, 10) == input_overlay_custom_sel)
      {
         char prefix[64];
         strlcpy(prefix, e->key, MIN(sizeof(prefix),
                  klen - STRLEN_CONST("_custom") + 1));
         string_list_append(descs, prefix, attr);
      }
   }
   for (i = 0; i < descs->size; i++)
   {
      char key[80];
      const char *prefix = descs->elems[i].data;
      if (label)
      {
         snprintf(key, sizeof(key), "%s_label", prefix);
         config_set_string(conf, key, label);
      }
      snprintf(key, sizeof(key), "%s_gamepad", prefix);
      if (gamepad)
         config_set_string(conf, key, gamepad);
      else if (bind != RARCH_BIND_LIST_END)
         config_unset(conf, key);
      if (bind != RARCH_BIND_LIST_END || gamepad)
      {
         char desc[256];
         const char *rest;
         if (     !config_get_array(conf, prefix, desc, sizeof(desc))
               || !(rest = strchr(desc, ',')))
            continue;
         {
            char out[300];
            snprintf(out, sizeof(out), "%s%s",
                  gamepad ? "nul" : input_config_bind_map_get_base(bind),
                  rest);
            config_set_string(conf, prefix, out);
         }
      }
   }
   string_list_free(descs);
   return input_overlay_custom_write(conf, path, ol);
}

bool input_overlay_custom_remove(void)
{
   unsigned page, pages = 0;
   char path[PATH_MAX_LENGTH];
   input_overlay_t *ol;
   config_file_t *conf;

   if (     !input_overlay_custom_sel
         || !(conf = input_overlay_custom_open(&ol, path, sizeof(path))))
      return false;
   config_get_uint(conf, "overlays", &pages);
   for (page = 0; page < pages; page++)
   {
      unsigned d, count = 0;
      char key[64];
      snprintf(key, sizeof(key), "overlay%u_descs", page);
      config_get_uint(conf, key, &count);
      for (d = count; d-- > 0; )
      {
         unsigned id = 0;
         snprintf(key, sizeof(key), "overlay%u_desc%u_custom", page, d);
         if (     config_get_uint(conf, key, &id)
               && id == input_overlay_custom_sel)
            input_overlay_custom_drop(conf, page, d);
      }
   }
   input_overlay_custom_sel = 0;
   return input_overlay_custom_write(conf, path, ol);
}

/* The name of the gamepad shown, or "" */
size_t input_overlay_layout_profile_current(char *s, size_t len)
{
   input_overlay_t *ol = input_overlay_layout_target();
   *s = '\0';
   if (!ol || !ol->path || !input_overlay_is_profile(ol->path))
      return 0;
   strlcpy(s, path_basename(ol->path), len);
   path_remove_extension(s);
   return strlen(s);
}

struct string_list *input_overlay_layout_profile_list(bool builtin)
{
   unsigned i;
   char dir[DIR_MAX_LENGTH];
   char path[PATH_MAX_LENGTH];
   union string_list_elem_attr attr;
   struct string_list *files;
   struct string_list *names = string_list_new();

   if (!names)
      return NULL;
   attr.i = 0;
   input_overlay_profiles_ensure();
   if (builtin)
      for (i = 0; i < ARRAY_SIZE(input_overlay_builtin_profiles); i++)
      {
         input_overlay_profile_file(path, sizeof(path),
               input_overlay_builtin_profiles[i]);
         if (path_is_valid(path))
            string_list_append(names, input_overlay_builtin_profiles[i], attr);
      }

   input_overlay_profiles_dir(dir, sizeof(dir));
   if ((files = dir_list_new(dir, "cfg", false, false, false, false)))
   {
      dir_list_sort_ignore_ext(files, false);
      for (i = 0; i < files->size; i++)
      {
         char name[NAME_MAX_LENGTH];
         strlcpy(name, path_basename(files->elems[i].data), sizeof(name));
         path_remove_extension(name);
         if (input_overlay_builtin_profile_find(name) < 0)
            string_list_append(names, name, attr);
      }
      string_list_free(files);
   }
   return names;
}

/* Puts built-in profile @i back as its overlay makes it */
static bool input_overlay_builtin_profile_reset(unsigned i)
{
   char pack[PATH_MAX_LENGTH];
   char out[PATH_MAX_LENGTH];
   if (!input_overlay_builtin_pack(i, pack, sizeof(pack)))
      return false;
   input_overlay_profile_file(out, sizeof(out),
         input_overlay_builtin_profiles[i]);
   return input_overlay_profile_make(out, pack, NULL, 0);
}

/* The overlay's config as it loads, with the user layouts over it */
static config_file_t *input_overlay_layout_open(const char *overlay_path)
{
   unsigned scope;
   char path[PATH_MAX_LENGTH];
   config_file_t *conf = config_file_new_from_path_to_string(overlay_path);

   /* A profile has its layout in it */
   for (scope = input_overlay_is_profile(overlay_path)
         ? OVERLAY_LAYOUT_SCOPE_LAST : 0;
         conf && scope < OVERLAY_LAYOUT_SCOPE_LAST; scope++)
   {
      config_file_t *layout;
      input_overlay_layout_path(path, sizeof(path), overlay_path,
            (enum overlay_layout_scope)scope);
      if (     *path && path_is_valid(path)
            && (layout = config_file_new_from_path_to_string(path)))
         config_file_append_conf(conf, layout);
   }
   return conf;
}

/* Copies control @add onto the active page, in the middle of the
 * screen, as descs the layout of the scope chosen adds. The overlay
 * has them once loaded again. */
static bool input_overlay_layout_add_control(input_overlay_t *ol,
      const input_overlay_addable_t *add_in)
{
   input_overlay_addable_t add = *add_in;
   size_t d, src;
   uint16_t group;
   char key[64];
   char val[32];
   char prefix[32];
   char path[PATH_MAX_LENGTH];
   const struct overlay *from;
   const struct overlay *active;
   struct config_entry_list *entry;
   unsigned dst;
   const struct overlay_desc *first = NULL;
   const struct overlay_desc *model = NULL;
   float x0 = 1e9f, x1 = -1e9f, y0 = 1e9f, y1 = -1e9f;
   float dx, dy;
   bool ret                = false;
   config_file_t *merged   = NULL;
   config_file_t *layout   = NULL;

   src   = add.page;
   group = add.group;

   input_overlay_layout_file(path, sizeof(path), ol->path);
   if (     !*path
         || !(merged = input_overlay_layout_open(ol->path)))
      return false;
   if (     !path_is_valid(path)
         || !(layout = config_file_new_from_path_to_string(path)))
      layout = config_file_new_alloc();
   if (!layout)
   {
      config_file_free(merged);
      return false;
   }

   from   = &ol->overlays[src];
   active = ol->active;

   for (d = 0; d < from->size; d++)
   {
      const struct overlay_desc *desc = &from->descs[d];
      if (add.desc >= 0 ? (int)d != add.desc : desc->layout_group != group)
         continue;
      if (!first)
         first = desc;
      x0 = MIN(x0, desc->x_shift - desc->range_x);
      x1 = MAX(x1, desc->x_shift + desc->range_x);
      y0 = MIN(y0, desc->y_shift - desc->range_y);
      y1 = MAX(y1, desc->y_shift + desc->range_y);
   }
   if (!first)
   {
      config_file_free(layout);
      config_file_free(merged);
      return false;
   }
   /* Where a page of its shape has it already, so they stay one;
    * else the middle of the screen, where nothing is meant to be */
   for (dst = 0; dst < ol->size && !model; dst++)
   {
      const struct overlay *page = &ol->overlays[dst];
      if (     page == active
            || !input_overlay_page_alike(page, active))
         continue;
      for (d = 0; d < page->size && !model; d++)
         if (     page->descs[d].layout_added
               && input_overlay_layout_same_buttons(&page->descs[d], first))
            model = &page->descs[d];
   }
   if (model)
   {
      dx = model->x_shift - first->x_shift;
      dy = model->y_shift - first->y_shift;
   }
   else
   {
      dx = (0.5f - active->mod_x) / active->mod_w - (x0 + x1) * 0.5f;
      dy = (0.5f - active->mod_y) / active->mod_h - (y0 + y1) * 0.5f;
   }

   /* The page shown, and those of its shape, as moves and sizes carry
    * to them; not a page that only hides the controls */
   for (dst = 0; dst < ol->size; dst++)
   {
      const struct overlay *page = &ol->overlays[dst];
      bool has                   = false;
      unsigned count             = 0;
      uint16_t added             = 0;

      if (page != active
            && (  !input_overlay_page_alike(page, active)
               || !(page->flags & OVERLAY_TAKES_OTHER_INPUT)))
         continue;
      for (d = 0; d < page->size; d++)
      {
         if (page->descs[d].layout_added > added)
            added = page->descs[d].layout_added;
         if (     page != active
               && page->descs[d].layout_added
               && input_overlay_layout_same_buttons(&page->descs[d], first))
            has = true;
      }
      if (has)
         continue;
      added++;

      for (d = 0; d < from->size; d++)
      {
         size_t _len;
         const struct overlay_desc *desc = &from->descs[d];
         unsigned to                     = (unsigned)page->size + count;

         if (add.desc >= 0 ? (int)d != add.desc : desc->layout_group != group)
            continue;

         _len = snprintf(prefix, sizeof(prefix), "overlay%u_desc%u",
               (unsigned)src, (unsigned)d);
         for (entry = merged->entries; entry; entry = entry->next)
         {
            const char *suffix;
            if (     !entry->key || !entry->value
                  || strncmp(entry->key, prefix, _len))
               continue;
            suffix = entry->key + _len;
            if (     (*suffix && *suffix != '_')
                  || string_starts_with(suffix, "_layout_"))
               continue;
            /* A cluster's button is reached through its area; alone it
             * needs its own */
            if (     add.desc >= 0
                  && string_starts_with(suffix, "_reach_"))
               continue;
            snprintf(key, sizeof(key), "overlay%u_desc%u%s", dst, to, suffix);
            config_set_string(layout, key, entry->value);
         }

         snprintf(key, sizeof(key), "overlay%u_desc%u_layout_x", dst, to);
         snprintf(val, sizeof(val), "%.6f", desc->x_shift + dx);
         config_set_string(layout, key, val);
         snprintf(key, sizeof(key), "overlay%u_desc%u_layout_y", dst, to);
         snprintf(val, sizeof(val), "%.6f", desc->y_shift + dy);
         config_set_string(layout, key, val);
         if (model && add.desc >= 0)
         {
            snprintf(key, sizeof(key), "overlay%u_desc%u_layout_w", dst, to);
            snprintf(val, sizeof(val), "%.6f", model->range_x);
            config_set_string(layout, key, val);
            snprintf(key, sizeof(key), "overlay%u_desc%u_layout_h", dst, to);
            snprintf(val, sizeof(val), "%.6f", model->range_y);
            config_set_string(layout, key, val);
         }
         snprintf(key, sizeof(key), "overlay%u_desc%u_layout_added", dst, to);
         snprintf(val, sizeof(val), "%u", (unsigned)added);
         config_set_string(layout, key, val);
         count++;
      }

      snprintf(key, sizeof(key), "overlay%u_descs", dst);
      snprintf(val, sizeof(val), "%u", (unsigned)page->size + count);
      config_set_string(layout, key, val);
   }

   {
      char dir[DIR_MAX_LENGTH];
      fill_pathname_basedir(dir, path, sizeof(dir));
      if (!path_is_directory(dir))
         path_mkdir(dir);
   }
   ret = config_file_write(layout, path, true);
   config_file_free(layout);
   config_file_free(merged);
   if (ret && input_overlay_is_profile(path))
      input_overlay_profile_normalize_file(path);
   return ret;
}
/* A pack just loaded: its added buttons fixed up, and the lists of its
 * controls made again */
void input_overlay_layout_loaded(input_overlay_t *ol)
{
   input_overlay_layout_reach_singles(ol);
   input_overlay_loads++;
}

/* The page shown before the layout changed, again */
void input_overlay_layout_restore_page(input_overlay_t *ol)
{
   unsigned page;
   if (input_overlay_layout_reload_page < 0)
      return;
   page                             = (unsigned)input_overlay_layout_reload_page;
   input_overlay_layout_reload_page = -1;
   if (     input_driver_st.overlay_ptr == ol
         && page < ol->size
         && ol->active != &ol->overlays[page])
   {
      bool rotate    = false;
      ol->next_index = page;
      command_event(CMD_EVENT_OVERLAY_NEXT, &rotate);
   }
#ifdef HAVE_MENU
   menu_state_get_ptr()->flags |= MENU_ST_FLAG_ENTRIES_NEED_REFRESH;
#endif
}

#undef input_driver_st

#endif
