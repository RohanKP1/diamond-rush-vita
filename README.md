# Diamond Rush for PS Vita

A native PS Vita port of Gameloft's *Diamond Rush* (2006, J2ME). This is not an emulator: the
phone game's code is translated to C++ and runs on a small Java runtime and MIDP layer over SDL2,
with widescreen, Vita controls and on-screen Vita buttons.

> **Bring your own game.** This repository contains no part of Diamond Rush: no game code, art or
> sound, and no ready-made `.vpk`. You build the `.vpk` yourself from your own copy of the game's
> `.jar`. Please don't share the `.vpk` you build, since it contains Gameloft's game.

## Features

- **Widescreen 16:9** (282×160) for every screen: gameplay, menus, maps, cutscenes, the HUD.
  Switch to the original 4:5 in Options.
- **Vita buttons in the game:** prompts, help and tips show ✕ ○ △ □ and the d-pad instead of phone keys.
- **Display options:** SCALE: FULL (fill the screen, sharp) or PIXEL (whole-pixel 3×).
- **Faithful audio:** the game's MIDI music and effects, pre-rendered when you build, played with
  the phone's timing.
- A HUD laid out for the wide screen, cutscene text in the top-left corner, a blurred main-menu
  backdrop, a widened title screen and world banners. The last two are generated from your copy
  of the game (see "Extra artwork" below).
- Native speed on the Vita, and saves in `ux0:data/diamondrush/`.

## Controls

| Vita | Game |
|---|---|
| D-pad / left stick | Move |
| ✕ | Action / OK (phone key 5) |
| ○ | Back (right soft key) |
| △ | Left soft key (Skip, Continue, …) |
| START | Pause menu |
| □ or L | `*` (go back to the last checkpoint and lose a life) |
| R | `#` |

**Options:** SOUND ON/OFF, DISPLAY: WIDE / ORIGINAL, SCALE: FULL / PIXEL.

## Building the .vpk

You need Windows 10/11 with WSL (Ubuntu), about 3 GB of free disk space, and **your copy of
`Diamond-Rush_J2ME_EN_v104-Nokia-N6101.jar`**: version 1.0.4, English, Nokia N6101 build. The
build checks this exact file by SHA-256, because the port's changes are written for it.

1. Clone this repository and put the jar in its top folder.
2. One-time setup, which downloads the JDK, the decompiler, Vita3K and the Vita firmware into `env\`:
   ```powershell
   .\tools\setup-windows.ps1
   ```
   and in WSL, which installs the build packages and VitaSDK into `~/vitasdk` (asks for your
   sudo password):
   ```bash
   bash tools/setup-wsl.sh all
   ```
3. Build:
   ```powershell
   .\build-vpk.ps1
   ```
   This decompiles your jar, applies the port's changes (`patches/java-src.patch`), translates
   the game to C++, renders the music and builds **`out\diamondrush.vpk`**.

### Installing on a Vita

You need a Vita with HENkaku / Ensō and VitaShell.

1. Copy `out\diamondrush.vpk` to the Vita (VitaShell: SELECT for FTP, or USB).
2. In VitaShell, select the vpk and press ✕ to install. The game appears as **Diamond Rush**
   (`DRSH00001`).
3. If an updated LiveArea doesn't show, use VitaShell's △ menu → Refresh LiveArea.

### Trying it on the PC

- `.\run-vita3k.ps1` installs the vpk into Vita3K and starts it.
- `wsl ./build.sh run-desktop` runs a desktop build in a window. Keys: arrows; Z / Enter / Space
  = action; X / Esc = back; A = left soft key; S = `*`; D = `#`.

## Extra artwork (optional)

Your build works without these. With them, the title screen, world-map banners and LiveArea look
like the screenshots. They're made from your copy of the game, so they aren't in the repository:

- `tools/make_title_art.py`: the widescreen title background, from a capture of the phone's
  title painting (`port/art/title_phone.png`) and a 16:9 painting of the title screen
  (`title_screen.png`). It also makes the LiveArea background and start image.
- `tools/extract-banners.py` then `tools/make_banners_wide.py`: the widened world-map banners.
- `tools/make_livearea.py`: LiveArea images from a title-screen screenshot.

Without them the game falls back to a blurred title backdrop and mirrored banners, and the
LiveArea uses the game's own icon (`tools/make_default_livearea.py`, run automatically).

## How it works

```
your .jar ──Vineflower──► java-src/ ──patches/java-src.patch──► port changes
          ──tools/j2cpp──► port/src/game/gen/*.cpp ──VitaSDK──► eboot.bin ─┐
          ──unzip + MIDI→OGG──► res/ (art, sounds) ───────────────────────┴► diamondrush.vpk
```

| Path | What |
|---|---|
| `patches/java-src.patch` | the port's changes to the decompiled game: widescreen, Vita buttons, HUD, options, fixes |
| `tools/j2cpp/` | Java → C++ translator (javac's own parser and type checker) |
| `port/src/jrt/` | Java semantics in C++: references, arrays, exceptions, strings, threads |
| `port/src/midp/` | MIDP: graphics rasteriser, canvas loop, record stores, media |
| `port/src/platform/` | SDL2: display, input, audio, files |
| `tools/` | setup, decompile, translate, probes, asset generators |
| `PLAN.md` | design notes and decisions |

After changing anything in `java-src/`, run `.\tools\translate.ps1` and `wsl ./build.sh vita`.
To update the patch from your edits: `.\tools\decompile.ps1 -Out <empty folder>`, then
`bash tools/make-patch.sh <that folder>` (it checks that the new patch reproduces `java-src/`).

Testing helpers: `tools/port-probe.sh` runs the desktop build headless with a key script and
saves screenshots. Environment variables: `DR_TRACE=1` (log caught exceptions and audio),
`DR_WIDESCREEN=0|1`, `DR_UNLOCK=1` (open every stage; test saves only).

## Credits and license

- The port's own code and tools are MIT licensed (see `LICENSE`). That license does not cover
  Diamond Rush itself.
- *Diamond Rush* © 2006 Gameloft. This project is not affiliated with or endorsed by Gameloft.
- [Vineflower](https://github.com/Vineflower/vineflower) (decompiler), [VitaSDK](https://vitasdk.org),
  [SDL2](https://www.libsdl.org) and SDL2_mixer, [stb_image](https://github.com/nothings/stb)
  (public domain), [FluidSynth](https://www.fluidsynth.org) with the FluidR3 GM soundfont,
  [Vita3K](https://vita3k.org), [FreeJ2ME-Plus](https://github.com/TASEmulators/freej2me-plus).
