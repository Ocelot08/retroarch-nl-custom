# What may come next

Notes and ideas for the Simple menu, not yet built. `plan.md` has the
architecture and order of work; `ia-v2.md` has the structure. Move an item
out of here when it is built.

## Design notes to apply

The background is drawn in two layers on Home only: the colours at the back
(`simple_band_back`), and the page's dark grey over them, clear at the top
and solid at the bottom (`simple_band_fade`). Still to do:

- **Box art** takes the back layer's place once it is drawn. Then check that
  HOME, RETROARCH, CONTINUE and the title stay readable over light covers;
  the front may need some grey at the very top as well.
- **Other pages** it could go on, from `ia-v2.md`: Games and its lists, a
  game's detail page and the Game menu. Settings, Downloads and Help stay
  plain.

- **Settings page notes, animated:** each note names in turn the settings on
  its page (`simple_cycle`). When it starts, how long a name stays and the
  delay from row to row are `SIMPLE_CYCLE_START`, `_HOLD` and `_WAVE`. Still
  to do: animate the change itself, so that it moves down the page as a
  wave, and follow Android's "Remove animations". The same could go on
  Home, whose Settings note is to rotate in `ia-v2.md`.

## Features to build

| Feature | Notes |
|---|---|
| Open All Settings in place | After More Downloads has been used for a while |
| Open the other More rows in place | Check each list's rows: some depend on the page they are on |
| RA TESTER's own folders | It shares history, playlists, saves and core settings with the working app |
| Keep settings changed during a game | Dropped now when the game has settings of its own; "Keeping changes" in `ia-v2.md` |
| Headings on other pages | The driver draws them from a list of rows that start a group |
| Box art, page animation, Quick Setup | See `plan.md` |

## Small fixes

- Saved folders in Browse Files show their raw `saf://` link.
- Use Camera Area has no description.
- Home counts "4 SYSTEMS", which are game lists.
- Edit Game Lists still has RetroArch's "Refresh All Playlists".
- Orientation does nothing until, probably, a restart: say so, or move it.
- Languages outside Latin show as boxes.

## To decide

1. Game Lists: hide Images, Netplay, Import Content, Contentless Cores and
   Explore, which are elsewhere in Games?
2. Is the right 40% of a row the right size for changing its value?
3. Should Simple turn Use Camera Area on once only, rather than each time
   the layout changes from Classic?
4. Should + and − on an opened row be bigger?

## Not tested

- A physical controller, landscape, and the Vulkan video driver. With a
  controller in touch mode, that the line at the left shows on up and down
  and goes at the next touch: arrow keys sent over USB do not reach the menu.
- RetroArch's own keyboard in Simple, with the system's turned off.
- The reworded Close, Restart and gamepad prompts.
- The update rows under More Downloads, and the two short messages for a
  missing game file or core.
