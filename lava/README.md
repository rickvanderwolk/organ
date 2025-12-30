# Lava

Lava lamp style visualizer - smooth floating blobs that react to audio.

```
[    ●●●●●        ●●●●           ●●●●●●    ]
      blob1       blob2           blob3
              ← floating →
```

## How It Works

1. Audio spike instantly spawns a blob
2. Blob slowly fades out, shrinks, and drifts
3. Colors blend where blobs overlap
4. Real-time trigger + smooth lava-like trail

## Color Modes

Set `COLOR_MODE` to choose:

- `0` = **Random:** each blob gets a random color
- `1` = **Frequency:** blob color based on dominant frequency (default)

### Frequency Mode Colors

| Frequency | Color |
|-----------|-------|
| Bass | Red |
| Low-mid | Orange |
| Mid | Yellow |
| High-mid | Green |
| High | Blue |
| Treble | Purple |

Kick drums create red blobs, hi-hats create blue/purple blobs.

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
| `BRIGHTNESS` | LED brightness (0-255) | 100 |
| `NUM_LEDS` | Total LEDs on your strip | 120 |
| `MAX_BLOBS` | Max simultaneous blobs | 8 |
| `TRIGGER_THRESHOLD` | Audio level to spawn blob (0-1) | 0.3 |
| `SPAWN_SIZE` | Initial blob size | 10.0 |
| `FADE_RATE` | How fast blobs fade (lower = faster) | 0.92 |
| `SHRINK_RATE` | How fast blobs shrink | 0.97 |
| `DRIFT_SPEED` | How fast blobs drift | 0.3 |
| `COLOR_MODE` | Color mode (see above) | 1 |

## Customization

Edit `BAND_COLORS` array to change the available colors (used by both modes).

## Troubleshooting

**Blobs spawn too often:** Increase `TRIGGER_THRESHOLD`

**Not reacting to audio:** Decrease `TRIGGER_THRESHOLD`

**Fading too fast:** Increase `FADE_RATE` (closer to 1.0)

**Colors mostly one color (frequency mode):** The system auto-calibrates - give it a few seconds to adapt. Or try `COLOR_MODE = 0` for random colors.
