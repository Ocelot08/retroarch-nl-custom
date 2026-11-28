# Quick Setup: plan

A first pass is built (below); the rest is a plan. The steps are those in `ia-v2.md`
(Quick Setup); this says how each would be built, in what order, and what
is still to decide.

## How it is built

- **A page per step.** Quick Setup is a short run of the menu's own lists,
  one question per page, as the brief asks: no Android activity. Each step
  is one new list (`DISPLAYLIST_SIMPLE_SETUP`, with the step number held by
  the menu), drawn by whichever menu driver is in use.
- **On a page:** the question as the page's description, the answers as
  rows, and Skip as the last row. Choosing an answer saves it and goes on to
  the next step, with the page move. Back goes to the step before.
- **Remembered:** a new saved setting records that Quick Setup has been done
  or skipped, so that Help and the Classic Main Menu can offer it again
  without it starting by itself each time.
- **Where it starts:** Help › Getting Started in Simple, and the Classic
  Main Menu (`ia-v2.md`, decision 8).

## The steps, and what each uses

| Step | What it sets or does | Already in RetroArch | New |
|---|---|---|---|
| 1. Welcome: touch, controller or both | Touch controls on or off, Hide When Controller Connected, Gamepad Mode | All three settings | The page |
| 2. Menu style: Simple or Classic | `menu_layout`; Simple writes its defaults once (`ia-v2.md`, Defaults written by Simple) | The settings | A record that the defaults were written, so they are written once |
| 3. Menu button (controller or both) | The menu's controller combo, or keep Back | The combo setting and its choices | The page |
| 4. Your games | The folder games are scanned from | The folder picker | Remembering the folder for step 10 |
| 5. Systems and cores | One recommended emulator per system found in the folder | Core info (`supported_extensions`, `database`); the core downloader | Which emulator to recommend for each system (see To decide) |
| 6. Essentials | Core info, databases, assets, controller profiles; box art optional | The Online Updater's own actions | A progress page (template T5, not built) |
| 7. BIOS check | Missing system files for the chosen emulators | `core_info_list_update_missing_firmware` | The page, MISSING or FOUND per file |
| 8. How saving works | One screen of text | | The text |
| 9. Remove Quick Setup from the Main Menu? (Classic only) | Whether the Main Menu shows it | Menu Item Visibility | A visibility setting for Quick Setup |
| 10. Done | Scan the folder, then open Games | `task_push_dbscan` | |

## Order of work

Each phase can be tried on the phone without starting a game.

1. **The run itself:** the saved setting, Help › Getting Started, and steps
   1, 2, 3 and 8 with Skip and Back. Nothing is downloaded.
2. **Games folder and scan:** steps 4 and 10.
3. **Essentials:** step 6, once the progress page exists.
4. **Systems, emulators and BIOS:** steps 5 and 7, once the recommendation
   list is decided.
5. **Classic:** the Main Menu entry and step 9.

## Decided (28 Nov 2026)

1. **Starting by itself:** once, the first time Simple is at Home with no
   game, with Skip on every page; also from Help › Getting Started and,
   later, the Main Menu.
2. **Recommended emulators:** a short list kept in this project, one core
   per system.
3. **Downloads:** asked once, before the first.
4. **Existing users:** settings they have changed are kept; Simple's
   defaults do not overwrite them.
5. **The games folder on Android:** Android's picker. The scanner reads a
   `saf://` folder: on the phone it listed all 66 files of a folder of 32
   zips, opening each zip.

## First pass (built)

Only what gets someone playing; the words are placeholders, for Nick to
write. Pages, each with Skip Setup: Welcome (Start), Your Games (Choose
Games Folder, Next), Cores (the systems found, each with its core,
Installed or To download; Download Cores; Next), Done (Open Games).

- The step is the list's path; the code is `action_ok_simple_setup` and
  the `menu_cbs_simple_setup_*` functions in `menu_cbs_ok.c`, and
  `menu_displaylist_simple_setup` in `menu_displaylist.c`.
- `menu_simple_setup_done` is set by Skip and Open Games.
- Android's picker gives its folder to Quick Setup through
  `android_saf_setup_ask` and `android_saf_setup_take` in
  `platform_unix.c`, in place of opening it in the file browser. Elsewhere
  Choose Games Folder opens Scan Directory.
- A system is found when its database playlist (`<system>.lpl`) exists.
- The list of cores, `simple_setup_cores`, is a placeholder.

Tested on the phone: it opens by itself and not after Skip or Done; Back
through each step; the picker, the scan and the Cores page; Open Games;
Help › Getting Started. Not tested: Download Cores (not tapped); the Cores
page with games found, since the test app has no databases, so nothing
matched.

Still to decide: the list of cores; whether it should open for people
already using Simple, as it does now; the breadcrumb, which repeats
"Quick Setup" for each step.
