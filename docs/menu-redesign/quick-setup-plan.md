# Quick Setup: plan

A plan only; nothing here is built yet. The steps are those in `ia-v2.md`
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

## To decide

1. **Starting by itself:** should Quick Setup open on its own the first
   time Simple is chosen, or only from Help and the Main Menu?
2. **Recommended emulators:** a short list kept in this project (one
   emulator per system), or worked out from the core info files? The brief
   leaves this open.
3. **Downloads:** steps 5 and 6 download several megabytes. Ask once before
   the first download, or say so on the page and let Skip be the way out?
4. **Existing users:** if someone has already changed a setting that
   Simple's defaults would write, keep theirs or apply Simple's?
5. **The games folder on Android:** Android's own folder picker gives a
   `saf://` folder, which this fork reads through
   `vfs_implementation_saf.c`. Whether the scanner works from one has not
   been tried. Use Android's picker if it does, else RetroArch's file
   browser?
