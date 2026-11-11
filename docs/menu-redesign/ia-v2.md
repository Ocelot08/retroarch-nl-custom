# Proposed menu structure (IA) v2: RetroArch Custom

- **Status:** v2, after the merged brief and the design rounds in Figma. For review; no code yet.
- **Follows:** `frontend-redesign-brief.md` and the merged brief "Merged Simple Menu IA" (a Claude Doc). `ia-v1.md` is kept for comparison.
- **Current menus:** `ia-current.csv`.
- **Figma** (file "Testing-MCP", page "Retroarch"):
  - [Chosen set](https://www.figma.com/design/LVjpP2ficnkwSXrLQrceU7/Testing-MCP?node-id=146-6374): one design per page type, by level and frequency.
  - [Pages](https://www.figma.com/design/LVjpP2ficnkwSXrLQrceU7/Testing-MCP?node-id=148-6440): the chosen set applied across this IA.
  - The option rounds the choices came from, from [Home layout options](https://www.figma.com/design/LVjpP2ficnkwSXrLQrceU7/Testing-MCP?node-id=91-5976) down.
  - References, at the far left: [current RetroArch menus](https://www.figma.com/design/LVjpP2ficnkwSXrLQrceU7/Testing-MCP?node-id=116-6308) and [its menu drivers](https://www.figma.com/design/LVjpP2ficnkwSXrLQrceU7/Testing-MCP?node-id=117-6308).

## Decisions since v1

1. **Who it's for:** most people: add games, start one, keep progress, make it look right. Power users keep Classic and the other menu drivers.
2. **Don't re-home advanced settings.** Settings ends in **All Settings**, which opens the classic Settings list unchanged. Curated "More" lists only for Controls, Screen, Sound and Saving; every other More row opens the existing classic list.
3. **Plain names:** Games, Emulators (cores), Screen (video), Sound, Game Options (core options), Touch Controls, System Files (BIOS), Box Art (thumbnails).
4. **Home has five rows.** Touch Controls is not on Home; it lives in Settings › Controls and the Game menu's Controls.
5. **The Game menu has a Home row**, for bigger changes during a game.
6. **Keeping changes is asked, not found in a menu** (see Keeping changes).
7. **Two menu modes, one structure.** Touch mode (full screen) and gamepad mode (condensed into the game screen's box above the touch gamepad). A **Menu Mode** setting picks Touch, Gamepad or Auto.
8. **Quick Setup is reached from the Classic Main Menu**, since Simple is not the default, and from Help › Getting Started in Simple.
9. **Art only where there's a game.** The art header is used on Home (the last game), Games and its lists, a game's detail page and the Game menu pages. Settings, Downloads and Help have plain headers.

## Visual rules

- **Words:** Work Sans Light. **Notes and values:** IBM Plex Mono small capitals, the size of the "SNES · 2 H AGO" line. Both are under the SIL Open Font Licence.
- **Yellow** means "needs you" (updates, missing files) and, in gamepad mode, the selection line. Nothing else is yellow.
- **Rows:** a word on the left, and on the right its live note, its value, or nothing. Lists end in a More or All row set slightly apart.
- **Gamepad mode:** the page condensed into the game screen's box, full width and up to the top edge; the selection is a yellow line; button hints along the bottom of the box.

## Templates (the chosen set)

| Page type | Template | From option |
|---|---|---|
| Home | Type and box art; touch and gamepad mode | Home 9, 10 |
| Game menu | Over the paused game; gamepad mode | GM1, GM5 |
| Games | List with covers | Games 2 |
| Settings, Downloads, Help (L2 and deeper) | Header and list; plain header outside games | L2-A |
| Values | Toggle and number on the row; short choice opens below; visual choice gets a page of previews; long choice is RetroArch's list | P1, P2, P4 |
| Game detail | Full-bleed art | D2 |
| Save slots | List with screenshots | S1 |
| Prompts | Sheet from the bottom | T2 |
| Quit, delete, reset | Whole-screen question | T3 |
| Button capture | Press a button, with countdown | T4 |
| Progress | Progress page; notices during play | T5, T6 |
| Step flows | One question per screen | Q1 |
| Read-only facts | Table | I1 |
| Long lists | Search and status | L1 |
| Typing | Android keyboard | K1 |

## Home (no game running)

```
Home
├─ Continue: <last game>       resumes mid-game; "Back to Game" while one is running
├─ Games ›                     8 SYSTEMS (one system: 412 GAMES)
├─ Downloads ›                 24 EMULATORS, or 3 UPDATES in yellow
├─ Settings ›                  rotates through its categories
└─ Help ›                      rotates through help topics; later "+ QUIT"
```

**Right-side notes:**

- **Games:** the number of systems; with only one system, the number of games.
- **Downloads:** the installed emulators; when updates exist, their number in yellow.
- **Settings:** names its categories in turn, about 4 s each with a short fade. It stops while being navigated or after a touch, and follows Android's "Remove animations".
- **Help:** help topics until Quick Setup is done and about 5 games have been started (from the play history), then "+ QUIT".

## Game menu (game running)

```
Game menu                      over the paused game
├─ Resume
├─ Save Progress               auto-numbered, with a screenshot
├─ Load Progress ›             saves with screenshots and times; Undo Load at the top
├─ Change Disc ›               only for multi-disc games
├─ Controls ›                  Show Touch Controls, Touch Layout, Edit Touch Layout, Change Buttons, Turbo, More
├─ Screen ›                    Screen Style, Screen Shape, Orientation, More (shaders, scaling)
├─ Game Options ›              core options
├─ More ›                      Fast-Forward & Rewind, Cheats, Achievements, Take Screenshot,
│                              Add to Favorites, Restart Game, Saved Settings for This Game,
│                              Latency, Game Info
├─ Home                        the top of the menu
└─ Close Game
```

## Games

```
Games
├─ Recent ›                    covers under the word; a cover starts its game
├─ Favorites ›                 covers under the word
├─ Systems ›                   each system with a fan of covers and its count
│  └─ <System> ›               its games A–Z; sort and search in the header
├─ Add Games Folder            scan; note "2 D AGO"
├─ Play Online ›               netplay lobby: rich rows
└─ More ›                      Browse Files, Load Core, Import, Explore, Contentless Cores,
                               Images/Music/Video, Load Disc, Dump Disc
```

A game's detail page (D2): Play, Favorite, Emulator, Information, Remove. A game list row: cover, title, system and last played.

## Downloads

```
Downloads
├─ Update Everything           3 UPDATES (yellow); progress page while running
├─ Emulators ›                 filters: For Your Games, Installed, Updates, All; search
├─ Missing System Files ›      per emulator: file, MISSING (yellow) or FOUND; Open System Folder
├─ Box Art ›                   Get Missing Box Art; Download While Browsing; counts per system
└─ More ›                      → Online Updater
```

## Settings

```
Settings
├─ Controls ›       Touch Controls ›, Set Up Controller, Menu Button, Vibration,
│                   Swap OK and Back, Hotkeys ›, More Controls Settings (curated)
├─ Screen ›         Screen Style › (previews), Screen Shape, Orientation, Show FPS,
│                   More Screen Settings (curated)
├─ Sound ›          Volume, Mute, Menu Sounds, Fast-Forward Sound, More Sound Settings (curated)
├─ Saving ›         Resume Where I Left Off, Save In-Game Progress Every, Number Saves
│                   Automatically, Keep Saves, Cloud Sync ›, More Saving Settings (curated)
├─ Library ›        Show Box Art, Download Missing Box Art, Rescan Games, Games Folder,
│                   Clean Up Lists, More → Playlists
├─ Accounts ›       RetroAchievements ›, Nickname, More → User
├─ Accessibility ›  Screen Reader, Reading Speed, AI Translation ›, More → Accessibility
├─ Interface ›      Language, Menu Style (Simple / Classic), Menu Mode (Touch / Gamepad / Auto),
│                   Ask Before Keeping Changes, Notifications ›, Parental Lock,
│                   More → User Interface
└─ All Settings ›   → classic Settings, unchanged
```

## Help

```
Help
├─ Getting Started             reruns Quick Setup
├─ How to Open the Menu ›      shown on the gamepad; change the method
├─ Something's Not Working ›   Controller, Picture, Sound, Game won't start, This Device,
│                              Save a Log File, Reset All Settings (whole-screen question)
├─ Switch to Classic Menu
├─ About ›                     version and credits (table)
├─ Restart RetroArch
└─ Quit RetroArch
```

## Keeping changes

- **When it asks:** on leaving Controls, Screen or Game Options during a game, if a setting there changed. RetroArch already flags every menu change (`SETTINGS_FLG_MODIFIED`); core options keep their own flag.
- **The prompt (T2):**
  - **For this game:** Save Game Overrides, plus the game's remap and options.
  - **For all ‹System› games:** the same at core level.
  - **Just for now:** reverted when the game closes. Backing out of the prompt counts as this.
- **No "For all games":** global changes are made from Home › Settings with no game running.
- **Just for now uses the existing override reload.** `retroarch.cfg` is saved when the game starts. The session is then marked so that closing the game reloads it, as closing a game with its own settings already does (`config_unload_override`).
- **The banner** on the Game menu's settings pages, and on Settings opened from Home during a game, names the most specific settings in use:

| Loaded for the game | Banner |
|---|---|
| Game settings | Changes here are for this game |
| Core settings only | Changes here are for all ‹System› games |
| Just for now | Changes here last until you close the game |
| Nothing of its own | No banner |

## Quick Setup

One question per screen; every step skippable.

1. **Welcome:** how will you play? Touch, controller or both. Sets the touch control defaults and the Menu Mode.
2. **Menu style:** Simple or Classic. Simple writes the defaults below, once.
3. **Menu button** (controller or both): pick the combo, or keep Back.
4. **Your games:** choose the folder, or later.
5. **Systems and emulators:** one recommended emulator per system found.
6. **Essentials:** core info, databases, assets, controller profiles; box art optional.
7. **BIOS check:** missing system files per chosen emulator.
8. **How saving works:** one screen.
9. **Remove Quick Setup from the Main Menu?** Classic only. "You can show it again in Settings › User Interface › Menu Item Visibility." Also asked when the setup is skipped.
10. **Done:** scan the folder and open Games.

## Defaults written by Simple

Checked against the Android build. `config_save_on_exit` and Touch Support are already on.

| Setting | Android today | Simple |
|---|---|---|
| `savestate_auto_save`, `savestate_auto_load` | off | on (Resume Where I Left Off) |
| `savestate_auto_index` | off | on (Number Saves Automatically) |
| `savestate_max_keep` | 0 (all) | a limit, e.g. 10 |
| `savestate_thumbnail_enable` | off on ARM | on (Load Progress needs it) |
| `menu_singleclick_playlists` | off | on |
| `input_overlay_hide_when_gamepad_connected` | off | on (Show Touch Controls: Auto) |

## Missing and error states

- **No games:** Add Games Folder button, or Browse Files.
- **No box art:** a cover set in type until art is fetched; Get Missing Box Art on the list.
- **No emulator for a file:** a prompt offering the recommended one.
- **Missing BIOS:** names the file and the folder it goes in.
- **Game didn't start:** the reason in plain words, with Try Another Emulator and Save a Log File.
- **First game:** a short notice says how to open the menu.

## Page types still to design, in order

1. Quick Setup steps after the first.
2. Gamepad-mode versions of the Downloads and Help branches.
3. Read-only pages beyond About and This Device.
4. Typing for platforms without Android's keyboard.

The pages behind More and All Settings are RetroArch's own lists and keep its existing look.

## Mapping to RetroArch

- **Menu Style:** a new setting, `menu_layout` (Classic / Simple), in the existing settings system. **Menu Mode** is a second one (Touch / Gamepad / Auto).
- **Home:** a branch inside `DISPLAYLIST_MAIN_MENU`. Every menu driver builds its root list through it, so the root label stays the same.
- **Game menu:** a branch inside `DISPLAYLIST_CONTENT_SETTINGS`, keeping its label. Drivers recognise the Quick Menu by that label, for the game title and save thumbnails.
- **Settings:** a branch inside `DISPLAYLIST_SETTINGS_ALL`. All Settings and the "More → …" rows push the existing lists.
- **New lists** follow the Touch Layout menu's pattern: a `DISPLAYLIST_*` id, deferred push, the `menu_cbs_*` entries and the msg_hash labels.
- **The look** (type, art bands, covers, the two modes) needs the new menu driver. RGUI can show the structure for testing but not the design.

## Open questions

1. Which shaders make up Retro TV and Handheld LCD, and are they fast enough on low-end phones?
2. The limit for kept saves.
3. Recrawl on an Android phone and a handheld: no game, a multi-disc game, achievements on. 57 lists were not crawled.
4. Who maintains the recommended emulator per system.
