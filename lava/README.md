# Lava

Lava lamp style visualizer - smooth floating blobs that react to audio.

```
[    ●●●●●        ●●●●           ●●●●●●    ]
      blob1       blob2           blob3
              ← floating →
```

## How It Works

1. Multiple "blobs" float across the strip
2. Each blob has soft edges (fades smoothly)
3. Audio makes blobs expand, brighten, and move faster (real-time)
4. Colors blend where blobs overlap
5. Smooth, hypnotic lava lamp effect

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
| `NUM_BLOBS` | Number of blobs (3-5 works well) | 4 |
| `BASE_SPEED` | Base movement speed | 0.3 |
| `BASE_SIZE` | Base blob size in LEDs | 6.0 |
| `AUDIO_EXPAND` | How much audio expands blobs | 12.0 |
| `AUDIO_SPEED` | How much audio speeds up movement | 0.5 |
| `AUDIO_BRIGHTNESS` | How much audio affects brightness | 0.8 |

## Blob Colors

Default warm lava colors (pink, orange, yellow, magenta). Edit `BLOB_COLORS` array to customize.

## Troubleshooting

**Blobs too fast:** Decrease `BASE_SPEED`

**Not reacting to audio:** Increase `AUDIO_EXPAND` and `AUDIO_BRIGHTNESS`

**Too chaotic:** Reduce `NUM_BLOBS` or `AUDIO_EXPAND`

**Too subtle:** Increase `AUDIO_EXPAND` and `AUDIO_BRIGHTNESS`
