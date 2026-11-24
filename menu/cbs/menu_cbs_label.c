/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#include <compat/strl.h>
#include <file/file_path.h>
#include <string/stdstring.h>

#include "../menu_driver.h"
#include "../menu_cbs.h"
#include "../../configuration.h"
#include "../../file_path_special.h"
#ifdef HAVE_CHEATS
#include "../../cheat_manager.h"
#endif

#ifndef BIND_ACTION_LABEL
#define BIND_ACTION_LABEL(cbs, name) (cbs)->action_label = (name)
#endif

#define FILL_LABEL_MACRO(func, lbl) \
static int (func)(file_list_t *list, unsigned type, unsigned i, const char *label, const char *path, char *s, size_t len) \
{ \
   strlcpy(s, msg_hash_to_str(lbl), len); \
   return 0; \
}

static int action_bind_label_generic(
      file_list_t *list,
      unsigned type, unsigned i,
      const char *label, const char *path,
      char *s, size_t len)
{
   return 0;
}

/* Plain names for the settings the Simple layout's short lists show */
static const enum msg_hash_enums simple_names[][2] = {
   { MENU_ENUM_LABEL_INPUT_OVERLAY_ENABLE,
      MENU_ENUM_LABEL_VALUE_SIMPLE_NAME_TOUCH_CONTROLS },
   { MENU_ENUM_LABEL_INPUT_MENU_ENUM_TOGGLE_GAMEPAD_COMBO,
      MENU_ENUM_LABEL_VALUE_SIMPLE_NAME_MENU_BUTTON },
   { MENU_ENUM_LABEL_ENABLE_DEVICE_VIBRATION,
      MENU_ENUM_LABEL_VALUE_SIMPLE_NAME_VIBRATION },
   { MENU_ENUM_LABEL_MENU_INPUT_SWAP_OK_CANCEL,
      MENU_ENUM_LABEL_VALUE_SIMPLE_NAME_SWAP_OK_BACK },
   { MENU_ENUM_LABEL_VIDEO_SMOOTH,
      MENU_ENUM_LABEL_VALUE_SIMPLE_NAME_SMOOTH },
   { MENU_ENUM_LABEL_VIDEO_ASPECT_RATIO_INDEX,
      MENU_ENUM_LABEL_VALUE_SIMPLE_NAME_SCREEN_SHAPE },
   { MENU_ENUM_LABEL_SCREEN_ORIENTATION,
      MENU_ENUM_LABEL_VALUE_SIMPLE_NAME_ORIENTATION },
   { MENU_ENUM_LABEL_VIDEO_NOTCH_WRITE_OVER,
      MENU_ENUM_LABEL_VALUE_SIMPLE_NAME_CAMERA_AREA },
   { MENU_ENUM_LABEL_FPS_SHOW,
      MENU_ENUM_LABEL_VALUE_SIMPLE_NAME_SHOW_FPS },
   { MENU_ENUM_LABEL_AUDIO_VOLUME,
      MENU_ENUM_LABEL_VALUE_SIMPLE_NAME_VOLUME },
   { MENU_ENUM_LABEL_AUDIO_MUTE,
      MENU_ENUM_LABEL_VALUE_SIMPLE_NAME_MUTE },
   { MENU_ENUM_LABEL_AUDIO_ENABLE_MENU,
      MENU_ENUM_LABEL_VALUE_SIMPLE_NAME_MENU_SOUNDS },
   { MENU_ENUM_LABEL_AUDIO_FASTFORWARD_MUTE,
      MENU_ENUM_LABEL_VALUE_SIMPLE_NAME_MUTE_FAST_FORWARD },
   { MENU_ENUM_LABEL_SAVESTATE_AUTO_LOAD,
      MENU_ENUM_LABEL_VALUE_SIMPLE_NAME_RESUME },
   { MENU_ENUM_LABEL_SAVESTATE_AUTO_SAVE,
      MENU_ENUM_LABEL_VALUE_SIMPLE_NAME_SAVE_ON_CLOSE },
   { MENU_ENUM_LABEL_AUTOSAVE_INTERVAL,
      MENU_ENUM_LABEL_VALUE_SIMPLE_NAME_SAVE_EVERY },
   { MENU_ENUM_LABEL_SAVESTATE_AUTO_INDEX,
      MENU_ENUM_LABEL_VALUE_SIMPLE_NAME_NUMBER_SAVES },
   { MENU_ENUM_LABEL_SAVESTATE_MAX_KEEP,
      MENU_ENUM_LABEL_VALUE_SIMPLE_NAME_KEEP_SAVES },
   { MENU_ENUM_LABEL_THUMBNAILS,
      MENU_ENUM_LABEL_VALUE_SIMPLE_NAME_SHOW_BOX_ART },
   { MENU_ENUM_LABEL_RGUI_BROWSER_DIRECTORY,
      MENU_ENUM_LABEL_VALUE_SIMPLE_NAME_GAMES_FOLDER },
   { MENU_ENUM_LABEL_NETPLAY_NICKNAME,
      MENU_ENUM_LABEL_VALUE_SIMPLE_NAME_NICKNAME },
   { MENU_ENUM_LABEL_ACCESSIBILITY_ENABLED,
      MENU_ENUM_LABEL_VALUE_SIMPLE_NAME_SCREEN_READER },
   { MENU_ENUM_LABEL_ACCESSIBILITY_NARRATOR_SPEECH_SPEED,
      MENU_ENUM_LABEL_VALUE_SIMPLE_NAME_READING_SPEED },
   { MENU_ENUM_LABEL_AI_SERVICE_SETTINGS,
      MENU_ENUM_LABEL_VALUE_SIMPLE_NAME_AI_TRANSLATION },
   { MENU_ENUM_LABEL_USER_LANGUAGE,
      MENU_ENUM_LABEL_VALUE_SIMPLE_NAME_LANGUAGE },
   { MENU_ENUM_LABEL_MENU_LAYOUT,
      MENU_ENUM_LABEL_VALUE_SIMPLE_NAME_MENU_STYLE },
   { MENU_ENUM_LABEL_ONSCREEN_NOTIFICATIONS_SETTINGS,
      MENU_ENUM_LABEL_VALUE_SIMPLE_NAME_NOTIFICATIONS },
   { MENU_ENUM_LABEL_MENU_ENABLE_KIOSK_MODE,
      MENU_ENUM_LABEL_VALUE_SIMPLE_NAME_PARENTAL_LOCK },
   { MENU_ENUM_LABEL_INPUT_TURBO_FIRE_SETTINGS,
      MENU_ENUM_LABEL_VALUE_SIMPLE_NAME_TURBO },
   { MENU_ENUM_LABEL_INPUT_RETROPAD_BINDS,
      MENU_ENUM_LABEL_VALUE_SIMPLE_NAME_SET_UP_CONTROLLER },
   { MENU_ENUM_LABEL_INPUT_HOTKEY_BINDS,
      MENU_ENUM_LABEL_VALUE_SIMPLE_NAME_HOTKEYS },
   { MENU_ENUM_LABEL_NETWORK_ON_DEMAND_THUMBNAILS,
      MENU_ENUM_LABEL_VALUE_SIMPLE_NAME_ART_BROWSING },
   { MENU_ENUM_LABEL_PLAYLIST_MANAGER_LIST,
      MENU_ENUM_LABEL_VALUE_SIMPLE_NAME_CLEAN_UP },
};

static int action_bind_label_simple_name(
      file_list_t *list,
      unsigned type, unsigned i,
      const char *label, const char *path,
      char *s, size_t len)
{
   size_t j;
   const char *row = (list && i < list->size) ? list->list[i].label : NULL;
   if (     !row
         || config_get_ptr()->uints.menu_layout != MENU_LAYOUT_SIMPLE)
      return 0;
   for (j = 0; j < ARRAY_SIZE(simple_names); j++)
      if (string_is_equal(row, msg_hash_to_str(simple_names[j][0])))
      {
         strlcpy(s, msg_hash_to_str(simple_names[j][1]), len);
         break;
      }
   return 0;
}

FILL_LABEL_MACRO(action_bind_label_rdb_entry_detail,         MENU_ENUM_LABEL_VALUE_RDB_ENTRY_DETAIL)
FILL_LABEL_MACRO(action_bind_label_internal_memory,          MSG_INTERNAL_STORAGE)
FILL_LABEL_MACRO(action_bind_label_removable_storage,        MSG_REMOVABLE_STORAGE)
FILL_LABEL_MACRO(action_bind_label_external_application_dir, MSG_EXTERNAL_APPLICATION_DIR)
FILL_LABEL_MACRO(action_bind_label_application_dir,          MSG_APPLICATION_DIR)
#if defined(ANDROID)
FILL_LABEL_MACRO(action_bind_label_video_windows_enable_android, MENU_ENUM_LABEL_VALUE_VIDEO_WINDOWS_ENABLE_ANDROID)
#endif

static int action_bind_label_playlist_collection_entry(
      file_list_t *list,
      unsigned type, unsigned i,
      const char *label, const char *path,
      char *s, size_t len)
{
   if (path && *path)
   {
      const char *playlist_file = path_basename_nocompression(path);

      if (playlist_file && *playlist_file)
      {
         if (string_is_equal_noncase(path_get_extension(playlist_file),
                  "lpl"))
         {
            if (string_is_equal(playlist_file, FILE_PATH_CONTENT_HISTORY))
               strlcpy(s, msg_hash_to_str(
                     config_get_ptr()->uints.menu_layout == MENU_LAYOUT_SIMPLE
                     ? MENU_ENUM_LABEL_VALUE_SIMPLE_RECENT
                     : MENU_ENUM_LABEL_VALUE_HISTORY_TAB), len);
            else if (string_is_equal(playlist_file, FILE_PATH_CONTENT_FAVORITES))
               strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_FAVORITES_TAB), len);
            else if (string_is_equal(playlist_file, FILE_PATH_CONTENT_IMAGE_HISTORY))
               strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_IMAGES_TAB), len);
            else if (string_is_equal(playlist_file, FILE_PATH_CONTENT_MUSIC_HISTORY))
               strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_MUSIC_TAB), len);
            else if (string_is_equal(playlist_file, FILE_PATH_CONTENT_VIDEO_HISTORY))
               strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_TAB), len);
            else
               fill_pathname(s, playlist_file, "", len);
         }
         /* This should never happen, but if it does just set
          * the label to the file name (it's better than nothing...) */
         else
            strlcpy(s, playlist_file, len);
      }
   }

   return 0;
}

#ifdef HAVE_CHEATS
static int action_bind_label_cheat_browse_address(
      file_list_t *list,
      unsigned type, unsigned i,
      const char *label, const char *path,
      char *s, size_t len)
{
   snprintf(s, len, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CHEAT_BROWSE_MEMORY), cheat_manager_state.browse_address);
   return 0;
}
#endif

int menu_cbs_init_bind_label(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx)
{
   if (!cbs)
      return -1;

   BIND_ACTION_LABEL(cbs, action_bind_label_generic);

   if (cbs->enum_idx == MSG_UNKNOWN)
      return 0;

   switch (cbs->enum_idx)
   {
      case MENU_ENUM_LABEL_INPUT_OVERLAY_ENABLE:
      case MENU_ENUM_LABEL_INPUT_MENU_ENUM_TOGGLE_GAMEPAD_COMBO:
      case MENU_ENUM_LABEL_ENABLE_DEVICE_VIBRATION:
      case MENU_ENUM_LABEL_MENU_INPUT_SWAP_OK_CANCEL:
      case MENU_ENUM_LABEL_VIDEO_SMOOTH:
      case MENU_ENUM_LABEL_VIDEO_ASPECT_RATIO_INDEX:
      case MENU_ENUM_LABEL_SCREEN_ORIENTATION:
      case MENU_ENUM_LABEL_FPS_SHOW:
      case MENU_ENUM_LABEL_AUDIO_VOLUME:
      case MENU_ENUM_LABEL_AUDIO_MUTE:
      case MENU_ENUM_LABEL_AUDIO_ENABLE_MENU:
      case MENU_ENUM_LABEL_AUDIO_FASTFORWARD_MUTE:
      case MENU_ENUM_LABEL_SAVESTATE_AUTO_LOAD:
      case MENU_ENUM_LABEL_SAVESTATE_AUTO_SAVE:
      case MENU_ENUM_LABEL_AUTOSAVE_INTERVAL:
      case MENU_ENUM_LABEL_SAVESTATE_AUTO_INDEX:
      case MENU_ENUM_LABEL_SAVESTATE_MAX_KEEP:
      case MENU_ENUM_LABEL_THUMBNAILS:
      case MENU_ENUM_LABEL_RGUI_BROWSER_DIRECTORY:
      case MENU_ENUM_LABEL_NETPLAY_NICKNAME:
      case MENU_ENUM_LABEL_ACCESSIBILITY_ENABLED:
      case MENU_ENUM_LABEL_ACCESSIBILITY_NARRATOR_SPEECH_SPEED:
      case MENU_ENUM_LABEL_AI_SERVICE_SETTINGS:
      case MENU_ENUM_LABEL_USER_LANGUAGE:
      case MENU_ENUM_LABEL_MENU_LAYOUT:
      case MENU_ENUM_LABEL_ONSCREEN_NOTIFICATIONS_SETTINGS:
      case MENU_ENUM_LABEL_MENU_ENABLE_KIOSK_MODE:
      case MENU_ENUM_LABEL_INPUT_TURBO_FIRE_SETTINGS:
      case MENU_ENUM_LABEL_INPUT_RETROPAD_BINDS:
      case MENU_ENUM_LABEL_INPUT_HOTKEY_BINDS:
      case MENU_ENUM_LABEL_NETWORK_ON_DEMAND_THUMBNAILS:
      case MENU_ENUM_LABEL_PLAYLIST_MANAGER_LIST:
      case MENU_ENUM_LABEL_VIDEO_NOTCH_WRITE_OVER:
         BIND_ACTION_LABEL(cbs, action_bind_label_simple_name);
         break;
      case MENU_ENUM_LABEL_LOAD_CONTENT_HISTORY:
      case MENU_ENUM_LABEL_GOTO_FAVORITES:
      case MENU_ENUM_LABEL_GOTO_IMAGES:
      case MENU_ENUM_LABEL_GOTO_MUSIC:
      case MENU_ENUM_LABEL_GOTO_VIDEO:
      case MENU_ENUM_LABEL_PLAYLIST_COLLECTION_ENTRY:
      case MENU_ENUM_LABEL_PLAYLIST_MANAGER_SETTINGS:
      case MENU_ENUM_LABEL_CONTENT_SETTINGS:
         BIND_ACTION_LABEL(cbs, action_bind_label_playlist_collection_entry);
         break;
      case MENU_ENUM_LABEL_CHEAT_BROWSE_MEMORY:
#ifdef HAVE_CHEATS
         BIND_ACTION_LABEL(cbs, action_bind_label_cheat_browse_address);
#endif
         break;
      case MSG_INTERNAL_STORAGE:
         BIND_ACTION_LABEL(cbs, action_bind_label_internal_memory);
         break;
      case MSG_REMOVABLE_STORAGE:
         BIND_ACTION_LABEL(cbs, action_bind_label_removable_storage);
         break;
      case MSG_APPLICATION_DIR:
         BIND_ACTION_LABEL(cbs, action_bind_label_application_dir);
         break;
      case MSG_EXTERNAL_APPLICATION_DIR:
         BIND_ACTION_LABEL(cbs, action_bind_label_external_application_dir);
         break;
      case MENU_ENUM_LABEL_RDB_ENTRY_DETAIL:
         BIND_ACTION_LABEL(cbs, action_bind_label_rdb_entry_detail);
         break;
      case MENU_ENUM_LABEL_VIDEO_WINDOWS_ENABLE:
#if defined(ANDROID)
         BIND_ACTION_LABEL(cbs, action_bind_label_video_windows_enable_android);
#endif
         break;
      default:
         break;
   }

   return 0;
}
