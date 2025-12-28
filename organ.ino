// Frequency Scroll v15 - Cyberpunk kleuren (tweakable)
// Blauw / Paars / Roze / Magenta palette

#include <Adafruit_NeoPixel.h>
#include <arduinoFFT.h>

// =============================================
// === GLOBALE INSTELLINGEN ===
// =============================================

const float DREMPEL = 0.70;
const int SCROLL_MS = 50;
const int BRIGHTNESS = 100;

// Hybrid instellingen
const int REALTIME_LEDS = 8;
const float FADE_AMOUNT = 0.92;
const bool USE_INTENSITY = true;
const bool REVERSE_DIRECTION = true;

// =============================================
// === PER-BAND INSTELLINGEN ===
// =============================================

const int NUM_BANDS = 8;

const int BAND_START[8] = {
  2, 3, 5, 7, 10, 14, 19, 25
};
const int BAND_END = 32;

const float BAND_NOISE[8] = {
  200, 80, 80, 80, 80, 80, 80, 80
};

const bool BAND_ENABLED[8] = {
  true, true, true, true, true, true, true, true
};

// CYBERPUNK kleuren (blauw → paars → roze → magenta)
const uint32_t KLEUREN[8] = {
  0x0044FF,  // Band 0: Diep blauw
  0x2200FF,  // Band 1: Blauw-paars
  0x6600FF,  // Band 2: Paars
  0x9900FF,  // Band 3: Violet
  0xCC00FF,  // Band 4: Magenta-paars
  0xFF00CC,  // Band 5: Magenta
  0xFF0088,  // Band 6: Hot pink
  0xFF0044   // Band 7: Roze-rood
};

// =============================================
// === HARDWARE ===
// =============================================
const int MIC_PIN = A0;
const int LED_PIN = 6;
const int NUM_LEDS = 120;

Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// FFT
const uint16_t SAMPLES = 64;
const double SAMPLE_FREQ = 5000;
double vReal[SAMPLES];
double vImag[SAMPLES];
ArduinoFFT<double> FFT = ArduinoFFT<double>(vReal, vImag, SAMPLES, SAMPLE_FREQ);

float bands[NUM_BANDS];
float bandPeaks[NUM_BANDS];

unsigned long lastScroll = 0;

int led(int i) {
  return REVERSE_DIRECTION ? (NUM_LEDS - 1 - i) : i;
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

  for (int b = 0; b < REALTIME_LEDS && b < NUM_BANDS; b++) {
    if (!BAND_ENABLED[b]) {
      strip.setPixelColor(led(b), 0);
      continue;
    }

    bool active = (bands[b] > BAND_NOISE[b]) &&
                  (bands[b] > bandPeaks[b] * DREMPEL);

    if (active) {
      if (USE_INTENSITY) {
        float intensity = (bands[b] - BAND_NOISE[b]) / (bandPeaks[b] - BAND_NOISE[b] + 1);
        intensity = constrain(intensity, 0.2, 1.0);
        strip.setPixelColor(led(b), dimColor(KLEUREN[b], intensity));
      } else {
        strip.setPixelColor(led(b), KLEUREN[b]);
      }
    } else {
      strip.setPixelColor(led(b), 0);
    }
  }

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

  for (int b = 0; b < NUM_BANDS; b++) {
    if (bands[b] > bandPeaks[b]) {
      bandPeaks[b] = bands[b];
    } else {
      bandPeaks[b] = bandPeaks[b] * 0.993 + BAND_NOISE[b] * 0.007;
    }
  }

  strip.show();
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
