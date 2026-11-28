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

- **Settings page notes:** the same fading names could go on Home, whose
  Settings note is to rotate in `ia-v2.md`.

## Features to build

| Feature | Notes |
|---|---|
| The rest of "Keeping changes" in `ia-v2.md` | The question on Game Options, the banner, Ask Before Keeping Changes. Shaders and which controller a player uses are not asked about |
| Box art beyond Home, the rest of the animation, Quick Setup | See `plan.md` |

## Small fixes

- The dot under letters such as ệ is faint: Work Sans Light draws it half the
  size of a full stop.
- Indic scripts show as boxes: no fallback font has them.
- A note with Hebrew or Arabic is drawn whole, so its Latin part, as in the
  Language row, loses the notes' letter spacing.

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
- The Settings page notes and the page move with Android's "Remove
  animations" on.
- The page move in Gamepad Mode in landscape, where the box is narrower than
  the screen.
- The update rows under More Downloads, and the two short messages for a
  missing game file or core.
