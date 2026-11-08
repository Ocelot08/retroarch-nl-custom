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

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

#include <retro_miscellaneous.h>
#include <string/stdstring.h>

#include "input_overlay_draw.h"
#include "../gfx/font_driver.h"

/* The neo-retropad look: a light outline round a dark middle */
#define OVERLAY_DRAW_LINE 0xC0
#define OVERLAY_DRAW_FILL 0x28

#define OVERLAY_DRAW_FONT_SIZE  64
#define OVERLAY_DRAW_FONT_FIRST 32
#define OVERLAY_DRAW_FONT_COUNT 95

typedef struct
{
   int x, y, w, h;
   int ox, oy;
   int advance;
} overlay_draw_glyph_t;

struct overlay_draw_font
{
   uint8_t *atlas;
   unsigned atlas_w;
   unsigned atlas_h;
   /* Height of a capital, to size labels by */
   int cap;
   overlay_draw_glyph_t glyphs[OVERLAY_DRAW_FONT_COUNT];
};

overlay_draw_font_t *input_overlay_draw_font_new(const char *font_path)
{
   unsigned i;
   const font_renderer_driver_t *drv = NULL;
   void *handle                      = NULL;
   const struct font_atlas *atlas;
   overlay_draw_font_t *font;

   if (!font_renderer_create_default(&drv, &handle, font_path,
            OVERLAY_DRAW_FONT_SIZE, FONT_ATLAS_FORMAT_A8))
      return NULL;
   if (!(font = (overlay_draw_font_t*)calloc(1, sizeof(*font))))
   {
      drv->free(handle);
      return NULL;
   }

   for (i = 0; i < OVERLAY_DRAW_FONT_COUNT; i++)
   {
      const struct font_glyph *g = drv->get_glyph(handle,
            OVERLAY_DRAW_FONT_FIRST + i);
      if (!g)
         continue;
      font->glyphs[i].x       = (int)VIDEO_SCALE_W(g->atlas_pos);
      font->glyphs[i].y       = (int)VIDEO_SCALE_H(g->atlas_pos);
      font->glyphs[i].w       = (int)VIDEO_SCALE_W(g->dims);
      font->glyphs[i].h       = (int)VIDEO_SCALE_H(g->dims);
      font->glyphs[i].ox      = g->draw_offset_x;
      font->glyphs[i].oy      = g->draw_offset_y;
      font->glyphs[i].advance = g->advance_x;
   }
   font->cap = font->glyphs['H' - OVERLAY_DRAW_FONT_FIRST].h;

   atlas = drv->get_atlas(handle);
   if (     atlas && atlas->buffer && atlas->format == FONT_ATLAS_FORMAT_A8
         && font->cap > 0
         && (font->atlas = (uint8_t*)malloc(atlas->width * atlas->height)))
   {
      memcpy(font->atlas, atlas->buffer, atlas->width * atlas->height);
      font->atlas_w = atlas->width;
      font->atlas_h = atlas->height;
   }
   drv->free(handle);

   if (!font->atlas)
   {
      free(font);
      return NULL;
   }
   return font;
}

void input_overlay_draw_font_free(overlay_draw_font_t *font)
{
   if (!font)
      return;
   free(font->atlas);
   free(font);
}

bool input_overlay_draw_classify(const char *action, bool movable,
      const char *label, overlay_draw_t *out)
{
   static const struct
   {
      const char *action;
      enum overlay_draw_shape shape;
      unsigned dir;
      const char *label;
   } buttons[] = {
      { "a",      OVERLAY_DRAW_ROUND, 0, "A"      },
      { "b",      OVERLAY_DRAW_ROUND, 0, "B"      },
      { "x",      OVERLAY_DRAW_ROUND, 0, "X"      },
      { "y",      OVERLAY_DRAW_ROUND, 0, "Y"      },
      { "l",      OVERLAY_DRAW_RECT,  0, "L1"     },
      { "r",      OVERLAY_DRAW_RECT,  0, "R1"     },
      { "l2",     OVERLAY_DRAW_RECT,  0, "L2"     },
      { "r2",     OVERLAY_DRAW_RECT,  0, "R2"     },
      { "l3",     OVERLAY_DRAW_RECT,  0, "L3"     },
      { "r3",     OVERLAY_DRAW_RECT,  0, "R3"     },
      { "start",  OVERLAY_DRAW_ROUND, 0, "START"  },
      { "select", OVERLAY_DRAW_ROUND, 0, "SELECT" },
      { "up",     OVERLAY_DRAW_ARROW, 0, ""       },
      { "down",   OVERLAY_DRAW_ARROW, 1, ""       },
      { "left",   OVERLAY_DRAW_ARROW, 2, ""       },
      { "right",  OVERLAY_DRAW_ARROW, 3, ""       }
   };
   size_t i;

   memset(out, 0, sizeof(*out));
   if (!action || !*action)
      return false;

   if (     string_is_equal(action, "analog_left")
         || string_is_equal(action, "analog_right"))
      out->shape = movable ? OVERLAY_DRAW_KNOB : OVERLAY_DRAW_RING;
   else
   {
      for (i = 0; i < ARRAY_SIZE(buttons); i++)
         if (string_is_equal(action, buttons[i].action))
         {
            out->shape = buttons[i].shape;
            out->dir   = buttons[i].dir;
            strlcpy(out->label, buttons[i].label, sizeof(out->label));
            break;
         }
      if (out->shape == OVERLAY_DRAW_NONE)
         return false;
   }
   if (label && *label)
      strlcpy(out->label, label, sizeof(out->label));
   return true;
}

static float overlay_draw_clamp(float v)
{
   return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}

/* Distance from @x, @y to a rectangle of half sizes @hx, @hy about the
 * middle, corners rounded @r: negative inside */
static float overlay_draw_rounded(float x, float y, float hx, float hy,
      float r)
{
   float qx = (float)fabs(x) - (hx - r);
   float qy = (float)fabs(y) - (hy - r);
   float ox = MAX(qx, 0.0f);
   float oy = MAX(qy, 0.0f);
   return (float)sqrt(ox * ox + oy * oy) + MIN(MAX(qx, qy), 0.0f) - r;
}

/* Distance from @x, @y to a convex polygon of @n points, negative
 * inside, near enough at the edges for their antialiasing */
static float overlay_draw_polygon(float x, float y, const float *pts,
      unsigned n)
{
   unsigned i;
   float cx = 0.0f, cy = 0.0f, d = -1e9f;

   for (i = 0; i < n; i++)
   {
      cx += pts[2 * i];
      cy += pts[2 * i + 1];
   }
   cx /= n;
   cy /= n;
   for (i = 0; i < n; i++)
   {
      float ax = pts[2 * i], ay = pts[2 * i + 1];
      float bx = pts[2 * ((i + 1) % n)], by = pts[2 * ((i + 1) % n) + 1];
      float nx = by - ay, ny = ax - bx;
      float len = (float)sqrt(nx * nx + ny * ny);
      float e;
      if (len <= 0.0f)
         continue;
      nx /= len;
      ny /= len;
      /* Outward: away from the middle */
      if ((cx - ax) * nx + (cy - ay) * ny > 0.0f)
      {
         nx = -nx;
         ny = -ny;
      }
      e = (x - ax) * nx + (y - ay) * ny;
      if (e > d)
         d = e;
   }
   return d;
}

/* @u, @v of a unit box pointing up, turned for @dir, as offsets of
 * @w x @h */
static void overlay_draw_turn(unsigned dir, float u, float v,
      float w, float h, float *x, float *y)
{
   switch (dir)
   {
      case 1: /* down */
         *x = u * w;
         *y = -v * h;
         break;
      case 2: /* left: up turned, across */
         *x = v * w;
         *y = u * h;
         break;
      case 3: /* right */
         *x = -v * w;
         *y = u * h;
         break;
      default:
         *x = u * w;
         *y = v * h;
         break;
   }
}

/* An arrow pointing up, its point down to the D-pad's middle, filling
 * @w x @h; and its small triangle, @tri_w x @tri_h however the arrow
 * is shaped. Turned for the other directions. */
static void overlay_draw_arrow_points(unsigned dir, float w, float h,
      float tri_w, float tri_h, float *body, float *tri)
{
   static const float up_body[] = {
      0.04f, 0.04f,  0.96f, 0.04f,  0.96f, 0.64f,  0.5f, 0.97f,
      0.04f, 0.64f };
   /* About the triangle's middle, which is 0.38 down the arrow */
   static const float up_tri[] = {
      0.0f, -0.18f,  0.24f, 0.09f,  -0.24f, 0.09f };
   float mx, my;
   unsigned i;

   for (i = 0; i < 5; i++)
   {
      overlay_draw_turn(dir, up_body[2 * i] - 0.5f,
            up_body[2 * i + 1] - 0.5f, w, h, &body[2 * i],
            &body[2 * i + 1]);
      body[2 * i]     += w * 0.5f;
      body[2 * i + 1] += h * 0.5f;
   }
   overlay_draw_turn(dir, 0.0f, 0.38f - 0.5f, w, h, &mx, &my);
   for (i = 0; i < 3; i++)
   {
      overlay_draw_turn(dir, up_tri[2 * i], up_tri[2 * i + 1],
            tri_w, tri_h, &tri[2 * i], &tri[2 * i + 1]);
      tri[2 * i]     += w * 0.5f + mx;
      tri[2 * i + 1] += h * 0.5f + my;
   }
}

/* @label's coverage over the w x h @cover, centred, its capitals
 * @cap_px high or less where it is wide */
static void overlay_draw_label(float *cover, unsigned w, unsigned h,
      const char *label, const overlay_draw_font_t *font, float cap_px,
      float max_w)
{
   const unsigned char *c;
   float s, pen, top = 1e9f, bottom = -1e9f, width = 0.0f;

   if (!font || !label || !*label)
      return;

   s = cap_px / font->cap;
   for (c = (const unsigned char*)label; *c; c++)
   {
      const overlay_draw_glyph_t *g;
      if (*c < OVERLAY_DRAW_FONT_FIRST
            || *c >= OVERLAY_DRAW_FONT_FIRST + OVERLAY_DRAW_FONT_COUNT)
         continue;
      g     = &font->glyphs[*c - OVERLAY_DRAW_FONT_FIRST];
      width += g->advance;
      if (g->h > 0)
      {
         top    = MIN(top, (float)g->oy);
         bottom = MAX(bottom, (float)(g->oy + g->h));
      }
   }
   if (width <= 0.0f || bottom <= top)
      return;
   if (width * s > max_w)
      s = max_w / width;

   pen = (w - width * s) * 0.5f;
   for (c = (const unsigned char*)label; *c; c++)
   {
      int dx, dy, gx0, gy0, gx1, gy1;
      const overlay_draw_glyph_t *g;
      if (*c < OVERLAY_DRAW_FONT_FIRST
            || *c >= OVERLAY_DRAW_FONT_FIRST + OVERLAY_DRAW_FONT_COUNT)
         continue;
      g   = &font->glyphs[*c - OVERLAY_DRAW_FONT_FIRST];
      /* The glyph's box on @cover, its baseline put so the label's
       * ink is in the middle */
      gx0 = (int)floor(pen + g->ox * s);
      gy0 = (int)floor(h * 0.5f + (g->oy - (top + bottom) * 0.5f) * s);
      gx1 = (int)ceil(pen + (g->ox + g->w) * s);
      gy1 = (int)ceil(h * 0.5f + (g->oy + g->h - (top + bottom) * 0.5f) * s);
      for (dy = MAX(gy0, 0); dy < MIN(gy1, (int)h); dy++)
         for (dx = MAX(gx0, 0); dx < MIN(gx1, (int)w); dx++)
         {
            /* Bilinear, from the glyph's cell */
            float u  = (dx + 0.5f - (pen + g->ox * s)) / s - 0.5f;
            float v  = (dy + 0.5f - (h * 0.5f + (g->oy
                        - (top + bottom) * 0.5f) * s)) / s - 0.5f;
            int iu   = (int)floor(u);
            int iv   = (int)floor(v);
            float fu = u - iu, fv = v - iv;
            float a  = 0.0f;
            int k;
            for (k = 0; k < 4; k++)
            {
               int su = iu + (k & 1);
               int sv = iv + (k >> 1);
               float wgt = ((k & 1) ? fu : 1.0f - fu)
                  * ((k >> 1) ? fv : 1.0f - fv);
               if (su < 0 || sv < 0 || su >= g->w || sv >= g->h)
                  continue;
               a += wgt * font->atlas[(g->y + sv) * font->atlas_w
                  + g->x + su] / 255.0f;
            }
            if (a > cover[dy * w + dx])
               cover[dy * w + dx] = overlay_draw_clamp(a);
         }
      pen += g->advance * s;
   }
}

bool input_overlay_draw_render(struct texture_image *img,
      const overlay_draw_t *draw, const overlay_draw_font_t *font)
{
   unsigned x, y;
   float body[10], tri[6];
   float *cover;
   uint32_t *pixels;
   unsigned w      = draw->width;
   unsigned h      = draw->height;
   float hx        = w * 0.5f - 1.0f;
   float hy        = h * 0.5f - 1.0f;
   float short_px  = (float)MIN(w, h);
   float stroke    = MAX(1.5f, short_px * 0.055f);
   float radius    = MIN(hx, hy);
   bool hollow     = draw->shape == OVERLAY_DRAW_RING;
   /* How much bigger than the overlay made it, along its longer side:
    * Shape only narrows the other, so this is the button's Size */
   float pack_w    = draw->pack_width  ? (float)draw->pack_width  : (float)w;
   float pack_h    = draw->pack_height ? (float)draw->pack_height : (float)h;
   float grow      = (float)MAX(w, h) / MAX(pack_w, pack_h);

   if (w < 4 || h < 4 || w > 2048 || h > 2048)
      return false;
   if (!(pixels = (uint32_t*)malloc(w * h * sizeof(uint32_t))))
      return false;
   if (!(cover = (float*)calloc(w * h, sizeof(float))))
   {
      free(pixels);
      return false;
   }

   if (draw->shape == OVERLAY_DRAW_RECT)
      radius = short_px * 0.16f;
   else if (draw->shape == OVERLAY_DRAW_RING)
      stroke = MAX(1.5f, short_px * 0.02f);
   if (draw->shape == OVERLAY_DRAW_ARROW)
      overlay_draw_arrow_points(draw->dir, (float)w, (float)h,
            pack_w * grow * 0.85f, pack_h * grow * 0.85f, body, tri);

   /* The label, or the arrow's triangle, as coverage */
   if (draw->shape == OVERLAY_DRAW_ARROW)
   {
      for (y = 0; y < h; y++)
         for (x = 0; x < w; x++)
            cover[y * w + x] = overlay_draw_clamp(0.5f
                  - overlay_draw_polygon(x + 0.5f, y + 0.5f, tri, 3));
   }
   else if (!hollow)
   {
      /* Short labels by their height, long ones by the room across;
       * as the button was made, grown with its Size, and kept inside it
       * however narrow its Shape makes it */
      float cap = MIN(pack_w, pack_h) * grow
         * (strlen(draw->label) > 2 ? 0.255f : 0.39f);
      cap       = MIN(cap, short_px * 0.72f);
      overlay_draw_label(cover, w, h, draw->label, font, cap,
            (draw->shape == OVERLAY_DRAW_RECT ? 0.8f : 0.72f) * w);
   }

   for (y = 0; y < h; y++)
      for (x = 0; x < w; x++)
      {
         float px = x + 0.5f - w * 0.5f;
         float py = y + 0.5f - h * 0.5f;
         float d, outer, inner, value, alpha, text;
         unsigned v, a;

         if (draw->shape == OVERLAY_DRAW_ARROW)
            d = overlay_draw_polygon(x + 0.5f, y + 0.5f, body, 5);
         else
            d = overlay_draw_rounded(px, py, hx, hy, radius);

         outer = overlay_draw_clamp(0.5f - d);
         inner = overlay_draw_clamp(0.5f - (d + stroke));
         if (hollow)
         {
            value = OVERLAY_DRAW_LINE;
            alpha = outer * (1.0f - inner);
         }
         else
         {
            value = OVERLAY_DRAW_LINE
               + (OVERLAY_DRAW_FILL - OVERLAY_DRAW_LINE) * inner;
            alpha = outer;
         }
         text  = cover[y * w + x] * inner;
         value = value + (OVERLAY_DRAW_LINE - value) * text;

         v = (unsigned)(value + 0.5f);
         a = (unsigned)(alpha * 255.0f + 0.5f);
         /* Grey, so ARGB and RGBA are the same but for where alpha is,
          * the top byte of both as a 32-bit word */
         pixels[y * w + x] = (a << 24) | (v << 16) | (v << 8) | v;
      }

   free(cover);
   img->pixels = pixels;
   img->width  = w;
   img->height = h;
   return true;
}
