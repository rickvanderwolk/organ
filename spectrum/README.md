# Spectrum

Real-time frequency visualizer - each active LED shows a frequency band, brightness = volume.

```
[bass]                                      [treble]
LED 0   2   4   6   8   ...                    118
  ■     ■       ■   ■                       ■
  dim       bright      off     dim     bright
```

## How It Works

1. Microphone captures audio (64 samples)
2. FFT converts sound to 32 frequency bins
3. Each bin maps to active LED(s)
4. LED brightness = volume of that frequency

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
| `NUM_LEDS` | Total LEDs on your strip | 120 |
| `RESOLUTION` | Skip LEDs (see below) | 4 |
| `SENSITIVITY` | Higher = more sensitive | 1.5 |
| `NOISE_LOW/MID/HIGH` | Noise threshold per freq range | 50/60/80 |
| `BASE_R/G/B` | LED color (RGB 0-255) | Green |

## Resolution Explained

Resolution determines which LEDs are used by skipping LEDs:

| Resolution | Uses LED | Active LEDs | Effect |
|------------|----------|-------------|--------|
| 1 | 0, 1, 2, 3... | 120 | Every LED |
| 2 | 0, 2, 4, 6... | 60 | Every 2nd LED |
| 3 | 0, 3, 6, 9... | 40 | Every 3rd LED |
| 4 | 0, 4, 8, 12... | 30 | Every 4th LED |

Higher resolution = more space between LEDs, cleaner look.

With 32 FFT frequency bins:
- Resolution 4 → 30 LEDs → almost 1:1 (each LED = unique frequency)
- Resolution 1 → 120 LEDs → ~4 LEDs share same frequency

## Troubleshooting

**LEDs flicker during silence:** Increase `NOISE_*` values

**No response to sound:** Decrease `NOISE_*` or increase `SENSITIVITY`

**High frequencies always on:** Increase `NOISE_HIGH`

## Acknowledgments

Thanks to Tim van der Wolk for coming up with this concept.
