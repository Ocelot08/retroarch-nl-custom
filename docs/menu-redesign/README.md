# Menu redesign

Working files for the phone-first menu redesign.

| File | What it is |
|---|---|
| `frontend-redesign-brief.md` | The brief: goal, constraints, build approach and first steps |
| `ia-current.csv` | Crawl of the current menus on Android, 4 levels below Main Menu |
| `ia-v1.md` | Proposed structure, v1: Claude Code's first pass |
| `ia-v2.md` | Proposed structure, v2: the merged direction, the chosen designs and the decisions since v1 |

Each proposed structure is a separate `ia-vN.md`, so versions can be
compared and tried one at a time.

## Test builds

APKs built from the menu redesign work are labelled as RGUI tests until
the new menu driver exists: the app name and the APK file name both say
"RetroArch Custom (RGUI test)", set at build time, not in
`pkg/android/phoenix/build.gradle`. The Simple layout is turned on in
Settings › User Interface › Menu Layout.
