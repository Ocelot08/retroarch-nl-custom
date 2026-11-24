#!/bin/sh
# Scripted check of the RA TESTER app (-PsimpleMenuTest) on a phone over adb.
# Finds rows by name from the driver's RASimple log lines, taps them, checks
# the page that opens and saves a screenshot per step: Home, Settings, Help,
# and Gamepad Mode from Settings > Interface.
# Usage: test_simple_menu.sh [screenshot dir]
# Needs USB debugging and the screen unlocked. Never starts a game or quits.
set -u

PKG=com.retroarch.custom.simpletest
ADB=${ADB:-adb}
command -v "$ADB" >/dev/null 2>&1 || ADB="${ANDROID_HOME:-$HOME/Library/Android/sdk}/platform-tools/adb"
OUT=${1:-"${TMPDIR:-/tmp}/ra-tester-$(date +%Y%m%d-%H%M%S)"}
CFG=/storage/emulated/0/Android/data/$PKG/files/retroarch.cfg
STEP=0
FAILS=0

mkdir -p "$OUT" || exit 1

# RASimple lines of this run, without the logcat prefix
log_lines() {
   "$ADB" logcat -d -v brief --pid="$PID" -s RASimple 2>/dev/null \
      | sed -n 's/^I\/RASimple( *[0-9]*): //p'
}

log_count() { log_lines | wc -l | tr -d ' '; }

last_page() { log_lines | grep '^page ' | tail -n 1; }

# Wait for a new log line matching $2 after line $1
wait_for() {
   w=0
   while [ $w -lt 40 ]; do
      log_lines | tail -n +"$(($1 + 1))" | grep -q -- "$2" && return 0
      sleep 0.2
      w=$((w + 1))
   done
   return 1
}

in_front() {
   "$ADB" shell dumpsys activity activities 2>/dev/null \
      | grep -m 1 topResumedActivity | grep -q "$PKG"
}

# Held for 150 ms: a bare tap can fall between two frames
press() {
   if ! in_front; then
      echo "ABORT: $PKG is not in front"
      exit 2
   fi
   "$ADB" shell input swipe "$1" "$2" "$1" "$2" 150
}

shot() {
   STEP=$((STEP + 1))
   "$ADB" exec-out screencap -p > "$OUT/$(printf %02d $STEP)-$1.png"
}

check() {
   if [ "$1" -eq 0 ]; then
      echo "PASS  $2"
   else
      echo "FAIL  $2"
      FAILS=$((FAILS + 1))
   fi
}

# Window to screen: the window ends at the bottom of the screen
screen_size() {
   "$ADB" shell wm size | sed -n 's/.*size: \([0-9]*\)x\([0-9]*\).*/\1 \2/p' | tail -n 1
}

offset_y() {
   win_h=$(last_page | sed -n 's/.*dims=[0-9]*x\([0-9]*\).*/\1/p')
   echo $((SCREEN_H - win_h))
}

# Centre of the named row on the current page, in screen pixels
row_y() {
   log_lines | awk -v want="$1" '
      /^page / { found = "" }
      /^row / {
         label = $0; sub(/^[^"]*"/, "", label); sub(/"$/, "", label)
         split($3, r, /[=-]/)
         if (label == want) found = (r[2] + r[3]) / 2
      }
      END { if (found != "") print int(found) }'
}

# A second argument, "value", taps the right side, which changes the row
tap_row() {
   if refused "$1"; then
      echo "REFUSED: $1"; exit 2
   fi
   wait_rows || return 1
   y=$(row_y "$1")
   if [ -z "$y" ]; then
      echo "FAIL  no row \"$1\" on: $(last_page)"
      FAILS=$((FAILS + 1))
      return 1
   fi
   case "${2:-}" in
      value) tap_at $((SCREEN_W * 9 / 10)) $((y + $(offset_y))) ;;
      *)     tap_at $((SCREEN_W / 4)) $((y + $(offset_y))) ;;
   esac
}

# A tap the app did not log (lost while a page settles) is tried once more
tap_at() {
   for attempt in 1 2; do
      before=$(log_lines | grep -c '^tap ')
      press "$1" "$2"
      t=0
      while [ $t -lt 10 ]; do
         [ "$(log_lines | grep -c '^tap ')" -gt "$before" ] && return 0
         sleep 0.2
         t=$((t + 1))
      done
   done
   echo "FAIL  the app logged no tap at $1,$2"
   FAILS=$((FAILS + 1))
   return 1
}

# The back line sits above the first row
# Label of row $1 on the current page
row_label() {
   log_lines | awk -v want="$1" '
      /^page / { found = "" }
      /^row / && $2 == want {
         found = $0; sub(/^[^"]*"/, "", found); sub(/"$/, "", found)
      }
      END { print found }'
}

# The current selection: the page's, then any move since
page_sel() {
   log_lines | awk '/^page / { s = $0; sub(/.* sel=/, "", s) }
                    /^sel / { s = $2 }
                    END { print s }'
}

refused() {
   case "$1" in
      Continue*|Quit*|Close*|Restart*|Reset*|Switch*) return 0 ;;
   esac
   return 1
}

# OK on the touch gamepad, only on the row named and never on one that
# starts a game or quits
pad_ok() {
   wait_rows || exit 2
   label=$(row_label "$(page_sel)")
   if refused "$label"; then
      echo "REFUSED: OK on \"$label\""; exit 2
   fi
   if [ "$label" != "$1" ]; then
      echo "ABORT: OK meant for \"$1\", but \"$label\" is highlighted"
      exit 2
   fi
   pad "$OK_BTN"
}

# Down on the touch gamepad until the named row is highlighted. A press
# can be lost while a page opens, so this goes by the row, not a count.
pad_to() {
   wait_rows || return 1
   tries=0
   while [ $tries -lt 8 ] && [ "$(row_label "$(page_sel)")" != "$1" ]; do
      n=$(log_count); pad down; wait_for "$n" '^sel '
      tries=$((tries + 1))
   done
   [ "$(row_label "$(page_sel)")" = "$1" ]
}

# A page's rows are logged a frame after its page line: wait for them
wait_rows() {
   r=0
   while [ $r -lt 20 ]; do
      log_lines | awk '/^page /{n=0} /^row /{n++} END{exit !(n>0)}' && return 0
      sleep 0.2
      r=$((r + 1))
   done
   echo "FAIL  no rows on: $(last_page)"
   FAILS=$((FAILS + 1))
   return 1
}

tap_back() {
   wait_rows || return 1
   first=$(log_lines | awk '/^page /{f=""} /^row / && f=="" {split($3, r, /[=-]/); f=r[2]} END{print f}')
   tap_at $((SCREEN_W / 4)) $((first * 6 / 10 + $(offset_y)))
}

# The default touch gamepad, as fractions of the screen
pad() {
   case "$(last_page)" in
      *mode=gamepad*) ;;
      *) echo "ABORT: touch gamepad press while not in Gamepad Mode"; exit 2 ;;
   esac
   case "$1" in
      up)   fx=0.208; fy=0.550 ;;
      down) fx=0.208; fy=0.642 ;;
      a)    fx=0.892; fy=0.596 ;;
      b)    fx=0.791; fy=0.642 ;;
   esac
   press "$(awk -v f=$fx -v w=$SCREEN_W 'BEGIN{print int(f*w)}')" \
         "$(awk -v f=$fy -v h=$SCREEN_H 'BEGIN{print int(f*h)}')"
}

set -- $(screen_size)
SCREEN_W=$1
SCREEN_H=$2
SWAP=$("$ADB" shell cat "$CFG" 2>/dev/null | sed -n 's/^menu_swap_ok_cancel_buttons = "\(.*\)"/\1/p')
if [ "$SWAP" = "true" ]; then OK_BTN=b; BACK_BTN=a; else OK_BTN=a; BACK_BTN=b; fi

echo "Screen ${SCREEN_W}x${SCREEN_H}, OK is $OK_BTN, screenshots in $OUT"

"$ADB" logcat -c
"$ADB" shell am force-stop "$PKG"
"$ADB" shell monkey -p "$PKG" 1 >/dev/null 2>&1
i=0
PID=
while [ $i -lt 50 ] && [ -z "$PID" ]; do
   PID=$("$ADB" shell pidof "$PKG" | tr -d '\r')
   sleep 0.2
   i=$((i + 1))
done
wait_for 0 '^row '
check $? "Home opens"

# Gamepad Mode is in Settings > Interface
open_interface_by_pad() {
   pad_to Settings || return 1
   n=$(log_count); pad_ok Settings; wait_for "$n" '^page "Settings"' || return 1
   pad_to Interface || return 1
   n=$(log_count); pad_ok Interface; wait_for "$n" '^page "Interface"'
}

# Force-stop skips the save on exit, so a run can start in Gamepad Mode:
# turn it off with the touch gamepad, then back to Home
case "$(last_page)" in *mode=gamepad*)
   sleep 1
   open_interface_by_pad || { echo "ABORT: started in Gamepad Mode, could not reach Interface"; exit 2; }
   pad_to "Gamepad Mode" || { echo "ABORT: could not reach Gamepad Mode"; exit 2; }
   n=$(log_count); pad_ok "Gamepad Mode"; wait_for "$n" 'mode=touch'
   check $? "Started in Gamepad Mode; turned it off"
   n=$(log_count); tap_back; wait_for "$n" '^page "Settings"'
   n=$(log_count); tap_back; wait_for "$n" '^page "Home"' ;;
esac
shot home

n=$(log_count); tap_row Settings && wait_for "$n" '^page "Settings"'
check $? "Tap Settings opens Settings"
shot settings

n=$(log_count); tap_back; wait_for "$n" '^page "Home"'
check $? "Tap the back line returns Home"
shot home-again

n=$(log_count); tap_row Help && wait_for "$n" '^page "Help"'
check $? "Tap Help opens Help"
shot help
wait_rows
[ -n "$(row_y "Switch to Classic Menu")" ]
check $? "Help has Switch to Classic Menu"
n=$(log_count); tap_back; wait_for "$n" '^page "Home"'

n=$(log_count); tap_row Settings && wait_for "$n" '^page "Settings"'
n=$(log_count); tap_row Interface && wait_for "$n" '^page "Interface"'
check $? "Settings > Interface opens"
shot interface

n=$(log_count); tap_row "Gamepad Mode" value && wait_for "$n" 'mode=gamepad'
check $? "Gamepad Mode on"
# The touch gamepad reloads in the background; a press before it is
# up is lost
sleep 1
shot gamepad-on

n=$(log_count); pad down; wait_for "$n" '^sel '
check $? "Touch gamepad down moves the selection"
shot pad-down

n=$(log_count); pad up; wait_for "$n" '^sel '
check $? "Touch gamepad up moves the selection"

# Once seen: after Back, OK opened the row above the highlighted one
n=$(log_count); pad "$BACK_BTN"; wait_for "$n" '^page "Settings"'
check $? "Back button returns to Settings in Gamepad Mode"
shot pad-back
wait_rows
want=$(row_label "$(page_sel)")
n=$(log_count); pad_ok "$want"; wait_for "$n" "^page \"$want\""
check $? "After Back, OK opens the highlighted row ($want)"
shot pad-ok-after-back

# Back to Interface's Gamepad Mode row to turn it off
case "$(last_page)" in
   'page "Interface"'*) ;;
   *) n=$(log_count); pad "$BACK_BTN"; wait_for "$n" '^page "Settings"'
      pad_to Interface || { echo "ABORT: could not reach Interface"; exit 2; }
      n=$(log_count); pad_ok Interface; wait_for "$n" '^page "Interface"' ;;
esac
pad_to "Gamepad Mode"
check $? "Touch gamepad down to Gamepad Mode"

n=$(log_count); pad_ok "Gamepad Mode"; wait_for "$n" 'mode=touch'
check $? "OK on Gamepad Mode turns it off"
shot gamepad-off

n=$(log_count); tap_back; wait_for "$n" '^page "Settings"'
n=$(log_count); tap_back; wait_for "$n" '^page "Home"'
check $? "Tap the back line twice returns Home"
shot home-end

log_lines > "$OUT/rasimple.log"
echo "$FAILS failed; log in $OUT/rasimple.log"
[ "$FAILS" -eq 0 ]
