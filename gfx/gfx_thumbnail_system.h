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

#ifndef __GFX_THUMBNAIL_SYSTEM_H
#define __GFX_THUMBNAIL_SYSTEM_H

/* The thumbnail server's name for a system, for a list called something
 * shorter. Header only: some tests link its users on their own. */

#include <stddef.h>
#include <string.h>

/* Other names, then file types only this system has: lower case, letters
 * and digits only, a space around each. A name without its maker is known
 * already. */
static const char * const gfx_thumbnail_systems[][3] = {
   { "Nintendo - Game Boy",           " gb dmg ",              " gb " },
   { "Nintendo - Game Boy Color",     " gbc cgb gameboycolour ", " gbc " },
   { "Nintendo - Game Boy Advance",   " gba agb ",             " gba " },
   { "Nintendo - Nintendo DS",        " ds nds ",              " nds " },
   { "Nintendo - Nintendo DSi",       " dsi ",                 " " },
   { "Nintendo - Nintendo 3DS",       " 3ds n3ds ",            " 3ds " },
   { "Nintendo - Nintendo Entertainment System",
      " nes fc famicom ",             " nes " },
   { "Nintendo - Family Computer Disk System",
      " fds famicomdisksystem ",      " fds " },
   { "Nintendo - Super Nintendo Entertainment System",
      " snes sfc supernintendo superfamicom supernes ", " sfc smc " },
   { "Nintendo - Nintendo 64",        " n64 ",                 " n64 z64 v64 " },
   { "Nintendo - Nintendo 64DD",      " 64dd n64dd ",          " ndd " },
   { "Nintendo - GameCube",           " gc gcn ngc ",          " gcm " },
   { "Nintendo - Wii",                " ",                     " wbfs " },
   { "Nintendo - Wii U",              " ",                     " wux " },
   { "Nintendo - Virtual Boy",        " vb ",                  " vb " },
   { "Nintendo - Pokemon Mini",       " pokemini ",            " min " },
   { "Nintendo - Satellaview",        " bsx ",                 " " },
   { "Sega - Mega Drive - Genesis",
      " md gen genesis megadrive segagenesis segamegadrive smd ", " gen smd " },
   { "Sega - Master System - Mark III",
      " sms mastersystem segamastersystem markiii ", " sms " },
   { "Sega - Mega-CD - Sega CD",      " segacd megacd scd ",   " " },
   { "Sega - 32X",                    " ",                     " 32x " },
   { "Sega - Game Gear",              " gg ",                  " gg " },
   { "Sega - Saturn",                 " ss ",                  " " },
   { "Sega - Dreamcast",              " dc ",                  " gdi " },
   { "Sega - SG-1000",                " ",                     " sg " },
   { "Sega - Naomi",                  " ",                     " " },
   { "Sony - PlayStation",            " ps1 psx psone ps ",    " " },
   { "Sony - PlayStation 2",          " ps2 ",                 " " },
   { "Sony - PlayStation Portable",   " psp ",                 " " },
   { "NEC - PC Engine - TurboGrafx 16",
      " pce pcengine tg16 turbografx turbografx16 ", " pce " },
   { "NEC - PC Engine CD - TurboGrafx-CD",
      " pcecd pcenginecd turbografxcd tgcd ", " " },
   { "NEC - PC Engine SuperGrafx",    " sgx supergrafx ",      " sgx " },
   { "NEC - PC-FX",                   " ",                     " " },
   { "SNK - Neo Geo",                 " ng aes ",              " " },
   { "SNK - Neo Geo CD",              " ngcd neocd ",          " " },
   { "SNK - Neo Geo Pocket",          " ngp ",                 " ngp " },
   { "SNK - Neo Geo Pocket Color",    " ngpc neogeopocketcolour ", " ngc ngpc " },
   { "Bandai - WonderSwan",           " ws ",                  " ws " },
   { "Bandai - WonderSwan Color",     " wsc wonderswancolour ", " wsc " },
   { "Atari - 2600",                  " a2600 vcs ",           " a26 " },
   { "Atari - 5200",                  " a5200 ",               " a52 " },
   { "Atari - 7800",                  " a7800 ",               " a78 " },
   { "Atari - Lynx",                  " ",                     " lnx " },
   { "Atari - Jaguar",                " ",                     " j64 " },
   { "Coleco - ColecoVision",         " coleco ",              " col " },
   { "Mattel - Intellivision",        " intv ",                " " },
   { "GCE - Vectrex",                 " ",                     " vec " },
   { "Commodore - 64",                " c64 ",                 " d64 " },
   { "Commodore - Amiga",             " ",                     " adf " },
   { "Microsoft - MSX",               " msx1 ",                " mx1 " },
   { "Microsoft - MSX2",              " ",                     " mx2 " },
   { "Amstrad - CPC",                 " ",                     " cdt " },
   { "Sinclair - ZX Spectrum",        " zx ",                  " tzx " },
   { "Sharp - X68000",                " x68k ",                " " }
};

/* Lower case letters and digits of @src, between two spaces */
static void gfx_thumbnail_system_key(char *dst, size_t len,
      const char *src, size_t src_len)
{
   size_t i;
   size_t n = 0;
   dst[n++] = ' ';
   for (i = 0; i < src_len && src[i] && n + 2 < len; i++)
   {
      char c = src[i];
      if (c >= 'A' && c <= 'Z')
         c = (char)(c - 'A' + 'a');
      if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))
         dst[n++] = c;
   }
   dst[n++] = ' ';
   dst[n]   = '\0';
}

/* @name's system under its full name, NULL if @name is one already or the
 * system is not known. What the file is, where its type says, goes before
 * what the list is called. */
static const char *gfx_thumbnail_system_full(const char *name,
      const char *content_path)
{
   size_t i;
   char key[64];
   const char *ext = content_path ? strrchr(content_path, '.') : NULL;
   size_t count    = sizeof(gfx_thumbnail_systems)
      / sizeof(gfx_thumbnail_systems[0]);

   if (!name || !*name || strstr(name, " - "))
      return NULL;
   if (ext && !strchr(ext, '/'))
   {
      gfx_thumbnail_system_key(key, sizeof(key), ext, strlen(ext));
      if (key[1] != ' ')
         for (i = 0; i < count; i++)
            if (strstr(gfx_thumbnail_systems[i][2], key))
               return gfx_thumbnail_systems[i][0];
   }
   gfx_thumbnail_system_key(key, sizeof(key), name, strlen(name));
   if (key[1] == ' ')
      return NULL;
   for (i = 0; i < count; i++)
   {
      char full[64];
      const char *system = gfx_thumbnail_systems[i][0];
      const char *model  = strstr(system, " - ") + 3;
      if (strstr(gfx_thumbnail_systems[i][1], key))
         return system;
      gfx_thumbnail_system_key(full, sizeof(full), system, strlen(system));
      if (!strcmp(full, key))
         return system;
      gfx_thumbnail_system_key(full, sizeof(full), model, strlen(model));
      /* (but not "64", which two makers have) */
      if (strlen(full) > 4 && !strcmp(full, key))
         return system;
   }
   return NULL;
}

#endif
