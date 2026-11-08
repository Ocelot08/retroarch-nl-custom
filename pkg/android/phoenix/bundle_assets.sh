#!/bin/sh
# Fills assets/ with what the app extracts to its data directory on first
# run (see bundle_assets_* in frontend/drivers/platform_unix.c): menu
# assets, overlays, Android controller profiles and core info files.
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

du -sh "$dst"/*
