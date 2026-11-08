# Roadmap

Planned features for the touch layout editor, not yet built, in the
order they are best built in. Each item says what it needs first and
how much work it is:

- **Small**: builds on what is there, in one or two places.
- **Medium**: new editor behaviour and saving, across several parts.
- **Large**: a new part of the overlay itself.

| # | Feature | Needs first | Effort |
|---|---------|-------------|--------|
| 1 | Screen placement | - | Medium |
| 2 | Button size | 1 | Medium |
| 3 | Drawn buttons | - | Large |
| 4 | Button shapes and styles | 3 | Small |
| 5 | Custom buttons | 3 | Medium |
| 6 | Buttons to switch profiles | 5 | Small |

## 1. Screen placement

*Needs first: nothing. Effort: Medium.*

- One box for the game screen and the menu, which both follow it.
  Drawn behind the controls while editing; drag to move, drag a corner
  to resize, snapped to the grid.
- Shared by the D-pad and analog pages.
- Saved with Layout For (all content, core or game) and with profiles,
  like the controls. It is written as the overlay page's own viewport
  setting (`overlayN_viewport`) in the layout file.
- To decide: a Stretch option to fill the box, or always keep the
  game's shape.
- To check: the Vulkan video driver places the menu as OpenGL does.
- First because it stands alone, and the corner handle it adds for
  resizing is the one button size uses.

## 2. Button size

*Needs first: 1, for its resize handle. Effort: Medium.*

- Resize each button, and make it wider or taller, in set steps (as
  the grid snaps positions) so buttons can be made the same size.
- Works on the overlay's own images as they are, so it does not wait
  for drawn buttons; once those come, they resize the same way.

## 3. Drawn buttons

*Needs first: nothing. Effort: Large.*

- Buttons drawn in code instead of from images: vector shapes that
  stay sharp at any size, with live text labels set apart from what
  the button does.
- The largest item: the overlay only draws images today, so this adds
  a way to draw shapes and text in their place.
- Before 4 and 5, which build on it.

## 4. Button shapes and styles

*Needs first: 3. Effort: Small.*

- A shape for the main buttons: circle or rounded rectangle.
- Styles, such as filled or outline only.
- Options of the drawn buttons, chosen in the editor.

## 5. Custom buttons

*Needs first: 3, for its label. Effort: Medium.*

- A button with its own label (for example "C") sending any RetroArch
  input (for example L1).
- Needs a way in the editor to make a button, type its label and pick
  its input.

## 6. Buttons to switch profiles

*Needs first: 5. Effort: Small.*

- On-screen buttons that switch to a saved layout profile.
- A custom button whose action is a profile instead of an input.
