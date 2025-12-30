// ============================================================================
// TRAIL - Scrolling frequency visualizer with history
// ============================================================================
//
// How it works:
// 1. Microphone captures audio (64 samples)
// 2. FFT splits sound into 8 frequency bands
// 3. First 8 LEDs show real-time which bands are active
// 4. Every SCROLL_MS the pattern shifts to the right
// 5. Old patterns slowly fade out
//
// Visual effect:
//   LED 0-7: Real-time frequency bands (bass left, treble right)
//   LED 8+:  History that scrolls right and fades out
//
//   [REAL-TIME] -----> [HISTORY scrolls and fades] ----->
//   [0][1][2]...[7] | [8][9][10].............[119]
//
// Frequency bands:
//   Band 0: ~150 Hz   (sub-bass)     - Color: Deep blue
//   Band 1: ~230 Hz   (bass)         - Color: Blue-purple
//   Band 2: ~390 Hz   (low-mid)      - Color: Purple
//   Band 3: ~550 Hz   (mid)          - Color: Violet
//   Band 4: ~780 Hz   (mid)          - Color: Magenta-purple
//   Band 5: ~1100 Hz  (high-mid)     - Color: Magenta
//   Band 6: ~1500 Hz  (presence)     - Color: Hot pink
//   Band 7: ~2000 Hz  (treble)       - Color: Pink-red
//
// ============================================================================

#include <Adafruit_NeoPixel.h>
#include <arduinoFFT.h>

// ============================================================================
// SETTINGS - Adjust to taste
// ============================================================================

// --- General ---
const int BRIGHTNESS = 100;           // LED brightness (0-255)
const bool REVERSE_DIRECTION = true;  // true = scroll left, false = scroll right

// --- Sensitivity ---
const float THRESHOLD = 0.70;         // Activation threshold (0.0-1.0)
                                      // Lower = more sensitive, higher = only loud sounds
const bool USE_INTENSITY = true;      // true = brightness varies with volume
                                      // false = always full brightness

// --- Scroll effect ---
const int SCROLL_MS = 50;             // Scroll speed in milliseconds
                                      // Lower = faster scrolling
const float FADE_AMOUNT = 0.92;       // Fade factor per scroll step (0.0-1.0)
                                      // Lower = faster fade out

// --- Number of real-time LEDs ---
const int REALTIME_LEDS = 8;          // How many LEDs for real-time display
                                      // = number of frequency bands

// ============================================================================
// FREQUENCY BANDS - For advanced users
// ============================================================================

const int NUM_BANDS = 8;

// FFT bin where each band starts (with 64 samples, 5000Hz sample rate)
// Bin number = frequency / (sample_rate / samples) = freq / 78.125
const int BAND_START[8] = {
  2,    // Band 0: ~156 Hz (sub-bass)
  3,    // Band 1: ~234 Hz (bass)
  5,    // Band 2: ~390 Hz (low-mid)
  7,    // Band 3: ~547 Hz (mid)
  10,   // Band 4: ~781 Hz (mid)
  14,   // Band 5: ~1094 Hz (high-mid)
  19,   // Band 6: ~1484 Hz (presence)
  25    // Band 7: ~1953 Hz (treble)
};
const int BAND_END = 32;              // Last FFT bin

// Noise floor per band (signal below this value is ignored)
// Band 0 (bass) has higher threshold because low freq have more noise
const float BAND_NOISE[8] = {
  200,  // Band 0: high threshold for sub-bass
  80,   // Band 1-7: standard threshold
  80,
  80,
  80,
  80,
  80,
  80
};

// Enable/disable individual bands
const bool BAND_ENABLED[8] = {
  true, true, true, true, true, true, true, true
};

// ============================================================================
// COLORS - Cyberpunk palette
// ============================================================================

// Hex color code per band (0xRRGGBB)
const uint32_t COLORS[8] = {
  0x0044FF,  // Band 0: Deep blue
  0x2200FF,  // Band 1: Blue-purple
  0x6600FF,  // Band 2: Purple
  0x9900FF,  // Band 3: Violet
  0xCC00FF,  // Band 4: Magenta-purple
  0xFF00CC,  // Band 5: Magenta
  0xFF0088,  // Band 6: Hot pink
  0xFF0044   // Band 7: Pink-red
};

// Other palette ideas:
// Fire:     0xFF0000, 0xFF2200, 0xFF4400, 0xFF6600, 0xFF8800, 0xFFAA00, 0xFFCC00, 0xFFFF00
// Ocean:    0x000033, 0x000066, 0x000099, 0x0000CC, 0x0033FF, 0x0066FF, 0x0099FF, 0x00CCFF
// Rainbow:  0xFF0000, 0xFF7F00, 0xFFFF00, 0x00FF00, 0x0000FF, 0x4B0082, 0x9400D3, 0xFF1493

// ============================================================================
// HARDWARE - Adjust for your setup
// ============================================================================

const int MIC_PIN = A0;               // Microphone analog pin
const int LED_PIN = 6;                // LED data pin
const int NUM_LEDS = 120;             // Total number of LEDs on strip

// ============================================================================
// FFT CONFIGURATION - Usually no need to change
// ============================================================================

const uint16_t SAMPLES = 64;          // Number of audio samples
const double SAMPLE_FREQ = 5000;      // Sample frequency in Hz

// ============================================================================
// INTERNAL VARIABLES - Do not change
// ============================================================================

Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

double vReal[SAMPLES];
double vImag[SAMPLES];
ArduinoFFT<double> FFT = ArduinoFFT<double>(vReal, vImag, SAMPLES, SAMPLE_FREQ);

float bands[NUM_BANDS];
float bandPeaks[NUM_BANDS];

unsigned long lastScroll = 0;

// ============================================================================
// FUNCTIONS
// ============================================================================

int led(int i) {
  return REVERSE_DIRECTION ? (NUM_LEDS - 1 - i) : i;
}

uint32_t dimColor(uint32_t color, float intensity) {
  uint8_t r = ((color >> 16) & 0xFF) * intensity;
  uint8_t g = ((color >> 8) & 0xFF) * intensity;
  uint8_t b = (color & 0xFF) * intensity;
  return strip.Color(r, g, b);
}

uint32_t fadeColor(uint32_t color, float factor) {
  uint8_t r = ((color >> 16) & 0xFF) * factor;
  uint8_t g = ((color >> 8) & 0xFF) * factor;
  uint8_t b = (color & 0xFF) * factor;
  return strip.Color(r, g, b);
}

void setup() {
  strip.begin();
  strip.setBrightness(BRIGHTNESS);

  for (int i = 0; i < NUM_LEDS; i++) {
    strip.setPixelColor(i, 0);
  }
  strip.show();

  for (int i = 0; i < NUM_BANDS; i++) {
    bandPeaks[i] = BAND_NOISE[i];
  }
}

void loop() {
  sampleAudio();
  analyzeFFT();

  // Update real-time LEDs (first 8)
  for (int b = 0; b < REALTIME_LEDS && b < NUM_BANDS; b++) {
    if (!BAND_ENABLED[b]) {
      strip.setPixelColor(led(b), 0);
      continue;
    }

    bool active = (bands[b] > BAND_NOISE[b]) &&
                  (bands[b] > bandPeaks[b] * THRESHOLD);

    if (active) {
      if (USE_INTENSITY) {
        float intensity = (bands[b] - BAND_NOISE[b]) / (bandPeaks[b] - BAND_NOISE[b] + 1);
        intensity = constrain(intensity, 0.2, 1.0);
        strip.setPixelColor(led(b), dimColor(COLORS[b], intensity));
      } else {
        strip.setPixelColor(led(b), COLORS[b]);
      }
    } else {
      strip.setPixelColor(led(b), 0);
    }
  }

  // Scroll history
  unsigned long now = millis();
  if (now - lastScroll >= SCROLL_MS) {
    lastScroll = now;

    for (int i = NUM_LEDS - 1; i >= REALTIME_LEDS; i--) {
      if (i >= REALTIME_LEDS + REALTIME_LEDS) {
        uint32_t c = strip.getPixelColor(led(i - REALTIME_LEDS));
        strip.setPixelColor(led(i), fadeColor(c, FADE_AMOUNT));
      } else {
        int srcLed = i - REALTIME_LEDS;
        strip.setPixelColor(led(i), strip.getPixelColor(led(srcLed)));
      }
    }
  }

  // Update peaks (auto-gain)
  for (int b = 0; b < NUM_BANDS; b++) {
    if (bands[b] > bandPeaks[b]) {
      bandPeaks[b] = bands[b];
    } else {
      bandPeaks[b] = bandPeaks[b] * 0.993 + BAND_NOISE[b] * 0.007;
    }
  }

  strip.show();
}

void sampleAudio() {
  unsigned long start = micros();
  for (int i = 0; i < SAMPLES; i++) {
    while (micros() - start < (i * (1000000.0 / SAMPLE_FREQ))) {}
    vReal[i] = analogRead(MIC_PIN);
    vImag[i] = 0;
  }
}

void analyzeFFT() {
  FFT.windowing(FFTWindow::Hamming, FFTDirection::Forward);
  FFT.compute(FFTDirection::Forward);
  FFT.complexToMagnitude();

  for (int b = 0; b < NUM_BANDS; b++) {
    bands[b] = 0;
    int count = 0;
    int endBin = (b < NUM_BANDS - 1) ? BAND_START[b + 1] : BAND_END;

    for (int i = BAND_START[b]; i < endBin; i++) {
      bands[b] += vReal[i];
      count++;
    }
    if (count > 0) bands[b] /= count;
  }
}
