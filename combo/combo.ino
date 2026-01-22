// ============================================================================
// COMBO - All visualizers with automatic switching
// ============================================================================
//
// Combines all three visualizers (Trail, Lava, Spectrum) and switches
// between them automatically:
// - Periodically (every SWITCH_INTERVAL_MS)
// - On audio spike (sudden loud sound)
//
// This file contains all code inline to work standalone.
//
// ============================================================================

#include <Adafruit_NeoPixel.h>
#include <arduinoFFT.h>

// ============================================================================
// HARDWARE CONFIG
// ============================================================================

const int MIC_PIN = A0;
const int LED_PIN = 6;
const int NUM_LEDS = 120;
const int BRIGHTNESS = 100;

// ============================================================================
// FFT CONFIG
// ============================================================================

const int SAMPLES = 64;
const float SAMPLE_FREQ = 8000.0;
const int NUM_BINS = SAMPLES / 2;

Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);
float vReal[SAMPLES];
float vImag[SAMPLES];
ArduinoFFT<float> FFT = ArduinoFFT<float>(vReal, vImag, SAMPLES, SAMPLE_FREQ);

// ============================================================================
// MODE SWITCHING CONFIG
// ============================================================================

const bool SWITCH_PERIODIC = true;
const bool SWITCH_ON_SPIKE = true;
const unsigned long SWITCH_INTERVAL_MS = 90000;
const unsigned long MIN_MODE_TIME_MS = 30000;
const float SPIKE_THRESHOLD = 0.5;

enum Mode { MODE_TRAIL, MODE_LAVA, MODE_SPECTRUM };
const int NUM_MODES = 3;
Mode currentMode = MODE_TRAIL;
unsigned long lastSwitch = 0;
unsigned long lastSpikeSwitch = 0;

// Audio level tracking for spike detection
float globalAudioLevel = 0;
float globalAudioPeak = 100;
float globalLastAudioLevel = 0;

// ============================================================================
// TRAIL CONFIG & STATE
// ============================================================================

const bool TRAIL_REVERSE = true;
const float TRAIL_THRESHOLD = 0.70;
const bool TRAIL_USE_INTENSITY = true;
const int TRAIL_SCROLL_MS = 50;
const float TRAIL_FADE = 0.92;
const int TRAIL_REALTIME_LEDS = 8;
const int TRAIL_NUM_BANDS = 8;

const int TRAIL_BAND_START[8] = {1, 2, 3, 4, 6, 9, 12, 16};
const int TRAIL_BAND_END = 32;
const float TRAIL_BAND_NOISE[8] = {200, 80, 80, 80, 80, 80, 80, 80};
const uint32_t TRAIL_COLORS[8] = {
  0x0044FF, 0x2200FF, 0x6600FF, 0x9900FF,
  0xCC00FF, 0xFF00CC, 0xFF0088, 0xFF0044
};

float trailBands[8];
float trailBandPeaks[8];
unsigned long trailLastScroll = 0;

// ============================================================================
// LAVA CONFIG & STATE
// ============================================================================

const int LAVA_MAX_BLOBS = 8;
const float LAVA_FADE_RATE = 0.92;
const float LAVA_DRIFT_SPEED = 0.3;
const float LAVA_SPAWN_SIZE = 10.0;
const float LAVA_SHRINK_RATE = 0.97;
const float LAVA_TRIGGER_THRESHOLD = 0.3;
const int LAVA_COLOR_MODE = 1;
const int LAVA_NUM_BANDS = 6;

const uint32_t LAVA_COLORS[6] = {
  0xFF0000, 0xFF4400, 0xFFAA00, 0x44FF00, 0x0088FF, 0x8800FF
};

struct LavaBlob {
  bool active;
  float position;
  float size;
  float brightness;
  float drift;
  int colorIndex;
};

LavaBlob lavaBlobs[8];
float lavaBandLevels[6];
float lavaBandPeaks[6];
int lavaDominantBand = 0;
float lavaAudioLevel = 0;
float lavaAudioPeak = 100;
float lavaLastAudioLevel = 0;

// ============================================================================
// SPECTRUM CONFIG & STATE
// ============================================================================

const float SPECTRUM_SENSITIVITY = 1.5;
const bool SPECTRUM_REVERSE = false;
const int SPECTRUM_RESOLUTION = 4;
const float SPECTRUM_NOISE_LOW = 50.0;
const float SPECTRUM_NOISE_MID = 60.0;
const float SPECTRUM_NOISE_HIGH = 80.0;
const uint8_t SPECTRUM_R = 0;
const uint8_t SPECTRUM_G = 255;
const uint8_t SPECTRUM_B = 0;

const int SPECTRUM_ACTIVE_LEDS = NUM_LEDS / SPECTRUM_RESOLUTION;
float spectrumBinPeaks[32];

// ============================================================================
// SHARED FUNCTIONS
// ============================================================================

void sampleAudio() {
  unsigned long start = micros();
  for (int i = 0; i < SAMPLES; i++) {
    while (micros() - start < (unsigned long)(i * (1000000.0 / SAMPLE_FREQ))) {}
    vReal[i] = analogRead(MIC_PIN);
    vImag[i] = 0;
  }
}

void computeFFT() {
  FFT.windowing(FFTWindow::Hamming, FFTDirection::Forward);
  FFT.compute(FFTDirection::Forward);
  FFT.complexToMagnitude();
}

void updateGlobalAudioLevel() {
  float audio = 0;
  for (int i = 1; i < NUM_BINS; i++) {
    audio += vReal[i];
  }
  audio /= (NUM_BINS - 1);

  globalLastAudioLevel = globalAudioLevel;

  if (audio > globalAudioLevel) {
    globalAudioLevel = audio;
  } else {
    globalAudioLevel = globalAudioLevel * 0.7 + audio * 0.3;
  }

  if (globalAudioLevel > globalAudioPeak) {
    globalAudioPeak = globalAudioLevel;
  } else {
    globalAudioPeak = globalAudioPeak * 0.99 + 50 * 0.01;
  }
}

bool detectAudioSpike() {
  if (globalAudioPeak <= 50) return false;
  float cur = constrain((globalAudioLevel - 50) / (globalAudioPeak - 50), 0, 1);
  float last = constrain((globalLastAudioLevel - 50) / (globalAudioPeak - 50), 0, 1);
  return (cur > SPIKE_THRESHOLD && cur > last + 0.25);
}

// ============================================================================
// TRAIL FUNCTIONS
// ============================================================================

int trailLed(int i) {
  return TRAIL_REVERSE ? (NUM_LEDS - 1 - i) : i;
}

uint32_t trailDimColor(uint32_t color, float intensity) {
  uint8_t r = ((color >> 16) & 0xFF) * intensity;
  uint8_t g = ((color >> 8) & 0xFF) * intensity;
  uint8_t b = (color & 0xFF) * intensity;
  return strip.Color(r, g, b);
}

uint32_t trailFadeColor(uint32_t color, float factor) {
  uint8_t r = ((color >> 16) & 0xFF) * factor;
  uint8_t g = ((color >> 8) & 0xFF) * factor;
  uint8_t b = (color & 0xFF) * factor;
  return strip.Color(r, g, b);
}

void trailSetup() {
  for (int i = 0; i < TRAIL_NUM_BANDS; i++) {
    trailBandPeaks[i] = TRAIL_BAND_NOISE[i];
  }
  strip.clear();
}

void trailUpdate() {
  // Analyze bands
  for (int b = 0; b < TRAIL_NUM_BANDS; b++) {
    trailBands[b] = 0;
    int count = 0;
    int endBin = (b < TRAIL_NUM_BANDS - 1) ? TRAIL_BAND_START[b + 1] : TRAIL_BAND_END;
    for (int i = TRAIL_BAND_START[b]; i < endBin; i++) {
      trailBands[b] += vReal[i];
      count++;
    }
    if (count > 0) trailBands[b] /= count;
  }

  // Update real-time LEDs
  for (int b = 0; b < TRAIL_REALTIME_LEDS && b < TRAIL_NUM_BANDS; b++) {
    bool active = (trailBands[b] > TRAIL_BAND_NOISE[b]) &&
                  (trailBands[b] > trailBandPeaks[b] * TRAIL_THRESHOLD);

    if (active) {
      if (TRAIL_USE_INTENSITY) {
        float intensity = (trailBands[b] - TRAIL_BAND_NOISE[b]) / (trailBandPeaks[b] - TRAIL_BAND_NOISE[b] + 1);
        intensity = constrain(intensity, 0.2, 1.0);
        strip.setPixelColor(trailLed(b), trailDimColor(TRAIL_COLORS[b], intensity));
      } else {
        strip.setPixelColor(trailLed(b), TRAIL_COLORS[b]);
      }
    } else {
      strip.setPixelColor(trailLed(b), 0);
    }
  }

  // Scroll history
  unsigned long now = millis();
  if (now - trailLastScroll >= TRAIL_SCROLL_MS) {
    trailLastScroll = now;
    for (int i = NUM_LEDS - 1; i >= TRAIL_REALTIME_LEDS; i--) {
      if (i >= TRAIL_REALTIME_LEDS + TRAIL_REALTIME_LEDS) {
        uint32_t c = strip.getPixelColor(trailLed(i - TRAIL_REALTIME_LEDS));
        strip.setPixelColor(trailLed(i), trailFadeColor(c, TRAIL_FADE));
      } else {
        int srcLed = i - TRAIL_REALTIME_LEDS;
        strip.setPixelColor(trailLed(i), strip.getPixelColor(trailLed(srcLed)));
      }
    }
  }

  // Update peaks
  for (int b = 0; b < TRAIL_NUM_BANDS; b++) {
    if (trailBands[b] > trailBandPeaks[b]) {
      trailBandPeaks[b] = trailBands[b];
    } else {
      trailBandPeaks[b] = trailBandPeaks[b] * 0.993 + TRAIL_BAND_NOISE[b] * 0.007;
    }
  }
}

// ============================================================================
// LAVA FUNCTIONS
// ============================================================================

void lavaSetup() {
  for (int i = 0; i < LAVA_MAX_BLOBS; i++) {
    lavaBlobs[i].active = false;
  }
  for (int i = 0; i < LAVA_NUM_BANDS; i++) {
    lavaBandPeaks[i] = 100;
  }
  lavaAudioLevel = 0;
  lavaAudioPeak = 100;
}

void lavaAnalyzeAudio() {
  const int bandBins[7] = {1, 3, 5, 8, 12, 19, 32};

  float maxBandLevel = 0;
  lavaDominantBand = 0;

  for (int b = 0; b < LAVA_NUM_BANDS; b++) {
    lavaBandLevels[b] = 0;
    for (int i = bandBins[b]; i < bandBins[b + 1]; i++) {
      lavaBandLevels[b] += vReal[i];
    }
    lavaBandLevels[b] /= (bandBins[b + 1] - bandBins[b]);

    if (lavaBandLevels[b] > lavaBandPeaks[b]) {
      lavaBandPeaks[b] = lavaBandLevels[b];
    } else {
      lavaBandPeaks[b] = lavaBandPeaks[b] * 0.995 + 50 * 0.005;
    }

    float normalizedLevel = 0;
    if (lavaBandPeaks[b] > 50) {
      normalizedLevel = constrain((lavaBandLevels[b] - 50) / (lavaBandPeaks[b] - 50), 0, 1);
    }

    if (normalizedLevel > maxBandLevel) {
      maxBandLevel = normalizedLevel;
      lavaDominantBand = b;
    }
  }

  float audio = 0;
  for (int i = 1; i < NUM_BINS; i++) {
    audio += vReal[i];
  }
  audio /= (NUM_BINS - 1);

  lavaLastAudioLevel = lavaAudioLevel;
  if (audio > lavaAudioLevel) {
    lavaAudioLevel = audio;
  } else {
    lavaAudioLevel = lavaAudioLevel * 0.7 + audio * 0.3;
  }

  if (lavaAudioLevel > lavaAudioPeak) {
    lavaAudioPeak = lavaAudioLevel;
  } else {
    lavaAudioPeak = lavaAudioPeak * 0.99 + 50 * 0.01;
  }
}

void lavaSpawnBlob(float intensity) {
  int slot = -1;
  for (int i = 0; i < LAVA_MAX_BLOBS; i++) {
    if (!lavaBlobs[i].active) {
      slot = i;
      break;
    }
  }

  if (slot == -1) {
    float minBrightness = 1.0;
    for (int i = 0; i < LAVA_MAX_BLOBS; i++) {
      if (lavaBlobs[i].brightness < minBrightness) {
        minBrightness = lavaBlobs[i].brightness;
        slot = i;
      }
    }
  }

  if (slot >= 0) {
    lavaBlobs[slot].active = true;
    lavaBlobs[slot].position = random(10, NUM_LEDS - 10);
    lavaBlobs[slot].size = LAVA_SPAWN_SIZE * (0.7 + intensity * 0.5);
    lavaBlobs[slot].brightness = 0.5 + intensity * 0.5;
    lavaBlobs[slot].drift = (random(0, 2) == 0 ? -1 : 1) * LAVA_DRIFT_SPEED * (0.5 + random(0, 100) / 100.0);
    lavaBlobs[slot].colorIndex = (LAVA_COLOR_MODE == 0) ? random(0, LAVA_NUM_BANDS) : lavaDominantBand;
  }
}

void lavaCheckTrigger() {
  float normalizedAudio = 0;
  if (lavaAudioPeak > 50) {
    normalizedAudio = constrain((lavaAudioLevel - 50) / (lavaAudioPeak - 50), 0, 1);
  }

  float normalizedLast = 0;
  if (lavaAudioPeak > 50) {
    normalizedLast = constrain((lavaLastAudioLevel - 50) / (lavaAudioPeak - 50), 0, 1);
  }

  if (normalizedAudio > LAVA_TRIGGER_THRESHOLD && normalizedAudio > normalizedLast + 0.1) {
    lavaSpawnBlob(normalizedAudio);
  }
}

void lavaUpdateBlobs() {
  for (int i = 0; i < LAVA_MAX_BLOBS; i++) {
    if (!lavaBlobs[i].active) continue;

    lavaBlobs[i].brightness *= LAVA_FADE_RATE;
    lavaBlobs[i].size *= LAVA_SHRINK_RATE;
    lavaBlobs[i].position += lavaBlobs[i].drift;

    if (lavaBlobs[i].brightness < 0.02 || lavaBlobs[i].size < 1) {
      lavaBlobs[i].active = false;
    }

    if (lavaBlobs[i].position < 0 || lavaBlobs[i].position >= NUM_LEDS) {
      lavaBlobs[i].drift *= -1;
      lavaBlobs[i].position = constrain(lavaBlobs[i].position, 0, NUM_LEDS - 1);
    }
  }
}

void lavaAddColorToPixel(int pixel, uint32_t color, float brightness) {
  uint8_t newR = ((color >> 16) & 0xFF) * brightness;
  uint8_t newG = ((color >> 8) & 0xFF) * brightness;
  uint8_t newB = (color & 0xFF) * brightness;

  uint32_t existing = strip.getPixelColor(pixel);
  uint8_t oldR = (existing >> 16) & 0xFF;
  uint8_t oldG = (existing >> 8) & 0xFF;
  uint8_t oldB = existing & 0xFF;

  strip.setPixelColor(pixel, strip.Color(
    min(255, oldR + newR),
    min(255, oldG + newG),
    min(255, oldB + newB)
  ));
}

void lavaRender() {
  strip.clear();

  for (int b = 0; b < LAVA_MAX_BLOBS; b++) {
    if (!lavaBlobs[b].active) continue;

    float size = lavaBlobs[b].size;
    float center = lavaBlobs[b].position;
    float brightness = lavaBlobs[b].brightness;
    uint32_t color = LAVA_COLORS[lavaBlobs[b].colorIndex];

    int startLed = max(0, (int)(center - size));
    int endLed = min(NUM_LEDS - 1, (int)(center + size));

    for (int i = startLed; i <= endLed; i++) {
      float distance = abs(i - center);
      float falloff = 1.0 - (distance / size);
      falloff = falloff * falloff * brightness;
      falloff = constrain(falloff, 0, 1);
      lavaAddColorToPixel(i, color, falloff);
    }
  }
}

void lavaUpdate() {
  lavaAnalyzeAudio();
  lavaCheckTrigger();
  lavaUpdateBlobs();
  lavaRender();
}

// ============================================================================
// SPECTRUM FUNCTIONS
// ============================================================================

float spectrumGetNoiseFloor(float freqRatio) {
  if (freqRatio < 0.33) return SPECTRUM_NOISE_LOW;
  if (freqRatio < 0.66) return SPECTRUM_NOISE_MID;
  return SPECTRUM_NOISE_HIGH;
}

uint32_t spectrumIntensityToColor(float intensity) {
  intensity = constrain(intensity, 0.0, 1.0);
  return strip.Color(SPECTRUM_R * intensity, SPECTRUM_G * intensity, SPECTRUM_B * intensity);
}

void spectrumSetup() {
  for (int i = 0; i < NUM_BINS; i++) {
    float freqRatio = (float)i / (NUM_BINS - 1);
    spectrumBinPeaks[i] = spectrumGetNoiseFloor(freqRatio) * 2;
  }
}

void spectrumUpdate() {
  // Update peaks
  for (int b = 1; b < NUM_BINS; b++) {
    float freqRatio = (float)b / (NUM_BINS - 1);
    float noiseFloor = spectrumGetNoiseFloor(freqRatio);

    if (vReal[b] > spectrumBinPeaks[b]) {
      spectrumBinPeaks[b] = vReal[b];
    } else {
      spectrumBinPeaks[b] = spectrumBinPeaks[b] * 0.995 + noiseFloor * 0.005;
    }
  }

  // Render
  strip.clear();

  for (int i = 0; i < SPECTRUM_ACTIVE_LEDS; i++) {
    int ledPos = i * SPECTRUM_RESOLUTION;
    int ledIndex = SPECTRUM_REVERSE ? (NUM_LEDS - 1 - ledPos) : ledPos;

    float ledRatio = (float)i / (float)(SPECTRUM_ACTIVE_LEDS - 1);
    float binFloat = 1.0 + (NUM_BINS - 2) * ledRatio;

    int binLow = (int)binFloat;
    int binHigh = min(binLow + 1, NUM_BINS - 1);
    float frac = binFloat - binLow;

    float value = vReal[binLow] * (1.0 - frac) + vReal[binHigh] * frac;
    float peak = spectrumBinPeaks[binLow] * (1.0 - frac) + spectrumBinPeaks[binHigh] * frac;

    float noiseFloor = spectrumGetNoiseFloor(ledRatio);

    float intensity = 0;
    if (value > noiseFloor && peak > noiseFloor) {
      intensity = (value - noiseFloor) / (peak - noiseFloor);
      intensity *= SPECTRUM_SENSITIVITY;
      intensity = constrain(intensity, 0.0, 1.0);
    }

    if (intensity >= 0.08) {
      strip.setPixelColor(ledIndex, spectrumIntensityToColor(intensity));
    }
  }
}

// ============================================================================
// MODE SWITCHING
// ============================================================================

void nextMode() {
  strip.clear();
  currentMode = (Mode)((currentMode + 1) % NUM_MODES);
  lastSwitch = millis();

  switch (currentMode) {
    case MODE_TRAIL:   trailSetup();    break;
    case MODE_LAVA:    lavaSetup();     break;
    case MODE_SPECTRUM: spectrumSetup(); break;
  }
}

void checkModeSwitch() {
  unsigned long now = millis();

  if (SWITCH_PERIODIC && (now - lastSwitch >= SWITCH_INTERVAL_MS)) {
    nextMode();
    return;
  }

  if (SWITCH_ON_SPIKE && (now - lastSpikeSwitch >= MIN_MODE_TIME_MS)) {
    if (detectAudioSpike()) {
      lastSpikeSwitch = now;
      nextMode();
    }
  }
}

// ============================================================================
// SETUP & LOOP
// ============================================================================

void setup() {
  strip.begin();
  strip.setBrightness(BRIGHTNESS);
  strip.clear();
  strip.show();

  trailSetup();
  lavaSetup();
  spectrumSetup();

  lastSwitch = millis();
  lastSpikeSwitch = millis();
}

void loop() {
  sampleAudio();
  computeFFT();
  updateGlobalAudioLevel();

  checkModeSwitch();

  switch (currentMode) {
    case MODE_TRAIL:    trailUpdate();    break;
    case MODE_LAVA:     lavaUpdate();     break;
    case MODE_SPECTRUM: spectrumUpdate(); break;
  }

  strip.show();
}
