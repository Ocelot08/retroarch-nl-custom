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

APKs built from the menu redesign work are labelled as RGUI tests until
the new menu driver exists: the app name and the APK file name both say
"RetroArch Custom (RGUI test)", set at build time, not in
`pkg/android/phoenix/build.gradle`. The Simple layout is turned on in
Settings › User Interface › Menu Layout.

## Simple menu driver (style test)

`menu/drivers/simple.c` is a new menu driver, `simple`, chosen like the
others in Settings › Drivers › Menu. It draws whatever list is open with
the Simple type and spacing: Work Sans Light for words, IBM Plex Mono for
notes and values, sizes in dp on a 360 dp wide design. Use it with Menu
Layout set to Simple. It takes taps and the menu buttons; it has no drag
scrolling, dialogs, keyboard, images or art yet.

The fonts are read from `<assets>/simple/`:

- `WorkSans-Light.ttf` (Work Sans, SIL Open Font Licence 1.1)
- `IBMPlexMono-Medium.ttf` (IBM Plex Mono, SIL Open Font Licence 1.1)

They are not in the repository yet. If either is missing the driver uses
the default font. An APK with this driver needs them added to its assets.
