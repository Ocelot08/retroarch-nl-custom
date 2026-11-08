# Roadmap

Planned features for the touch layout editor, not yet built, in the
order they are best built in. Each item says what it needs first and
how much work it is:

- **Small**: builds on what is there, in one or two places.
- **Medium**: new editor behaviour and saving, across several parts.
- **Large**: a new part of the overlay itself.

| # | Feature | Needs first | Effort |
|---|---------|-------------|--------|
| 1 | Screen placement (built) | - | Medium |
| 2 | Button size (built) | - | Medium |
| 3 | Drawn buttons | - | Large |
| 4 | Button shapes and styles | 3 | Small |
| 5 | Custom buttons | 3 | Medium |
| 6 | Buttons to switch profiles | 5 | Small |

Test on the phone at each checkpoint below before building on: the
Linux build used for development has no Android Back, keyboard,
Vulkan or real touch, and each next item builds on what came before.

Both 1 and 2 use one way of editing: tap something in the editor to
select it, and a panel of notched sliders opens under the editor bar,
away from what is being sized, with Reset and Done. Built together.

## 1. Screen placement

*Needs first: nothing. Effort: Medium.*

- One box for the game screen and the menu, which both follow it.
  Drawn dashed behind the controls while editing. Tap it to select it,
  then drag it to move it (snapped to the grid), and set its width and
  height with sliders notched in grid cells.
- The game keeps its own shape inside the box. A Stretch option to
  fill it can come later.
- Shared by the D-pad and analog pages.
- Saved with Layout For (all content, core or game) and with profiles,
  like the controls. It is written as the overlay page's own viewport
  setting (`overlayN_viewport`) in the layout file.
- The Vulkan video driver places the menu through the same viewport
  code as OpenGL, so it follows the box there too (read in the code,
  not yet seen on a device).

## 2. Button size

*Needs first: nothing. Effort: Medium.*

- Tap a button to select it; dragging still moves it.
- A Size slider notched in half grid cells (1 to 12 cells), so buttons
  set to the same number are the same size whatever their images.
- A Shape slider from wide to tall, notched, with original proportions
  in the middle.
- The D-pad and face buttons size as one, spacing and all.
- Works on the overlay's own images as they are, so it does not wait
  for drawn buttons; once those come, they size the same way.

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

## Test checkpoints

### A. Now: after 1 and 2, before 3

Item 3 changes how buttons are drawn and sized, so sizing should be
right first.

- Tap a button, a D-pad and the face buttons: each is outlined and
  the panel opens under the bar, as wide as it.
- Drag Size and Shape: notches are easy to hit, the value shown
  matches, a cluster keeps its middle and spacing.
- Two different buttons set to the same Size look the same size.
- Reset, then Done. Cancel after resizing puts the old sizes back.
- Dragging a button still moves it, and does not select it.
- Tap the screen box, drag it, set Width and Height; play a game and
  open the menu: both sit in the box. Reset puts it back at the top.
- Check both of the above with the video driver on Vulkan and on
  OpenGL (Settings > Drivers > Video).
- Turn the phone, and tick Analog: sizes and the screen box carry to
  the analog page, and landscape keeps its own.
- Save for a core and for a game (Layout For); Save Layout as Profile,
  load it elsewhere: sizes and the screen come with it.
- DS and 3DS: the same, on their own controls; the swap screens
  button still swaps.
- Restart the app: everything is where it was saved.

### B. After 3, before 4 and 5

- Every button of the main, DS and 3DS controls is drawn, pressed
  states included, at every size from item 2.
- Text is sharp at small and large sizes, and in both orientations.
- Speed: no slower than with images, in a demanding game.

### C. After 5 and 6

- A custom button sends the input chosen, in game, and its label
  shows; it survives Save, profiles and a restart.
- A profile button switches layouts in game, including between a
  core's and a game's.

