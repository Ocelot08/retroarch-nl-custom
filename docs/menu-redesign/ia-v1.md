# Proposed menu structure (IA): RetroArch Custom

- **Status:** v1, Claude Code's first pass. A draft for discussion, one of several versions to compare. It covers L1 and L2, plus the Settings categories (L3) and the onboarding flow.
- **Figma:** https://www.figma.com/design/LVjpP2ficnkwSXrLQrceU7/Testing-MCP?node-id=79-4579
- **Current menus, for comparison:**
  - map: https://www.figma.com/design/LVjpP2ficnkwSXrLQrceU7/Testing-MCP?node-id=70-1543
  - data: `ia-current.csv`
- **Build approach:** option 2 (new lists behind a "Menu layout: Classic / Simple" switch) plus option 3 (a phone-first menu driver). See `frontend-redesign-brief.md`.

**Markers used in the trees:**

| Marker | Meaning |
|---|---|
| **TOP** | Highest priority for a phone player. A judgement call, not usage data. |
| ADV | Advanced: behind a "More …" row, or shown only with "Show Advanced Settings" on. |
| › | Opens another list. |
| ( ) | A setting's value. |

## Principles

1. **Two short top-level menus:** Home when no game is running, and the Game menu when one is.
2. **Each list has one home.** Today 8 lists can be reached from two places.
3. **Everyday settings sit at L2/L3.** Each category ends in a "More … settings" row holding its advanced items, so they stay next to what they tune.
4. **One Advanced category** holds only the categories that are advanced as a whole.
5. **At most about 9 rows per everyday list.** Anything beyond that goes behind "More".
6. **Actions people repeat are rows, not submenus.** Examples: Save State and Load State.
7. **Plain words over RetroArch terms:** "Keep Settings For…" instead of Overrides, "Downloads" instead of Online Updater.
8. **Nothing is removed.** Every current setting stays reachable. Only its place changes.

## 1. Home (no game running): replaces Main Menu, 11 rows down to 6

```
Home
├─ Continue: <last game>                TOP  action
├─ Library ›                            TOP
├─ Gamepads ›                           TOP
├─ Settings ›
├─ Downloads ›
└─ Help & About ›
```

### Library ›

Merges Load Content, History, Favorites, Playlists, Load Core and Import Content.

```
├─ Recent ›                             TOP   (History)
├─ Favorites ›                          TOP
├─ Systems ›                                  (playlists)
├─ Browse Files ›                             (Load Content)
├─ Add Games Folder                           action (scan)
└─ Load Core ›                          ADV
```

### Gamepads ›

From Quick Menu › On-Screen Overlay › Touch Layout (L5 today) and the overlay settings.

```
├─ Edit Layout                          TOP   action
├─ Load Gamepad ›                       TOP
├─ Toggle Controls ›
├─ Custom Buttons ›
├─ Button Style ›
├─ Overlay Options ›                          (opacity, hide in menu, hide with controller, sensitivity, auto-rotate/scale, keyboard overlay)
└─ Manage Gamepads ›                          (Save as, Delete, Reset, Export, Import)
```

### Downloads ›

From Online Updater.

```
├─ Cores ›                              TOP   (Core Downloader, Update Installed Cores)
├─ System Files (BIOS) ›                      (Core System Files Downloader)
├─ Update Everything                          action (core info, assets, profiles, cheats, databases, overlays, shaders)
├─ Thumbnails ›
├─ Content Downloader ›
└─ Download Settings ›                  ADV   (Updater Settings)
```

### Help & About ›

From Information, Restart and Quit.

```
├─ Getting Started                            action (reruns onboarding)
├─ System Information ›
├─ Controllers & Network Info ›
├─ Database Manager ›                   ADV
├─ Restart RetroArch                    ADV   action
└─ Quit RetroArch                             action
```

## 2. Game menu (game running): replaces Quick Menu

```
Game menu
├─ Resume                               TOP  action
├─ Save State                           TOP  action
├─ Load State                           TOP  action
├─ State Slot (0)
├─ Gamepad ›                            TOP
├─ Display ›
├─ Core Options ›
├─ Keep Settings For… ›
├─ Cheats ›
├─ More ›
├─ Reset                                     action
└─ Close Game                                action
```

### Gamepad ›

The same pages as Home › Gamepads. A gamepad picked here lasts until the game closes, unless it is kept with Keep Settings For.

```
├─ Load Gamepad ›                       TOP
├─ Edit Layout                          TOP   action
├─ Toggle Controls ›
├─ Button Style ›
└─ Overlay Options ›
```

### Display ›

From Shaders and Settings › Video › Scaling, applied to this game.

```
├─ Aspect Ratio (Core provided)         TOP
├─ Integer Scale (OFF)
├─ Shader Preset ›
├─ Shader Parameters ›
└─ Bilinear Filtering (OFF)
```

### Keep Settings For… ›

From Overrides, remap files and Save Core Options.

```
├─ This Game                            TOP   action
├─ This Core (system)                         action
├─ This Folder                          ADV   action
├─ Remove Saved Settings                      action
└─ Load From File ›                     ADV
```

### More ›

The Quick Menu rows that are used rarely.

```
├─ Undo Save / Load State                     action
├─ Take Screenshot                            action
├─ Add to Favorites                           action
├─ Controls ›                                 (port remaps, turbo fire)
├─ Rewind ›
├─ Latency ›                            ADV
└─ Game Information ›
```

## 3. Settings: 18 categories down to 8 plus Advanced

```
Settings
├─ Display ›                            TOP
├─ Audio ›
├─ Controls ›                           TOP
├─ Saving ›                             TOP
├─ Interface ›
├─ Library ›
├─ Accounts ›
├─ Accessibility ›
└─ Advanced ›                           ADV
```

Where each current Settings category goes:

| Current | Proposed |
|---|---|
| User Interface | Interface: Appearance, Menu Item Visibility, Notifications, File Browser (ADV), confirmations, language, kiosk |
| User Interface › On-Screen Overlay | Home › Gamepads (one home) |
| User Interface › Menu Controls | Controls › Menu Controls (one home) |
| Video | Display (Scaling, Output basics, VSync); the rest goes to More Display Settings |
| Audio | Audio; MIDI, Mixer and resampler go to More Audio Settings |
| Input | Controls: Port binds, Hotkeys, Turbo, Vibration, Sensors, Menu Controls; deadzones and advanced input go to More |
| Latency | Advanced › Latency (one home; Game menu › More links to it) |
| Frame Throttle | Advanced › Frame Throttle (Rewind keeps one home here) |
| Drivers | Advanced › Drivers |
| Playlists | Library |
| Core | Advanced › Cores (Updater Settings lives only in Downloads) |
| Saving | Saving (+ Cloud Sync) |
| Configuration | Advanced › Configuration Files (+ Main Menu › Configuration File) |
| Accessibility | Accessibility (+ AI Service) |
| Power Management | Advanced › Power |
| Network | Advanced › Network & Netplay |
| Achievements | Accounts › RetroAchievements |
| User | Accounts (Username, Privacy, streaming keys) |
| Directory | Advanced › Directories |
| Logging | Advanced › Logging |

### Two everyday categories, as examples

```
Display ›
├─ Aspect Ratio (Core provided)         TOP
├─ Integer Scale (OFF)
├─ Screen Orientation (Auto)
├─ Bilinear Filtering (OFF)
├─ VSync (ON)
└─ More Display Settings ›              ADV   (refresh rate, output, fullscreen/windowed, stereo, CRT, filters)

Saving ›
├─ SaveRAM Autosave (10 s)              TOP
├─ Auto Save State on Close (OFF)       TOP
├─ Auto Load State (OFF)
├─ Cloud Sync ›
└─ More Saving Settings ›               ADV   (folder sorting, compression, replays, screenshots, runtime logs)
```

### Advanced ›

These categories are kept whole, just one tap further away.

```
├─ Latency ›                            (run-ahead, frame delay, audio latency)
├─ Frame Throttle ›                     (fast-forward, slow motion, rewind)
├─ Network & Netplay ›
├─ Cores ›                              (manage cores, core info behaviour)
├─ Drivers ›
├─ Directories ›
├─ Configuration Files ›
├─ Power Management ›
└─ Logging ›
```

### Why use both "More …" rows and Advanced

- **A single global Advanced page would split related settings.** Refresh-rate options would end up far from Aspect Ratio.
- **"More Display Settings" keeps them together,** one tap away (L4 at most).
- **Advanced holds only categories that are advanced as a whole.**
- **The existing "Show Advanced Settings" switch can stay** in Interface, as "show everything inline" for power users.

## 4. First-run setup (onboarding)

- **When it runs:** once, on first launch. Every step can be skipped.
- **Running it again:** from Help & About › Getting Started.
- **Form:** built as RetroArch menu lists, not a separate Android screen.

| Step | Shows | Data or task it uses |
|---|---|---|
| 1. Welcome | "How will you play?": touch gamepad / controller / both | Sets overlay defaults |
| 2. Your games | Choose the games folder (Android picker), or "later" | Saves the folder; feeds the scan |
| 3. Systems & cores | Systems found, with one recommended core each (on by default) and "Show all cores per system" | File extensions (core info `supported_extensions`) plus a curated recommended list; core updater task |
| 4. Essentials | One download: core info, databases, assets, controller profiles; thumbnails optional | The existing updater tasks |
| 5. BIOS check | Missing BIOS files per chosen core; open the System/BIOS folder | Core info `firmware*` entries |
| 6. Done | Scan the folder and open the Library | The existing scan task, which creates playlists |

**Prompts at the point of need:**

- An empty Library shows an "Add Games Folder" button.
- Opening a file with no core for it asks "Download a core for this system?" and offers the recommended core.
- A core with a missing BIOS file names the file and the folder it goes in, instead of failing to load.
- Available updates show as a count badge on Downloads.

## Open questions

1. Should Restart and Quit move to Help & About? Android's back button already handles exiting.
2. Should the Game menu keep "Gamepad" as its own row, or fold it into Controls?
3. Should Classic layout stay reachable, and from where? Interface › Menu Layout is one option.
4. Who maintains the list of recommended cores per system?
5. Should the priorities be confirmed with usage data, e.g. counts of which menu rows get opened?
