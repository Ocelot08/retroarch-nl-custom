# Menu redesign

Working files for the phone-first menu redesign.

| File | What it is |
|---|---|
| `frontend-redesign-brief.md` | The brief: goal, constraints, build approach and first steps |
| `ia-current.csv` | Crawl of the current menus on Android, 4 levels below Main Menu |
| `ia-v1.md` | Proposed structure, v1: Claude Code's first pass |
| `ia-v2.md` | Proposed structure, v2: the merged direction, the chosen designs and the decisions since v1 |

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
menu buttons; the breadcrumb line goes back. Choices step from small
arrows with 48dp targets, numbers open a slider row, and prompts show as
a card with answer rows. It has no keyboard, images or art yet.

In the Simple layout, Settings › Controls, Screen, Sound and Saving are
short lists of the common settings under plain names, each ending in a
More row that opens the classic list.

Gamepad Mode (Help, or Settings › User Interface) draws the menu in a 4:3
box at the top, smaller, with button hints, and shows the touch controls
below it to work the menu with. Without it the menu fills the screen and
the touch controls are hidden while the menu is open. Other drivers keep
"Hide Overlay in Menu" as it is.

The fonts are in `pkg/android/phoenix/simple_fonts/` with their licences
(SIL Open Font Licence 1.1); `bundle_assets.sh` copies them to
`<assets>/simple/`, where the driver reads them. Work Sans is the Latin
subset. If a font is missing the driver uses the default font.
