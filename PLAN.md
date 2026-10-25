# Diamond Rush (J2ME) → PS Vita (.vpk) port plan

## What we're porting

`Diamond-Rush_J2ME_EN_v104-Nokia-N6101.jar` — Gameloft, v1.0.4, MIDP-2.0 / CLDC-1.0, built for the
Nokia 6101 (**128×160** screen).

| Class | Size | Role (to be confirmed after decompiling) |
|---|---|---|
| `GloftDIRU` | 2 KB | MIDlet entry point |
| `i` | 193 KB | the game: canvas, main loop, logic, rendering, menus |
| `c` | 14 KB | sprite / font renderer (likely) |
| `a` | 10 KB | helper (resource loading / packs, likely) |
| `e`, `h`, `g`, `b`, `f`, `d` | < 5 KB each | small helpers |
| `com/nokia/mid/{ui,sound}/*` | – | Nokia API stubs bundled by the converter |

Data files (`*.f`, `*.bin`, `map_*.out`, `snd.f`, …) are Gameloft's packed resource formats. We don't
need to understand them: the translated code reads them exactly like the original did.

External API surface (from `tools/classrefs.py`) is small: `Canvas`, `Graphics` (fillRect,
drawRegion with transforms, drawRGB, clip, roundRect…), `Image` (PNG decode, mutable images,
createRGBImage, getRGB), `RecordStore`, `media.Player` + `VolumeControl`, `Display.vibrate`, plus
`String`, `StringBuffer`, `Hashtable`, `Random`, `Thread`, `System.arraycopy`.

## Strategy: native C++ port (not a JVM on the Vita)

Options considered:

1. **Run a J2ME emulator on the Vita.** No maintained J2ME runtime exists for the Vita; the PSP's
   PSPKVM runs under Adrenaline but doesn't produce a native `.vpk`, and it's slow and buggy.
2. **Port a small JVM (e.g. miniJVM) to the Vita + write MIDP in Java on top.** Works for any
   jar, but it's a large project, and interpreted bytecode plus GC tuning on the Vita is extra risk.
3. **Translate the game to C++ and reimplement the ~60 MIDP methods it uses.** ✅ Chosen.
   The game is ~230 KB of bytecode, mostly one class. The result is a native, fast, small `.vpk`
   that we can debug like normal C++.

The trick that keeps option 3 faithful: a **Java-semantics runtime** in C++ (`jarray<T>` with
bounds checks, Java integer overflow/shift/division rules, `jstring`, ref-counted objects). With it,
the decompiled Java translates almost line-for-line.

```
 ┌────────────────────────────────────────┐
 │ game/  translated classes (i, c, a, …) │  ← mechanical translation of decompiled Java
 ├────────────────────────────────────────┤
 │ midp/  Canvas Graphics Image RMS Player│  ← hand-written MIDP/Nokia shim (software renderer)
 │ jrt/   jarray jstring StringBuffer …   │  ← Java semantics in C++
 ├────────────────────────────────────────┤
 │ platform/  SDL2: screen, input, files  │  ← the only Vita/desktop-specific code
 └────────────────────────────────────────┘
        desktop (WSLg, ASan)      PS Vita (VitaSDK → .vpk)
```

## Test environment

| Tool | Where | Purpose |
|---|---|---|
| VitaSDK (`~/vitasdk`) | WSL Ubuntu | ARM cross-compiler, Vita libs (SDL2, SDL2_mixer), vpk packaging |
| gcc + SDL2 + ASan/UBSan | WSL Ubuntu | **desktop build** of the same code: fast edit/run loop, catches memory bugs |
| Vita3K | `env/vita3k` | Vita emulator: install and run the `.vpk` on Windows |
| FreeJ2ME-Plus + JDK | `env/` | runs the original jar, as the ground truth for behaviour and visuals |
| Vineflower | `env/vineflower.jar` | decompiler |
| real Vita (HENkaku/Ενσο + VitaShell) | – | final testing over FTP (`ux0:`), since Vita3K isn't perfect |

Setup:

```powershell
# Windows (PowerShell): Vita3K, firmware, JDK, decompiler, reference emulator
.\tools\setup-windows.ps1
```
```bash
# WSL: host packages (asks for your sudo password), then VitaSDK (no sudo)
bash tools/setup-wsl.sh deps
bash tools/setup-wsl.sh vitasdk
```
First Vita3K launch: run `env\vita3k\Vita3K.exe` and install `env\PSVUPDAT.PUP` when the
setup wizard asks for firmware.

Day-to-day loop:

```bash
./build.sh run-desktop      # build + run in a window via WSLg (keyboard: arrows, Z/Enter, Q, W, 0-9)
./build.sh vita             # -> out/diamondrush.vpk
```
```powershell
.\run-vita3k.ps1            # install out\diamondrush.vpk into Vita3K and boot it
```

## Status (2026-10-25)

| Phase | State |
|---|---|
| 0 Toolchain (VitaSDK, SDL2, Vita3K, probes) | ✅ |
| 1 Decompile + analyse (`java-src`, 10 decompiler fixes, recompiled jar plays in FreeJ2ME) | ✅ |
| 2 Runtime (`jrt`) + MIDP (`midp`) + platform | ✅ |
| 3 Translator (`tools/j2cpp`): all 10 game classes, ~23k lines of C++, no hand edits | ✅ |
| 4 Vita polish: controls, fit/pixel scaling, **widescreen 282×160**, Vita button glyphs, LiveArea, settings | ✅ |
| 5 Real-hardware playthrough of every world | ⏳ needs a Vita |

Verified: boots, menus, sound/music, saves (RMS) and gameplay on desktop, and in Vita3K in both
4:5 and 16:9.

### How the pieces fit
- **Java is the master copy.** Fixes and features go into `java-src` (marked `// [port]`), get tested
  in FreeJ2ME (`tools/build-jar.sh`), then `tools/translate.ps1` regenerates the C++.
- **Translator guarantees** (see `J2Cpp.java`):
  - locals are hoisted (goto/switch-safe), and labeled break/continue become goto;
  - `catch` dispatches on the Java exception type;
  - Java's left-to-right evaluation order is enforced with temporaries (C++ leaves it unspecified);
  - auto-unboxing, `Object[]` covariance, Java `/` `%` and shifts are handled.
- **Threads:** the game's threads run under one VM lock released in `sleep`/`yield`, like a
  single-core phone. Key presses are delivered at those yield points and held for at least 90 ms,
  because the game polls a key bitmask every 62 ms.
- **Widescreen** applies to every screen. Screens laid out for 128 px are drawn translated by
  `class_9.OX = (SW - 128) / 2`, and anything full-width spans `-OX .. SW - OX`: backgrounds, menu
  bars, loading panel and bar, cutscene bars, the HUD pipe (tiled from its first 24 px),
  and soft keys at the real corners. The title / main menu background is
  `port/res/port_title_wide.png`, built by `tools/make_title_art.py`: the phone's own title
  painting (`port/art/title_phone.png`) in the middle, pixel for pixel, with the painted sides of
  `title_screen.png`, scaled and placed so its copy of the painting lies on the phone's; the
  painting's outer 24 columns ease into the softer sides. It replaces the 128x160 painting.
  Behind the main menu the background shows blurred and dimmed (`class_9.menuBlur`, built once
  per width), instead of the phone's checkerboard of black pixels. The LiveArea `bg.png` is that menu background (4x whole pixels, then
  fitted); `startup.png` is `startup.png` centred, with rounded corners. The world-map banners are
  `port/res/port_banner_<n>.png`: each mirrored at its edges (seamless joins), with exemplar
  inpainting (`tools/widen.py`) wherever the mirror would double a building
  (`tools/make_banners_wide.py`).
- **Port settings in Options:** DISPLAY (wide / original) and SCALE (full / pixel) reach the host
  via `platformRequest("port:toggle-widescreen" / "port:toggle-scale")` and
  `getAppProperty("Port-Widescreen" / "Port-Scale")`; there are no key bindings for them.
- **Widescreen layout tweaks:** the HUD bar is cut into two column groups (`class_9.hudFrame`):
  columns 0..85 (compass, face, lives, energy: the phone's left part, intact) at the left edge and
  85..128 (diamond jars) at the right edge, with the bar's plain pipe module tiled between. Keys
  sit on that pipe just left of the jars, each centred by its visible pixels, both kinds when both
  are held ([silver n] [gold n]; the phone shows one, above the jars over the playfield); the bar is
  repainted when they change. Cutscene dialogue moves into
  the top-left corner: the portrait in the corner, the text box beside it in the letterbox bar.
- **Vita buttons in the UI:** the font draws chars U+E000.. as 9x9 glyphs from `port/res/port_btn.png`
  (`tools/make_glyphs.py`): cross, circle, triangle, square, d-pad arrows, L, R. `class_9.portKeys`
  rewrites phone keys in all game text ("Press 5" -> cross, "*" -> square, "4, 6" / "2, 8" ->
  d-pad). Tips use `@` and bytes 0x80.. for glyphs. The movement tip's key images, the dialogue's
  "next" key, and the soft-key corner icons (triangle = left soft key, circle = right) are drawn as glyphs.
- **Widescreen gameplay:** `class_9.SW` follows `Canvas.getWidth()`:
  - gameplay renders SW wide, with the camera dead zone and clamps relative to SW, and levels
    narrower than the screen centered;
  - menus and the pause menu are centered 128-px columns;
  - "is it on screen" checks use the visible edge.
- **Decompiler `byte` counters:** Vineflower types some int loop counters as `byte`; past 127 they
  wrap (the world map's background loop never ended, freezing CONTINUE). Those loops are `int` now.
- **Audio:** the game's 21 MIDI clips are rendered to OGG at build time (`tools/extract_sounds.py`)
  and matched at runtime by an FNV-1a hash of the MIDI bytes the game passes to `createPlayer`.
  Each clip ends at its MIDI's last event, as on the phone (`snd/lengths.txt`; the synth tail
  fades out in 120 ms): the game plays one sound at a time by priority and skips sounds while a
  higher-priority one still plays, so the renders' multi-second tails used to swallow sounds.

## Next steps
1. Play every world on a real Vita: Angkor, Scotland (Bavaria), Tibet, plus the secret levels. Look for:
   - widescreen edge cases: scripted events that trigger on "visible edge", and narrow rooms;
   - performance (expected to be trivial: 16 fps logic, software blit at 282×160).
2. Optional: touch controls (front panel as D-pad and soft keys), a bezel image in pixel mode,
   and save on PS-button suspend.

## Notes / risks
- **Legal:** Diamond Rush is Gameloft's copyrighted game. The port is for personal use with a jar you
  own. Don't distribute the `.vpk`, which contains the game's code and assets.
- Gameloft code often relies on Java integer semantics and catches `Exception` broadly. The `jrt`
  layer must reproduce that, or bugs will surface as subtle gameplay differences.
- Timing: J2ME games usually sleep a fixed time per frame. Keep the original frame pacing (no
  uncapped speed-up at 60 Hz).
