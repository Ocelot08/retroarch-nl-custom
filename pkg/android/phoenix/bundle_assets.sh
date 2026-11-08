#!/bin/sh
# Fills assets/ with what the app extracts to its data directory on first
# run (see bundle_assets_* in frontend/drivers/platform_unix.c): menu
# assets with this build's RGUI themes, overlays, Android controller
# profiles and core info files.
# Usage: bundle_assets.sh [download dir]
set -eu

here=$(cd "$(dirname "$0")" && pwd)
src=${1:-"$here/build/bundle-src"}
dst="$here/assets"

mkdir -p "$src"
for repo in retroarch-assets common-overlays \
            retroarch-joypad-autoconfig libretro-core-info; do
   if [ -d "$src/$repo/.git" ]; then
      git -C "$src/$repo" pull -q --ff-only
   else
      git clone -q --depth 1 "https://github.com/libretro/$repo.git" "$src/$repo"
   fi
done

rm -rf "$dst"
mkdir -p "$dst/assets" "$dst/overlays" "$dst/autoconfig" "$dst/info"

# As retroarch-assets' own install, less what only other platforms use
(cd "$src/retroarch-assets" && tar cf - --exclude=.git --exclude=src \
   --exclude=Makefile --exclude=configure --exclude=ctr --exclude=switch \
   --exclude=nxrgui --exclude=pkg --exclude=scripts .) \
   | (cd "$dst/assets" && tar xf -)
(cd "$src/common-overlays" && tar cf - --exclude=.git .) \
   | (cd "$dst/overlays" && tar xf -)
cp -R "$src/retroarch-joypad-autoconfig/android" "$dst/autoconfig/"
cp "$src/libretro-core-info/"*.info "$dst/info/"
# RGUI theme presets of this build; Highlighter is the default
cp "$here/rgui_themes/"*.cfg "$dst/assets/rgui/"
# This build's DS overlay, the DS layout with the neo-retropad buttons
# (made by ds_overlay/make_ds_overlay.py), in place of common-overlays'
mkdir -p "$dst/overlays/gamepads/neo-ds/img"
cp "$here/ds_overlay/img/"*.png "$dst/overlays/gamepads/neo-ds/img/"
for name in "Nintendo - Nintendo DS" "Nintendo - Nintendo DSi"; do
   cp "$here/ds_overlay/Nintendo - Nintendo DS.cfg" \
      "$dst/overlays/gamepads/Named_Overlays/$name.cfg"
done

du -sh "$dst"/*
