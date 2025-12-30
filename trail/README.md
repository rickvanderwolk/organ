# Trail

Scrolling frequency visualizer with fading history - patterns flow across the strip.

```
[REAL-TIME]     [HISTORY → fades out]
[0][1]...[7]    [8][9][10]........[119]
 ↑ 8 bands       scrolls and fades →
```

## How It Works

1. Microphone captures audio (64 samples)
2. FFT splits sound into 8 frequency bands
3. First 8 LEDs show real-time band activity
4. Every frame, pattern scrolls right
5. Old patterns slowly fade out

## Frequency Bands

| Band | Frequency | Color |
|------|-----------|-------|
| 0 | ~150 Hz (sub-bass) | Deep blue |
| 1 | ~230 Hz (bass) | Blue-purple |
| 2 | ~390 Hz (low-mid) | Purple |
| 3 | ~550 Hz (mid) | Violet |
| 4 | ~780 Hz (mid) | Magenta-purple |
| 5 | ~1100 Hz (high-mid) | Magenta |
| 6 | ~1500 Hz (presence) | Hot pink |
| 7 | ~2000 Hz (treble) | Pink-red |

## Wiring

```
Microphone OUT  →  A0
LED Data        →  D6
LED 5V          →  5V (external power for >30 LEDs)
LED GND         →  GND
```

## Key Settings

| Parameter | Description | Default |
|-----------|-------------|---------|
| `SCROLL_MS` | Scroll speed in ms (lower = faster) | 50 |
| `FADE_AMOUNT` | Fade factor 0-1 (lower = faster fade) | 0.92 |
| `THRESHOLD` | Activation threshold 0-1 | 0.70 |
| `COLORS[8]` | Color per band (hex 0xRRGGBB) | Cyberpunk |

## Color Palettes

```cpp
// Cyberpunk (default)
0x0044FF, 0x2200FF, 0x6600FF, 0x9900FF, 0xCC00FF, 0xFF00CC, 0xFF0088, 0xFF0044

// Fire
0xFF0000, 0xFF2200, 0xFF4400, 0xFF6600, 0xFF8800, 0xFFAA00, 0xFFCC00, 0xFFFF00

// Ocean
0x000033, 0x000066, 0x000099, 0x0000CC, 0x0033FF, 0x0066FF, 0x0099FF, 0x00CCFF
```

## Troubleshooting

**LEDs flicker during silence:** Increase `BAND_NOISE` values

**No response to sound:** Decrease `THRESHOLD`

**Scrolling too fast/slow:** Adjust `SCROLL_MS`
