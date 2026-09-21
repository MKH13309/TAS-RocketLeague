# Rocket League TAS Plugin

A BakkesMod plugin for making tool assisted shots (TAS) in Rocket League. It records the car, ball, and inputs every frame so you can edit and replay runs without messing up the physics.

![preview](1.png)

## Features

- Built-in GUI in BakkesMod (F2 -> Plugins -> TAS)
- Saves car position, ball, and inputs on every frame
- Branching: jump back to an earlier frame, take over, and record a new path
- Start frame slider to resume from any point
- Undo / redo system (Ctrl+Z / Ctrl+Y) with a take history slider
- Only keeps the changed frames in memory with a 64MB limit so it doesn't lag the game
- Separate speeds for replaying and recording (you can slow down the game when recording hard inputs)
- Saves everything as JSON files
- Checks that your map, hitbox, and sensitivities match so it doesn't desync
- Works in freeplay, workshop maps, and offline matches
- Two-Player TAS support (can be done in an Exhibition match with another bot)

## How to use

1. Go into freeplay, an offline match, or a workshop map (Two-Player TAS can be done in an Exhibition match, with another Bot).
2. Press F2, go to Plugins -> TAS -> Controls.
3. Click "New TAS" and enter a name.
4. Set your replay and record speed.
5. In Settings, set your interrupt key if you want to take over during replay.
6. Pick your start frame with the slider and click Start.
7. Click "Stop & Update" if you want to save the attempt as a new revision.
8. Click "Stop (Discard)" if you messed up and want to trash the attempt.
9. Use the take history slider or Ctrl+Z / Ctrl+Y to switch between takes.
10. Save the run under Loaded TAS when you're done.

Replays are saved in your BakkesMod folder under `data/TAS/`. If you had old files from BakkesTAS, it moves them over automatically.

## Console commands

You can also use these in the BakkesMod console (F6):

- `tas_start [frame]` - start replay/recording
- `tas_stop` - stop
- `tas_update` - update the current run
- `tas_stopandupdate` - stop and save the take
- `tas_changespeed` - change speed

### Keybinds

You can bind commands (like `tas_start` or `tas_stopandupdate`) to a keyboard key or controller button so you don't have to open the menu every time:
- **Through GUI**: Press F2 and go to the **Bindings** tab to add it.
- **Through F6 console**: Use the `bind` command:
  ```text
  bind XboxTypeS_DPad_Up "tas_start"
  bind T "tas_stopandupdate"
  ```

## Notes & Compatibility

- The plugin checks your map, car hitbox, steering sensitivity, and aerial sensitivity before starting. If anything is different from when you recorded it, it won't run so the physics don't break. Don't change sensitivities mid-run.
- Turn on notifications in BakkesMod (under Misc) so you can see if the plugin gives an error.

## Building from source

Requirements:
- Windows 10/11
- Visual Studio 2022 (with C++ workload)
- CMake 3.24+

All dependencies (BakkesMod SDK, ImGui, json, ninja) are in the `vendor` folder.

To build:
```powershell
.\build.ps1
```
The DLL will be in `dist/TAS.dll`.

To build and copy directly to your BakkesMod folder:
```powershell
.\build.ps1 -Deploy
```
Then load it in the console with `plugin load TAS`.

---

This project was made in collaboration with a local version of Qwen 3.8 27B.
