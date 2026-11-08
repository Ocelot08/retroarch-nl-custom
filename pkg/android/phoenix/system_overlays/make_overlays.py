#!/usr/bin/env python3
"""Writes the Nintendo DS overlay of this build: the layout of the DS
overlay in common-overlays (gamepads/Named_Overlays), drawn with the
neo-retropad buttons, so that it looks like the rest and its controls
are laid out with the touch layout editor. Usage:
make_ds_overlay.py <out.cfg>"""
import sys

NEO = "../neo-retropad/img/default/"
OWN = "../neo-ds/img/"

LANDSCAPE = 1.77777778
PORTRAIT = 0.56236559


def desc(lines, page, i, spec, image=None, **extra):
    lines.append('overlay%d_desc%d = "%s"' % (page, i, spec))
    if image:
        lines.append('overlay%d_desc%d_overlay = "%s"' % (page, i, image))
    for k, v in extra.items():
        lines.append("overlay%d_desc%d_%s = %s" % (page, i, k, v))


def spec(action, x, y, shape, w, h):
    return "%s,%.5f,%.5f,%s,%.5f,%.5f" % (action, x, y, shape, w, h)


def cluster(landscape):
    """The neo-retropad D-pad or face buttons, as offsets from their
    middle, in the units of a landscape or portrait page"""
    if landscape:
        dpad = [  # action, dx, dy, w, h, image
            ("down", 0, 0.08700, 0.03441, 0.07553, "dpad_down"),
            ("left", -0.04893, 0, 0.04247, 0.06119, "dpad_left"),
            ("right", 0.04892, 0, 0.04247, 0.06119, "dpad_right"),
            ("up", 0, -0.08700, 0.03441, 0.07553, "dpad_up"),
            ("down", 0, 0.12476, 0.03441, 0.03776, None),
            ("left", -0.07016, 0, 0.02124, 0.06119, None),
            ("right", 0.07016, 0, 0.02124, 0.06119, None),
            ("up", 0, -0.12476, 0.03441, 0.03776, None),
            ("down|left", -0.06720, 0.11950, 0.02419, 0.04302, None),
            ("down|right", 0.06720, 0.11950, 0.02419, 0.04302, None),
            ("up|left", -0.06720, -0.11950, 0.02419, 0.04302, None),
            ("up|right", 0.06720, -0.11950, 0.02419, 0.04302, None)]
        area = (0.089583, 0.159259)
        face = [("a", 0.05699, 0), ("b", 0, 0.10134),
                ("x", 0, -0.10134), ("y", -0.05699, 0)]
        face_size = (0.03441, 0.06119)
        combos = [("x|y", -0.03638, -0.06398), ("x|y", -0.02388, -0.04175),
                  ("a|b", 0.02612, 0.04713), ("a|b", 0.03862, 0.06936),
                  ("y|b", -0.03638, 0.06936), ("y|b", -0.02388, 0.04713),
                  ("x|a", 0.02612, -0.04175), ("x|a", 0.03862, -0.06398)]
        combo_size = (0.01389, 0.02469)
    else:
        dpad = [
            ("down", 0, 0.04892, 0.06119, 0.04247, "dpad_down"),
            ("left", -0.08700, 0, 0.07553, 0.03441, "dpad_left"),
            ("right", 0.08700, 0, 0.07553, 0.03441, "dpad_right"),
            ("up", 0, -0.04892, 0.06119, 0.04247, "dpad_up"),
            ("down", 0, 0.07016, 0.06119, 0.02124, None),
            ("left", -0.12476, 0, 0.03776, 0.03441, None),
            ("right", 0.12476, 0, 0.03776, 0.03441, None),
            ("up", 0, -0.07016, 0.06119, 0.02124, None),
            ("down|left", -0.11950, 0.06720, 0.04302, 0.02419, None),
            ("down|right", 0.11950, 0.06720, 0.04302, 0.02419, None),
            ("up|left", -0.11950, -0.06720, 0.04302, 0.02419, None),
            ("up|right", 0.11950, -0.06720, 0.04302, 0.02419, None)]
        area = (0.159259, 0.089583)
        face = [("a", 0.10134, 0), ("b", 0, 0.05699),
                ("x", 0, -0.05699), ("y", -0.10134, 0)]
        face_size = (0.06119, 0.03441)
        combos = [("x|y", -0.06460, -0.03768), ("x|y", -0.03935, -0.01998),
                  ("a|b", 0.04297, 0.02355), ("a|b", 0.06312, 0.04124),
                  ("y|b", -0.06120, 0.03671), ("y|b", -0.04105, 0.02355),
                  ("x|a", 0.04467, -0.02338), ("x|a", 0.06397, -0.03768)]
        combo_size = (0.02469, 0.01389)
    return dpad, area, face, face_size, combos, combo_size


def pad(lines, page, n, dpad_at, face_at, landscape):
    dpad, area, face, face_size, combos, combo_size = cluster(landscape)
    cx, cy = dpad_at
    desc(lines, page, n, spec("dpad_area", cx, cy, "radial", *area),
         reach_x=1.9, reach_y=1.6, range_mod=1.1, range_mod_exclusive="true")
    n += 1
    for action, dx, dy, w, h, image in dpad:
        desc(lines, page, n, spec(action, cx + dx, cy + dy, "rect", w, h),
             NEO + image + ".png" if image else None, reach_x=0)
        n += 1
    cx, cy = face_at
    desc(lines, page, n, spec("abxy_area", cx, cy, "rect", *area),
         reach_x=1.4, reach_y=1.4, range_mod=1.1, range_mod_exclusive="true")
    n += 1
    for action, dx, dy in face:
        desc(lines, page, n, spec(action, cx + dx, cy + dy, "rect",
             *face_size), NEO + "button_" + action + ".png", reach_x=0)
        n += 1
    for action, dx, dy in combos:
        desc(lines, page, n, spec(action, cx + dx, cy + dy, "radial",
             *combo_size), reach_x=0)
        n += 1
    return n


def page_header(lines, page, name, aspect, y_separation):
    lines += ["overlay%d_name = \"%s\"" % (page, name),
              "overlay%d_full_screen = true" % page,
              "overlay%d_normalized = true" % page,
              "overlay%d_aspect_ratio = %s" % (page, aspect),
              "overlay%d_block_x_separation = false" % page,
              "overlay%d_block_y_separation = %s" % (page, y_separation),
              "overlay%d_range_mod = 1.5" % page,
              "overlay%d_alpha_mod = 2.0" % page, ""]


def extras(lines, page, y, xs, hot):
    n = 0
    for (action, image, shape), x in zip(
            (("save_state", "savestate", "radial"),
             ("load_state", "loadstate", "radial"),
             ("turbo_enable", "turbo", "rect"),
             ("hold_enable", "hold", "radial"),
             ("osk_toggle", "osk", "rect")), xs):
        desc(lines, page, n, spec(action, x, y, shape, *hot), NEO + image + ".png")
        n += 1
    return n


def main(out):
    head = ["# Nintendo DS, laid out as the common-overlays DS overlay, with",
            "# the neo-retropad buttons. Made by make_ds_overlay.py.",
            "overlays = 5", ""]
    pages = []

    # Landscape
    hot = (0.02150, 0.03824)
    big = (0.03011, 0.03824)
    lr = (0.03011, 0.04589)
    l = []
    page_header(head, 0, "landscape", LANDSCAPE, "false")
    n = pad(l, 0, 0, (0.12500, 0.77778), (0.85938, 0.75000), True)
    for action, x, y, size, image, ex in (
            ("start", 0.93750, 0.28889, big, NEO + "button_start.png",
             dict(reach_x=1.5, reach_y=1.6, exclusive="true")),
            ("select", 0.06250, 0.28889, big, NEO + "button_select.png",
             dict(reach_x=1.5, reach_y=1.6, exclusive="true")),
            ("l", 0.03011, 0.50556, lr, OWN + "button_l.png",
             dict(reach_x=1.2, reach_y=1.6, exclusive="true")),
            ("r", 0.96989, 0.50556, lr, OWN + "button_r.png",
             dict(reach_x=1.2, reach_y=1.6, exclusive="true"))):
        desc(l, 0, n, spec(action, x, y, "rect", *size), image, **ex)
        n += 1
    desc(l, 0, n, spec("menu_toggle", 0.93750, 0.08889, "radial", *hot),
         NEO + "hotkey_menu.png", reach_x=1.6, reach_y=1.6)
    n += 1
    desc(l, 0, n, spec("overlay_next", 2.5, 2.5, "radial", 0.1, 0.1),
         next_target='"portrait"')
    n += 1
    desc(l, 0, n, spec("overlay_next", 0.80000, 0.08889, "radial", *hot),
         NEO + "hotkey_hide.png", next_target='"hidden"',
         reach_x=1.6, reach_y=1.6)
    n += 1
    desc(l, 0, n, spec("r2|r3", 0.06250, 0.08889, "radial", *hot),
         OWN + "hotkey_swap_screens.png", reach_x=1.6, reach_y=1.6)
    n += 1
    desc(l, 0, n, spec("toggle_fast_forward", 0.20000, 0.08889, "radial",
         *hot), NEO + "hotkey_fast_forward.png", reach_x=1.6, reach_y=1.6)
    n += 1
    pages.append(["", "## Overlay 0: landscape", "overlay0_descs = %d" % n] + l)

    # Portrait
    hot = (0.03824, 0.02151)
    big = (0.05354, 0.02151)
    lr = (0.05354, 0.02581)
    l = []
    page_header(head, 1, "portrait", PORTRAIT, "true")
    n = pad(l, 1, 0, (0.21606, 0.90440), (0.78393, 0.90440), False)
    for action, x, y, size, image, ex in (
            ("select", 0.5, 0.895, big, NEO + "button_select.png",
             dict(reach_x=1.4, reach_y=1.4, exclusive="true")),
            ("start", 0.5, 0.96, big, NEO + "button_start.png",
             dict(reach_x=1.4, reach_y=1.4, exclusive="true")),
            ("l", 0.1, 0.77, lr, OWN + "button_l.png",
             dict(reach_x=1.2, reach_y=2.0, exclusive="true")),
            ("r", 0.9, 0.77, lr, OWN + "button_r.png",
             dict(reach_x=1.2, reach_y=2.0, exclusive="true"))):
        desc(l, 1, n, spec(action, x, y, "rect", *size), image, **ex)
        n += 1
    desc(l, 1, n, spec("menu_toggle", 0.5, 0.77, "radial", *hot),
         NEO + "hotkey_menu.png", reach_x=1.6, reach_y=1.6, exclusive="true")
    n += 1
    desc(l, 1, n, spec("overlay_next", 2.5, 2.5, "radial", 0.1, 0.1),
         next_target='"landscape"')
    n += 1
    desc(l, 1, n, spec("r2|r3", 0.37, 0.77, "radial", *hot),
         OWN + "hotkey_swap_screens.png", reach_x=1.6, reach_y=1.6,
         exclusive="true")
    n += 1
    desc(l, 1, n, spec("toggle_fast_forward", 0.63, 0.77, "radial", *hot),
         NEO + "hotkey_fast_forward.png", reach_x=1.6, reach_y=1.6,
         exclusive="true")
    n += 1
    pages.append(["", "## Overlay 1: portrait", "overlay1_descs = %d" % n] + l)

    # Hidden
    l = []
    page_header(head, 2, "hidden", LANDSCAPE, "false")
    desc(l, 2, 0, spec("overlay_next", 0.80000, 0.08889, "radial",
         0.02150, 0.03824), NEO + "hotkey_show.png",
         next_target='"landscape"', reach_x=1.6, reach_y=1.6)
    pages.append(["", "## Overlay 2: hidden", "overlay2_descs = 1"] + l)

    # Never shown: controls the touch layout editor can add
    l = []
    page_header(head, 3, "landscape-extras", LANDSCAPE, "false")
    n = extras(l, 3, 0.95, (0.30, 0.40, 0.50, 0.60, 0.70), (0.02150, 0.03824))
    pages.append(["", "## Overlay 3: landscape-extras, to add from",
                  "overlay3_descs = %d" % n] + l)
    l = []
    page_header(head, 4, "portrait-extras", PORTRAIT, "true")
    n = extras(l, 4, 0.70, (0.175, 0.275, 0.375, 0.475, 0.575),
               (0.02850, 0.01600))
    pages.append(["", "## Overlay 4: portrait-extras, to add from",
                  "overlay4_descs = %d" % n] + l)

    with open(out, "w") as f:
        f.write("\n".join(head + sum(pages, [])) + "\n")


if __name__ == "__main__":
    main(sys.argv[1])
