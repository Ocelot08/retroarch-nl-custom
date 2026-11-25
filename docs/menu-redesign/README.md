# Menu redesign

Working files for the phone-first menu redesign.

| File | What it is |
|---|---|
| `frontend-redesign-brief.md` | The brief: goal, constraints, build approach and first steps |
| `ia-current.csv` | Crawl of the current menus on Android, 4 levels below Main Menu |
| `ia-v1.md` | Proposed structure, v1: Claude Code's first pass |
| `ia-v2.md` | Proposed structure, v2: the merged direction, the chosen designs and the decisions since v1 |
| `next.md` | Notes and features that may come next, not yet built |

Each proposed structure is a separate `ia-vN.md`, so versions can be
compared and tried one at a time.

## History

- 10 Nov 2026, 7:13 PM: work with Claude Code began on the IA, with plans
  for changing the interface and menus.

## Test builds

Test APKs are labelled with what they test, so they can be told apart.

- RGUI test: "RetroArch Custom (RGUI test)", for the Simple layout in
  RGUI. The Simple layout is turned on in Settings › User Interface ›
  Menu Layout.
- Simple test: "RA TESTER", with the logo in orange (#F59F00), for
  the simple menu driver. Built with

  ```
  cd pkg/android/phoenix
  sh bundle_assets.sh
  ./gradlew -PsimpleMenuTest assembleCustomDebug
  ```

  `-PsimpleMenuTest` gives the app its own id (`com.retroarch.custom.simpletest`),
  so it installs beside RetroArch Custom, and makes it start in the
  simple driver with the Simple layout. Its name and orange icon come
  from `resValue` and `res-tester/`, used only with this switch. The
  orange icon shows on Android 8 and later; older versions show the
  normal one.

  Test builds are signed with `~/.android/ra-tester.keystore`, a debug
  key kept outside the repository (another path can be given with
  `-PRA_TESTER_STORE_FILE=...`). Copy that file to every machine that
  builds test APKs, so each build installs over the last one. Without
  it, a build falls back to the machine's own debug key and says so.
  No signing key is in the repository.

  RA TESTER keeps its saves, states, playlists, core settings, system
  files, screenshots and the rest in `RA_TESTER` in the phone's storage
  (`ANDROID_USER_DIR` in `platform_unix.c`), where other builds use
  `RetroArch`. Deleting that folder and uninstalling the app removes all
  of it. An RA TESTER installed before this has the old folders in its
  `retroarch.cfg`, which must be changed there once.

  To check a build on a phone over USB, with the screen unlocked:

  ```
  sh pkg/android/phoenix/test_simple_menu.sh [screenshot folder]
  ```

  It taps through Home, Settings, Help and Gamepad Mode, prints PASS or
  FAIL per step, and saves screenshots and the run's log. It finds rows
  by name from the driver's `RASimple` logcat lines, which only test
  builds write, and never starts a game or quits.

## Rollback

Test apps install beside the working app, never over it: the Simple test
is `com.retroarch.custom.simpletest`, the working app is
`com.retroarch.custom`.

Known-good points from before the menu redesign:

- `d265c2c505`, the tip of `custom` (the default branch) when this work
  started.
- `0596aea4bc`, the last commit on this branch before the redesign
  (10 Nov 2026); everything after it is menu redesign work.

To keep the installed working app, copy its APK off the phone before
testing, and note its version:

```
adb shell dumpsys package com.retroarch.custom | grep -E "versionName|versionCode|lastUpdateTime"
adb shell pm path com.retroarch.custom
adb pull <path printed above> retroarch-custom-known-good.apk
```

To put it back: `adb install -r -d retroarch-custom-known-good.apk`.

## Simple menu driver (style test)

`menu/drivers/simple.c` is a new menu driver, `simple`, chosen like the
others in Settings › Drivers › Menu. It draws whatever list is open with
the Simple type and spacing: Work Sans Light for words, IBM Plex Mono for
notes and values, sizes in dp on a 360 dp wide design. Use it with Menu
Layout set to Simple. It takes taps, swipes (with an edge bounce) and the
menu buttons; the breadcrumb line and Android's Back go back. A setting's
word opens its options under its description; the right side changes it,
or opens the list of a long one. Numbers open a slider row, those with no
set end keep small arrows, and prompts show as a card with answer rows.
Text is typed with the system's keyboard into a box at the top. On
Home the last game's box art (Settings › Library › Show Box Art) is behind
Continue, or colours where it has none; in a wide band, as in landscape, it
is at the right. A list can have a short name: GBA's art, shown or
downloaded, is also that of "Nintendo - Game Boy Advance", by the file's
type or else the list's name (`gfx/gfx_thumbnail_system.h`, in every menu).

The Settings page, Controls, Screen, Saving and Interface have section
labels; the shorter pages have none. A page's last row is drawn as a label
too. "More" opens the rest of the page in place, without what is already
above, with + or −: Downloads, Screen, Sound and Accounts. "All" opens
RetroArch's own list, with ›: Controls, Saving, Library and Interface, and
All Settings on the Settings page. Accessibility has neither. File pickers,
passwords, drivers and rows that show or hide others are only in All.

Settings › Interface › Highlight Color picks one colour (violet unless
changed) for section labels, Continue and the gamepad selection, and for the update notice in
`ia-v2.md` once it is built. The choices are in `simple_accent`.

Lines go only between two rows of the same list or section. Section labels
and More rows have more space above and no line. The top bar has a
line under it, and an option page's description one above and below, which
stay as the rows scroll. On Home, Continue counts as the first row.

Settings › Screen › Orientation (`orientation_lock`) is Auto, Portrait or
Landscape, either way round, for the menu and games. It changes at once,
and again as the video driver starts. RetroArch's Screen Orientation, which
on Android waits for a restart, is under All Settings; a lock overrides it.

A name too wide for its row takes the smaller type, then a second line.
A text value too wide shows as SET, and passwords always do.

In the Simple layout, every Settings category (Controls, Screen, Sound,
Saving, Library, Accounts, Accessibility, Interface), Downloads and the
Game menu's Controls and Screen are short lists of the common settings under plain
names, most ending in a More or All row for the rest (below). RetroArch's
own lists, and the More rows, are drawn in smaller type. On the Settings
page a category's note names in turn the settings on its page, a moment
later from row to row, and holds still after a touch. Games has Game
Lists, RetroArch's playlists, ending in Edit Game Lists. Downloads has
headings over its groups, and its More row opens in place:
the rest of the Online Updater, without what is already above. The
breadcrumb shows the whole path,
the middle levels as an ellipsis when it is too long. Help has Switch to
Classic Menu; in the Classic layout the Main Menu lists Settings, so the
way back is Settings › User Interface › Menu Layout.

Choosing the Simple layout draws the menu behind a camera cutout
(Settings › Screen › Use Camera Area); the header moves clear of it, and
a game's picture stays below it. A game always has touch controls, the
default ones if its own cannot be read, or Back to open the Game menu.
Settings changed before a game starts are saved first when that game's
core has settings of its own. Leaving the Game menu's Controls or Screen
with settings changed opens Keep Changes?: For this game, For all ‹System›
games or Just for now, which any other way out also is. Only what was
changed goes into the override (`config_keep_*`); remaps go into their own
files. While these pages are open `retroarch.cfg` is not written. A game that cannot be opened says why,
and offers to choose its folder.

Gamepad Mode (Help, or Settings › Interface) draws the menu in a 4:3
box, smaller, with button hints, and shows the touch controls to work the
menu with. The box is at the top with the controls below it, or in
landscape in the middle with the controls at the sides, the same page
enlarged to the screen's height. Touch mode in a window too short for Home,
as in landscape, uses a shorter header and Continue. Without it the menu fills the screen and
the touch controls are hidden while the menu is open. Other drivers keep
"Hide Overlay in Menu" as it is. The line at the left of the selected row
is for Gamepad Mode; in touch mode it shows only while a controller's
up and down are used, until the next touch.

The fonts are in `pkg/android/phoenix/simple_fonts/` with their licences
(SIL Open Font Licence 1.1); `bundle_assets.sh` copies them to
`<assets>/simple/`, where the driver reads them. Work Sans is the Latin
subset. If a font is missing the driver uses the default font.
