# Roadmap

Planned features for the touch layout editor, not yet built.

## Screen placement

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

## Button size and shape

- Resize each button, and make it wider or taller, in set steps (as
  the grid snaps positions) so buttons can be made the same size.
- A shape for the main buttons: circle or rounded rectangle.

## Buttons to switch profiles

- On-screen buttons that switch to a saved layout profile.

## Custom buttons

- A button with its own label (for example "C") sending any RetroArch
  input (for example L1).

## Drawn buttons

- Buttons drawn in code instead of from images: vector shapes that
  stay sharp at any size, with live text labels set apart from what
  the button does.
- Styles, such as filled or outline only.
- The shapes, labels and styles above build on this, so it comes
  first.
