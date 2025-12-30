// ============================================================================
// LAVA - Audio-triggered blobs with fade/float trail
// ============================================================================
//
// How it works:
// 1. Audio instantly triggers bright blobs
// 2. Blobs slowly fade out and drift
// 3. New audio = new blobs spawn
// 4. Creates lava-like trailing effect
//
// ============================================================================

#include <Adafruit_NeoPixel.h>
#include <arduinoFFT.h>

// ============================================================================
// SETTINGS
// ============================================================================

const int BRIGHTNESS = 100;           // LED brightness (0-255)
const int MAX_BLOBS = 8;              // Max simultaneous blobs
const float FADE_RATE = 0.92;         // How fast blobs fade (lower = faster)
const float DRIFT_SPEED = 0.3;        // How fast blobs drift
const float SPAWN_SIZE = 10.0;        // Initial blob size
const float SHRINK_RATE = 0.97;       // How fast blobs shrink
const float TRIGGER_THRESHOLD = 0.3;  // Audio level to spawn blob (0-1)

// ============================================================================
// HARDWARE
// ============================================================================

const int MIC_PIN = A0;
const int LED_PIN = 6;
const int NUM_LEDS = 120;

Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// ============================================================================
// FFT
// ============================================================================

const int SAMPLES = 64;
const float SAMPLE_FREQ = 8000.0;
float vReal[SAMPLES];
float vImag[SAMPLES];
ArduinoFFT<float> FFT = ArduinoFFT<float>(vReal, vImag, SAMPLES, SAMPLE_FREQ);

float audioLevel = 0;
float audioPeak = 100;
float lastAudioLevel = 0;

// ============================================================================
// BLOB COLORS
// ============================================================================

const uint32_t BLOB_COLORS[6] = {
  0xFF0040,  // Pink-red
  0xFF4000,  // Orange
  0xFFAA00,  // Yellow-orange
  0xFF0080,  // Magenta
  0xFF2000,  // Red-orange
  0xFF6000   // Orange
};

// ============================================================================
// BLOB DATA
// ============================================================================

struct Blob {
  bool active;
  float position;
  float size;
  float brightness;
  float drift;
  int colorIndex;
};

Blob blobs[MAX_BLOBS];
int nextColor = 0;

// ============================================================================
// SETUP
// ============================================================================

void setup() {
  strip.begin();
  strip.setBrightness(BRIGHTNESS);
  strip.clear();
  strip.show();

  for (int i = 0; i < MAX_BLOBS; i++) {
    blobs[i].active = false;
  }
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
  sampleAudio();
  analyzeAudio();
  checkTrigger();
  updateBlobs();
  renderBlobs();
  strip.show();
}

// ============================================================================
// AUDIO
// ============================================================================

void sampleAudio() {
  unsigned long start = micros();
  for (int i = 0; i < SAMPLES; i++) {
    while (micros() - start < (unsigned long)(i * (1000000.0 / SAMPLE_FREQ))) {}
    vReal[i] = analogRead(MIC_PIN);
    vImag[i] = 0;
  }
}

void analyzeAudio() {
  FFT.windowing(FFTWindow::Hamming, FFTDirection::Forward);
  FFT.compute(FFTDirection::Forward);
  FFT.complexToMagnitude();

  // Get overall audio level
  float audio = 0;
  for (int i = 1; i < SAMPLES / 2; i++) {
    audio += vReal[i];
  }
  audio /= (SAMPLES / 2 - 1);

  lastAudioLevel = audioLevel;

  // Very fast response
  if (audio > audioLevel) {
    audioLevel = audio;  // Instant attack
  } else {
    audioLevel = audioLevel * 0.7 + audio * 0.3;
  }

  // Update peak
  if (audioLevel > audioPeak) {
    audioPeak = audioLevel;
  } else {
    audioPeak = audioPeak * 0.99 + 50 * 0.01;
  }
}

// ============================================================================
// TRIGGER NEW BLOBS
// ============================================================================

void checkTrigger() {
  // Calculate normalized audio (0-1)
  float normalizedAudio = 0;
  if (audioPeak > 50) {
    normalizedAudio = (audioLevel - 50) / (audioPeak - 50);
    normalizedAudio = constrain(normalizedAudio, 0, 1);
  }

  // Detect rise in audio (trigger on attack)
  float normalizedLast = 0;
  if (audioPeak > 50) {
    normalizedLast = (lastAudioLevel - 50) / (audioPeak - 50);
    normalizedLast = constrain(normalizedLast, 0, 1);
  }

  // Spawn blob on audio spike
  if (normalizedAudio > TRIGGER_THRESHOLD && normalizedAudio > normalizedLast + 0.1) {
    spawnBlob(normalizedAudio);
  }
}

void spawnBlob(float intensity) {
  // Find inactive blob slot
  int slot = -1;
  for (int i = 0; i < MAX_BLOBS; i++) {
    if (!blobs[i].active) {
      slot = i;
      break;
    }
  }

  // If no slot, replace oldest (lowest brightness)
  if (slot == -1) {
    float minBrightness = 1.0;
    for (int i = 0; i < MAX_BLOBS; i++) {
      if (blobs[i].brightness < minBrightness) {
        minBrightness = blobs[i].brightness;
        slot = i;
      }
    }
  }

  if (slot >= 0) {
    blobs[slot].active = true;
    blobs[slot].position = random(10, NUM_LEDS - 10);  // Random position
    blobs[slot].size = SPAWN_SIZE * (0.7 + intensity * 0.5);
    blobs[slot].brightness = 0.5 + intensity * 0.5;
    blobs[slot].drift = (random(0, 2) == 0 ? -1 : 1) * DRIFT_SPEED * (0.5 + random(0, 100) / 100.0);
    blobs[slot].colorIndex = nextColor;
    nextColor = (nextColor + 1) % 6;
  }
}

// ============================================================================
// UPDATE BLOBS (fade + drift)
// ============================================================================

void updateBlobs() {
  for (int i = 0; i < MAX_BLOBS; i++) {
    if (!blobs[i].active) continue;

    // Fade
    blobs[i].brightness *= FADE_RATE;

    // Shrink
    blobs[i].size *= SHRINK_RATE;

    // Drift
    blobs[i].position += blobs[i].drift;

    // Deactivate if too dim or small
    if (blobs[i].brightness < 0.02 || blobs[i].size < 1) {
      blobs[i].active = false;
    }

    // Bounce off edges
    if (blobs[i].position < 0 || blobs[i].position >= NUM_LEDS) {
      blobs[i].drift *= -1;
      blobs[i].position = constrain(blobs[i].position, 0, NUM_LEDS - 1);
    }
  }
}

// ============================================================================
// RENDER
// ============================================================================

void renderBlobs() {
  // Clear
  for (int i = 0; i < NUM_LEDS; i++) {
    strip.setPixelColor(i, 0);
  }

  // Render each active blob
  for (int b = 0; b < MAX_BLOBS; b++) {
    if (!blobs[b].active) continue;

    float size = blobs[b].size;
    float center = blobs[b].position;
    float brightness = blobs[b].brightness;
    uint32_t color = BLOB_COLORS[blobs[b].colorIndex];

    int startLed = max(0, (int)(center - size));
    int endLed = min(NUM_LEDS - 1, (int)(center + size));

    for (int i = startLed; i <= endLed; i++) {
      float distance = abs(i - center);
      float falloff = 1.0 - (distance / size);
      falloff = falloff * falloff;  // Smooth falloff
      falloff = falloff * brightness;
      falloff = constrain(falloff, 0, 1);

      addColorToPixel(i, color, falloff);
    }
  }
}

void addColorToPixel(int pixel, uint32_t color, float brightness) {
  uint8_t newR = ((color >> 16) & 0xFF) * brightness;
  uint8_t newG = ((color >> 8) & 0xFF) * brightness;
  uint8_t newB = (color & 0xFF) * brightness;

  uint32_t existing = strip.getPixelColor(pixel);
  uint8_t oldR = (existing >> 16) & 0xFF;
  uint8_t oldG = (existing >> 8) & 0xFF;
  uint8_t oldB = existing & 0xFF;

  uint8_t r = min(255, oldR + newR);
  uint8_t g = min(255, oldG + newG);
  uint8_t b = min(255, oldB + newB);

  strip.setPixelColor(pixel, strip.Color(r, g, b));
}
