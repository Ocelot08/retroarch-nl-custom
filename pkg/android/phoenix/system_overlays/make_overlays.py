#!/usr/bin/env python3
"""Writes this build's Nintendo DS and 3DS overlays: the layouts of the
DS and 3DS overlays in common-overlays (gamepads/Named_Overlays), drawn
with the neo-retropad buttons, so that they look like the rest and are
laid out with the touch layout editor like them. Usage:
make_overlays.py <out dir>"""
import sys

NEO = "../neo-retropad/img/default/"
OWN = "../neo-custom/img/"

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


def stick(lines, page, n, action, x, y, bg, knob, knob_image):
    """A neo-retropad analog stick: its ring, and the knob that moves,
    labelled C when it is the C-stick (for when buttons are drawn)"""
    desc(lines, page, n, spec(action, x, y, "radial", *bg),
         NEO + "analog_bg.png", saturate_pct=1000000.0, reach_x=0)
    extra = dict(movable="true", reach_x=1.3, reach_y=1.3,
                 range_mod=3.0, range_mod_exclusive="true")
    if knob_image.endswith("_c.png"):
        extra["label"] = '"C"'
    desc(lines, page, n + 1, spec(action, x, y, "radial", *knob),
         knob_image, **extra)
    return n + 2


def build(out, title, landscape, portrait):
    """@landscape and @portrait: the middle of the D-pad and of the face
    buttons, then the rest as (action, x, y, shape, size, image, extra)
    or (stick, action, x, y, ring size, knob size, knob image), and
    (rotate,) for the page turned to on rotating"""
    head = ["# %s, laid out as the common-overlays %s overlay, with" % (
        title, title),
            "# the neo-retropad buttons. Made by make_overlays.py.",
            "overlays = 5", ""]
    pages = []

    for page, name, aspect, ysep, other, layout in (
            (0, "landscape", LANDSCAPE, "false", "portrait", landscape),
            (1, "portrait", PORTRAIT, "true", "landscape", portrait)):
        l = []
        page_header(head, page, name, aspect, ysep)
        n = pad(l, page, 0, layout[0], layout[1], page == 0)
        for c in layout[2]:
            if c[0] == "stick":
                n = stick(l, page, n, *c[1:])
                continue
            if c[0] == "rotate":
                # Turned to on rotating
                desc(l, page, n, spec("overlay_next", 2.5, 2.5, "radial",
                     0.1, 0.1), next_target='"%s"' % other)
                n += 1
                continue
            action, x, y, shape, size, image, ex = c
            desc(l, page, n, spec(action, x, y, shape, *size), image, **ex)
            n += 1
        pages.append(["", "## Overlay %d: %s" % (page, name),
                      "overlay%d_descs = %d" % (page, n)] + l)

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


# Sizes on a landscape page, and on a portrait one
L_HOT, L_BIG, L_LR = (0.02150, 0.03824), (0.03011, 0.03824), (0.03011, 0.04589)
P_HOT, P_BIG, P_LR = (0.03824, 0.02151), (0.05354, 0.02151), (0.05354, 0.02581)
HOT = dict(reach_x=1.6, reach_y=1.6)
HOT_X = dict(reach_x=1.6, reach_y=1.6, exclusive="true")
L_SHOULDER = dict(reach_x=1.2, reach_y=1.6, exclusive="true")
P_SHOULDER = dict(reach_x=1.2, reach_y=2.0, exclusive="true")
P_SHOULDER2 = dict(reach_x=1.6, reach_y=2.0, exclusive="true")
L_MIDDLE = dict(reach_x=1.5, reach_y=1.6, exclusive="true")
P_MIDDLE = dict(reach_x=1.4, reach_y=1.4, exclusive="true")


def hide(x, y):
    return ("overlay_next", x, y, "radial", L_HOT, NEO + "hotkey_hide.png",
            dict(next_target='"hidden"', reach_x=1.6, reach_y=1.6))


def ds(out):
    build(out, "Nintendo DS", (
        (0.12500, 0.77778), (0.85938, 0.75000), (
            ("start", 0.93750, 0.28889, "rect", L_BIG, NEO + "button_start.png", L_MIDDLE),
            ("select", 0.06250, 0.28889, "rect", L_BIG, NEO + "button_select.png", L_MIDDLE),
            ("l", 0.03011, 0.50556, "rect", L_LR, OWN + "button_l.png", dict(L_SHOULDER, label='"L"')),
            ("r", 0.96989, 0.50556, "rect", L_LR, OWN + "button_r.png", dict(L_SHOULDER, label='"R"')),
            ("menu_toggle", 0.93750, 0.08889, "radial", L_HOT, NEO + "hotkey_menu.png", HOT),
            ("rotate",),
            hide(0.80000, 0.08889),
            ("r2|r3", 0.06250, 0.08889, "radial", L_HOT, OWN + "hotkey_swap_screens.png", HOT),
            ("toggle_fast_forward", 0.20000, 0.08889, "radial", L_HOT, NEO + "hotkey_fast_forward.png", HOT))), (
        (0.21606, 0.90440), (0.78393, 0.90440), (
            ("select", 0.5, 0.895, "rect", P_BIG, NEO + "button_select.png", P_MIDDLE),
            ("start", 0.5, 0.96, "rect", P_BIG, NEO + "button_start.png", P_MIDDLE),
            ("l", 0.1, 0.77, "rect", P_LR, OWN + "button_l.png", dict(P_SHOULDER, label='"L"')),
            ("r", 0.9, 0.77, "rect", P_LR, OWN + "button_r.png", dict(P_SHOULDER, label='"R"')),
            ("menu_toggle", 0.5, 0.77, "radial", P_HOT, NEO + "hotkey_menu.png", HOT_X),
            ("rotate",),
            ("r2|r3", 0.37, 0.77, "radial", P_HOT, OWN + "hotkey_swap_screens.png", HOT_X),
            ("toggle_fast_forward", 0.63, 0.77, "radial", P_HOT, NEO + "hotkey_fast_forward.png", HOT_X))))


def n3ds(out):
    build(out, "Nintendo 3DS", (
        (0.12500, 0.77778), (0.85938, 0.75000), (
            ("start", 0.4, 0.93, "rect", L_BIG, NEO + "button_start.png", L_MIDDLE),
            ("select", 0.6, 0.93, "rect", L_BIG, NEO + "button_select.png", L_MIDDLE),
            ("l", 0.03011, 0.08889, "rect", L_LR, OWN + "button_l.png", dict(L_SHOULDER, label='"L"')),
            ("l2", 0.12917, 0.08889, "rect", L_LR, OWN + "button_zl.png", dict(L_SHOULDER, label='"ZL"')),
            ("r", 0.96989, 0.08889, "rect", L_LR, OWN + "button_r.png", dict(L_SHOULDER, label='"R"')),
            ("r2", 0.87083, 0.08889, "rect", L_LR, OWN + "button_zr.png", dict(L_SHOULDER, label='"ZR"')),
            ("menu_toggle", 0.5, 0.93, "radial", L_HOT, NEO + "hotkey_menu.png", HOT),
            hide(0.80000, 0.08889),
            ("toggle_fast_forward", 0.20000, 0.08889, "radial", L_HOT, NEO + "hotkey_fast_forward.png", HOT),
            ("stick", "analog_left", 0.095, 0.35, (0.08710, 0.15488),
             (0.05054, 0.08987), NEO + "analog_stick.png"),
            ("stick", "analog_right", 0.93, 0.35, (0.06406, 0.11388),
             (0.03772, 0.06667), OWN + "analog_stick_c.png"),
            ("rotate",))), (
        (0.21606, 0.90440), (0.78393, 0.90440), (
            ("select", 0.5, 0.895, "rect", P_BIG, NEO + "button_select.png", P_MIDDLE),
            ("start", 0.5, 0.96, "rect", P_BIG, NEO + "button_start.png", P_MIDDLE),
            ("l", 0.1, 0.77, "rect", P_LR, OWN + "button_l.png", dict(P_SHOULDER, label='"L"')),
            ("l2", 0.25, 0.77, "rect", P_LR, OWN + "button_zl.png", dict(P_SHOULDER2, label='"ZL"')),
            ("r", 0.9, 0.77, "rect", P_LR, OWN + "button_r.png", dict(P_SHOULDER, label='"R"')),
            ("r2", 0.75, 0.77, "rect", P_LR, OWN + "button_zr.png", dict(P_SHOULDER2, label='"ZR"')),
            ("toggle_fast_forward", 0.5, 0.83, "radial", P_HOT, NEO + "hotkey_fast_forward.png", HOT_X),
            ("menu_toggle", 0.5, 0.77, "radial", P_HOT, NEO + "hotkey_menu.png", HOT_X),
            ("stick", "analog_left", 0.15, 0.625, (0.15488, 0.08710),
             (0.08987, 0.05054), NEO + "analog_stick.png"),
            ("stick", "analog_right", 0.88, 0.625, (0.11388, 0.06406),
             (0.06667, 0.03772), OWN + "analog_stick_c.png"),
            ("rotate",))))


if __name__ == "__main__":
    import os
    ds(os.path.join(sys.argv[1], "Nintendo - Nintendo DS.cfg"))
    n3ds(os.path.join(sys.argv[1], "Nintendo - Nintendo 3DS.cfg"))
