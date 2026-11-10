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

#ifndef INPUT_OVERLAY_LAYOUT_H__
#define INPUT_OVERLAY_LAYOUT_H__

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* What input_driver.c and input_overlay_layout.c, the layout editor
 * and gamepads, have of each other */

#ifdef ANDROID
/* This build's own, for the few systems the main overlay does not fit;
 * every other system has the main overlay and its touch layouts */
#define SYSTEM_OVERLAY_DIR "gamepads/Named_Overlays_Custom"
#else
#define SYSTEM_OVERLAY_DIR "gamepads/Named_Overlays"
#endif

/* input_driver.c */
int16_t input_frame_or_driver(input_driver_t *input, void *data,
      const input_device_driver_t *joypad,
      rarch_joypad_info_t *joypad_info,
      const retro_keybind_set *binds, bool keyboard_mapping_blocked,
      unsigned port, unsigned device, unsigned idx, unsigned id);
const unsigned *input_overlay_led_map(void);
const char *input_overlay_path(bool want_osk);
void input_overlay_viewport_publish(void);
void input_overlay_deinit(void);

/* input_overlay_layout.c */
const char *input_overlay_gamepad_path(const char *pack);
void input_overlay_edit_poll(input_overlay_t *ol, float opacity);
void input_overlay_edit_detach(input_overlay_t *ol);
void input_overlay_layout_reach_singles(input_overlay_t *ol);
void input_overlay_layout_loaded(input_overlay_t *ol);
void input_overlay_layout_restore_page(input_overlay_t *ol);

RETRO_END_DECLS

#endif
