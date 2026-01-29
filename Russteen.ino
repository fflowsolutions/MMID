//Code is generated with AI

#include <Adafruit_NeoPixel.h>

// PINS (Grove Base Shield)
const int PIN_ULTRASONIC = 2;   // D2: Grove Ultrasonic (single-pin)
const int PIN_VIB        = 3;   // D3: Grove Vibration module
const int PIN_NEOPIXEL   = 6;   // D6: NeoPixel data

// NeoPixel
const int NUM_PIXELS = 16;      // pas aan (8/12/16)
Adafruit_NeoPixel pixels(NUM_PIXELS, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);

// Gedrag
const unsigned long SESSION_MS = 5UL * 60UL * 1000UL;

// Start detectie (stabiel)
const int START_DISTANCE_CM = 35;
const int START_COUNT_NEEDED = 8;   // ~1 sec bij delay(120)
int startCounter = 0;

// Modus wissel
const int SWITCH_MODE_CM = 10;
const unsigned long SWITCH_COOLDOWN_MS = 1500;
unsigned long lastModeSwitchMs = 0;

// LED brightness (bijna uit)
const uint8_t BR_MIN = 1;   // bijna uit
const uint8_t BR_MAX = 10;  // heel subtiel

// Vibratie subtiel
const unsigned long VIB_BREATH_MS = 20; // korte tik bij in en uit
const unsigned long VIB_START_MS  = 90; // iets duidelijker bij start
const unsigned long VIB_MODE_MS   = 70; // bevestiging bij modus wissel

// Modi
struct Mode { unsigned long inhaleMs; unsigned long exhaleMs; uint8_t r,g,b; };
Mode modes[] = {
  {4000, 6000, 255, 176, 102}, // Modus 1: amber
  {4000, 8000, 127, 182, 255}  // Modus 2: zacht blauw
};
int currentMode = 0;

// State
enum State { IDLE, RUNNING };
State state = IDLE;

unsigned long sessionStartMs = 0;
unsigned long lastFrameMs = 0;

// Trilling
bool vibActive = false;
unsigned long vibOffMs = 0;

// Helpers
void setAll(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness) {
  pixels.setBrightness(brightness);
  for (int i = 0; i < NUM_PIXELS; i++) pixels.setPixelColor(i, pixels.Color(r,g,b));
  pixels.show();
}

void vibPulse(unsigned long ms) {
  digitalWrite(PIN_VIB, HIGH);
  vibActive = true;
  vibOffMs = millis() + ms;
}

void vibUpdate() {
  if (vibActive && millis() >= vibOffMs) {
    digitalWrite(PIN_VIB, LOW);
    vibActive = false;
  }
}

long readDistanceCm() {
  pinMode(PIN_ULTRASONIC, OUTPUT);
  digitalWrite(PIN_ULTRASONIC, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_ULTRASONIC, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_ULTRASONIC, LOW);

  pinMode(PIN_ULTRASONIC, INPUT);
  long duration = pulseIn(PIN_ULTRASONIC, HIGH, 30000);
  if (duration == 0) return 999;
  return duration / 58;
}

void fadeOut(unsigned long msTotal = 2500) {
  const int steps = 25;
  Mode m = modes[currentMode];
  for (int i = steps; i >= 0; i--) {
    uint8_t br = (uint8_t)((float)i / (float)steps * (float)BR_MAX);
    setAll(m.r, m.g, m.b, br);
    delay(msTotal / steps);
  }
  setAll(0,0,0,0);
  digitalWrite(PIN_VIB, LOW);
  vibActive = false;
}

void enterIdle() {
  state = IDLE;
  startCounter = 0;
  setAll(0,0,0,0);
  digitalWrite(PIN_VIB, LOW);
  vibActive = false;
}

void startCue() {
  vibPulse(VIB_START_MS);
  setAll(255, 217, 179, BR_MAX); // heel zacht warm wit
  delay(150);
}

void enterRunning() {
  state = RUNNING;
  sessionStartMs = millis();
  startCue();
}

void switchMode() {
  currentMode = (currentMode + 1) % 2;
  vibPulse(VIB_MODE_MS);

  Mode m = modes[currentMode];
  // subtiele visuele bevestiging, bijna uit
  setAll(m.r, m.g, m.b, BR_MAX);
  delay(120);
  setAll(0,0,0,0);
  delay(100);
  setAll(m.r, m.g, m.b, BR_MAX);
  delay(120);
}

void updateBreathing() {
  unsigned long now = millis();
  unsigned long elapsed = now - sessionStartMs;

  if (elapsed >= SESSION_MS) {
    fadeOut(2500);
    enterIdle();
    return;
  }

  if (now - lastFrameMs < 25) return;
  lastFrameMs = now;

  Mode m = modes[currentMode];
  unsigned long cycleMs = m.inhaleMs + m.exhaleMs;
  unsigned long t = elapsed % cycleMs;

  float phase;
  if (t < m.inhaleMs) {
    phase = (float)t / (float)m.inhaleMs; // 0..1
    if (t < 30) vibPulse(VIB_BREATH_MS);  // subtiele inhale cue
  } else {
    unsigned long te = t - m.inhaleMs;
    phase = 1.0f - (float)te / (float)m.exhaleMs; // 1..0
    if (te < 30) vibPulse(VIB_BREATH_MS);         // subtiele exhale cue
  }

  uint8_t br = (uint8_t)(BR_MIN + phase * (BR_MAX - BR_MIN));
  setAll(m.r, m.g, m.b, br);
}

void setup() {
  pinMode(PIN_VIB, OUTPUT);
  digitalWrite(PIN_VIB, LOW);

  pixels.begin();
  pixels.clear();
  pixels.show();

  enterIdle();
}

void loop() {
  vibUpdate();

  long d = readDistanceCm();

  // Modus wisselen tijdens RUNNING
  if (state == RUNNING && d <= SWITCH_MODE_CM && millis() - lastModeSwitchMs > SWITCH_COOLDOWN_MS) {
    lastModeSwitchMs = millis();
    switchMode();
  }

  // Starten tijdens IDLE met stabiele metingen
  if (state == IDLE) {
    if (d <= START_DISTANCE_CM) startCounter++;
    else startCounter = 0;

    if (startCounter >= START_COUNT_NEEDED) {
      enterRunning();
      startCounter = 0;
    }
  }

  if (state == RUNNING) updateBreathing();

  delay(120);
}
