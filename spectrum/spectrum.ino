// ============================================================================
// SPECTRUM - Real-time frequency visualizer
// ============================================================================
//
// How it works:
// 1. Microphone captures audio (64 samples)
// 2. FFT (Fast Fourier Transform) converts sound to frequencies (32 bins)
// 3. Each frequency bin is mapped to LED(s)
// 4. LED brightness = volume of that frequency
//
// Frequency distribution:
// - 64 samples @ 8kHz = frequency range 0-4000 Hz
// - 32 FFT bins, each bin = ~125 Hz wide
// - Bin 1 = 125 Hz (bass), Bin 31 = 3875 Hz (treble)
//
// LED mapping (with RESOLUTION=4, 30 active LEDs):
// - 30 LEDs for 31 bins = almost 1:1 mapping
// - Each LED shows unique frequency
//
// LED mapping (with RESOLUTION=1, 120 active LEDs):
// - 120 LEDs for 31 bins = ~4 LEDs per bin
// - Multiple LEDs show same frequency (interpolated)
//
// ============================================================================

#include <Adafruit_NeoPixel.h>
#include <arduinoFFT.h>

// ============================================================================
// SETTINGS - Adjust to taste
// ============================================================================

// --- General ---
const int BRIGHTNESS = 100;           // LED brightness (0-255)
const float SENSITIVITY = 1.5;        // Sensitivity (1.0 = normal, 2.0 = more sensitive)
const bool REVERSE_DIRECTION = false; // true = LED 0 on right, false = LED 0 on left

// --- Resolution ---
// Determines how many LEDs are active and spacing between them
// Higher value = fewer LEDs, more space, more unique frequencies per LED
const int RESOLUTION = 4;
// 1 = 120 LEDs (every LED)      - ~4 LEDs per frequency (overlap)
// 2 = 60 LEDs  (every 2nd LED)  - ~2 LEDs per frequency
// 3 = 40 LEDs  (every 3rd LED)  - ~1.3 LEDs per frequency
// 4 = 30 LEDs  (every 4th LED)  - ~1 LED per frequency (unique)

// --- Noise floor ---
// Signal below this value is ignored (prevents flickering during silence)
// High frequencies often have more noise, so higher threshold
const float NOISE_LOW = 50.0;         // Bass (0-33% of strip)
const float NOISE_MID = 60.0;         // Mid (33-66% of strip)
const float NOISE_HIGH = 80.0;        // Treble (66-100% of strip)
// Increase these values if LEDs flicker during silence

// --- Color ---
// RGB values (0-255) for LED color
// Brightness is automatically adjusted based on volume
const uint8_t BASE_R = 0;
const uint8_t BASE_G = 255;
const uint8_t BASE_B = 0;

// Color presets (copy to BASE_R/G/B):
// Green:   R=0,   G=255, B=0
// Blue:    R=0,   G=0,   B=255
// Purple:  R=150, G=0,   B=255
// Cyan:    R=0,   G=255, B=255
// Orange:  R=255, G=100, B=0
// Pink:    R=255, G=0,   B=100
// White:   R=255, G=255, B=255

// ============================================================================
// HARDWARE - Adjust for your setup
// ============================================================================

const int MIC_PIN = A0;               // Microphone analog pin
const int LED_PIN = 6;                // LED data pin
const int NUM_LEDS = 120;             // Total number of LEDs on strip

// ============================================================================
// FFT CONFIGURATION - Usually no need to change
// ============================================================================

const int SAMPLES = 64;               // Number of audio samples (more = better freq resolution, but slower)
const float SAMPLE_FREQ = 8000.0;     // Sample frequency in Hz (max freq = half of this)
const int NUM_BINS = SAMPLES / 2;     // Usable FFT bins (32)

// ============================================================================
// INTERNAL VARIABLES - Do not change
// ============================================================================

const int ACTIVE_LEDS = NUM_LEDS / RESOLUTION;

Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

float vReal[SAMPLES];
float vImag[SAMPLES];
ArduinoFFT<float> FFT = ArduinoFFT<float>(vReal, vImag, SAMPLES, SAMPLE_FREQ);

float binPeaks[NUM_BINS];

// ============================================================================
// FUNCTIONS
// ============================================================================

uint32_t intensityToColor(float intensity) {
  intensity = constrain(intensity, 0.0, 1.0);
  return strip.Color(BASE_R * intensity, BASE_G * intensity, BASE_B * intensity);
}

float getNoiseFloor(float freqRatio) {
  if (freqRatio < 0.33) {
    return NOISE_LOW;
  } else if (freqRatio < 0.66) {
    return NOISE_MID;
  } else {
    return NOISE_HIGH;
  }
}

void setup() {
  strip.begin();
  strip.setBrightness(BRIGHTNESS);
  strip.clear();
  strip.show();

  for (int i = 0; i < NUM_BINS; i++) {
    float freqRatio = (float)i / (NUM_BINS - 1);
    binPeaks[i] = getNoiseFloor(freqRatio) * 2;
  }
}

void loop() {
  sampleAudio();
  analyzeFFT();
  updateStrip();
  strip.show();
}

void sampleAudio() {
  unsigned long startMicros = micros();

  for (int i = 0; i < SAMPLES; i++) {
    while (micros() - startMicros < (unsigned long)(i * (1000000.0 / SAMPLE_FREQ))) {}
    vReal[i] = analogRead(MIC_PIN);
    vImag[i] = 0;
  }
}

void analyzeFFT() {
  FFT.windowing(FFTWindow::Hamming, FFTDirection::Forward);
  FFT.compute(FFTDirection::Forward);
  FFT.complexToMagnitude();

  for (int b = 1; b < NUM_BINS; b++) {
    float freqRatio = (float)b / (NUM_BINS - 1);
    float noiseFloor = getNoiseFloor(freqRatio);

    if (vReal[b] > binPeaks[b]) {
      binPeaks[b] = vReal[b];
    } else {
      binPeaks[b] = binPeaks[b] * 0.995 + noiseFloor * 0.005;
    }
  }
}

void updateStrip() {
  strip.clear();

  for (int i = 0; i < ACTIVE_LEDS; i++) {
    int ledPos = i * RESOLUTION;
    int ledIndex = REVERSE_DIRECTION ? (NUM_LEDS - 1 - ledPos) : ledPos;

    // Linear distribution: frequencies evenly spread across LEDs
    float ledRatio = (float)i / (float)(ACTIVE_LEDS - 1);
    float binFloat = 1.0 + (NUM_BINS - 2) * ledRatio;

    int binLow = (int)binFloat;
    int binHigh = min(binLow + 1, NUM_BINS - 1);
    float frac = binFloat - binLow;

    float value = vReal[binLow] * (1.0 - frac) + vReal[binHigh] * frac;
    float peak = binPeaks[binLow] * (1.0 - frac) + binPeaks[binHigh] * frac;

    float noiseFloor = getNoiseFloor(ledRatio);

    float intensity = 0;
    if (value > noiseFloor && peak > noiseFloor) {
      intensity = (value - noiseFloor) / (peak - noiseFloor);
      intensity *= SENSITIVITY;
      intensity = constrain(intensity, 0.0, 1.0);
    }

    if (intensity < 0.08) {
      strip.setPixelColor(ledIndex, 0);
    } else {
      strip.setPixelColor(ledIndex, intensityToColor(intensity));
    }
  }
}
