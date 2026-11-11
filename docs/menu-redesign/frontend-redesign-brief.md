# Brief: front-end / menu redesign for RetroArch Custom (Android)

## Context

- **Repo:** https://github.com/Ocelot08/retroarch-nl-custom, a fork of RetroArch for Android phones used in portrait (tested on a Galaxy S24). Package `com.retroarch.custom`, app name "RetroArch Custom".
- **Branches:**
  - `merge-upstream` is the current work: the fork plus a merge of upstream RetroArch.
  - `custom` is waiting to be fast-forwarded to it once the last test APK checks out.
- **Rules:** read AGENTS.md (C89, short comments, stage files by name, run the related CI jobs).
  - Don't build the APK until asked.
  - Keep the signing key and password out of the repo.

### What the fork already changes (not merged upstream)

The touch-controls work is described in the README under "What's new in this fork". In short:

- **Touch Layout menu:** Edit Layout, Toggle Controls, Custom Buttons, Button Style, Save/Load/Delete/Reset Gamepad, Export/Import Gamepads.
- **Layout editor:** drag on a grid, size and shape sliders, and a box for the game screen.
- **"Gamepads":** overlay presets kept in `overlays/gamepads/custom/*.cfg`, chosen per core or game through ordinary RetroArch overrides (`input_overlay`).
  - Built-ins: Default (the shipped `pkg/android/phoenix/system_overlays/Default.cfg`), Nintendo DS and Nintendo 3DS.
- **Drawn buttons:** buttons drawn in code (Outline/Filled, stroke 0–20 px, fill opacity), plus custom buttons (any input, hotkey or gamepad switch).
- **Code:** mostly `input/input_overlay_layout.c`, `input/input_overlay_draw.c` and `tasks/task_overlay.c`, plus small hooks in `input/input_driver.c`.
  - The new menu entries follow RetroArch's usual multi-file menu pattern: msg_hash, `menu_cbs_*`, `menu_displaylist.c`, `intl/msg_hash_lbl.h` and `intl/msg_hash_us.h`.
- **Android defaults:** RGUI with the Highlighter theme, touch controls shown over the menu, and no automatic per-system overlay.

## Goal of this project

A new front end and menu organisation that's easier on a phone:

- short top-level menus;
- the everyday settings up front;
- advanced settings one tap further away;
- a first-run setup for cores, BIOS and the games folder.

**Constraint:** layer on top of RetroArch and connect to its existing parts. Don't change its architecture: no new settings systems, no changes to how settings, overrides, cores or content loading work.

## Research already done

- **The current menus were crawled from the running app:** about 110 lists, about 1,070 rows, five levels deep. They are mapped in Figma: https://www.figma.com/design/LVjpP2ficnkwSXrLQrceU7/Testing-MCP?node-id=70-1543
- **Findings:**
  - Settings alone has 18 categories and about 74 lists.
  - Eight lists are reachable from two places: On-Screen Overlay, Touch Layout, Keyboard Overlay, Latency, Rewind, Turbo Fire, Menu Controls and Updater Settings.
  - Touch Layout's pages sit at level 5 from the Quick Menu.
  - Only 113 rows are hidden by "Show Advanced Settings".
- **Proposed L1/L2 structure:** https://www.figma.com/design/LVjpP2ficnkwSXrLQrceU7/Testing-MCP?node-id=79-4579
  - **Home (no game):** Continue, Library (merging Load Content, History, Favorites and Playlists), Gamepads (Touch Layout moved up), Settings, Downloads (Online Updater), Help & About.
  - **Game menu (replaces Quick Menu):** Resume, Save State, Load State, State Slot, Gamepad, Display, Core Options, Keep Settings For… (overrides in plain words), Cheats, More, Reset, Close Game.
  - **Settings:** Display, Audio, Controls, Saving, Interface, Library, Accounts, Accessibility, plus Advanced (Latency, Frame Throttle, Network, Cores, Drivers, Directories, Config Files, Power, Logging).
    - Each everyday category ends with a "More … settings" row that holds its advanced items.
  - **Onboarding, all skippable:**
    1. How you play.
    2. Pick the games folder.
    3. Detected systems, with a recommended core for each.
    4. Download the essentials.
    5. BIOS check.
    6. Scan, then open the Library.
  - **Prompts at the point of need:** empty Library, no core for a file, missing BIOS, updates available.

## Can it be done without changing RetroArch's architecture? Mostly yes, depending on the layer

RetroArch already separates **what a menu list contains** from **how it's drawn**:

- **Content:** `menu/menu_displaylist.c` builds the lists out of the existing settings, actions and commands.
- **Drawing:** the menu drivers in `menu/drivers/` (rgui, ozone, xmb, materialui).
- **The settings themselves don't care which list they appear in.**

So a reorganised menu is new lists that point at the same settings and actions. That is the pattern the Touch Layout menu already uses.

**Options, from least invasive to most:**

1. **Native Android launcher layer (no RetroArch C changes):**
   - **What it covers:** a Kotlin/Java front in `pkg/android/phoenix` for Home, Library and Onboarding.
   - **Starting games:** it launches games through the existing activity intent, with `ROM`, `LIBRETRO` and `CONFIGFILE` extras.
   - **Settings and downloads:** it writes settings to `retroarch.cfg` and override files, and downloads cores from the buildbot itself.
   - **Talking to a running game:** RetroArch's command interface, e.g. `SAVE_STATE_SLOT`, `LOAD_STATE_SLOT`, `MENU_TOGGLE`, `SET_SHADER`, `GET_CONFIG_PARAM`, `SHOW_MSG`. See `command.h`.
   - **Limit:** the in-game menu stays RetroArch's own.
2. **Reorganised menu lists inside RetroArch (small additions, same architecture):**
   - **What changes:** new top-level and category lists in `menu_displaylist.c`, behind one switch such as "Menu layout: Classic / Simple".
   - **Upstream menus stay intact,** so merging upstream stays easy.
   - **Drawing:** works with any menu driver, RGUI included.
3. **A new menu driver:** a new `menu_ctx_driver_t` for a phone-first look. It is visual only, so pair it with option 2 for the structure.

**Decision: options 2 and 3.**

- **Option 2 (structure):** new Home, Game menu and Settings lists behind a "Menu layout" switch, built from the existing settings and actions.
- **Option 3 (look):** a phone-first menu driver in `menu/drivers/` that draws those lists.
- **Order:** do the structure first, so it can be tested in RGUI before the new driver exists. The new driver must also draw the Classic lists, so both layouts work with it.
- **Onboarding:** a short sequence of RetroArch lists using the same pattern, not an Android activity.
- **Option 1 is not planned.**

**Things to settle early:**

- **Where the new home lives:** Android-native, or the RetroArch menu.
- **Whether "Classic" stays reachable.**
- **Onboarding's core list:** where the system-to-recommended-core mapping comes from. Core info files have `supported_extensions`, `firmware*` and `database`; a curated list goes on top.

## First steps for the new chat

1. Read README.md (fork features), AGENTS.md, `menu/menu_displaylist.c` (how Main Menu, Quick Menu and Settings are built) and the Touch Layout menu code as the worked example.
2. Read one existing menu driver end to end (`menu/drivers/rgui.c` is the simplest, `materialui.c` the most touch-oriented) to see what a driver must provide.
3. Write a short plan for options 2 and 3.
4. Prototype Home and the Game menu as new lists behind the layout switch in RGUI.
5. Start the new driver. Test on Linux under Xvfb at 540x1170 (portrait), then on the phone.
