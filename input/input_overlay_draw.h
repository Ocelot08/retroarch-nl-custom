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

#ifndef INPUT_OVERLAY_DRAW_H__
#define INPUT_OVERLAY_DRAW_H__

#include <stdint.h>
#include <boolean.h>
#include <retro_common_api.h>

#include <formats/image.h>

RETRO_BEGIN_DECLS

/* Overlay buttons drawn in code in place of their images: shapes and
 * labels made at the size they are shown, so they stay sharp and keep
 * their shape however they are sized. */

enum overlay_draw_shape
{
   OVERLAY_DRAW_NONE = 0,
   /* A round button, a capsule when it is not square */
   OVERLAY_DRAW_ROUND,
   /* A rectangle with rounded corners */
   OVERLAY_DRAW_RECT,
   /* A D-pad arrow, pointing up, down, left or right */
   OVERLAY_DRAW_ARROW,
   /* An analog stick's ring, and its knob */
   OVERLAY_DRAW_RING,
   OVERLAY_DRAW_KNOB
};

typedef struct
{
   enum overlay_draw_shape shape;
   /* For an arrow: 0 up, 1 down, 2 left, 3 right */
   unsigned dir;
   unsigned width;
   unsigned height;
   /* Its size as the overlay made it, in the same pixels: the label
    * and arrow follow the button's size but not its shape */
   unsigned pack_width;
   unsigned pack_height;
   /* The outline's width in the same pixels, the same for every
    * button whatever its size; 0 for a width by its size */
   float stroke;
   /* Filled: a light button with dark label, rather than a dark one
    * with a light outline and label */
   bool filled;
   char label[16];
} overlay_draw_t;

/* Printable ASCII, rendered once on the main thread, so that buttons
 * can be drawn where the font driver may not be used */
typedef struct overlay_draw_font overlay_draw_font_t;

overlay_draw_font_t *input_overlay_draw_font_new(const char *font_path);
void input_overlay_draw_font_free(overlay_draw_font_t *font);

/* What a desc doing @action (as overlay configs write it, "a" or
 * "left|up") is drawn as, if it is one of the buttons drawn: @movable
 * tells an analog stick's knob from its ring. @label, when not empty,
 * is used in place of the one @action gives. */
bool input_overlay_draw_classify(const char *action, bool movable,
      const char *label, overlay_draw_t *out);

/* @img's pixels (ARGB, or RGBA as @img->supports_rgba says) made for
 * @draw. False when they could not be had. */
bool input_overlay_draw_render(struct texture_image *img,
      const overlay_draw_t *draw, const overlay_draw_font_t *font);

RETRO_END_DECLS

#endif
