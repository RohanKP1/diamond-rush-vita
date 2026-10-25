"""Extract the game's MIDI clips from snd.f and pre-render them to OGG.

snd.f layout (from class_2.method_8/method_9): 1 skipped byte, a 168-byte table
(21 entries x 8 bytes; clip size = u16 LE at entry offset 4), then the clips back to back.

At runtime, Manager.createPlayer() hashes the MIDI bytes it is given (FNV-1a 32) and plays
snd/<hash>.ogg, so this file and port/src/midp/media.cpp must agree on the hash.

snd/lengths.txt lists each clip's MIDI playing time ("<hash> <ms>"): a phone's player stops at
the file's last event, while the synth render rings on for seconds, so the port ends clips there
(the game plays one sound at a time and skips new ones while the current one still plays).

Usage: python3 -I tools/extract_sounds.py jar/snd.f <out-dir> [soundfont.sf2]
"""
import shutil
import struct
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from midi_info import midi_seconds  # noqa: E402

CLIPS = 21


def fnv1a(data: bytes) -> int:
    h = 0x811C9DC5
    for b in data:
        h = ((h ^ b) * 0x01000193) & 0xFFFFFFFF
    return h


def main():
    src, out = Path(sys.argv[1]), Path(sys.argv[2])
    sf2 = sys.argv[3] if len(sys.argv) > 3 else "/usr/share/sounds/sf2/FluidR3_GM.sf2"
    data = src.read_bytes()
    table = data[1:1 + CLIPS * 8]
    pos = 1 + CLIPS * 8
    out.mkdir(parents=True, exist_ok=True)
    fluidsynth = shutil.which("fluidsynth")
    lengths = []
    for i in range(CLIPS):
        size = struct.unpack_from("<H", table, i * 8 + 4)[0]
        clip = data[pos:pos + size]
        pos += size
        name = f"{fnv1a(clip):08x}"
        mid = out / f"{name}.mid"
        ogg = out / f"{name}.ogg"
        mid.write_bytes(clip)
        lengths.append(f"{name} {round(midi_seconds(clip) * 1000)}")
        if fluidsynth and not ogg.exists():
            # -T oga: Ogg Vorbis via libsndfile; -g: gain, phones played these loud
            subprocess.run([fluidsynth, "-ni", "-q", "-g", "1.0", "-r", "44100",
                            "-T", "oga", "-F", str(ogg), sf2, str(mid)],
                           check=True, stdout=subprocess.DEVNULL)
        print(f"clip {i:2d}: {size:6d} bytes -> {name}")
    (out / "lengths.txt").write_text("\n".join(lengths) + "\n")
    if pos != len(data):
        print(f"warning: {len(data) - pos} trailing bytes", file=sys.stderr)


if __name__ == "__main__":
    main()
