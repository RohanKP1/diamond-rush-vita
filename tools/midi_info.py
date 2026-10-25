"""Minimal Standard MIDI File reader: playing time, for matching rendered clips to the phone's.

A phone's MIDI player reports a clip as playing until the file's last event; a software synth
render keeps going while notes release and reverb fades. midi_seconds() is the former.
"""
import struct


def _varlen(d, i):
    v = 0
    while True:
        b = d[i]
        i += 1
        v = (v << 7) | (b & 0x7F)
        if not b & 0x80:
            return v, i


def midi_seconds(data: bytes) -> float:
    """Time of the last event (end of track included), honouring tempo changes."""
    assert data[:4] == b"MThd", "not a MIDI file"
    hlen, fmt, ntrk, div = struct.unpack(">IHHH", data[4:14])
    assert not div & 0x8000, "SMPTE time division not supported"
    pos = 8 + hlen
    events = []  # (tick, tempo or None)
    last_tick = 0
    for _ in range(ntrk):
        assert data[pos:pos + 4] == b"MTrk"
        tlen = struct.unpack(">I", data[pos + 4:pos + 8])[0]
        i, end = pos + 8, pos + 8 + tlen
        tick, status = 0, 0
        while i < end:
            dt, i = _varlen(data, i)
            tick += dt
            b = data[i]
            if b & 0x80:
                status = b
                i += 1
            if status == 0xFF:
                mtype = data[i]
                mlen, i = _varlen(data, i + 1)
                if mtype == 0x51:
                    events.append((tick, int.from_bytes(data[i:i + 3], "big")))
                i += mlen
            elif status in (0xF0, 0xF7):
                slen, i = _varlen(data, i)
                i += slen
            else:
                i += 1 if (status & 0xF0) in (0xC0, 0xD0) else 2
            last_tick = max(last_tick, tick)
        pos = end
    # ticks -> seconds through the tempo map (default 120 bpm)
    events.sort()
    secs, t, tempo = 0.0, 0, 500000
    for et, tp in events:
        if et > last_tick:
            break
        secs += (et - t) * tempo / div / 1e6
        t, tempo = et, tp
    return secs + (last_tick - t) * tempo / div / 1e6


def ogg_seconds(data: bytes, rate: int = 44100) -> float:
    """Length of an Ogg Vorbis file from its last page's granule position."""
    i = data.rfind(b"OggS")
    return struct.unpack_from("<q", data, i + 6)[0] / rate
