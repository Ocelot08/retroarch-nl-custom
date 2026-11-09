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
| 3 | Drawn buttons (built) | - | Large |
| 4 | Button style (built) | 3 | Small |
| 5 | Custom buttons (built) | 3 | Medium |
| 6 | Buttons to switch gamepads (built) | 5 | Small |

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
- Saved in the gamepad, like the controls. It is written as the overlay page's own viewport
  setting (`overlayN_viewport`) in the layout file.
- Reset keeps the width and fits the height to the game's shape.
- The box is placed from the top on OpenGL as on Vulkan: RetroArch's
  own placement code had it upside down on OpenGL, now fixed.

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

*Needs first: nothing. Effort: Large. Built.*

- The face buttons, shoulder buttons, Start, Select, D-pad arrows and
  analog sticks are drawn in code when the overlay loads, at the size
  they are shown, in place of their images. Shapes are drawn from
  their outlines (round ends, rounded corners, arrows) and labels as
  text in the menu's font, so a button made wider stays a capsule
  with upright letters.
- The editor draws a control again when its slider is let go.
- Labels come from what the button does (A, L1, START), or from a
  `_label` key in the overlay or layout ("L" and "ZL" on DS and 3DS,
  "C" on the C-stick). Typing one in the editor comes with item 5.
- Hotkeys (menu, fast-forward, hide, save state, swap screens, ...)
  keep their icons.
- Touch Layout > Drawn Buttons switches between drawn buttons and the
  images, per gamepad.
- Made through the overlay's own images, so every video driver, the
  press highlight, hiding and sizing work as before.

## 4. Button style

*Needs first: 3. Effort: Small. Built.*

- Touch Layout > Button Style, for all buttons at once, per
  gamepad: Style (Outline or Filled), Drawn Buttons (on or off) and Stroke
  Width (the same outline on every drawn button, whatever its size).
- Filled applies to drawn buttons; images, hotkeys included, stay as
  they are.
- Choosing a shape (round or rounded corners) per button was dropped
  for this.

## 5. Custom buttons

*Needs first: 3, for its label. Effort: Medium. Built.*

- A button with its own label (for example "C") sending any RetroArch
  input (for example L1): a button of the gamepad or a hotkey.
- Touch Layout > Custom Buttons: New Custom Button picks the input,
  then the label is typed; each one listed opens Label, Input and
  Remove. It is placed and sized in Edit Layout like other controls.
- Drawn round with its label whatever it sends. With drawn buttons off,
  and in other RetroArch, it shows a blank button image.
- Written in the gamepad as a plain overlay button, with `_label` and
  a `_custom` id shared by its copies on each page of the shape.

## 6. Buttons to switch gamepads

*Needs first: 5. Effort: Small. Built.*

- On-screen buttons that switch to another gamepad.
- A custom button whose action is a gamepad instead of an input: the
  gamepads are at the end of its Input list. It switches when let go
  of, as Load Gamepad does (for the game running, kept by an override).
- Written as a "nul" button with `_gamepad`, so it does nothing in
  other RetroArch.

## Test checkpoints

### A. After 1 and 2, before 3 (done)

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
- Save as Gamepad, load it elsewhere: sizes and the screen come with
  it.
- DS and 3DS: the same, on their own controls; the swap screens
  button still swaps.
- Restart the app: everything is where it was saved.

### B. After 3, before 4 and 5 (done)

- Main, DS and 3DS controls: every face and shoulder button, Start,
  Select, the D-pad and sticks are drawn, light up when pressed, and
  hotkeys still show their icons.
- Labels are sharp and upright at small and large sizes, and after
  Shape is set wider or taller (once the slider is let go).
- Both orientations, and after rotating while a game runs.
- Touch Layout > Drawn Buttons off brings the images back, on draws
  again; per core or game too.
- The overlay appears as quickly as before when a game starts, and
  games run no slower.

### C. Now: after 5 and 6

- A custom button sends the input chosen, in game, and its label
  shows; it survives Save, gamepads and a restart.
- Label, Input and Remove change it on every page (D-pad, analog,
  expanded), and it moves and sizes in Edit Layout.
- A gamepad button switches gamepads in game, including between a
  core's and a game's.

