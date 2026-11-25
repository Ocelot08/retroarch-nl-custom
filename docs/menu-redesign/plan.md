# Build plan: architecture and order of work

- **Status:** 22 Nov 2026, after the two lines of work were combined: the design and value controls on one side, the touch fix, lists and test tooling on the other.
- **Follows:** `frontend-redesign-brief.md` (options 2 and 3) and `ia-v2.md` (structure, templates, states).
- **Where things stand:**
  - The simple driver (`menu/drivers/simple.c`) has three page styles: Home, Simple's own lists, and RetroArch's lists in smaller type. They are told apart by list id.
  - It takes taps in touch mode and the touch gamepad in Gamepad Mode. Its button hints follow the OK/Cancel swap.
  - It scrolls by drag and flick with an edge bounce. Choices step on the row, numbers open a slider, and prompts show as a card.
  - It has no art, keyboard or page animation yet.
  - Short lists: all eight Settings categories, Downloads, Help, and the Game menu's Controls and Screen.
  - Options pages with descriptions, a text box, drawing behind the camera, and Back are in.
  - Not seen on a phone yet: a physical controller, RetroArch's own keyboard in Simple, and the reworded Close, Restart and gamepad prompts.

## What is architectural

Some decisions are expensive to change later because other work is built on them. These four come first.

### 1. How a page names its template

`ia-v2.md` has about 15 page types: list, values, Game menu, Games with covers, game detail, sheet, whole-screen question, button capture, progress, step flow, table and long list.

The driver needs one agreed way to learn which one to draw, for a page and for a row: "toggle row", "header with summary", "sheet". Two ways to do it:

- **By list label,** as Ozone and XMB already spot the Quick Menu.
  - Needs no new data.
  - Breaks quietly when a label changes.
- **By a type set where the list is built,** in `menu_displaylist.c`.
  - Explicit, and survives renames.
  - Adds a field the other drivers ignore.

Every page style, setting page and prompt depends on this, so settle it before building more page styles.

**So far:** pages go by list id, in `simple_set_page`, and rows by their setting's type. RetroArch keeps no per-list data to hang a type on, and ids don't change when a title does.

### 2. One layout model for drawing, touch and animation

- **Today:** `simple_entry_rect` places each row, and drawing, touch and scrolling all ask it. The slider, the arrows and the prompt card still work out their own places.
- **Instead:** each frame builds one list of boxes, and drawing, hit-testing, scrolling, focus and animation all read it.
- **Why:** animation is then those boxes moving over time, using RetroArch's existing `gfx_animation`. It needs no special cases.

Build this before any animation.

### 3. The input model

- **What it covers:**
  - Touch, Gamepad and Auto modes.
  - Who gets a touch while the touch gamepad is shown: buttons get theirs, and the rest go to the menu.
  - Drag scrolling with momentum.
  - One meaning of Back for the breadcrumb, the controller button and Android's back gesture.
  - The OK/Cancel swap.
- **Where it lives:** partly outside the driver.
  - `menu_driver.c` reads the touchscreen and decides when the overlay owns a touch.
  - `input_driver.c` holds the overlay state.
  - The first test build broke because the driver never set `p_disp->framebuf_dims`, which the touch reader scales by.
- **Keep changes in shared files small,** to keep upstream merges easy.

### 4. Where notices are drawn

- **The problem:** toasts like "Extracting base.apk" are RetroArch's widgets (`gfx/gfx_widgets.c`), not the menu driver.
  - They draw during play too.
  - They are shared by every driver.
- **Choose one:**
  - **Restyle the widgets.** One look in the menu and in play, but it changes a shared layer.
  - **Let `simple.c` draw its own notices while the menu is open.** Self-contained, but two looks.

Decide before designing notifications and error states.

### Less architectural than they look

- **IA changes, groupings, sub-pages:** new lists in `menu_displaylist.c` pointing at existing settings. The work is wide but follows one pattern, the Touch Layout menu's.
- **Connecting settings to RetroArch:** mostly free, because settings don't care which list shows them. The new behaviour is small:
  - Keeping changes;
  - Menu Mode;
  - `menu_layout`;
  - Quick Setup;
  - the defaults Simple writes.

## Easy to miss

- **Scrolling and long lists:** drag, momentum, and search for the long-list template.
- **Box art:** covers and art bands need the thumbnail system (`gfx_thumbnail`). It loads in the background, keeps a cache, and the page needs a placeholder until the art arrives.
- **A modal layer:**
  - Sheets, whole-screen questions, button capture and typing sit over the page.
  - They take input first and hand focus back when they close.
- **Sizes and rotation:**
  - Landscape, tablets and foldables, not only the S24.
  - Sizes come from screen density. MaterialUI notes that touch breaks on rotation unless the screen size is set every frame.
- **Text:**
  - Long names need truncation.
  - Every label goes through `msg_hash` and `intl/` so it can be translated.
- **Accessibility:**
  - RetroArch's narrator should read the new rows.
  - Contrast, and a text size setting.
- **Speed:**
  - The menu draws over a paused game.
  - Check frame time and font atlas sizes on a low-end phone.
- **Upstream merges:** keep work in `simple.c` and in new lists, and touch shared files only where nothing else works.
- **Classic and the other drivers:**
  - New lists must still read correctly in RGUI, Ozone and MaterialUI.
  - Classic must stay reachable.

## Tooling first

Each of these cost time in the first device test.

Done: driver logging, the scripted run and the signing key. Left: separate test data, and CI off this Mac.

- **Driver logging.** A short log line per tap, per action and per page change in `simple.c`, behind a debug flag, so a failure shows up in logcat.
- **A scripted device run.** An adb script that opens the test app, taps through the main paths and saves screenshots, so every build gets the same check.
  - Wait for the app to be in front before each tap.
  - Hold each press for about 150 ms: a bare `input tap` can fall between frames.
- **One debug signing key, kept outside the repository.** Test builds from different machines then install over each other. Today a build from a second machine meant uninstalling the test app.
- **Separate test data.** The Simple test app uses the same `/storage/emulated/0/RetroArch` saves and states as the working app. Point it at its own folders.
- **CI.** The jobs AGENTS.md asks for run on Linux, and `tools/ci_local.py` skips them on a Mac, with docker or without. Run them in a Linux machine or a cloud session, with `TMPDIR` unset (Wine crashed with it set) and Python 3.12. Known failures from before the redesign: `input_state_grab_check` on `input/input_overlay_layout.c` (since `919eca3c3d`), and `overlay_apng_press_on_screen_test` (cause not found).

## Order of work

1. **Tooling,** as above.
2. **Structure.** The final IA in `menu_displaylist.c` behind `menu_layout`: groupings, sub-pages, the "More" and "All Settings" rows. Check it in RGUI.
3. **UI system core:**
   - style tokens (colours, type scale, spacing, row heights, touch targets);
   - the layout model;
   - how a page names its template.
4. **Input model:**
   - scrolling;
   - Back everywhere;
   - Menu Mode with Auto;
   - taps on the menu in Gamepad Mode where there is no touch button.
5. **Page templates, most used first:**
   1. value rows (toggle, number, short choice);
   2. the modal layer (sheet, whole-screen question);
   3. Game menu over the paused game;
   4. Games with covers;
   5. game detail;
   6. progress;
   7. Quick Setup steps;
   8. table and long list.
6. **New behaviour:**
   - Keeping changes and its banner;
   - Menu Mode Auto;
   - Quick Setup;
   - the defaults Simple writes.
7. **Notices and error states,** once "where notices are drawn" is decided.
   - The states from `ia-v2.md`: no games, no art, no emulator, missing BIOS, game didn't start, first game.
   - Their notices.
8. **Animation:**
   - page changes;
   - selection movement;
   - sheets opening;
   - list scrolling.
9. **Polish:** accessibility, translations, speed on low-end phones, landscape and tablets.

## Known issues from the first device test

The first and the lost presses were seen on the driver before the two lines were combined.

- **Wrong item after Back** (once, Gamepad Mode):
  - After going back from Help, Help was highlighted but OK opened Settings.
  - It did not repeat when the selection was moved first.
  - Needs driver logging to find.
- **Menu rows don't take taps in Gamepad Mode.** The touch gamepad owns every touch while shown. Covered by step 4.
- **Not yet tested with a physical controller.**
- **Touch gamepad presses sometimes all lost after a page opens**
  (Gamepad Mode): once, eight presses in a row after Help opened did
  nothing and logged nothing; the next run was fine. The scripted check
  stops rather than guess. Possibly the same cause as "wrong item after
  Back".
- **Not yet in the Settings lists, against `ia-v2.md`:** Screen Style
  (needs its preview page and the shader choice), one Resume toggle
  for auto save and load (two rows for now), Menu Mode with Auto (the
  Gamepad Mode switch for now), Ask Before Keeping Changes. Folder settings show no
  value in the simple driver.
- **A tap now and then is not logged** right after going back to a page:
  the app saw no tap at all. The scripted check retries a tap the app did
  not log, and refuses touch gamepad presses outside Gamepad Mode, where
  they would land on the menu.
- **Downloads, against `ia-v2.md`:**
  - Update Cores updates installed cores, not everything: RetroArch has no single update-all.
  - System Files is RetroArch's downloader, and does not know what is missing.
  - There are no update counts or summary: they need a request to the server when the page opens.
- **The text box's cursor** stays at the end on Android, which does not
  report where it is.
- **Languages** outside Latin show as boxes: the bundled font is a subset.
- **The sublabel map is binary searched.** Rows go in enum order; `tools/sublabel_map_check.py` checks it. On an Apple silicon Mac, run the Python checks with an arm64 Python.
