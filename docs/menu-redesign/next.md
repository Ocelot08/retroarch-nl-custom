# What may come next

Notes and ideas for the Simple menu, not yet built. `plan.md` has the
architecture and order of work; `ia-v2.md` has the structure. Move an item
out of here when it is built.

## Design notes to apply

Home has the last game's box art behind Continue, under the page's dark
grey (`simple_art_draw`, `simple_band_fade`). Still to do:

- **Other pages** it could go on, from `ia-v2.md`: Games and its lists, and a
  game's detail page. The Game menu waits for its own redesign. Settings,
  Downloads and Help stay plain.
- **Downloading covers** for a list with a short name (below in
  `README.md`) is not tried: on 25 Nov 2026 the thumbnail server listed no
  systems at all.

- **Settings page notes, animated:** each note names in turn the settings on
  its page (`simple_cycle`). When it starts, how long a name stays and the
  delay from row to row are `SIMPLE_CYCLE_START`, `_HOLD` and `_WAVE`. Still
  to do: animate the change itself, so that it moves down the page as a
  wave, and follow Android's "Remove animations". The same could go on
  Home, whose Settings note is to rotate in `ia-v2.md`.

## Features to build

| Feature | Notes |
|---|---|
| The rest of "Keeping changes" in `ia-v2.md` | The question on Game Options, the banner, Ask Before Keeping Changes. Settings changed from Home during a game are still dropped when it has settings of its own. Shaders and which controller a player uses are not asked about |
| Box art beyond Home, page animation, Quick Setup | See `plan.md` |

## Small fixes

- Languages outside Latin show as boxes.

## To decide

1. Game Lists: hide Images, Netplay, Import Content, Contentless Cores and
   Explore, which are elsewhere in Games?
2. Is the right 40% of a row the right size for changing its value?
3. Should Simple turn Use Camera Area on once only, rather than each time
   the layout changes from Classic?
4. Highlight Color: is a colour of one's own wanted (a hex code), or whole
   themes, as RGUI has?
5. Should the labels inside More Downloads (Update Files, Settings) be grey,
   so that they do not look equal to it?

## Not tested

- Keep Changes? in Gamepad Mode, after a resume from its page, and with
  changed binds.
- With Vulkan, a game, text being typed and prompts: the menu's pages were
  the same as with OpenGL (25 Nov 2026).
- A physical controller. With a
  controller in touch mode, that the line at the left shows on up and down
  and goes at the next touch: arrow keys sent over USB do not reach the menu.
- RetroArch's own keyboard in Simple, with the system's turned off.
- The reworded Close, Restart and gamepad prompts.
- The update rows under More Downloads, and the two short messages for a
  missing game file or core.
