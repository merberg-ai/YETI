#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <time.h>
#include <math.h>
#include <ctype.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// =====================================================
// YETI v1.7.4-sleep-preview-compile-fix
// ESP32 OLED face + Wi-Fi setup + WebUI + MPU shake + touch
//
// v1.7.4 sleep preview compile fix:
// - Adds the missing classic renderer redraw throttle variable used when a
//   sleep preview exits, fixing ESP32-C3 compile with RoboEyes selected.
//
// v1.7.3 sleep preview pass:
// - Added 5 second sleep-animation preview action/button that returns to normal.
// - Sleep Now now force-starts/restarts the Zzz burst immediately instead of waiting for an intermittent gap.
// - Added configurable scheduled sleep window, sleepy OLED animation bursts, display-off blank gaps, shake/WebUI wake, and early-wake attitude.
//
// v1.6.2 grudge engine pass:
// - Added persistent Grudge Engine: poke/shake/Wi-Fi/weather/reboot/neglect grudges, slow decay, and mood biasing.
// v1.6.3 needs/emotional drift pass:
// - Added boredom/energy/irritation/loneliness meters, needs drift, and needs-biased idle moods.
// v1.6.4 daily memory pass:
// - Added today/yesterday memory counters, daily rollover, and yesterday-influenced boot attitude.
//
// v1.6.6 sass ticker / memory phrases pass:
// - Added optional OLED sass ticker phrases driven by memory, grudges, needs, and judgment.
// - Added phrase cooldowns, sass level/frequency controls, event/idle/grudge phrase gates, and manual test endpoints.
// - Sass overlays are guarded so setup/status/clock/weather/custom text remain readable.
//
// v1.6.5 judgment / opinion pass:
// - Added readable opinion summaries, forgiveness status, mood bias text, and current grievance reporting.
// - Added grudge diagnostics/actions to the Memory WebUI while keeping flash writes throttled.
// - Repeated pokes/shakes/bad Wi-Fi/weather failures now make reactions stronger, longer, and sassier.
// - Full personality system checkpoint: presets, idle, events, weather/Wi-Fi, movement, and acting sequences are integrated.
// - Added display/mood separation diagnostics and overlay-safe sequence pause/resume.
// - Added cleaner personality status summaries for mobile debugging without serial spelunking.
//
// v1.5.5 upgrade pass:
// - Added non-blocking acting sequence runner for poke, boot drama, bad Wi-Fi tantrum, weather bits, and settings-saved flourish.
// - Manual WebUI/serial moods interrupt active sequences cleanly.
// - Added sequence diagnostics and WebUI test controls.
//
// v1.5.4 upgrade pass:
// - Added MPU movement mood reactions: moved, tilted/picked-up, shaken, and long-still sleepy behavior.
// - Added movement mood toggle, movement/shake sensitivity sliders, cooldowns, and diagnostics.
// - Kept legacy shake expression trigger compatibility while routing personality reactions through the mood engine.
//
// v1.5.3 upgrade pass:
// - Added event-based mood reactions for WebUI activity, weather updates/errors, and Wi-Fi RSSI.
// - Added reaction cooldowns and automation toggles so YETI reacts without becoming a mood blender.
// - Added WebUI testers/diagnostics for weather and Wi-Fi mood reactions.
//
// v1.5.2 upgrade pass:
// - Added named personality presets: Classic, Friendly, Sleepy YETI, Feral Goblin, and Weather Gremlin.
// - Presets update sliders/base mood from the WebUI and save cleanly to Preferences.
// - Status JSON now reports the active/matching preset or Custom when sliders drift off-script.
//
// v1.5.1 upgrade pass:
// - Smarter weighted idle personality behavior with less chaotic mood roulette.
// - Personality traits now influence idle delay, mood choice, and idle mood duration.
// - Added idle diagnostics and an Idle Mood Now tester to the Personality WebUI.
//
// v1.5.0 upgrade pass:
// - Mood/personality engine layered over the existing RoboEyes expression renderer.
// - Personality WebUI page with manual mood buttons, base mood, sliders, poke/calm/random, and idle mood behavior.
// - Base mood and personality settings persist in Preferences.
//
// v1.3.1 upgrade pass:
// - RoboEyes experimental face engine: FluxGarage animated eyes replace the hand-rolled face loop.
//
// v1.4.1 upgrade pass:
// - Mobile page/section WebUI menu to reduce constant DOM churn on weak Wi-Fi.
// - Reboot action button under System/Maintenance.
// - Weather ticker include/exclude settings.
// - Safer clock offset source handling and visible offset source.
// - OLED readability pass: big clock/date sequence and large scrolling weather ticker.
//
// v1.3.0 upgrade pass:
// - Open-Meteo weather card using saved latitude/longitude, no API key required.
// - NTP-backed clock with configurable UTC offset and refresh/display intervals.
// - WebUI location helper, weather refresh/test actions, and OLED clock/weather cards.
//
// v1.2.1 upgrade pass:
// - Configurable hostname saved in persistent config and first-run setup.
// - Hostname applied to station Wi-Fi/mDNS where supported.
// - WebUI toast notifications for loaded/saved/error states.
//
// v1.2.0 upgrade pass:
// - Configurable OLED face frame pacing for smoother WebUI / less lag.
// - Dashboard diagnostics: I2C scanner, render timing, event log.
// - Demo mode expression carousel for testing the face quickly.
// - Show network/IP info on the OLED from WebUI or Serial.
// - Mood line / extra status fields in /api/status.
// =====================================================

// =====================================================
// HARDWARE PROFILE CONFIG
// =====================================================
//
// Pick the board profile here when you want to force one:
//   #define YETI_BOARD_PROFILE YETI_BOARD_ESP32_DEVKIT
//   #define YETI_BOARD_PROFILE YETI_BOARD_ESP32_C3_MINI
//   #define YETI_BOARD_PROFILE YETI_BOARD_CUSTOM
//
// By default the sketch auto-picks ESP32-C3 when the Arduino board target is C3,
// otherwise it uses the classic ESP32 DevKit profile.
//
// NOTE: ESP32-C3 Mini/SuperMini style boards do not all use the same silkscreen
// labels or safe breakout pins. If the OLED/MPU do not respond, change only the
// SDA/SCL lines in the selected profile below. Tiny goblin board tax.

#define YETI_BOARD_ESP32_DEVKIT 1
#define YETI_BOARD_ESP32_C3_MINI 2
#define YETI_BOARD_CUSTOM 99

#ifndef YETI_BOARD_PROFILE
  #if defined(CONFIG_IDF_TARGET_ESP32C3) || defined(ARDUINO_ESP32C3_DEV)
    #define YETI_BOARD_PROFILE YETI_BOARD_ESP32_C3_MINI
  #else
    #define YETI_BOARD_PROFILE YETI_BOARD_ESP32_DEVKIT
  #endif
#endif

#if YETI_BOARD_PROFILE == YETI_BOARD_ESP32_DEVKIT
  #define YETI_BOARD_PROFILE_NAME "ESP32 DevKit / WROOM"
  #define YETI_I2C_SDA_PIN 21
  #define YETI_I2C_SCL_PIN 22
  #define YETI_OLED_ENABLED 1
  #define YETI_OLED_I2C_ADDRESS 0x3C
  #define YETI_OLED_RESET_PIN -1
  #define YETI_MPU_ENABLED 1
  #define YETI_MPU_I2C_ADDRESS 0x68
  #define YETI_TOUCH_ENABLED 1
  #define YETI_TOUCH_PIN T0
  #define YETI_TOUCH_PIN_LABEL "D4 / GPIO4 / T0"

#elif YETI_BOARD_PROFILE == YETI_BOARD_ESP32_C3_MINI
  #define YETI_BOARD_PROFILE_NAME "ESP32-C3 Super Mini"
  // Verified wiring for this ESP32-C3 Super Mini build.
  // I2C scanner found OLED at 0x3C and MPU at 0x68 on GPIO1/GPIO3.
  #define YETI_I2C_SDA_PIN 1
  #define YETI_I2C_SCL_PIN 3
  #define YETI_OLED_ENABLED 1
  #define YETI_OLED_I2C_ADDRESS 0x3C
  #define YETI_OLED_RESET_PIN -1
  #define YETI_MPU_ENABLED 1
  #define YETI_MPU_I2C_ADDRESS 0x68
  // ESP32-C3 does not have the same built-in capacitive touchRead pads as classic ESP32.
  // Use an external touch module/button on a GPIO if you want touch on C3, then add code for it.
  #define YETI_TOUCH_ENABLED 0
  #define YETI_TOUCH_PIN_LABEL "disabled on ESP32-C3"

#elif YETI_BOARD_PROFILE == YETI_BOARD_CUSTOM
  #define YETI_BOARD_PROFILE_NAME "Custom"
  // Change only this block for a one-off wiring layout.
  #define YETI_I2C_SDA_PIN 21
  #define YETI_I2C_SCL_PIN 22
  #define YETI_OLED_ENABLED 1
  #define YETI_OLED_I2C_ADDRESS 0x3C
  #define YETI_OLED_RESET_PIN -1
  #define YETI_MPU_ENABLED 1
  #define YETI_MPU_I2C_ADDRESS 0x68
  #define YETI_TOUCH_ENABLED 1
  #define YETI_TOUCH_PIN T0
  #define YETI_TOUCH_PIN_LABEL "custom touch pin"

#else
  #error "Unknown YETI_BOARD_PROFILE selected."
#endif

#define YETI_I2C_CLOCK_HZ 400000

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define OLED_ADDRESS YETI_OLED_I2C_ADDRESS
#define OLED_RESET YETI_OLED_RESET_PIN

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
bool oledReady = false;

// =====================================================
// FACE ENGINE SELECTION
// =====================================================
// RoboEyes branch notes:
// - Uses FluxGarage_RoboEyes for the animated eye renderer.
// - Keeps YETI's Wi-Fi/WebUI/weather/clock/sensor plumbing.
// - Set YETI_FACE_ENGINE to YETI_FACE_ENGINE_CLASSIC if you want to compile
//   the old hand-rolled face renderer again without undoing this branch.
//
// Arduino IDE: Library Manager -> search/install "FluxGarage RoboEyes".

#define YETI_FACE_ENGINE_CLASSIC 0
#define YETI_FACE_ENGINE_ROBOEYES 1

#ifndef YETI_FACE_ENGINE
  #define YETI_FACE_ENGINE YETI_FACE_ENGINE_ROBOEYES
#endif

#if YETI_FACE_ENGINE == YETI_FACE_ENGINE_ROBOEYES
  #include <FluxGarage_RoboEyes.h>
  RoboEyes<Adafruit_SSD1306> roboEyes(display);
#endif

// Startup OLED screens are intentionally readable, not blink-and-you-miss-it.
// Set either to 0 if you want faster boots later.
const unsigned long YETI_BOOT_STEP_SCREEN_MS = 0; //1200;
const unsigned long YETI_BOOT_FINAL_INFO_MS = 0;  //7000;

// Runtime tuning:
// SSD1306 display.display() flushes the whole 128x64 framebuffer over I2C.
// At 400 kHz that is noticeably blocking, so do not redraw every loop.
// v1.2.0 makes the face refresh interval configurable from the WebUI.
// In the RoboEyes branch this becomes the max eye animation pacing too, so
// YETI can stay expressive without choking the WebUI. Tiny OLED, tiny pipe,
// big personality.
const uint32_t YETI_FACE_FRAME_DEFAULT_MS = 83;  // ~12 FPS; good lag/smoothness balance.
const uint32_t YETI_FACE_FRAME_MIN_MS = 40;      // 25 FPS cap. Any faster is mostly I2C goblin smoke.
const uint32_t YETI_FACE_FRAME_MAX_MS = 250;     // 4 FPS low-power/chill mode.
const unsigned long YETI_LOOP_IDLE_DELAY_MS = 1;

// =====================================================
// APP / WIFI CONFIG
// =====================================================

const char* APP_NAME = "Yeti";
const char* APP_VERSION = "1.7.4-sleep-preview-compile-fix";
const char* DEFAULT_HOSTNAME = "yeti";
const uint8_t YETI_HOSTNAME_MAX_LEN = 31;
String configuredHostname = DEFAULT_HOSTNAME;

// Empty = open setup hotspot.
// Use 8+ chars if you want setup AP password protection.
const char* SETUP_AP_PASSWORD = "";

WebServer server(80);
DNSServer dnsServer;
Preferences prefs;

const byte DNS_PORT = 53;

IPAddress setupIP(192, 168, 4, 1);
IPAddress setupGateway(192, 168, 4, 1);
IPAddress setupSubnet(255, 255, 255, 0);

bool setupMode = false;
String setupApName = "";

String savedSsid = "";
String savedPassword = "";

unsigned long lastReconnectAttempt = 0;


// =====================================================
// WEATHER / CLOCK CONFIG
// =====================================================
// Weather uses Open-Meteo because it needs no API key for non-commercial use.
// The ESP32 fetches only the small "current" payload to keep RAM use sane.

const uint32_t YETI_WEATHER_UPDATE_DEFAULT_MS = 900000;   // 15 minutes.
const uint32_t YETI_WEATHER_UPDATE_MIN_MS = 300000;       // 5 minutes; polite to free APIs.
const uint32_t YETI_WEATHER_UPDATE_MAX_MS = 21600000;     // 6 hours.

const uint32_t YETI_INFO_CARD_DEFAULT_MS = 60000;         // OLED clock/weather card interval.
const uint32_t YETI_INFO_CARD_MIN_MS = 10000;
const uint32_t YETI_INFO_CARD_MAX_MS = 3600000;
const uint32_t YETI_INFO_CARD_DURATION_MS = 6500;
const uint32_t YETI_CLOCK_TIME_PAGE_MS = 4200;
const uint32_t YETI_CLOCK_DATE_PAGE_MS = 3600;
const uint32_t YETI_CLOCK_SEQUENCE_DURATION_MS = YETI_CLOCK_TIME_PAGE_MS + YETI_CLOCK_DATE_PAGE_MS;
const uint32_t YETI_WEATHER_TICKER_STEP_MS = 45;    // Scroll cadence. Lower = faster/more OLED work.
const uint8_t YETI_WEATHER_TICKER_PX_PER_STEP = 2;
const uint32_t YETI_WEATHER_TICKER_MIN_MS = 11000;
const uint32_t YETI_WEATHER_TICKER_MAX_MS = 28000;
const uint32_t YETI_SASS_TICKER_MIN_MS = 6500;
const uint32_t YETI_SASS_TICKER_MAX_MS = 15000;
const unsigned long YETI_SASS_COOLDOWN_MIN_MS = 30000UL;
const unsigned long YETI_SASS_COOLDOWN_MAX_MS = 300000UL;
const unsigned long YETI_SASS_IDLE_CHECK_MS = 45000UL;

bool weatherEnabled = false;
String weatherLocationName = "";
float weatherLatitude = 0.0f;
float weatherLongitude = 0.0f;
bool weatherMetric = false;
uint32_t weatherUpdateMs = YETI_WEATHER_UPDATE_DEFAULT_MS;

// OLED weather ticker field toggles. Keep them configurable so the tiny
// screen can show either a lean temp crawl or full sky-gossip mode.
bool weatherTickerShowLocation = true;
bool weatherTickerShowTemp = true;
bool weatherTickerShowCondition = true;
bool weatherTickerShowFeelsLike = true;
bool weatherTickerShowHumidity = true;
bool weatherTickerShowWind = true;
bool weatherTickerShowPrecip = true;
bool weatherTickerShowUpdated = true;

bool clockEnabled = true;
bool clock24h = false;
int16_t clockUtcOffsetMinutes = -420;  // Pacific daylight default; change in WebUI.
bool clockUseWeatherOffset = true;

bool infoCardsEnabled = true;
bool infoCardClockEnabled = true;
bool infoCardWeatherEnabled = true;
uint32_t infoCardIntervalMs = YETI_INFO_CARD_DEFAULT_MS;

// =====================================================
// SLEEP MODE CONFIG
// =====================================================
// Scheduled sleep keeps the OLED mostly off overnight, pauses info/sass tickers,
// and only wakes for shake/manual controls. Default: 9:00 PM -> 6:00 AM.

const int16_t YETI_SLEEP_START_DEFAULT_MIN = 21 * 60;
const int16_t YETI_SLEEP_END_DEFAULT_MIN = 6 * 60;
const uint32_t YETI_SLEEP_ANIM_DEFAULT_MS = 12000;
const uint32_t YETI_SLEEP_ANIM_MIN_MS = 2000;
const uint32_t YETI_SLEEP_ANIM_MAX_MS = 120000;
const uint32_t YETI_SLEEP_GAP_MIN_DEFAULT_MS = 45000;
const uint32_t YETI_SLEEP_GAP_MAX_DEFAULT_MS = 180000;
const uint32_t YETI_SLEEP_GAP_MIN_ALLOWED_MS = 5000;
const uint32_t YETI_SLEEP_GAP_MAX_ALLOWED_MS = 3600000;
const uint32_t YETI_SLEEP_WAKE_HOLD_DEFAULT_MS = 900000;   // 15 minutes before schedule may reclaim him.
const uint32_t YETI_SLEEP_WAKE_HOLD_MIN_MS = 30000;
const uint32_t YETI_SLEEP_WAKE_HOLD_MAX_MS = 21600000;     // 6 hours.
const uint32_t YETI_SLEEP_FRAME_MS = 240;
const uint32_t YETI_SLEEP_PREVIEW_DEFAULT_MS = 5000;

bool sleepModeEnabled = true;
int16_t sleepStartMinute = YETI_SLEEP_START_DEFAULT_MIN;
int16_t sleepEndMinute = YETI_SLEEP_END_DEFAULT_MIN;
uint32_t sleepAnimDurationMs = YETI_SLEEP_ANIM_DEFAULT_MS;
uint32_t sleepGapMinMs = YETI_SLEEP_GAP_MIN_DEFAULT_MS;
uint32_t sleepGapMaxMs = YETI_SLEEP_GAP_MAX_DEFAULT_MS;
uint32_t sleepWakeHoldMs = YETI_SLEEP_WAKE_HOLD_DEFAULT_MS;

bool sleepModeActive = false;
bool sleepAnimationActive = false;
bool sleepPreviewActive = false;
bool oledDisplayOff = false;
unsigned long sleepAnimationStartedMs = 0;
unsigned long sleepPreviewStartedMs = 0;
uint32_t sleepPreviewDurationMs = YETI_SLEEP_PREVIEW_DEFAULT_MS;
unsigned long nextSleepAnimationAtMs = 0;
unsigned long lastSleepFrameMs = 0;
unsigned long sleepWakeOverrideUntilMs = 0;
uint32_t sleepEntryCount = 0;
uint32_t sleepWakeCount = 0;
String lastSleepNote = "Sleep mode waiting for clock";
String lastWakeReason = "Not woken yet";

bool ntpStarted = false;
unsigned long lastNtpRequestMs = 0;

bool weatherAvailable = false;
bool weatherFetchInProgress = false;
unsigned long lastWeatherAttemptMs = 0;
unsigned long lastWeatherSuccessMs = 0;
int weatherHttpCode = 0;
String weatherLastError = "Not fetched yet";
String weatherCurrentTime = "";
String weatherTimezone = "";
int weatherUtcOffsetSeconds = 0;
float weatherTemperature = 0.0f;
float weatherFeelsLike = 0.0f;
float weatherHumidity = 0.0f;
float weatherPrecipitation = 0.0f;
float weatherWindSpeed = 0.0f;
float weatherCloudCover = 0.0f;
int weatherCode = -1;
bool weatherIsDay = true;

unsigned long nextInfoCardAt = 0;
bool nextInfoCardClock = true;

// =====================================================
// TOUCH STATE
// =====================================================
//
// Touch hardware is selected in the hardware profile above.
// Classic ESP32 profile uses T0 / GPIO4. ESP32-C3 profile disables built-in touch.

bool touchReady = false;

int touchBaseline = 0;
int touchThreshold = 0;
int lastTouchValue = 0;

bool lastTouched = false;

unsigned long lastTouchRead = 0;
unsigned long lastTouchPrint = 0;

const float TOUCH_THRESHOLD_RATIO = 0.70;

// =====================================================
// MPU-6500 CONFIG
// =====================================================

#define MPU_ADDRESS YETI_MPU_I2C_ADDRESS

#define MPU_REG_PWR_MGMT_1 0x6B
#define MPU_REG_CONFIG     0x1A
#define MPU_REG_GYRO_CFG   0x1B
#define MPU_REG_ACCEL_CFG  0x1C
#define MPU_REG_ACCEL_XOUT 0x3B
#define MPU_REG_WHO_AM_I   0x75

bool mpuReady = false;

float lastAx = 0;
float lastAy = 0;
float lastAz = 0;
bool haveLastAccel = false;

float lastShakeScore = 0.0;
float lastAccelMagnitude = 0.0;
float lastMotionJerk = 0.0;
float lastTiltDelta = 0.0;

float baselineAx = 0;
float baselineAy = 0;
float baselineAz = 1;
bool haveAccelBaseline = false;

unsigned long lastMotionRead = 0;
unsigned long lastShakePrint = 0;
unsigned long lastShakeTrigger = 0;
unsigned long lastMovementActivityMs = 0;
bool tiltMoodLatched = false;

const float SHAKE_JERK_THRESHOLD_G = 0.55;
const float SHAKE_MAG_THRESHOLD_G = 0.45;

// =====================================================
// FACE CONFIG
// =====================================================

enum Expression {
  FACE_NORMAL,
  FACE_ANGRY,
  FACE_SLEEPY,
  FACE_HAPPY,
  FACE_SURPRISED,
  FACE_SHOCKED,
  FACE_BORED,
  FACE_COUNT
};

enum FaceTheme {
  FACE_THEME_CLASSIC,
  FACE_THEME_INVERTED,
  FACE_THEME_OUTLINE,
  FACE_THEME_TWO_ZONE,
  FACE_THEME_COUNT
};

FaceTheme faceTheme = FACE_THEME_CLASSIC;  // Yeti v1.2.0: original face only for now.

Expression currentExpression = FACE_NORMAL;

// =====================================================
// MOOD / PERSONALITY CONFIG
// =====================================================
// v1.5.0 keeps the old Expression enum as the final render layer.
// The new mood system owns what YETI feels, then maps that mood to
// an existing expression/RoboEyes configuration. Tiny feelings, safer plumbing.

enum YetiMood {
  MOOD_DEADPAN,
  MOOD_HAPPY,
  MOOD_ANNOYED,
  MOOD_ANGRY,
  MOOD_SLEEPY,
  MOOD_CURIOUS,
  MOOD_STARTLED,
  MOOD_SMUG,
  MOOD_SAD,
  MOOD_WEATHER,
  MOOD_BOOTING,
  MOOD_COUNT
};

enum MoodPriority {
  PRIORITY_IDLE = 1,
  PRIORITY_INFO = 2,
  PRIORITY_REACTION = 3,
  PRIORITY_ALERT = 4,
  PRIORITY_MANUAL = 5
};

// v1.5.3: event layer. Events describe why a reaction happened;
// moods still describe what the face should feel like. Keep them separate
// so weather/Wi-Fi/WebUI goblin behavior does not invade the renderer.
enum YetiEvent {
  EVENT_BOOT,
  EVENT_WIFI_CONNECTED,
  EVENT_WIFI_FAILED,
  EVENT_WIFI_WEAK,
  EVENT_WIFI_VERY_WEAK,
  EVENT_WIFI_RESTORED,
  EVENT_WEBUI_OPENED,
  EVENT_WEBUI_COMMAND,
  EVENT_WEATHER_UPDATED,
  EVENT_WEATHER_FAILED,
  EVENT_TIME_DISPLAYED,
  EVENT_WEATHER_DISPLAYED,
  EVENT_CUSTOM_MESSAGE,
  EVENT_POKED,
  EVENT_GYRO_MOVED,
  EVENT_GYRO_TILTED,
  EVENT_GYRO_SHAKEN,
  EVENT_GYRO_IDLE_LONG,
  EVENT_REBOOT_REQUESTED,
  EVENT_SETTINGS_SAVED
};

// v1.5.5: non-blocking acting sequences. These are tiny scripted
// mood arcs that run from loop() using millis(), never delay().
enum YetiSequence {
  SEQ_NONE,
  SEQ_POKE_REACTION,
  SEQ_BOOT_DRAMA,
  SEQ_BAD_WIFI_TANTRUM,
  SEQ_WEATHER_HAPPY_COLD,
  SEQ_WEATHER_RAIN_SAD,
  SEQ_IDLE_BOREDOM,
  SEQ_SETTINGS_SAVED,
  SEQ_COUNT
};

// v1.6.6: tiny memory phrases. These are not a chat system; they are
// short OLED-safe one-liners that make YETI feel like he is keeping receipts.
enum YetiPhraseCategory {
  PHRASE_NONE,
  PHRASE_POKE,
  PHRASE_SHAKE,
  PHRASE_WIFI,
  PHRASE_WEATHER,
  PHRASE_IDLE,
  PHRASE_BOOT,
  PHRASE_FORGIVE,
  PHRASE_PRAISE,
  PHRASE_GRUDGE,
  PHRASE_NEEDS,
  PHRASE_JUDGMENT
};

struct YetiPersonality {
  int grumpiness;
  int curiosity;
  int sleepiness;
  int chaos;
  int friendliness;
};

struct YetiPersonalityPreset {
  const char* id;
  const char* name;
  const char* description;
  YetiMood baseMood;
  YetiPersonality traits;
};

const YetiPersonalityPreset PERSONALITY_PRESETS[] = {
  {"classic", "Classic YETI", "The original suspicious desktop jerk. Balanced grump, mild curiosity, low friendliness.", MOOD_DEADPAN, {70, 45, 40, 30, 25}},
  {"friendly", "Friendly YETI", "More happy reactions, curious glances, and fewer rage goblin incidents.", MOOD_HAPPY, {20, 70, 30, 15, 80}},
  {"sleepy", "Sleepy YETI", "Low-energy, tired eyes, long sleepy idle moods, and haunted paperweight vibes.", MOOD_SLEEPY, {35, 20, 90, 15, 40}},
  {"feral", "Feral Goblin", "Maximum chaos, grumpiness cranked, friendliness murdered in an alley.", MOOD_ANNOYED, {90, 80, 10, 95, 5}},
  {"weather", "Weather Gremlin", "Curious, reactive, and ready for future weather-based mood nonsense.", MOOD_CURIOUS, {50, 80, 25, 45, 35}}
};

const uint8_t PERSONALITY_PRESET_COUNT = sizeof(PERSONALITY_PRESETS) / sizeof(PERSONALITY_PRESETS[0]);

YetiMood currentMood = MOOD_DEADPAN;
YetiMood baseMood = MOOD_DEADPAN;
MoodPriority currentMoodPriority = PRIORITY_IDLE;

unsigned long moodUntilMs = 0;
unsigned long lastMoodChangeMs = 0;

YetiPersonality personality = {
  70,  // grumpiness
  45,  // curiosity
  40,  // sleepiness
  30,  // chaos
  25   // friendliness
};

// v1.6.1: lightweight persistent memory. This is not an LLM brain;
// it is tiny emotional bookkeeping so YETI can remember repeated nonsense
// without wearing out flash. Runtime counters change immediately in RAM;
// Preferences writes are throttled with memoryDirty + updateMemorySystem().
struct YetiMemoryStats {
  uint32_t lifetimeBoots;
  uint32_t lifetimePokes;
  uint32_t lifetimeShakes;
  uint32_t lifetimeTilts;
  uint32_t lifetimeBadWifi;
  uint32_t lifetimeWeatherFails;
  uint32_t lifetimeWebVisits;
  uint32_t lifetimeCommands;
  uint32_t lifetimeReboots;
  uint32_t lifetimeSettingsSaves;
  uint32_t lifetimeMoodChanges;
  uint32_t lifetimeSequences;
};

struct YetiRelationship {
  int affection;
  int annoyance;
  int trust;
  int suspicion;
};

struct YetiGrudges {
  int pokeGrudge;
  int shakeGrudge;
  int wifiGrudge;
  int weatherGrudge;
  int rebootGrudge;
  int neglectGrudge;
};

struct YetiNeeds {
  int boredom;
  int energy;
  int irritation;
  int loneliness;
};

struct YetiDailyMemory {
  uint16_t todayPokes;
  uint16_t todayShakes;
  uint16_t todayBadWifi;
  uint16_t todayWeatherFails;
  uint16_t todayWebVisits;
  uint16_t todayCommands;
  uint16_t todayAngryMoodCount;
  uint16_t todayHappyMoodCount;

  uint16_t yesterdayPokes;
  uint16_t yesterdayShakes;
  uint16_t yesterdayBadWifi;
  uint16_t yesterdayWeatherFails;
  uint16_t yesterdayAngryMoodCount;
  uint16_t yesterdayHappyMoodCount;

  char lastMemoryDate[11]; // YYYY-MM-DD, empty until clock syncs
};

bool memoryEnabled = true;
bool memoryDirty = false;
unsigned long lastMemorySaveMs = 0;
const unsigned long YETI_MEMORY_SAVE_INTERVAL_MS = 60000UL;

YetiMemoryStats memoryStats = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
YetiRelationship relationship = {35, 20, 50, 25};
YetiGrudges grudges = {0, 0, 0, 0, 0, 0};
YetiNeeds needs = {20, 70, 20, 10};
YetiDailyMemory dailyMemory = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, ""};
bool needsEnabled = true;
String lastMemoryEvent = "None yet";
String lastMemoryNote = "Daily memory system loaded";
String lastGrudgeNote = "No grudges filed yet";
String lastNeedsNote = "Needs are idling quietly";
String lastDailyNote = "Daily memory waiting for clock sync";
uint32_t memoryEventCount = 0;
uint32_t grudgeEventCount = 0;
uint32_t needsEventCount = 0;
uint32_t dailyRolloverCount = 0;
unsigned long lastGrudgeDecayMs = 0;
unsigned long lastNeedsUpdateMs = 0;
unsigned long lastNeedsMoodMs = 0;
unsigned long lastNeedsPersistMarkMs = 0;
const unsigned long YETI_GRUDGE_DECAY_INTERVAL_MS = 600000UL;
const unsigned long YETI_NEEDS_UPDATE_INTERVAL_MS = 30000UL;
const unsigned long YETI_NEEDS_MOOD_COOLDOWN_MS = 90000UL;
const unsigned long YETI_NEEDS_PERSIST_INTERVAL_MS = 600000UL;

// v1.6.6 sass ticker state. Config is stored in behavior prefs because it
// controls UI/display behavior, while runtime phrase counters stay RAM-only.
bool sassTickerEnabled = true;
bool sassIdleOnly = false;
bool sassEventEnabled = true;
bool sassGrudgeEnabled = true;
uint8_t sassLevel = 55;
uint8_t sassFrequency = 35;
String lastSassPhrase = "None yet";
String lastSassCategory = "none";
String lastSassNote = "Sass ticker loaded";
uint32_t sassPhraseCount = 0;
uint32_t sassSuppressedCount = 0;
unsigned long lastSassPhraseMs = 0;
unsigned long lastSassCheckMs = 0;

bool idleMoodEnabled = true;
unsigned long lastInteractionMs = 0;
unsigned long lastIdleBehaviorMs = 0;
unsigned long nextIdleBehaviorDelayMs = 30000;
unsigned long lastIdleMoodMs = 0;
YetiMood lastIdleMood = MOOD_DEADPAN;
String lastIdleReason = "None yet";
uint32_t idleMoodCount = 0;

// v1.5.3 automatic reaction toggles and cooldown diagnostics.
bool weatherMoodEnabled = true;
bool wifiMoodEnabled = true;
bool webMoodEnabled = true;
bool movementMoodEnabled = true;

uint8_t movementSensitivity = 50;
uint8_t shakeSensitivity = 50;

unsigned long lastWifiMoodMs = 0;
unsigned long lastWeatherMoodMs = 0;
unsigned long lastWebMoodMs = 0;
unsigned long lastGyroMoodMs = 0;
unsigned long lastGyroMoveMoodMs = 0;
unsigned long lastGyroShakeMoodMs = 0;
unsigned long lastGyroIdleMoodMs = 0;
unsigned long lastWifiMoodCheckMs = 0;
bool lastWifiConnectedState = false;
int lastWifiRssi = 0;
String lastReactionEvent = "None yet";
String lastReactionReason = "None yet";
String lastMovementEvent = "None yet";
String lastMovementReason = "None yet";
uint32_t reactionMoodCount = 0;
uint32_t movementMoodCount = 0;

bool pokeStagePending = false;
unsigned long pokeStageAtMs = 0;

YetiSequence activeSequence = SEQ_NONE;
YetiSequence lastSequence = SEQ_NONE;
uint8_t sequenceStep = 0;
unsigned long sequenceStartedMs = 0;
unsigned long sequenceStepStartedMs = 0;
unsigned long nextSequenceStepAtMs = 0;
MoodPriority activeSequencePriority = PRIORITY_REACTION;
String lastSequenceReason = "None yet";
uint32_t sequenceRunCount = 0;
bool applyingSequenceMood = false;
bool sequencePausedForOverlay = false;

#if YETI_FACE_ENGINE == YETI_FACE_ENGINE_ROBOEYES
Expression roboEyesLastExpression = FACE_COUNT;
uint32_t roboEyesLastFrameMs = 0;
bool roboEyesStarted = false;
#endif

// Runtime behavior/performance config, persisted in Preferences.
uint32_t faceFrameMs = YETI_FACE_FRAME_DEFAULT_MS;
unsigned long lastFaceRenderMs = 0;
unsigned long lastFaceDraw = 0; // Classic renderer throttle; reset after sleep preview to force redraw.

// Temporary OLED info screens requested from the WebUI/serial console.
// While active, the face renderer politely backs off so the info stays readable.
enum OledOverlayMode {
  OLED_OVERLAY_NONE,
  OLED_OVERLAY_STATUS,
  OLED_OVERLAY_CLOCK_SEQUENCE,
  OLED_OVERLAY_WEATHER_TICKER,
  OLED_OVERLAY_SASS_TICKER
};

bool oledOverlayActive = false;
OledOverlayMode oledOverlayMode = OLED_OVERLAY_NONE;
unsigned long oledOverlayStarted = 0;
unsigned long oledOverlayUntil = 0;
unsigned long lastOverlayDraw = 0;
String oledTickerText = "";
uint16_t oledTickerTextWidth = 0;

// Demo mode: a harmless little expression carousel for testing the face.
bool demoMode = false;
unsigned long demoModeUntil = 0;
unsigned long nextDemoStep = 0;
int demoExpressionIndex = 0;

float eyeOffsetX = 0;
float eyeOffsetY = 0;

float targetEyeOffsetX = 0;
float targetEyeOffsetY = 0;

unsigned long nextLookTime = 0;
unsigned long nextBlinkTime = 0;
unsigned long nextExpressionTime = 0;

bool blinking = false;
unsigned long blinkStartTime = 0;
unsigned long blinkDuration = 180;

bool doubleBlinkQueued = false;
unsigned long doubleBlinkTime = 0;

// =====================================================
// SENSOR TRIGGER CONFIG
// =====================================================

struct SensorTriggerConfig {
  bool enabled;
  uint32_t holdMs;
  uint32_t expressionMask;
};

uint32_t expressionBit(Expression expression) {
  return 1UL << (int)expression;
}

const uint32_t DEFAULT_SHAKE_MASK =
  expressionBit(FACE_ANGRY) |
  expressionBit(FACE_SURPRISED) |
  expressionBit(FACE_SHOCKED);

const uint32_t DEFAULT_TOUCH_MASK =
  expressionBit(FACE_HAPPY) |
  expressionBit(FACE_SLEEPY) |
  expressionBit(FACE_SURPRISED);

SensorTriggerConfig shakeConfig = {
  true,
  1800,
  DEFAULT_SHAKE_MASK
};

SensorTriggerConfig touchConfig = {
  true,
  1600,
  DEFAULT_TOUCH_MASK
};

Expression sensorOverrideExpression = FACE_NORMAL;
unsigned long sensorOverrideUntil = 0;
uint8_t sensorOverridePriority = 0;
String lastTriggerName = "None";

bool boredActive = false;
uint32_t boredTimeoutMs = 180000;
unsigned long lastEngagementMs = 0;

const uint8_t TOUCH_PRIORITY = 10;
const uint8_t SHAKE_PRIORITY = 20;

// =====================================================
// EVENT LOG / DIAGNOSTICS
// =====================================================

struct EventLogEntry {
  uint32_t ms;
  char type[14];
  char message[72];
};

const uint8_t YETI_EVENT_LOG_SIZE = 16;
EventLogEntry eventLog[YETI_EVENT_LOG_SIZE];
uint8_t eventLogHead = 0;
uint8_t eventLogCount = 0;

// =====================================================
// GENERAL HELPERS
// =====================================================

String yesNo(bool value) {
  return value ? "Yes" : "No";
}

String onOff(bool value) {
  return value ? "On" : "Off";
}

String jsonEscape(String value) {
  value.replace("\\", "\\\\");
  value.replace("\"", "\\\"");
  value.replace("\n", "\\n");
  value.replace("\r", "\\r");
  value.replace("\t", "\\t");
  return value;
}

String htmlEscape(String value) {
  value.replace("&", "&amp;");
  value.replace("<", "&lt;");
  value.replace(">", "&gt;");
  value.replace("\"", "&quot;");
  value.replace("'", "&#39;");
  return value;
}

void copyToBuffer(char *dest, size_t destSize, String value) {
  if (destSize == 0) {
    return;
  }

  value.toCharArray(dest, destSize);
  dest[destSize - 1] = '\0';
}

void addEvent(String type, String message) {
  EventLogEntry &entry = eventLog[eventLogHead];

  entry.ms = millis();
  copyToBuffer(entry.type, sizeof(entry.type), type);
  copyToBuffer(entry.message, sizeof(entry.message), message);

  eventLogHead = (eventLogHead + 1) % YETI_EVENT_LOG_SIZE;

  if (eventLogCount < YETI_EVENT_LOG_SIZE) {
    eventLogCount++;
  }
}

uint32_t sanitizeFaceFrameMs(uint32_t value, uint32_t fallback) {
  if (value < YETI_FACE_FRAME_MIN_MS || value > YETI_FACE_FRAME_MAX_MS) {
    return fallback;
  }

  return value;
}

bool isHostnameAllowedChar(char c) {
  return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-';
}

String sanitizeHostname(String value) {
  value.trim();
  value.toLowerCase();

  String cleaned = "";
  bool previousHyphen = false;

  for (uint16_t i = 0; i < value.length() && cleaned.length() < YETI_HOSTNAME_MAX_LEN; i++) {
    char c = value.charAt(i);

    if (c >= 'A' && c <= 'Z') {
      c = c + 32;
    }

    if (c == '_' || c == ' ' || c == '.') {
      c = '-';
    }

    if (!isHostnameAllowedChar(c)) {
      continue;
    }

    if (c == '-') {
      if (cleaned.length() == 0 || previousHyphen) {
        continue;
      }
      previousHyphen = true;
    } else {
      previousHyphen = false;
    }

    cleaned += c;
  }

  while (cleaned.endsWith("-")) {
    cleaned.remove(cleaned.length() - 1);
  }

  if (cleaned.length() == 0) {
    cleaned = DEFAULT_HOSTNAME;
  }

  return cleaned;
}

String hostnameLocalUrl() {
  return String("http://") + configuredHostname + ".local/";
}

bool beginConfiguredMdns() {
  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }

  MDNS.end();

  if (MDNS.begin(configuredHostname.c_str())) {
    Serial.print("mDNS: ");
    Serial.println(hostnameLocalUrl());
    addEvent("mDNS", String("Started: ") + configuredHostname + ".local");
    return true;
  }

  Serial.println("mDNS failed. Use the IP address instead.");
  addEvent("mDNS", "Start failed");
  return false;
}


void showTemporaryOledStatus(String title, String line1, String line2, String line3, String line4, String line5, unsigned long durationMs);
uint16_t oledTextWidth(String text, uint8_t size);
void drawCenteredOledText(String text, int16_t y, uint8_t size);
void startOledOverlay(OledOverlayMode mode, unsigned long durationMs);
void stopOledOverlay();
const char* oledOverlayModeName(OledOverlayMode mode);
String oledOverlayModeLabel(OledOverlayMode mode);
String displayModeLabel();
String moodAutomationSummary();
String moodSafetySummary();
void drawClockSequenceOverlay();
void drawWeatherTickerOverlay();
void updateOledOverlay();
void initFaceEngine();
void updateFaceEngine();
const char* moodToString(YetiMood mood);
YetiMood stringToMood(String moodName);
Expression expressionForMood(YetiMood mood);
YetiMood moodFromExpression(Expression expression);
bool setMood(YetiMood mood, unsigned long durationMs = 0, MoodPriority priority = PRIORITY_REACTION, bool countAsInteraction = true);
void setBaseMood(YetiMood mood);
void applyMood(YetiMood mood);
void updateMoodEngine();
void markInteraction();
void updateIdleBehavior();
void scheduleNextIdleBehavior();
unsigned long idleMoodDurationMs(YetiMood mood);
YetiMood chooseIdleMoodFromPersonality();
String idleMoodReason(YetiMood mood);
int weightedTraitValue(int raw);
bool runRandomIdleBehavior();
void triggerPokeYeti();
void calmDownYeti();
const char* sequenceToString(YetiSequence seq);
String sequenceLabel(YetiSequence seq);
YetiSequence stringToSequence(String seqName);
bool sequenceActive();
bool startSequence(YetiSequence seq, String reason = "manual", MoodPriority priority = PRIORITY_REACTION);
void stopSequence(bool returnToBase = false);
void updateSequence();
bool sequenceStepMood(YetiMood mood, unsigned long durationMs, MoodPriority priority);
void finishSequence(bool returnToBase = true);
void saveBehaviorConfig();
const YetiPersonalityPreset* findPersonalityPreset(String presetId);
bool personalityMatchesPreset(const YetiPersonalityPreset &preset);
String activePersonalityPresetId();
String activePersonalityPresetLabel();
bool applyPersonalityPreset(String presetId, bool saveNow = true);
String personalityPresetOptionsJson();
const char* yetiEventName(YetiEvent event);
bool cooldownReady(unsigned long lastMs, unsigned long cooldownMs);
void recordReaction(YetiEvent event, String reason);
void handleYetiEvent(YetiEvent event);
void reactToCurrentWeather(String triggerReason = "weather update");
void updateWifiMoodReactions(bool force = false);
uint8_t sanitizeSensitivity(int value, uint8_t fallback = 50);
float movementJerkThresholdG();
float movementTiltThresholdG();
float movementShakeJerkThresholdG();
float movementShakeMagThresholdG();
bool gyroMoodReady(unsigned long lastMs, unsigned long cooldownMs);
void recordMovementReaction(YetiEvent event, String reason);
int clampPercent(int value);
void loadMemorySettings();
void saveMemorySettings(bool force = false);
void resetMemorySettings();
void markMemoryDirty();
void updateMemorySystem();
void adjustRelationship(int affectionDelta, int annoyanceDelta, int trustDelta, int suspicionDelta);
void adjustGrudgeValue(int &value, int delta);
void increaseGrudge(YetiEvent event, int amount);
void reduceAllGrudges(int amount);
void decayGrudges(bool force = false);
int getTotalGrudge();
bool isHoldingGrudge();
String getWorstGrudgeName();
String getGrudgeSummary();
void adjustNeedValue(int &value, int delta, bool markDirtyNow = true);
void adjustNeeds(int boredomDelta, int energyDelta, int irritationDelta, int lonelinessDelta, bool markDirtyNow = true);
void updateNeeds();
YetiMood moodFromNeeds();
bool shouldNeedsTriggerMood();
String getNeedsSummary();
String getNeedsBiasSummary();
String currentDailyMemoryDateKey();
void setDailyMemoryDate(const String& dateKey);
void clearTodayDailyMemory();
void resetDailyMemory();
void rolloverDailyMemoryIfNeeded(bool force = false);
void recordDailyEvent(YetiEvent event);
void recordDailyMood(YetiMood mood);
String getTodaySummary();
String getYesterdaySummary();
String getDailyMemorySummary();
String getYesterdayBootReason();
YetiSequence bootSequenceFromDailyMemory();
String getMemoryBiasSummary();
int getToleranceScore();
int getForgivenessScore();
String getJudgmentLabel();
String getOpinionSummary();
String getMoodBiasSummary();
String getCurrentGrievance();
String getForgivenessStatus();
String getMemoryFlavorText();
String getTrustStatus();
String getRelationshipTone();
String getDailyOpinionSummary();
String getYetiReportCard();
const char* phraseCategoryName(YetiPhraseCategory category);
String phraseCategoryLabel(YetiPhraseCategory category);
YetiPhraseCategory stringToPhraseCategory(String name);
YetiPhraseCategory phraseCategoryForEvent(YetiEvent event);
uint32_t sassCooldownMs();
bool sassCooldownReady(bool force = false);
bool canShowSassPhrase(bool force = false, YetiPhraseCategory category = PHRASE_IDLE);
uint32_t sassTickerDurationFor(String text);
const char* pickPhrase(YetiPhraseCategory category);
bool showSassPhrase(String phrase, YetiPhraseCategory category, bool force = false);
bool maybeShowMemoryPhrase(YetiPhraseCategory category, bool force = false);
void drawSassTickerOverlay();
void updateSassSystem();
int activeClockOffsetSeconds();
int16_t sanitizeMinuteOfDay(long value, int16_t fallback = 0);
int16_t parseTimeToMinute(String value, int16_t fallback);
String minuteToTimeString(int16_t minute);
uint32_t sanitizeSleepAnimMs(uint32_t value, uint32_t fallback = YETI_SLEEP_ANIM_DEFAULT_MS);
uint32_t sanitizeSleepGapMs(uint32_t value, uint32_t fallback);
uint32_t sanitizeSleepWakeHoldMs(uint32_t value, uint32_t fallback = YETI_SLEEP_WAKE_HOLD_DEFAULT_MS);
bool sleepScheduleActiveNow();
bool sleepWakeOverrideActive();
String sleepStatusLabel();
String sleepSummaryLine();
bool wakeYetiFromSleep(String reason, bool holdOverride = true);
bool sleepRenderBlockActive();
void startSleepAnimationPreview(uint32_t durationMs = YETI_SLEEP_PREVIEW_DEFAULT_MS);
void stopSleepAnimationPreview(bool redrawFace = true);
void updateSleepAnimationPreview();
void enterSleepMode(String reason = "schedule");
void exitSleepMode(String reason = "wake", bool userInitiated = false);
void updateSleepSystem();
String buildSleepJson();
YetiMood biasMoodWithGrudge(YetiMood proposedMood, YetiEvent event);
unsigned long biasDurationWithGrudge(unsigned long durationMs, YetiEvent event);
void recordMemoryEvent(YetiEvent event);
String getYetiJudgment();
String getRecentGrievance();
String getRelationshipSummary();
String buildMemoryJson();
void sendMemoryApiResponse(bool ok, String error = "");
void handleApiMemoryGet();
void handleApiMemorySave();
void handleApiMemoryReset();
void handleApiMemoryForgive();
void handleApiMemoryAnnoy();
void handleApiMemoryPraise();
void handleApiMemoryDecay();
void handleApiMemoryAttention();
void handleApiMemoryCalm();
void handleApiMemoryBore();
void handleApiMemoryWake();
void handleApiMemoryRollover();
void handleApiMemoryClearToday();
void handleApiSassTest();
void handleApiSassJudgment();
void handleApiSassGrievance();
void handleApiSassRandom();
void handleApiSassClear();
void handleApiMoodWeatherNow();
void handleApiMoodWifiNow();
void handleApiMoodMovementNow();
void handleApiSequencePost();
void handleApiSequenceStop();

String sanitizeLocationName(String value) {
  value.trim();
  value.replace("\n", " ");
  value.replace("\r", " ");

  while (value.indexOf("  ") >= 0) {
    value.replace("  ", " ");
  }

  if (value.length() > 48) {
    value = value.substring(0, 48);
  }

  return value;
}

float sanitizeLatitude(float value) {
  if (isnan(value) || value < -90.0f || value > 90.0f) {
    return 0.0f;
  }
  return value;
}

float sanitizeLongitude(float value) {
  if (isnan(value) || value < -180.0f || value > 180.0f) {
    return 0.0f;
  }
  return value;
}

int16_t sanitizeUtcOffsetMinutes(long value) {
  if (value < -840 || value > 840) {
    return -420;
  }
  return (int16_t)value;
}

uint32_t sanitizeWeatherUpdateMs(uint32_t value, uint32_t fallback) {
  if (value < YETI_WEATHER_UPDATE_MIN_MS || value > YETI_WEATHER_UPDATE_MAX_MS) {
    return fallback;
  }
  return value;
}

uint32_t sanitizeWeatherUpdateMs(uint32_t value) {
  return sanitizeWeatherUpdateMs(value, YETI_WEATHER_UPDATE_DEFAULT_MS);
}

uint32_t sanitizeInfoCardIntervalMs(uint32_t value, uint32_t fallback) {
  if (value < YETI_INFO_CARD_MIN_MS || value > YETI_INFO_CARD_MAX_MS) {
    return fallback;
  }
  return value;
}

uint32_t sanitizeInfoCardIntervalMs(uint32_t value) {
  return sanitizeInfoCardIntervalMs(value, YETI_INFO_CARD_DEFAULT_MS);
}

int16_t sanitizeMinuteOfDay(long value, int16_t fallback) {
  if (value < 0 || value > 1439) {
    return fallback;
  }
  return (int16_t)value;
}

int16_t parseTimeToMinute(String value, int16_t fallback) {
  value.trim();
  if (value.length() == 0) {
    return fallback;
  }

  int colon = value.indexOf(':');
  if (colon > 0) {
    int hour = value.substring(0, colon).toInt();
    int minute = value.substring(colon + 1).toInt();
    if (hour >= 0 && hour <= 23 && minute >= 0 && minute <= 59) {
      return (int16_t)(hour * 60 + minute);
    }
    return fallback;
  }

  return sanitizeMinuteOfDay(value.toInt(), fallback);
}

String minuteToTimeString(int16_t minute) {
  minute = sanitizeMinuteOfDay(minute, 0);
  char buf[6];
  snprintf(buf, sizeof(buf), "%02d:%02d", minute / 60, minute % 60);
  return String(buf);
}

uint32_t sanitizeSleepAnimMs(uint32_t value, uint32_t fallback) {
  if (value < YETI_SLEEP_ANIM_MIN_MS || value > YETI_SLEEP_ANIM_MAX_MS) {
    return fallback;
  }
  return value;
}

uint32_t sanitizeSleepGapMs(uint32_t value, uint32_t fallback) {
  if (value < YETI_SLEEP_GAP_MIN_ALLOWED_MS || value > YETI_SLEEP_GAP_MAX_ALLOWED_MS) {
    return fallback;
  }
  return value;
}

uint32_t sanitizeSleepWakeHoldMs(uint32_t value, uint32_t fallback) {
  if (value < YETI_SLEEP_WAKE_HOLD_MIN_MS || value > YETI_SLEEP_WAKE_HOLD_MAX_MS) {
    return fallback;
  }
  return value;
}

String compactMs(uint32_t value) {
  if (value < 1000) {
    return String(value) + " ms";
  }

  uint32_t seconds = value / 1000;
  if (seconds < 60) {
    return String(seconds) + " sec";
  }

  uint32_t minutes = seconds / 60;
  if (minutes < 60) {
    return String(minutes) + " min";
  }

  return String(minutes / 60) + " hr " + String(minutes % 60) + " min";
}

bool weatherLocationConfigured() {
  bool validLat = !isnan(weatherLatitude) && weatherLatitude >= -90.0f && weatherLatitude <= 90.0f;
  bool validLon = !isnan(weatherLongitude) && weatherLongitude >= -180.0f && weatherLongitude <= 180.0f;

  if (!validLat || !validLon) {
    return false;
  }

  // Prevent the blank default 0,0 from silently becoming "middle of the ocean" weather.
  return weatherLocationName.length() > 0 || fabs(weatherLatitude) > 0.001f || fabs(weatherLongitude) > 0.001f;
}

String weatherTempUnit() {
  return weatherMetric ? "C" : "F";
}

String weatherWindUnit() {
  return weatherMetric ? "km/h" : "mph";
}

String weatherPrecipUnit() {
  return weatherMetric ? "mm" : "in";
}

String weatherCodeDescription(int code) {
  switch (code) {
    case 0: return "Clear";
    case 1: return "Mainly clear";
    case 2: return "Partly cloudy";
    case 3: return "Cloudy";
    case 45:
    case 48: return "Fog";
    case 51:
    case 53:
    case 55: return "Drizzle";
    case 56:
    case 57: return "Freezing drizzle";
    case 61:
    case 63:
    case 65: return "Rain";
    case 66:
    case 67: return "Freezing rain";
    case 71:
    case 73:
    case 75: return "Snow";
    case 77: return "Snow grains";
    case 80:
    case 81:
    case 82: return "Rain showers";
    case 85:
    case 86: return "Snow showers";
    case 95: return "Thunderstorm";
    case 96:
    case 99: return "Thunder + hail";
    default: return "Unknown";
  }
}

String weatherSummaryLine() {
  if (!weatherEnabled) {
    return "Disabled";
  }

  if (!weatherLocationConfigured()) {
    return "Needs location";
  }

  if (!weatherAvailable) {
    return weatherLastError.length() ? weatherLastError : String("Not fetched");
  }

  return String(weatherTemperature, 1) + " °" + weatherTempUnit() + " · " + weatherCodeDescription(weatherCode);
}

String jsonObjectForKey(const String &json, const String &key) {
  String token = "\"" + key + "\":{";
  int tokenIndex = json.indexOf(token);

  if (tokenIndex < 0) {
    return "";
  }

  int start = json.indexOf('{', tokenIndex);
  if (start < 0) {
    return "";
  }

  int depth = 0;
  bool inString = false;
  bool escape = false;

  for (int i = start; i < (int)json.length(); i++) {
    char c = json.charAt(i);

    if (escape) {
      escape = false;
      continue;
    }

    if (c == '\\') {
      escape = true;
      continue;
    }

    if (c == '"') {
      inString = !inString;
      continue;
    }

    if (inString) {
      continue;
    }

    if (c == '{') {
      depth++;
    } else if (c == '}') {
      depth--;
      if (depth == 0) {
        return json.substring(start, i + 1);
      }
    }
  }

  return "";
}

bool jsonFindNumber(const String &json, const String &key, float &out) {
  String token = "\"" + key + "\":";
  int index = json.indexOf(token);

  if (index < 0) {
    return false;
  }

  index += token.length();
  while (index < (int)json.length() && isspace(json.charAt(index))) {
    index++;
  }

  if (json.startsWith("null", index)) {
    return false;
  }

  int start = index;
  while (index < (int)json.length()) {
    char c = json.charAt(index);
    if ((c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.' || c == 'e' || c == 'E') {
      index++;
    } else {
      break;
    }
  }

  if (index == start) {
    return false;
  }

  out = json.substring(start, index).toFloat();
  return true;
}

bool jsonFindInt(const String &json, const String &key, int &out) {
  float value = 0;
  if (!jsonFindNumber(json, key, value)) {
    return false;
  }
  out = (int)round(value);
  return true;
}

bool jsonFindString(const String &json, const String &key, String &out) {
  String token = "\"" + key + "\":";
  int index = json.indexOf(token);

  if (index < 0) {
    return false;
  }

  index += token.length();
  while (index < (int)json.length() && isspace(json.charAt(index))) {
    index++;
  }

  if (index >= (int)json.length() || json.charAt(index) != '"') {
    return false;
  }

  index++;
  String value = "";
  bool escape = false;

  while (index < (int)json.length()) {
    char c = json.charAt(index++);

    if (escape) {
      value += c;
      escape = false;
      continue;
    }

    if (c == '\\') {
      escape = true;
      continue;
    }

    if (c == '"') {
      out = value;
      return true;
    }

    value += c;
  }

  return false;
}

void ensureClockStarted() {
  if (!clockEnabled || WiFi.status() != WL_CONNECTED) {
    return;
  }

  unsigned long now = millis();
  bool alreadySynced = time(nullptr) > 1700000000;

  if (ntpStarted && alreadySynced && now - lastNtpRequestMs < 21600000UL) {
    return;
  }

  if (ntpStarted && !alreadySynced && now - lastNtpRequestMs < 60000UL) {
    return;
  }

  configTime(0, 0, "pool.ntp.org", "time.nist.gov", "time.google.com");
  ntpStarted = true;
  lastNtpRequestMs = now;
  addEvent("Clock", "NTP sync requested");
}

bool clockSynced() {
  return time(nullptr) > 1700000000;
}

bool weatherClockOffsetUsable() {
  if (!weatherAvailable || weatherTimezone.length() == 0) {
    return false;
  }

  // Valid civil time zones are roughly UTC-12 through UTC+14.
  // Treat a zero weather offset as usable only when the API clearly reports a
  // UTC/GMT style zone; otherwise prefer the manual offset to avoid surprise
  // "why is my desk goblin on London time" moments.
  if (weatherUtcOffsetSeconds < -43200 || weatherUtcOffsetSeconds > 50400) {
    return false;
  }

  if (weatherUtcOffsetSeconds == 0) {
    String tz = weatherTimezone;
    tz.toUpperCase();
    return tz.indexOf("UTC") >= 0 || tz.indexOf("GMT") >= 0 || tz.indexOf("LONDON") >= 0;
  }

  return true;
}

int activeClockOffsetSeconds() {
  if (clockUseWeatherOffset && weatherClockOffsetUsable()) {
    return weatherUtcOffsetSeconds;
  }
  return (int)clockUtcOffsetMinutes * 60;
}

String clockOffsetSource() {
  if (clockUseWeatherOffset && weatherClockOffsetUsable()) {
    return String("Weather: ") + weatherTimezone;
  }

  if (clockUseWeatherOffset && weatherAvailable && !weatherClockOffsetUsable()) {
    return "Manual fallback";
  }

  return "Manual";
}

String clockOffsetLabel() {
  int minutes = activeClockOffsetSeconds() / 60;
  char sign = minutes < 0 ? '-' : '+';
  minutes = abs(minutes);

  char buf[12];
  snprintf(buf, sizeof(buf), "UTC%c%02d:%02d", sign, minutes / 60, minutes % 60);
  return String(buf);
}

String formatClockTime() {
  if (!clockSynced()) {
    return "--:--";
  }

  time_t raw = time(nullptr) + activeClockOffsetSeconds();
  struct tm localTime;
  gmtime_r(&raw, &localTime);

  char buf[18];
  if (clock24h) {
    strftime(buf, sizeof(buf), "%H:%M:%S", &localTime);
  } else {
    strftime(buf, sizeof(buf), "%I:%M:%S %p", &localTime);
  }

  String out = String(buf);
  if (!clock24h && out.startsWith("0")) {
    out.remove(0, 1);
  }
  return out;
}

String formatClockDate() {
  if (!clockSynced()) {
    return "Waiting for NTP";
  }

  time_t raw = time(nullptr) + activeClockOffsetSeconds();
  struct tm localTime;
  gmtime_r(&raw, &localTime);

  char buf[24];
  strftime(buf, sizeof(buf), "%a %b %d, %Y", &localTime);
  return String(buf);
}

String weatherUrl() {
  String url = "https://api.open-meteo.com/v1/forecast?latitude=";
  url += String(weatherLatitude, 4);
  url += "&longitude=";
  url += String(weatherLongitude, 4);
  url += "&current=temperature_2m,relative_humidity_2m,apparent_temperature,precipitation,weather_code,cloud_cover,wind_speed_10m,is_day";
  url += "&temperature_unit=";
  url += weatherMetric ? "celsius" : "fahrenheit";
  url += "&wind_speed_unit=";
  url += weatherMetric ? "kmh" : "mph";
  url += "&precipitation_unit=";
  url += weatherMetric ? "mm" : "inch";
  url += "&timezone=auto&forecast_days=1";
  return url;
}

bool fetchWeatherNow(bool force = false) {
  if (!weatherEnabled) {
    weatherLastError = "Weather disabled";
    weatherAvailable = false;
    return false;
  }

  if (!weatherLocationConfigured()) {
    weatherLastError = "Set location first";
    weatherAvailable = false;
    return false;
  }

  if (WiFi.status() != WL_CONNECTED) {
    weatherLastError = "Wi-Fi offline";
    weatherAvailable = false;
    handleYetiEvent(EVENT_WEATHER_FAILED);
    return false;
  }

  unsigned long now = millis();
  if (!force && lastWeatherAttemptMs > 0 && now - lastWeatherAttemptMs < 30000UL) {
    return weatherAvailable;
  }

  weatherFetchInProgress = true;
  lastWeatherAttemptMs = now;
  weatherHttpCode = 0;

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(8000);

  HTTPClient http;
  http.setTimeout(8000);
  http.useHTTP10(true);

  String url = weatherUrl();
  Serial.println("Fetching weather:");
  Serial.println(url);
  addEvent("Weather", "Fetch started");

  if (!http.begin(client, url)) {
    weatherFetchInProgress = false;
    weatherLastError = "HTTP begin failed";
    addEvent("Weather", "HTTP begin failed");
    handleYetiEvent(EVENT_WEATHER_FAILED);
    return false;
  }

  http.addHeader("User-Agent", "Yeti-ESP32/1.7.1");

  int code = http.GET();
  weatherHttpCode = code;

  if (code != 200) {
    weatherFetchInProgress = false;
    weatherLastError = String("HTTP ") + String(code);
    http.end();
    addEvent("Weather", weatherLastError);
    handleYetiEvent(EVENT_WEATHER_FAILED);
    return false;
  }

  String payload = http.getString();
  http.end();

  String current = jsonObjectForKey(payload, "current");
  if (current.length() == 0) {
    weatherFetchInProgress = false;
    weatherLastError = "Bad weather JSON";
    addEvent("Weather", "Bad JSON");
    handleYetiEvent(EVENT_WEATHER_FAILED);
    return false;
  }

  float number = 0.0f;
  int integer = 0;
  String text = "";

  if (jsonFindNumber(current, "temperature_2m", number)) weatherTemperature = number;
  if (jsonFindNumber(current, "apparent_temperature", number)) weatherFeelsLike = number;
  if (jsonFindNumber(current, "relative_humidity_2m", number)) weatherHumidity = number;
  if (jsonFindNumber(current, "precipitation", number)) weatherPrecipitation = number;
  if (jsonFindNumber(current, "wind_speed_10m", number)) weatherWindSpeed = number;
  if (jsonFindNumber(current, "cloud_cover", number)) weatherCloudCover = number;
  if (jsonFindInt(current, "weather_code", integer)) weatherCode = integer;
  if (jsonFindInt(current, "is_day", integer)) weatherIsDay = integer != 0;
  if (jsonFindString(current, "time", text)) weatherCurrentTime = text;

  if (jsonFindInt(payload, "utc_offset_seconds", integer)) weatherUtcOffsetSeconds = integer;
  if (jsonFindString(payload, "timezone", text)) weatherTimezone = text;

  weatherAvailable = true;
  weatherFetchInProgress = false;
  weatherLastError = "";
  lastWeatherSuccessMs = millis();

  addEvent("Weather", String("Updated: ") + weatherSummaryLine());
  handleYetiEvent(EVENT_WEATHER_UPDATED);
  return true;
}

void updateWeatherIfNeeded() {
  if (!weatherEnabled || setupMode) {
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  unsigned long now = millis();
  if (lastWeatherAttemptMs == 0 || now - lastWeatherAttemptMs >= weatherUpdateMs) {
    fetchWeatherNow(lastWeatherAttemptMs == 0);
  }
}

String weatherAgeLabel() {
  if (!weatherAvailable || lastWeatherSuccessMs == 0) {
    return "Never";
  }
  return compactMs(millis() - lastWeatherSuccessMs) + " ago";
}

String formatClockTimeBig() {
  if (!clockSynced()) {
    return "--:--";
  }

  time_t raw = time(nullptr) + activeClockOffsetSeconds();
  struct tm localTime;
  gmtime_r(&raw, &localTime);

  char buf[10];
  if (clock24h) {
    strftime(buf, sizeof(buf), "%H:%M", &localTime);
  } else {
    strftime(buf, sizeof(buf), "%I:%M", &localTime);
  }

  String out = String(buf);
  if (!clock24h && out.startsWith("0")) {
    out.remove(0, 1);
  }
  return out;
}

String formatClockAmPmLabel() {
  if (!clockSynced() || clock24h) {
    return "";
  }

  time_t raw = time(nullptr) + activeClockOffsetSeconds();
  struct tm localTime;
  gmtime_r(&raw, &localTime);

  char buf[4];
  strftime(buf, sizeof(buf), "%p", &localTime);
  return String(buf);
}

String formatClockDateBig() {
  if (!clockSynced()) {
    return "WAIT NTP";
  }

  time_t raw = time(nullptr) + activeClockOffsetSeconds();
  struct tm localTime;
  gmtime_r(&raw, &localTime);

  char buf[16];
  strftime(buf, sizeof(buf), "%a %b %d", &localTime);
  return String(buf);
}

String formatClockYearBig() {
  if (!clockSynced()) {
    return "";
  }

  time_t raw = time(nullptr) + activeClockOffsetSeconds();
  struct tm localTime;
  gmtime_r(&raw, &localTime);

  char buf[8];
  strftime(buf, sizeof(buf), "%Y", &localTime);
  return String(buf);
}

void appendTickerPart(String &line, const String &part) {
  if (part.length() == 0) {
    return;
  }

  if (line.length() > 0) {
    line += " | ";
  }

  line += part;
}

String weatherTickerLine() {
  String loc = weatherLocationName.length() ? weatherLocationName : String(weatherLatitude, 2) + "," + String(weatherLongitude, 2);
  String tempText = String(weatherTemperature, weatherMetric ? 1 : 0) + " " + weatherTempUnit();
  String feelsText = String(weatherFeelsLike, weatherMetric ? 1 : 0) + " " + weatherTempUnit();
  String windText = String(weatherWindSpeed, 1) + " " + weatherWindUnit();
  String rainText = String(weatherPrecipitation, weatherMetric ? 1 : 2) + " " + weatherPrecipUnit();

  String line = "";

  if (weatherTickerShowLocation) appendTickerPart(line, loc);
  if (weatherTickerShowTemp) appendTickerPart(line, String("Temp ") + tempText);
  if (weatherTickerShowCondition) appendTickerPart(line, weatherCodeDescription(weatherCode));
  if (weatherTickerShowFeelsLike) appendTickerPart(line, String("Feels ") + feelsText);
  if (weatherTickerShowHumidity) appendTickerPart(line, String("Hum ") + String(weatherHumidity, 0) + "%");
  if (weatherTickerShowWind) appendTickerPart(line, String("Wind ") + windText);
  if (weatherTickerShowPrecip) appendTickerPart(line, String("Precip ") + rainText);
  if (weatherTickerShowUpdated) appendTickerPart(line, String("Updated ") + weatherAgeLabel());

  if (line.length() == 0) {
    line = tempText + " " + weatherCodeDescription(weatherCode);
  }

  return line;
}

uint32_t weatherTickerDurationFor(String text) {
  uint16_t width = oledTextWidth(text, 2);
  uint32_t travelPixels = SCREEN_WIDTH + width + 24;
  uint32_t duration = ((travelPixels * YETI_WEATHER_TICKER_STEP_MS) / YETI_WEATHER_TICKER_PX_PER_STEP) + 1200;

  if (duration < YETI_WEATHER_TICKER_MIN_MS) duration = YETI_WEATHER_TICKER_MIN_MS;
  if (duration > YETI_WEATHER_TICKER_MAX_MS) duration = YETI_WEATHER_TICKER_MAX_MS;
  return duration;
}

void drawClockSequenceOverlay() {
  if (!oledReady) {
    return;
  }

  unsigned long now = millis();
  if (lastOverlayDraw != 0 && now - lastOverlayDraw < 250) {
    return;
  }
  lastOverlayDraw = now;

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  if (!clockSynced()) {
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print("YETI CLOCK");
    display.drawLine(0, 10, 127, 10, SSD1306_WHITE);
    drawCenteredOledText("SYNC", 18, 3);
    drawCenteredOledText("Waiting for NTP", 52, 1);
    display.display();
    return;
  }

  unsigned long elapsed = now - oledOverlayStarted;
  bool datePage = elapsed >= YETI_CLOCK_TIME_PAGE_MS;

  if (!datePage) {
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print("YETI CLOCK");
    display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

    String timeText = formatClockTimeBig();
    String ampm = formatClockAmPmLabel();
    drawCenteredOledText(timeText, 20, 3);

    if (ampm.length()) {
      drawCenteredOledText(ampm + "  " + clockOffsetLabel(), 54, 1);
    } else {
      drawCenteredOledText(clockOffsetLabel(), 54, 1);
    }
  } else {
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print("YETI DATE");
    display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

    drawCenteredOledText(formatClockDateBig(), 20, 2);
    drawCenteredOledText(formatClockYearBig(), 43, 2);
  }

  display.display();
}

void drawWeatherTickerOverlay() {
  if (!oledReady || oledTickerText.length() == 0) {
    return;
  }

  unsigned long now = millis();
  if (lastOverlayDraw != 0 && now - lastOverlayDraw < YETI_WEATHER_TICKER_STEP_MS) {
    return;
  }
  lastOverlayDraw = now;

  uint32_t travel = SCREEN_WIDTH + oledTickerTextWidth + 24;
  uint32_t offset = (((now - oledOverlayStarted) / YETI_WEATHER_TICKER_STEP_MS) * YETI_WEATHER_TICKER_PX_PER_STEP) % travel;
  int16_t x = SCREEN_WIDTH - (int16_t)offset;

  String loc = weatherLocationName.length() ? weatherLocationName : String(weatherLatitude, 2) + "," + String(weatherLongitude, 2);

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(clipText(loc, 21));
  display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

  display.setTextSize(2);
  display.setCursor(x, 24);
  display.print(oledTickerText);

  display.setTextSize(1);
  display.setCursor(0, 56);
  display.print(clipText(String("Updated ") + weatherAgeLabel(), 21));
  display.display();
}

void drawSassTickerOverlay() {
  if (!oledReady || oledTickerText.length() == 0) {
    return;
  }

  unsigned long now = millis();
  if (lastOverlayDraw != 0 && now - lastOverlayDraw < YETI_WEATHER_TICKER_STEP_MS) {
    return;
  }
  lastOverlayDraw = now;

  uint32_t travel = SCREEN_WIDTH + oledTickerTextWidth + 24;
  uint32_t offset = (((now - oledOverlayStarted) / YETI_WEATHER_TICKER_STEP_MS) * YETI_WEATHER_TICKER_PX_PER_STEP) % travel;
  int16_t x = SCREEN_WIDTH - (int16_t)offset;

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("YETI SAYS");
  display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

  display.setTextSize(2);
  display.setCursor(x, 24);
  display.print(oledTickerText);

  display.setTextSize(1);
  display.setCursor(0, 56);
  display.print(clipText(phraseCategoryLabel(stringToPhraseCategory(lastSassCategory)), 21));
  display.display();
}

void updateOledOverlay() {
  if (!oledOverlayActive) {
    return;
  }

  unsigned long now = millis();
  if (now >= oledOverlayUntil) {
    stopOledOverlay();
    return;
  }

  if (oledOverlayMode == OLED_OVERLAY_CLOCK_SEQUENCE) {
    drawClockSequenceOverlay();
  } else if (oledOverlayMode == OLED_OVERLAY_WEATHER_TICKER) {
    drawWeatherTickerOverlay();
  } else if (oledOverlayMode == OLED_OVERLAY_SASS_TICKER) {
    drawSassTickerOverlay();
  }
}

void showClockCard() {
  ensureClockStarted();
  startOledOverlay(OLED_OVERLAY_CLOCK_SEQUENCE, YETI_CLOCK_SEQUENCE_DURATION_MS);
  drawClockSequenceOverlay();
  addEvent("OLED", "Displayed clock sequence");
  handleYetiEvent(EVENT_TIME_DISPLAYED);
}

void showWeatherCard() {
  if (weatherEnabled && !weatherAvailable) {
    fetchWeatherNow(true);
  }

  String title = "Yeti Weather";
  String loc = weatherLocationName.length() ? weatherLocationName : String(weatherLatitude, 2) + "," + String(weatherLongitude, 2);

  if (!weatherEnabled) {
    showTemporaryOledStatus(title, "Weather disabled", "Enable in WebUI", "", "", "", YETI_INFO_CARD_DURATION_MS);
  } else if (!weatherAvailable) {
    showTemporaryOledStatus(title, "Weather unavailable", weatherLastError, loc, "", "", YETI_INFO_CARD_DURATION_MS);
  } else {
    oledTickerText = weatherTickerLine();
    oledTickerTextWidth = oledTextWidth(oledTickerText, 2);
    startOledOverlay(OLED_OVERLAY_WEATHER_TICKER, weatherTickerDurationFor(oledTickerText));
    drawWeatherTickerOverlay();
  }

  addEvent("OLED", weatherAvailable ? "Displayed weather ticker" : "Displayed weather status");
  handleYetiEvent(weatherAvailable ? EVENT_WEATHER_DISPLAYED : EVENT_WEATHER_FAILED);
}

void updateInfoCards() {
  if (!infoCardsEnabled || setupMode || oledOverlayActive || sleepModeActive) {
    return;
  }

  bool canShowClock = infoCardClockEnabled && clockEnabled;
  bool canShowWeather = infoCardWeatherEnabled && weatherEnabled;

  if (!canShowClock && !canShowWeather) {
    return;
  }

  unsigned long now = millis();
  if (nextInfoCardAt == 0) {
    nextInfoCardAt = now + infoCardIntervalMs;
    return;
  }

  if (now < nextInfoCardAt) {
    return;
  }

  nextInfoCardAt = now + infoCardIntervalMs;

  if (canShowClock && canShowWeather) {
    if (nextInfoCardClock) {
      showClockCard();
    } else {
      showWeatherCard();
    }
    nextInfoCardClock = !nextInfoCardClock;
    return;
  }

  if (canShowClock) {
    showClockCard();
  } else {
    showWeatherCard();
  }
}

String wifiStatusName(wl_status_t status) {
  switch (status) {
    case WL_IDLE_STATUS: return "Idle";
    case WL_NO_SSID_AVAIL: return "SSID unavailable";
    case WL_SCAN_COMPLETED: return "Scan completed";
    case WL_CONNECTED: return "Connected";
    case WL_CONNECT_FAILED: return "Connect failed";
    case WL_CONNECTION_LOST: return "Connection lost";
    case WL_DISCONNECTED: return "Disconnected";
    default: return "Unknown";
  }
}

void printHardwareConfig() {
  Serial.println();
  Serial.println("Hardware profile:");
  Serial.print("  Board: ");
  Serial.println(YETI_BOARD_PROFILE_NAME);
  Serial.print("  I2C SDA: GPIO");
  Serial.println(YETI_I2C_SDA_PIN);
  Serial.print("  I2C SCL: GPIO");
  Serial.println(YETI_I2C_SCL_PIN);
  Serial.print("  I2C clock: ");
  Serial.print(YETI_I2C_CLOCK_HZ);
  Serial.println(" Hz");

#if YETI_OLED_ENABLED
  Serial.print("  OLED: enabled at 0x");
  Serial.println(YETI_OLED_I2C_ADDRESS, HEX);
#else
  Serial.println("  OLED: disabled");
#endif

#if YETI_MPU_ENABLED
  Serial.print("  MPU: enabled at 0x");
  Serial.println(YETI_MPU_I2C_ADDRESS, HEX);
#else
  Serial.println("  MPU: disabled");
#endif

#if YETI_TOUCH_ENABLED
  Serial.print("  Touch: ");
  Serial.println(YETI_TOUCH_PIN_LABEL);
#else
  Serial.print("  Touch: ");
  Serial.println(YETI_TOUCH_PIN_LABEL);
#endif
}

String clipText(String value, int maxChars) {
  if (value.length() <= maxChars) {
    return value;
  }

  if (maxChars <= 3) {
    return value.substring(0, maxChars);
  }

  return value.substring(0, maxChars - 3) + "...";
}

String hexAddress(byte address) {
  char buf[6];
  snprintf(buf, sizeof(buf), "0x%02X", address);
  return String(buf);
}

String chipSuffix() {
  uint64_t id = ESP.getEfuseMac();
  uint32_t low = (uint32_t)id;

  char buf[7];
  snprintf(buf, sizeof(buf), "%06X", (unsigned int)(low & 0xFFFFFF));
  return String(buf);
}

String bytesToHuman(size_t bytes) {
  if (bytes < 1024) {
    return String(bytes) + " B";
  } else if (bytes < 1024 * 1024) {
    return String(bytes / 1024.0, 2) + " KB";
  } else {
    return String(bytes / 1024.0 / 1024.0, 2) + " MB";
  }
}

String uptimeText() {
  unsigned long totalSeconds = millis() / 1000;
  unsigned long days = totalSeconds / 86400;
  unsigned long hours = (totalSeconds % 86400) / 3600;
  unsigned long minutes = (totalSeconds % 3600) / 60;
  unsigned long seconds = totalSeconds % 60;

  if (days > 0) {
    return String(days) + "d " + String(hours) + "h " + String(minutes) + "m";
  }

  if (hours > 0) {
    return String(hours) + "h " + String(minutes) + "m " + String(seconds) + "s";
  }

  if (minutes > 0) {
    return String(minutes) + "m " + String(seconds) + "s";
  }

  return String(seconds) + "s";
}

String expressionName(Expression expression) {
  switch (expression) {
    case FACE_NORMAL: return "Normal";
    case FACE_ANGRY: return "Angry";
    case FACE_SLEEPY: return "Sleepy";
    case FACE_HAPPY: return "Happy";
    case FACE_SURPRISED: return "Surprised";
    case FACE_SHOCKED: return "Shocked";
    case FACE_BORED: return "Bored";
    default: return "Unknown";
  }
}

Expression expressionFromName(String name) {
  name.toLowerCase();

  if (name == "normal") return FACE_NORMAL;
  if (name == "angry") return FACE_ANGRY;
  if (name == "sleepy") return FACE_SLEEPY;
  if (name == "happy") return FACE_HAPPY;
  if (name == "surprised") return FACE_SURPRISED;
  if (name == "shocked") return FACE_SHOCKED;
  if (name == "bored") return FACE_BORED;

  return FACE_NORMAL;
}

const char* moodToString(YetiMood mood) {
  switch (mood) {
    case MOOD_DEADPAN: return "deadpan";
    case MOOD_HAPPY: return "happy";
    case MOOD_ANNOYED: return "annoyed";
    case MOOD_ANGRY: return "angry";
    case MOOD_SLEEPY: return "sleepy";
    case MOOD_CURIOUS: return "curious";
    case MOOD_STARTLED: return "startled";
    case MOOD_SMUG: return "smug";
    case MOOD_SAD: return "sad";
    case MOOD_WEATHER: return "weather";
    case MOOD_BOOTING: return "booting";
    default: return "deadpan";
  }
}

String moodLabel(YetiMood mood) {
  String label = moodToString(mood);
  if (label.length() > 0) {
    label.setCharAt(0, toupper(label.charAt(0)));
  }
  return label;
}

YetiMood stringToMood(String moodName) {
  moodName.trim();
  moodName.toLowerCase();

  if (moodName == "deadpan" || moodName == "normal" || moodName == "default") return MOOD_DEADPAN;
  if (moodName == "happy") return MOOD_HAPPY;
  if (moodName == "annoyed") return MOOD_ANNOYED;
  if (moodName == "angry") return MOOD_ANGRY;
  if (moodName == "sleepy" || moodName == "tired") return MOOD_SLEEPY;
  if (moodName == "curious" || moodName == "surprised") return MOOD_CURIOUS;
  if (moodName == "startled" || moodName == "shocked") return MOOD_STARTLED;
  if (moodName == "smug") return MOOD_SMUG;
  if (moodName == "sad") return MOOD_SAD;
  if (moodName == "weather") return MOOD_WEATHER;
  if (moodName == "booting" || moodName == "boot") return MOOD_BOOTING;

  return MOOD_DEADPAN;
}

Expression expressionForMood(YetiMood mood) {
  switch (mood) {
    case MOOD_HAPPY: return FACE_HAPPY;
    case MOOD_ANNOYED: return FACE_ANGRY;
    case MOOD_ANGRY: return FACE_ANGRY;
    case MOOD_SLEEPY: return FACE_SLEEPY;
    case MOOD_CURIOUS: return FACE_SURPRISED;
    case MOOD_STARTLED: return FACE_SHOCKED;
    case MOOD_SMUG: return FACE_HAPPY;
    case MOOD_SAD: return FACE_SLEEPY;
    case MOOD_WEATHER: return FACE_SURPRISED;
    case MOOD_BOOTING: return FACE_SHOCKED;
    case MOOD_DEADPAN:
    default: return FACE_NORMAL;
  }
}

YetiMood moodFromExpression(Expression expression) {
  switch (expression) {
    case FACE_ANGRY: return MOOD_ANGRY;
    case FACE_SLEEPY: return MOOD_SLEEPY;
    case FACE_HAPPY: return MOOD_HAPPY;
    case FACE_SURPRISED: return MOOD_CURIOUS;
    case FACE_SHOCKED: return MOOD_STARTLED;
    case FACE_BORED: return MOOD_SLEEPY;
    case FACE_NORMAL:
    default: return MOOD_DEADPAN;
  }
}

int sanitizePersonalityTrait(int value, int fallback = 50) {
  if (value < 0 || value > 100) {
    return fallback;
  }
  return value;
}

const YetiPersonalityPreset* findPersonalityPreset(String presetId) {
  presetId.trim();
  presetId.toLowerCase();

  for (uint8_t i = 0; i < PERSONALITY_PRESET_COUNT; i++) {
    if (presetId == PERSONALITY_PRESETS[i].id) {
      return &PERSONALITY_PRESETS[i];
    }
  }

  return nullptr;
}

bool personalityMatchesPreset(const YetiPersonalityPreset &preset) {
  return baseMood == preset.baseMood &&
         personality.grumpiness == preset.traits.grumpiness &&
         personality.curiosity == preset.traits.curiosity &&
         personality.sleepiness == preset.traits.sleepiness &&
         personality.chaos == preset.traits.chaos &&
         personality.friendliness == preset.traits.friendliness;
}

String activePersonalityPresetId() {
  for (uint8_t i = 0; i < PERSONALITY_PRESET_COUNT; i++) {
    if (personalityMatchesPreset(PERSONALITY_PRESETS[i])) {
      return PERSONALITY_PRESETS[i].id;
    }
  }

  return "custom";
}

String activePersonalityPresetLabel() {
  for (uint8_t i = 0; i < PERSONALITY_PRESET_COUNT; i++) {
    if (personalityMatchesPreset(PERSONALITY_PRESETS[i])) {
      return PERSONALITY_PRESETS[i].name;
    }
  }

  return "Custom";
}

bool applyPersonalityPreset(String presetId, bool saveNow) {
  const YetiPersonalityPreset* preset = findPersonalityPreset(presetId);
  if (!preset) {
    return false;
  }

  personality = preset->traits;
  idleMoodEnabled = true;
  idleMoodCount = 0;
  lastIdleMood = preset->baseMood;
  lastIdleReason = String("Preset applied: ") + preset->name;
  setBaseMood(preset->baseMood);
  setMood(preset->baseMood, 4500, PRIORITY_MANUAL, true);
  scheduleNextIdleBehavior();

  if (saveNow) {
    saveBehaviorConfig();
  }

  addEvent("Mood", String("Personality preset applied: ") + preset->name);
  return true;
}

String moodPriorityLabel(MoodPriority priority) {
  switch (priority) {
    case PRIORITY_IDLE: return "idle";
    case PRIORITY_INFO: return "info";
    case PRIORITY_REACTION: return "reaction";
    case PRIORITY_ALERT: return "alert";
    case PRIORITY_MANUAL: return "manual";
    default: return "unknown";
  }
}

bool moodTemporaryActive() {
  return moodUntilMs > 0 && millis() < moodUntilMs;
}

const char* yetiEventName(YetiEvent event) {
  switch (event) {
    case EVENT_BOOT: return "boot";
    case EVENT_WIFI_CONNECTED: return "wifi_connected";
    case EVENT_WIFI_FAILED: return "wifi_failed";
    case EVENT_WIFI_WEAK: return "wifi_weak";
    case EVENT_WIFI_VERY_WEAK: return "wifi_very_weak";
    case EVENT_WIFI_RESTORED: return "wifi_restored";
    case EVENT_WEBUI_OPENED: return "webui_opened";
    case EVENT_WEBUI_COMMAND: return "webui_command";
    case EVENT_WEATHER_UPDATED: return "weather_updated";
    case EVENT_WEATHER_FAILED: return "weather_failed";
    case EVENT_TIME_DISPLAYED: return "time_displayed";
    case EVENT_WEATHER_DISPLAYED: return "weather_displayed";
    case EVENT_CUSTOM_MESSAGE: return "custom_message";
    case EVENT_POKED: return "poked";
    case EVENT_GYRO_MOVED: return "gyro_moved";
    case EVENT_GYRO_TILTED: return "gyro_tilted";
    case EVENT_GYRO_SHAKEN: return "gyro_shaken";
    case EVENT_GYRO_IDLE_LONG: return "gyro_idle_long";
    case EVENT_REBOOT_REQUESTED: return "reboot_requested";
    case EVENT_SETTINGS_SAVED: return "settings_saved";
    default: return "unknown";
  }
}

bool cooldownReady(unsigned long lastMs, unsigned long cooldownMs) {
  return lastMs == 0 || millis() - lastMs >= cooldownMs;
}

void recordReaction(YetiEvent event, String reason) {
  lastReactionEvent = yetiEventName(event);
  lastReactionReason = reason;
  reactionMoodCount++;
  addEvent("Mood", String("Trigger: ") + yetiEventName(event) + " -> " + reason);
}

uint8_t sanitizeSensitivity(int value, uint8_t fallback) {
  if (value < 0 || value > 100) {
    return fallback;
  }
  return (uint8_t)value;
}

float movementJerkThresholdG() {
  // Higher sensitivity lowers the needed acceleration delta.
  float threshold = 0.28f - ((float)movementSensitivity * 0.0018f);
  if (threshold < 0.08f) threshold = 0.08f;
  if (threshold > 0.32f) threshold = 0.32f;
  return threshold;
}

float movementTiltThresholdG() {
  // Vector delta from boot/resting baseline. About 0.25-0.40 is a noticeable pick-up/tilt.
  float threshold = 0.48f - ((float)movementSensitivity * 0.0022f);
  if (threshold < 0.22f) threshold = 0.22f;
  if (threshold > 0.52f) threshold = 0.52f;
  return threshold;
}

float movementShakeJerkThresholdG() {
  float threshold = 0.95f - ((float)shakeSensitivity * 0.0060f);
  if (threshold < 0.32f) threshold = 0.32f;
  if (threshold > 1.10f) threshold = 1.10f;
  return threshold;
}

float movementShakeMagThresholdG() {
  float threshold = 0.78f - ((float)shakeSensitivity * 0.0048f);
  if (threshold < 0.28f) threshold = 0.28f;
  if (threshold > 0.85f) threshold = 0.85f;
  return threshold;
}

bool gyroMoodReady(unsigned long lastMs, unsigned long cooldownMs) {
  return movementMoodEnabled && mpuReady && cooldownReady(lastMs, cooldownMs);
}

void recordMovementReaction(YetiEvent event, String reason) {
  lastMovementEvent = yetiEventName(event);
  lastMovementReason = reason;
  movementMoodCount++;
  recordReaction(event, reason);
}

const char* sequenceToString(YetiSequence seq) {
  switch (seq) {
    case SEQ_NONE: return "none";
    case SEQ_POKE_REACTION: return "poke_reaction";
    case SEQ_BOOT_DRAMA: return "boot_drama";
    case SEQ_BAD_WIFI_TANTRUM: return "bad_wifi_tantrum";
    case SEQ_WEATHER_HAPPY_COLD: return "weather_happy_cold";
    case SEQ_WEATHER_RAIN_SAD: return "weather_rain_sad";
    case SEQ_IDLE_BOREDOM: return "idle_boredom";
    case SEQ_SETTINGS_SAVED: return "settings_saved";
    default: return "unknown";
  }
}

String sequenceLabel(YetiSequence seq) {
  switch (seq) {
    case SEQ_NONE: return "None";
    case SEQ_POKE_REACTION: return "Poke Reaction";
    case SEQ_BOOT_DRAMA: return "Boot Drama";
    case SEQ_BAD_WIFI_TANTRUM: return "Bad Wi-Fi Tantrum";
    case SEQ_WEATHER_HAPPY_COLD: return "Weather Happy/Cold";
    case SEQ_WEATHER_RAIN_SAD: return "Weather Rain Sad";
    case SEQ_IDLE_BOREDOM: return "Idle Boredom";
    case SEQ_SETTINGS_SAVED: return "Settings Saved";
    default: return "Unknown";
  }
}

YetiSequence stringToSequence(String seqName) {
  seqName.trim();
  seqName.toLowerCase();
  seqName.replace(" ", "_");
  seqName.replace("-", "_");

  if (seqName == "poke" || seqName == "poke_reaction") return SEQ_POKE_REACTION;
  if (seqName == "boot" || seqName == "boot_drama") return SEQ_BOOT_DRAMA;
  if (seqName == "wifi" || seqName == "bad_wifi" || seqName == "bad_wifi_tantrum") return SEQ_BAD_WIFI_TANTRUM;
  if (seqName == "cold" || seqName == "weather_cold" || seqName == "weather_happy_cold") return SEQ_WEATHER_HAPPY_COLD;
  if (seqName == "rain" || seqName == "weather_rain" || seqName == "weather_rain_sad") return SEQ_WEATHER_RAIN_SAD;
  if (seqName == "bored" || seqName == "idle" || seqName == "idle_boredom") return SEQ_IDLE_BOREDOM;
  if (seqName == "saved" || seqName == "settings" || seqName == "settings_saved") return SEQ_SETTINGS_SAVED;
  return SEQ_NONE;
}

bool sequenceActive() {
  return activeSequence != SEQ_NONE;
}

bool sequenceStepMood(YetiMood mood, unsigned long durationMs, MoodPriority priority) {
  MoodPriority effectivePriority = priority;
  if (sequenceActive() && activeSequencePriority > effectivePriority) {
    effectivePriority = activeSequencePriority;
  }

  applyingSequenceMood = true;
  bool ok = setMood(mood, durationMs, effectivePriority, false);
  applyingSequenceMood = false;
  return ok;
}

bool startSequence(YetiSequence seq, String reason, MoodPriority priority) {
  if (seq == SEQ_NONE) {
    return false;
  }

  if (moodTemporaryActive() && priority < currentMoodPriority) {
    addEvent("Sequence", String("Blocked lower-priority sequence: ") + sequenceLabel(seq));
    return false;
  }

  if (sequenceActive()) {
    addEvent("Sequence", String("Superseded: ") + sequenceLabel(activeSequence) + " -> " + sequenceLabel(seq));
  }

  activeSequence = seq;
  lastSequence = seq;
  sequenceStep = 0;
  sequencePausedForOverlay = false;
  sequenceStartedMs = millis();
  sequenceStepStartedMs = sequenceStartedMs;
  nextSequenceStepAtMs = sequenceStartedMs;
  activeSequencePriority = priority;
  lastSequenceReason = reason.length() ? reason : String("manual");
  sequenceRunCount++;
  if (memoryEnabled) {
    memoryStats.lifetimeSequences++;
    markMemoryDirty();
  }
  pokeStagePending = false;

  addEvent("Sequence", String("Started: ") + sequenceLabel(seq) + " (" + lastSequenceReason + ")");
  updateSequence();
  return true;
}

void stopSequence(bool returnToBase) {
  if (!sequenceActive()) {
    if (returnToBase) {
      setMood(baseMood, 0, PRIORITY_MANUAL, true);
      currentMoodPriority = PRIORITY_IDLE;
    }
    return;
  }

  String stopped = sequenceLabel(activeSequence);
  activeSequence = SEQ_NONE;
  sequenceStep = 0;
  nextSequenceStepAtMs = 0;
  sequencePausedForOverlay = false;
  addEvent("Sequence", String("Stopped: ") + stopped);

  if (returnToBase) {
    sequenceStepMood(baseMood, 0, activeSequencePriority);
    currentMoodPriority = PRIORITY_IDLE;
  }
}

void finishSequence(bool returnToBase) {
  if (!sequenceActive()) {
    return;
  }

  String finished = sequenceLabel(activeSequence);
  MoodPriority finishPriority = activeSequencePriority;
  activeSequence = SEQ_NONE;
  sequenceStep = 0;
  nextSequenceStepAtMs = 0;
  sequencePausedForOverlay = false;
  addEvent("Sequence", String("Finished: ") + finished);

  if (returnToBase) {
    applyingSequenceMood = true;
    setMood(baseMood, 0, finishPriority, false);
    applyingSequenceMood = false;
    currentMoodPriority = PRIORITY_IDLE;
  }
}

void updateSequence() {
  if (!sequenceActive()) {
    return;
  }

  unsigned long now = millis();
  if (nextSequenceStepAtMs > 0 && now < nextSequenceStepAtMs) {
    return;
  }

  // v1.6.0 display/mood separation: low-priority scripted acting waits
  // until readable OLED overlays finish. The mood can still exist internally,
  // but text/ticker modes stay readable and the sequence resumes on the face.
  if (oledOverlayActive && activeSequencePriority <= PRIORITY_INFO) {
    if (!sequencePausedForOverlay) {
      sequencePausedForOverlay = true;
      addEvent("Sequence", String("Paused for OLED overlay: ") + sequenceLabel(activeSequence));
    }
    return;
  }

  if (sequencePausedForOverlay) {
    sequencePausedForOverlay = false;
    addEvent("Sequence", String("Resumed after OLED overlay: ") + sequenceLabel(activeSequence));
  }

  sequenceStepStartedMs = now;

  switch (activeSequence) {
    case SEQ_POKE_REACTION:
      if (sequenceStep == 0) {
        unsigned long d = biasDurationWithGrudge(2500UL, EVENT_POKED);
        if (grudges.pokeGrudge >= 70) d = 1800UL;
        sequenceStepMood(MOOD_STARTLED, d, PRIORITY_ALERT);
        sequenceStep = 1;
        nextSequenceStepAtMs = now + d;
      } else if (sequenceStep == 1) {
        YetiMood pokeMood = biasMoodWithGrudge(MOOD_ANNOYED, EVENT_POKED);
        unsigned long d = biasDurationWithGrudge(6000UL, EVENT_POKED);
        sequenceStepMood(pokeMood, d, PRIORITY_REACTION);
        sequenceStep = 2;
        nextSequenceStepAtMs = now + d;
      } else if (sequenceStep == 2 && grudges.pokeGrudge >= 70) {
        sequenceStepMood(MOOD_SMUG, 3500UL, PRIORITY_REACTION);
        sequenceStep = 3;
        nextSequenceStepAtMs = now + 3500UL;
      } else {
        finishSequence(true);
      }
      break;

    case SEQ_BOOT_DRAMA:
      if (sequenceStep == 0) {
        sequenceStepMood(MOOD_BOOTING, 3000, PRIORITY_ALERT);
        sequenceStep = 1;
        nextSequenceStepAtMs = now + 3000UL;
      } else if (sequenceStep == 1) {
        sequenceStepMood(MOOD_CURIOUS, 2600, PRIORITY_REACTION);
        sequenceStep = 2;
        nextSequenceStepAtMs = now + 2600UL;
      } else if (sequenceStep == 2) {
        sequenceStepMood(MOOD_DEADPAN, 1800, PRIORITY_REACTION);
        sequenceStep = 3;
        nextSequenceStepAtMs = now + 1800UL;
      } else {
        finishSequence(true);
      }
      break;

    case SEQ_BAD_WIFI_TANTRUM:
      if (sequenceStep == 0) {
        sequenceStepMood(biasMoodWithGrudge(MOOD_CURIOUS, EVENT_WIFI_WEAK), 1800, PRIORITY_ALERT);
        sequenceStep = 1;
        nextSequenceStepAtMs = now + 1800UL;
      } else if (sequenceStep == 1) {
        unsigned long d = biasDurationWithGrudge(4500UL, EVENT_WIFI_WEAK);
        sequenceStepMood(biasMoodWithGrudge(MOOD_ANNOYED, EVENT_WIFI_WEAK), d, PRIORITY_ALERT);
        sequenceStep = 2;
        nextSequenceStepAtMs = now + d;
      } else if (sequenceStep == 2) {
        int rssi = WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : -99;
        YetiMood finalMood = (rssi <= -80 || grudges.wifiGrudge >= 65) ? MOOD_ANGRY : MOOD_SMUG;
        unsigned long durationMs = biasDurationWithGrudge(rssi <= -80 ? 6500UL : 3200UL, EVENT_WIFI_VERY_WEAK);
        sequenceStepMood(finalMood, durationMs, PRIORITY_ALERT);
        sequenceStep = 3;
        nextSequenceStepAtMs = now + durationMs;
      } else {
        finishSequence(true);
      }
      break;

    case SEQ_WEATHER_HAPPY_COLD:
      if (sequenceStep == 0) {
        YetiMood firstMood = weightedTraitValue(personality.friendliness) > 45 ? MOOD_HAPPY : MOOD_SMUG;
        sequenceStepMood(firstMood, 4500, PRIORITY_INFO);
        sequenceStep = 1;
        nextSequenceStepAtMs = now + 4500UL;
      } else if (sequenceStep == 1) {
        sequenceStepMood(MOOD_SMUG, 3200, PRIORITY_INFO);
        sequenceStep = 2;
        nextSequenceStepAtMs = now + 3200UL;
      } else {
        finishSequence(true);
      }
      break;

    case SEQ_WEATHER_RAIN_SAD:
      if (sequenceStep == 0) {
        YetiMood firstMood = weightedTraitValue(personality.grumpiness) > 60 ? MOOD_ANNOYED : MOOD_SAD;
        sequenceStepMood(firstMood, 5000, PRIORITY_INFO);
        sequenceStep = 1;
        nextSequenceStepAtMs = now + 5000UL;
      } else if (sequenceStep == 1) {
        sequenceStepMood(MOOD_SLEEPY, 3200, PRIORITY_INFO);
        sequenceStep = 2;
        nextSequenceStepAtMs = now + 3200UL;
      } else {
        finishSequence(true);
      }
      break;

    case SEQ_IDLE_BOREDOM:
      if (sequenceStep == 0) {
        YetiMood firstMood = weightedTraitValue(personality.sleepiness) > 60 ? MOOD_SLEEPY : MOOD_SMUG;
        sequenceStepMood(firstMood, 5200, PRIORITY_IDLE);
        sequenceStep = 1;
        nextSequenceStepAtMs = now + 5200UL;
      } else if (sequenceStep == 1) {
        YetiMood secondMood = weightedTraitValue(personality.grumpiness) > 60 ? MOOD_ANNOYED : MOOD_CURIOUS;
        sequenceStepMood(secondMood, 3600, PRIORITY_IDLE);
        sequenceStep = 2;
        nextSequenceStepAtMs = now + 3600UL;
      } else {
        finishSequence(true);
      }
      break;

    case SEQ_SETTINGS_SAVED:
      if (sequenceStep == 0) {
        sequenceStepMood(MOOD_HAPPY, 2500, PRIORITY_REACTION);
        sequenceStep = 1;
        nextSequenceStepAtMs = now + 2500UL;
      } else if (sequenceStep == 1) {
        sequenceStepMood(MOOD_SMUG, 3000, PRIORITY_REACTION);
        sequenceStep = 2;
        nextSequenceStepAtMs = now + 3000UL;
      } else {
        finishSequence(true);
      }
      break;

    case SEQ_NONE:
    default:
      finishSequence(false);
      break;
  }
}

bool weatherCodeIsRain(int code) {
  return code == 51 || code == 53 || code == 55 ||
         code == 56 || code == 57 ||
         code == 61 || code == 63 || code == 65 ||
         code == 66 || code == 67 ||
         code == 80 || code == 81 || code == 82;
}

bool weatherCodeIsSnow(int code) {
  return code == 71 || code == 73 || code == 75 ||
         code == 77 || code == 85 || code == 86;
}

bool weatherCodeIsStorm(int code) {
  return code == 95 || code == 96 || code == 99;
}

void reactToCurrentWeather(String triggerReason) {
  if (!weatherMoodEnabled || !weatherAvailable || setupMode) {
    return;
  }

  if (!cooldownReady(lastWeatherMoodMs, 60000UL)) {
    return;
  }

  YetiMood mood = MOOD_WEATHER;
  YetiSequence sequence = SEQ_NONE;
  unsigned long durationMs = 6500;
  String reason = triggerReason + ": " + weatherSummaryLine();

  int grumpy = weightedTraitValue(personality.grumpiness);
  int friendly = weightedTraitValue(personality.friendliness);

  float tempF = weatherMetric ? (weatherTemperature * 9.0f / 5.0f + 32.0f) : weatherTemperature;

  if (weatherCodeIsStorm(weatherCode)) {
    mood = grumpy > 65 ? MOOD_ANNOYED : MOOD_STARTLED;
    durationMs = 8000;
    reason += " · storm gremlin response";
  } else if (weatherCodeIsSnow(weatherCode)) {
    mood = friendly > 50 ? MOOD_HAPPY : MOOD_SMUG;
    sequence = SEQ_WEATHER_HAPPY_COLD;
    durationMs = 9500;
    reason += " · snow/cold delight";
  } else if (weatherCodeIsRain(weatherCode)) {
    mood = grumpy > 60 ? MOOD_ANNOYED : MOOD_SAD;
    sequence = SEQ_WEATHER_RAIN_SAD;
    durationMs = 8000;
    reason += " · rain detected";
  } else if (tempF >= 85.0f) {
    mood = grumpy > 70 ? MOOD_ANGRY : MOOD_ANNOYED;
    durationMs = 8500;
    reason += " · hot weather complaint";
  } else if (tempF <= 40.0f) {
    mood = friendly > 40 ? MOOD_HAPPY : MOOD_SMUG;
    sequence = SEQ_WEATHER_HAPPY_COLD;
    durationMs = 8500;
    reason += " · cold weather approved";
  } else if (weatherWindSpeed >= (weatherMetric ? 40.0f : 25.0f)) {
    mood = MOOD_CURIOUS;
    durationMs = 7000;
    reason += " · windy enough to investigate";
  }

  mood = biasMoodWithGrudge(mood, EVENT_WEATHER_UPDATED);
  durationMs = biasDurationWithGrudge(durationMs, EVENT_WEATHER_UPDATED);

  lastWeatherMoodMs = millis();
  if (sequence != SEQ_NONE) {
    if (startSequence(sequence, reason, PRIORITY_INFO)) {
      recordReaction(EVENT_WEATHER_UPDATED, reason);
    }
    return;
  }

  if (setMood(mood, durationMs, PRIORITY_INFO, false)) {
    recordReaction(EVENT_WEATHER_UPDATED, reason);
  }
}

void handleYetiEvent(YetiEvent event) {
  recordMemoryEvent(event);

  if (setupMode && event != EVENT_BOOT && event != EVENT_WIFI_FAILED) {
    return;
  }

  unsigned long now = millis();

  switch (event) {
    case EVENT_BOOT: {
      YetiSequence bootSeq = bootSequenceFromDailyMemory();
      String reason = getYesterdayBootReason();
      startSequence(bootSeq, reason, PRIORITY_ALERT);
      recordReaction(event, String("boot sequence initialized: ") + reason);
      break;
    }

    case EVENT_WIFI_CONNECTED:
    case EVENT_WIFI_RESTORED:
      if (wifiMoodEnabled && cooldownReady(lastWifiMoodMs, 30000UL)) {
        lastWifiMoodMs = now;
        if (setMood(MOOD_HAPPY, 4000, PRIORITY_REACTION, false)) {
          recordReaction(event, String("network online, RSSI ") + String(WiFi.RSSI()) + " dBm");
        }
      }
      break;

    case EVENT_WIFI_FAILED:
      if (wifiMoodEnabled && cooldownReady(lastWifiMoodMs, 30000UL)) {
        lastWifiMoodMs = now;
        if (startSequence(SEQ_BAD_WIFI_TANTRUM, "saved Wi-Fi connection failed", PRIORITY_ALERT)) {
          recordReaction(event, "bad Wi-Fi tantrum sequence started");
        }
      }
      break;

    case EVENT_WIFI_WEAK:
    case EVENT_WIFI_VERY_WEAK:
      if (wifiMoodEnabled && cooldownReady(lastWifiMoodMs, 30000UL)) {
        int rssi = WiFi.RSSI();
        lastWifiMoodMs = now;
        if (startSequence(SEQ_BAD_WIFI_TANTRUM, String("weak Wi-Fi RSSI ") + String(rssi) + " dBm", PRIORITY_ALERT)) {
          recordReaction(event, String("bad Wi-Fi tantrum sequence started, RSSI ") + String(rssi) + " dBm");
        }
      }
      break;

    case EVENT_WEBUI_OPENED:
      markInteraction();
      if (webMoodEnabled && cooldownReady(lastWebMoodMs, 10000UL)) {
        lastWebMoodMs = now;
        if (setMood(MOOD_CURIOUS, 4500, PRIORITY_REACTION, false)) {
          recordReaction(event, "dashboard opened");
        }
      }
      break;

    case EVENT_WEBUI_COMMAND:
      markInteraction();
      if (webMoodEnabled && cooldownReady(lastWebMoodMs, 10000UL)) {
        lastWebMoodMs = now;
        if (setMood(MOOD_SMUG, 3000, PRIORITY_REACTION, false)) {
          recordReaction(event, "WebUI command received");
        }
      }
      break;

    case EVENT_WEATHER_UPDATED:
      reactToCurrentWeather("weather updated");
      break;

    case EVENT_WEATHER_FAILED:
      if (weatherMoodEnabled && cooldownReady(lastWeatherMoodMs, 60000UL)) {
        lastWeatherMoodMs = now;
        YetiMood mood = biasMoodWithGrudge(MOOD_ANNOYED, event);
        unsigned long d = biasDurationWithGrudge(8000UL, event);
        if (setMood(mood, d, PRIORITY_INFO, false)) {
          recordReaction(event, weatherLastError.length() ? weatherLastError : String("weather fetch failed"));
        }
      }
      break;

    case EVENT_TIME_DISPLAYED:
      setMood(MOOD_CURIOUS, 3500, PRIORITY_INFO, false);
      recordReaction(event, "time ticker displayed");
      break;

    case EVENT_WEATHER_DISPLAYED:
      setMood(MOOD_WEATHER, 5000, PRIORITY_INFO, false);
      recordReaction(event, "weather ticker displayed");
      break;

    case EVENT_CUSTOM_MESSAGE:
      setMood(MOOD_CURIOUS, 4000, PRIORITY_INFO, false);
      recordReaction(event, "custom message displayed");
      break;

    case EVENT_POKED:
      // Poke uses triggerPokeYeti() / sequence engine; memory records it separately.
      break;

    case EVENT_GYRO_MOVED:
      if (gyroMoodReady(lastGyroMoveMoodMs, 5000UL)) {
        lastGyroMoveMoodMs = now;
        lastGyroMoodMs = now;
        YetiMood mood = weightedTraitValue(personality.curiosity) >= 55 ? MOOD_CURIOUS : MOOD_STARTLED;
        unsigned long durationMs = weightedTraitValue(personality.chaos) > 70 ? 3200UL : 4200UL;
        if (setMood(mood, durationMs, PRIORITY_REACTION, false)) {
          recordMovementReaction(event, String("movement jerk ") + String(lastMotionJerk, 2) + "g");
        }
      }
      break;

    case EVENT_GYRO_TILTED:
      if (gyroMoodReady(lastGyroMoodMs, 8000UL)) {
        lastGyroMoodMs = now;
        YetiMood mood = weightedTraitValue(personality.grumpiness) > 70 ? MOOD_ANNOYED : MOOD_STARTLED;
        mood = biasMoodWithGrudge(mood, event);
        unsigned long d = biasDurationWithGrudge(5200UL, event);
        if (setMood(mood, d, PRIORITY_REACTION, false)) {
          recordMovementReaction(event, String("tilt delta ") + String(lastTiltDelta, 2) + "g from baseline");
        }
      }
      break;

    case EVENT_GYRO_SHAKEN:
      if (gyroMoodReady(lastGyroShakeMoodMs, 4500UL)) {
        lastGyroShakeMoodMs = now;
        lastGyroMoodMs = now;
        int grumpy = weightedTraitValue(personality.grumpiness);
        int friendly = weightedTraitValue(personality.friendliness);
        YetiMood mood = friendly > 75 && grumpy < 45 ? MOOD_STARTLED : MOOD_ANGRY;
        unsigned long durationMs = 8500UL + (unsigned long)grumpy * 25UL;
        mood = biasMoodWithGrudge(mood, event);
        durationMs = biasDurationWithGrudge(durationMs, event);
        if (setMood(mood, durationMs, PRIORITY_ALERT, false)) {
          recordMovementReaction(event, String("shake score ") + String(lastShakeScore, 2) + "g");
        }
      }
      break;

    case EVENT_GYRO_IDLE_LONG:
      if (gyroMoodReady(lastGyroIdleMoodMs, 120000UL) && faceDisplayAvailable()) {
        lastGyroIdleMoodMs = now;
        lastGyroMoodMs = now;
        if (setMood(MOOD_SLEEPY, 11000, PRIORITY_IDLE, false)) {
          recordMovementReaction(event, "no movement for a while, getting sleepy");
        }
      }
      break;

    case EVENT_SETTINGS_SAVED:
      if (startSequence(SEQ_SETTINGS_SAVED, "settings saved", PRIORITY_REACTION)) {
        recordReaction(event, "settings-saved flourish sequence started");
      }
      break;

    case EVENT_REBOOT_REQUESTED:
      setMood(biasMoodWithGrudge(MOOD_BOOTING, event), biasDurationWithGrudge(2500UL, event), PRIORITY_ALERT, false);
      recordReaction(event, String("reboot requested · ") + getGrudgeSummary());
      break;
  }

  YetiPhraseCategory phraseCategory = phraseCategoryForEvent(event);
  if (phraseCategory != PHRASE_NONE) {
    maybeShowMemoryPhrase(phraseCategory, false);
  }
}

void updateWifiMoodReactions(bool force) {
  if (!wifiMoodEnabled || setupMode) {
    return;
  }

  unsigned long now = millis();
  if (!force && lastWifiMoodCheckMs > 0 && now - lastWifiMoodCheckMs < 15000UL) {
    return;
  }
  lastWifiMoodCheckMs = now;

  bool connected = WiFi.status() == WL_CONNECTED;

  if (connected && !lastWifiConnectedState) {
    lastWifiConnectedState = true;
    lastWifiRssi = WiFi.RSSI();
    handleYetiEvent(EVENT_WIFI_RESTORED);
    return;
  }

  if (!connected) {
    lastWifiConnectedState = false;
    return;
  }

  int rssi = WiFi.RSSI();
  bool wasWeak = lastWifiRssi <= -70 && lastWifiRssi != 0;
  lastWifiRssi = rssi;

  if (rssi <= -80) {
    handleYetiEvent(EVENT_WIFI_VERY_WEAK);
  } else if (rssi <= -70) {
    handleYetiEvent(EVENT_WIFI_WEAK);
  } else if (force || (wasWeak && rssi > -65 && cooldownReady(lastWifiMoodMs, 30000UL))) {
    handleYetiEvent(EVENT_WIFI_RESTORED);
  }
}

bool faceDisplayAvailable() {
  return oledReady && !setupMode && !oledOverlayActive && !demoMode && !sleepModeActive;
}

const char* oledOverlayModeName(OledOverlayMode mode) {
  switch (mode) {
    case OLED_OVERLAY_NONE: return "none";
    case OLED_OVERLAY_STATUS: return "status";
    case OLED_OVERLAY_CLOCK_SEQUENCE: return "clock_sequence";
    case OLED_OVERLAY_WEATHER_TICKER: return "weather_ticker";
    case OLED_OVERLAY_SASS_TICKER: return "sass_ticker";
    default: return "unknown";
  }
}

String oledOverlayModeLabel(OledOverlayMode mode) {
  switch (mode) {
    case OLED_OVERLAY_NONE: return "None";
    case OLED_OVERLAY_STATUS: return "Status Card";
    case OLED_OVERLAY_CLOCK_SEQUENCE: return "Clock / Date Ticker";
    case OLED_OVERLAY_WEATHER_TICKER: return "Weather Ticker";
    case OLED_OVERLAY_SASS_TICKER: return "Sass Ticker";
    default: return "Unknown";
  }
}

String displayModeLabel() {
  if (setupMode) return "Setup Portal";
  if (!oledReady) return "OLED Unavailable";
  if (sleepModeActive) return String("Sleep Mode / ") + sleepStatusLabel();
  if (oledOverlayActive) return oledOverlayModeLabel(oledOverlayMode);
  if (demoMode) return "Demo Face";
  return "Face";
}

String moodAutomationSummary() {
  String summary = "";
  if (idleMoodEnabled) summary += "Idle ";
  if (weatherMoodEnabled) summary += "Weather ";
  if (wifiMoodEnabled) summary += "Wi-Fi ";
  if (webMoodEnabled) summary += "WebUI ";
  if (movementMoodEnabled && mpuReady) summary += "Movement ";
  if (movementMoodEnabled && !mpuReady) summary += "Movement(waiting) ";
  if (sassTickerEnabled) summary += "Sass ";
  if (sleepModeEnabled) summary += "Sleep ";
  summary.trim();
  return summary.length() ? summary : String("Manual only");
}

String moodSafetySummary() {
  if (setupMode) return "Setup portal guard active";
  if (!oledReady) return "OLED missing guard active";
  if (sleepModeActive) return String("Sleep guard: ") + sleepStatusLabel();
  if (oledOverlayActive) return String("OLED overlay guard: ") + oledOverlayModeLabel(oledOverlayMode);
  if (sequencePausedForOverlay) return "Sequence overlay pause armed";
  if (sensorOverridePriority > 0 && millis() < sensorOverrideUntil) return "Sensor override guard active";
  return "Normal face mode";
}

String faceThemeName(FaceTheme theme) {
  switch (theme) {
    case FACE_THEME_CLASSIC: return "Classic";
    case FACE_THEME_INVERTED: return "Inverted";
    case FACE_THEME_OUTLINE: return "Outline";
    case FACE_THEME_TWO_ZONE: return "Two-zone";
    default: return "Classic";
  }
}

FaceTheme faceThemeFromId(int id) {
  if (id < 0 || id >= FACE_THEME_COUNT) {
    return FACE_THEME_CLASSIC;
  }

  return (FaceTheme)id;
}

uint16_t faceBgColor() {
  return faceTheme == FACE_THEME_INVERTED ? SSD1306_WHITE : SSD1306_BLACK;
}

uint16_t faceFgColor() {
  return faceTheme == FACE_THEME_INVERTED ? SSD1306_BLACK : SSD1306_WHITE;
}

uint16_t faceCutColor() {
  return faceTheme == FACE_THEME_INVERTED ? SSD1306_WHITE : SSD1306_BLACK;
}

bool faceOutlineMode() {
  return faceTheme == FACE_THEME_OUTLINE;
}

bool faceTwoZoneMode() {
  return faceTheme == FACE_THEME_TWO_ZONE;
}

uint32_t validExpressionMask(uint32_t mask, uint32_t fallback) {
  uint32_t allMask = 0;

  for (int i = 0; i < FACE_COUNT; i++) {
    allMask |= (1UL << i);
  }

  mask &= allMask;

  if (mask == 0) {
    return fallback;
  }

  return mask;
}

uint32_t sanitizeHoldMs(uint32_t value, uint32_t fallback) {
  if (value < 250 || value > 30000) {
    return fallback;
  }

  return value;
}

uint32_t sanitizeBoredTimeoutMs(uint32_t value, uint32_t fallback) {
  // 0 disables boredom timeout.
  // Otherwise allow 10 seconds to 60 minutes.
  if (value == 0) {
    return 0;
  }

  if (value < 10000 || value > 3600000) {
    return fallback;
  }

  return value;
}

void markEngaged() {
  lastEngagementMs = millis();
  boredActive = false;
}

void markInteraction() {
  lastInteractionMs = millis();
  markEngaged();
}

Expression chooseExpressionFromMask(uint32_t mask, Expression fallback) {
  mask = validExpressionMask(mask, expressionBit(fallback));

  int count = 0;

  for (int i = 0; i < FACE_COUNT; i++) {
    if (mask & (1UL << i)) {
      count++;
    }
  }

  if (count <= 0) {
    return fallback;
  }

  int pick = random(0, count);
  int seen = 0;

  for (int i = 0; i < FACE_COUNT; i++) {
    if (mask & (1UL << i)) {
      if (seen == pick) {
        return (Expression)i;
      }

      seen++;
    }
  }

  return fallback;
}

Expression activeExpression() {
  unsigned long now = millis();

  if (sensorOverridePriority > 0 && now < sensorOverrideUntil) {
    return sensorOverrideExpression;
  }

  if (demoMode) {
    return currentExpression;
  }

  if (boredActive) {
    return FACE_BORED;
  }

  return expressionForMood(currentMood);
}

Expression randomExpression() {
  // Legacy helper: Bored is controlled by the boredom timeout or mood system.
  int pick = random(0, FACE_COUNT - 1);
  return (Expression)pick;
}

YetiMood randomMood() {
  int pick = random(0, 9);
  switch (pick) {
    case 0: return MOOD_DEADPAN;
    case 1: return MOOD_HAPPY;
    case 2: return MOOD_ANNOYED;
    case 3: return MOOD_ANGRY;
    case 4: return MOOD_SLEEPY;
    case 5: return MOOD_CURIOUS;
    case 6: return MOOD_STARTLED;
    case 7: return MOOD_SMUG;
    case 8: return MOOD_SAD;
    default: return MOOD_DEADPAN;
  }
}

void scheduleNextLook() {
  nextLookTime = millis() + random(350, 1600);
}

void scheduleNextBlink() {
  nextBlinkTime = millis() + random(1400, 5200);
}

void scheduleNextExpression() {
  nextExpressionTime = millis() + random(4500, 11000);
}

void applyMood(YetiMood mood) {
  currentExpression = expressionForMood(mood);

  if (currentExpression == FACE_SURPRISED || currentExpression == FACE_SHOCKED || currentExpression == FACE_BORED) {
    targetEyeOffsetX = 0;
    targetEyeOffsetY = 0;
  }

#if YETI_FACE_ENGINE == YETI_FACE_ENGINE_ROBOEYES
  // Force the renderer to re-apply settings next face frame.
  roboEyesLastExpression = FACE_COUNT;
#endif
}

bool setMood(YetiMood mood, unsigned long durationMs, MoodPriority priority, bool countAsInteraction) {
  unsigned long now = millis();

  if (countAsInteraction) {
    demoMode = false;
    markInteraction();
  }

  // Manual mood commands should override/interrupt any non-blocking acting sequence.
  if (!applyingSequenceMood && sequenceActive() && priority >= PRIORITY_MANUAL) {
    stopSequence(false);
  }

  if (moodTemporaryActive() && priority < currentMoodPriority) {
    return false;
  }

  currentMood = mood;
  if (memoryEnabled) {
    memoryStats.lifetimeMoodChanges++;
    recordDailyMood(mood);
    markMemoryDirty();
  }
  currentMoodPriority = priority;
  moodUntilMs = durationMs > 0 ? now + durationMs : 0;
  lastMoodChangeMs = now;
  boredActive = false;
  applyMood(mood);

  String msg = String("Current mood: ") + moodToString(mood);
  if (durationMs > 0) {
    msg += String(" for ") + String(durationMs) + " ms";
  }
  msg += String(" [") + moodPriorityLabel(priority) + "]";
  addEvent("Mood", msg);

  Serial.print("Mood set to: ");
  Serial.print(moodToString(mood));
  if (durationMs > 0) {
    Serial.print(" for ");
    Serial.print(durationMs);
    Serial.print(" ms");
  }
  Serial.println();

  return true;
}

void setBaseMood(YetiMood mood) {
  stopSequence(false);
  baseMood = mood;
  currentMood = mood;
  moodUntilMs = 0;
  currentMoodPriority = PRIORITY_IDLE;
  pokeStagePending = false;
  applyMood(mood);
  scheduleNextIdleBehavior();
  addEvent("Mood", String("Base mood set: ") + moodToString(baseMood));
}

void calmDownYeti() {
  pokeStagePending = false;
  stopSequence(false);
  setMood(baseMood, 0, PRIORITY_MANUAL, true);
  currentMoodPriority = PRIORITY_IDLE;
  addEvent("Mood", "Calm down -> base mood");
}

void triggerPokeYeti() {
  wakeYetiFromSleep("poke", true);
  markInteraction();
  recordMemoryEvent(EVENT_POKED);
  startSequence(SEQ_POKE_REACTION, String("poke button · ") + getGrudgeSummary(), PRIORITY_MANUAL);
  recordReaction(EVENT_WEBUI_COMMAND, String("poke sequence started · ") + getGrudgeSummary());
}

void updateMoodEngine() {
  unsigned long now = millis();

  if (sensorOverridePriority > 0 && now >= sensorOverrideUntil) {
    sensorOverridePriority = 0;
    lastTriggerName = "None";
  }

  updateSequence();

  if (moodUntilMs > 0 && now >= moodUntilMs) {
    moodUntilMs = 0;
    currentMoodPriority = PRIORITY_IDLE;
    currentMood = baseMood;
    applyMood(currentMood);
    addEvent("Mood", String("Temporary mood expired -> ") + moodToString(baseMood));
  }
}

void scheduleNextIdleBehavior() {
  int chaos = sanitizePersonalityTrait(personality.chaos, 30);
  int curiosity = sanitizePersonalityTrait(personality.curiosity, 45);
  int sleepiness = sanitizePersonalityTrait(personality.sleepiness, 40);
  int grumpiness = sanitizePersonalityTrait(personality.grumpiness, 70);
  int friendliness = sanitizePersonalityTrait(personality.friendliness, 25);

  // v1.5.1: personality shapes the idle cadence instead of one blunt chaos knob.
  // Chaos and curiosity shorten the wait. Sleepiness can either make him dozy sooner
  // or, when combined with low chaos, let him sit there like a haunted paperweight.
  long minDelay = 28000L;
  minDelay -= (long)chaos * 130L;
  minDelay -= (long)curiosity * 45L;
  minDelay -= (long)grumpiness * 25L;
  minDelay += (long)friendliness * 10L;

  long maxDelay = 90000L;
  maxDelay -= (long)chaos * 430L;
  maxDelay -= (long)curiosity * 110L;
  maxDelay += (long)(100 - sleepiness) * 60L;

  if (sleepiness > 70 && chaos < 40) {
    maxDelay += 14000L;
  }

  if (chaos > 80) {
    minDelay -= 3000L;
    maxDelay -= 9000L;
  }

  // v1.6.4: internal needs bend idle timing. Bored/irritated YETI fidgets sooner;
  // low-energy YETI waits longer like a haunted paperweight conserving spite.
  if (memoryEnabled && needsEnabled) {
    minDelay -= (long)needs.boredom * 45L;
    minDelay -= (long)needs.irritation * 25L;
    maxDelay -= (long)needs.boredom * 130L;
    maxDelay -= (long)needs.irritation * 70L;
    maxDelay -= (long)needs.loneliness * 45L;

    if (needs.energy < 30) {
      maxDelay += (long)(30 - needs.energy) * 650L;
    }
  }

  if (minDelay < 7000L) minDelay = 7000L;
  if (maxDelay < minDelay + 6000L) maxDelay = minDelay + 6000L;
  if (maxDelay > 120000L) maxDelay = 120000L;

  nextIdleBehaviorDelayMs = (unsigned long)random(minDelay, maxDelay);
}

int weightedTraitValue(int raw) {
  return max(0, sanitizePersonalityTrait(raw, 50));
}

int clampIdleWeight(int value) {
  if (value < 0) return 0;
  if (value > 180) return 180;
  return value;
}

YetiMood chooseIdleMoodFromPersonality() {
  int grumpy = weightedTraitValue(personality.grumpiness);
  int curious = weightedTraitValue(personality.curiosity);
  int sleepy = weightedTraitValue(personality.sleepiness);
  int chaos = weightedTraitValue(personality.chaos);
  int friendly = weightedTraitValue(personality.friendliness);

  YetiMood moods[9] = {
    MOOD_DEADPAN,
    MOOD_CURIOUS,
    MOOD_SLEEPY,
    MOOD_ANNOYED,
    MOOD_SMUG,
    MOOD_HAPPY,
    MOOD_SAD,
    MOOD_STARTLED,
    MOOD_ANGRY
  };

  int weights[9];
  weights[0] = clampIdleWeight(36 + ((100 - chaos) / 3) + ((100 - curious) / 5));
  weights[1] = clampIdleWeight(8 + curious + (chaos / 4));
  weights[2] = clampIdleWeight(8 + sleepy + ((100 - chaos) / 6));
  weights[3] = clampIdleWeight(6 + grumpy + (chaos / 3) - (friendly / 2));
  weights[4] = clampIdleWeight(4 + (chaos / 2) + (grumpy / 3) + (curious / 4));
  weights[5] = clampIdleWeight(5 + friendly + (curious / 5) - (grumpy / 3));
  weights[6] = clampIdleWeight(2 + (sleepy / 2) + (grumpy / 4) - (friendly / 3));
  weights[7] = clampIdleWeight((chaos > 35 ? 1 + (chaos / 5) : 0) + (curious / 12));
  weights[8] = clampIdleWeight((grumpy > 55 || chaos > 70) ? 1 + (grumpy / 4) + (chaos / 7) - (friendly / 2) : 0);

  // v1.6.4: needs add creature-state bias on top of personality sliders.
  if (memoryEnabled && needsEnabled) {
    weights[0] = clampIdleWeight(weights[0] - (needs.boredom / 3) - (needs.irritation / 4));
    weights[1] = clampIdleWeight(weights[1] + (needs.boredom / 2) + (needs.loneliness / 8));
    weights[2] = clampIdleWeight(weights[2] + max(0, 55 - needs.energy));
    weights[3] = clampIdleWeight(weights[3] + (needs.irritation / 2));
    weights[4] = clampIdleWeight(weights[4] + (needs.boredom / 3) + (needs.irritation / 5));
    weights[5] = clampIdleWeight(weights[5] + max(0, needs.energy - 70) / 2 - (needs.irritation / 3));
    weights[6] = clampIdleWeight(weights[6] + (needs.loneliness / 2) + max(0, 45 - needs.energy));
    weights[7] = clampIdleWeight(weights[7] + (needs.boredom > 75 ? 8 : 0) + (needs.irritation > 80 ? 8 : 0));
    weights[8] = clampIdleWeight(weights[8] + (needs.irritation > 65 ? needs.irritation / 3 : 0));
  }

  // Keep high friendliness from turning YETI into a rage slot machine.
  if (friendly > 70) {
    weights[3] = max(1, weights[3] / 2);
    weights[8] = max(0, weights[8] / 3);
  }

  // Feral Goblin mode should actually feel feral.
  if (chaos > 85 && grumpy > 75) {
    weights[4] += 18;
    weights[7] += 10;
    weights[8] += 12;
  }

  int total = 0;
  for (int i = 0; i < 9; i++) {
    total += max(0, weights[i]);
  }

  if (total <= 0) {
    return baseMood;
  }

  int roll = random(0, total);
  int cursor = 0;

  for (int i = 0; i < 9; i++) {
    cursor += max(0, weights[i]);
    if (roll < cursor) {
      return moods[i];
    }
  }

  return baseMood;
}

unsigned long idleMoodDurationMs(YetiMood mood) {
  int grumpy = weightedTraitValue(personality.grumpiness);
  int curious = weightedTraitValue(personality.curiosity);
  int sleepy = weightedTraitValue(personality.sleepiness);
  int chaos = weightedTraitValue(personality.chaos);
  int friendly = weightedTraitValue(personality.friendliness);

  unsigned long minMs = 3500;
  unsigned long maxMs = 9000;

  switch (mood) {
    case MOOD_CURIOUS:
      minMs = 4500;
      maxMs = 9500 + (unsigned long)curious * 35UL;
      break;
    case MOOD_SLEEPY:
      minMs = 6500;
      maxMs = 13000 + (unsigned long)sleepy * 55UL;
      break;
    case MOOD_ANNOYED:
      minMs = 3500;
      maxMs = 8500 + (unsigned long)grumpy * 35UL;
      if (friendly > 60) maxMs -= 1500;
      break;
    case MOOD_ANGRY:
      minMs = 2600;
      maxMs = 6500 + (unsigned long)grumpy * 28UL + (unsigned long)chaos * 16UL;
      break;
    case MOOD_SMUG:
      minMs = 4200;
      maxMs = 9800 + (unsigned long)chaos * 25UL;
      break;
    case MOOD_HAPPY:
      minMs = 4200;
      maxMs = 8500 + (unsigned long)friendly * 38UL;
      break;
    case MOOD_SAD:
      minMs = 4200;
      maxMs = 9500 + (unsigned long)sleepy * 22UL;
      break;
    case MOOD_STARTLED:
      minMs = 1200;
      maxMs = 2800 + (unsigned long)chaos * 8UL;
      break;
    case MOOD_DEADPAN:
    default:
      minMs = 4200;
      maxMs = 8500 + (unsigned long)(100 - chaos) * 18UL;
      break;
  }

  if (maxMs < minMs + 500UL) {
    maxMs = minMs + 500UL;
  }

  return (unsigned long)random((long)minMs, (long)maxMs);
}

String idleMoodReason(YetiMood mood) {
  switch (mood) {
    case MOOD_CURIOUS: return "curiosity weighted idle pick";
    case MOOD_SLEEPY: return "sleepiness weighted idle pick";
    case MOOD_ANNOYED: return "grumpiness weighted idle pick";
    case MOOD_ANGRY: return "high grumpiness/chaos idle pick";
    case MOOD_SMUG: return "chaos/grumpiness smug idle pick";
    case MOOD_HAPPY: return "friendliness weighted idle pick";
    case MOOD_SAD: return "sleepy/grumpy low-energy idle pick";
    case MOOD_STARTLED: return "chaos twitch idle pick";
    case MOOD_DEADPAN:
    default: return "deadpan baseline idle pick";
  }
}

bool runRandomIdleBehavior() {
  if (!faceDisplayAvailable()) {
    return false;
  }

  YetiMood picked = chooseIdleMoodFromPersonality();
  int chaos = sanitizePersonalityTrait(personality.chaos, 30);

  // v1.5.5: occasionally turn an idle pick into a tiny acting sequence.
  if (!sequenceActive() &&
      (picked == MOOD_SLEEPY || picked == MOOD_SMUG || picked == MOOD_ANNOYED) &&
      random(100) < (10 + chaos / 3)) {
    bool sequenceStarted = startSequence(SEQ_IDLE_BOREDOM, "weighted idle boredom", PRIORITY_IDLE);
    if (sequenceStarted) {
      lastIdleMood = picked;
      lastIdleMoodMs = millis();
      lastIdleReason = "idle boredom sequence";
      idleMoodCount++;
      return true;
    }
  }

  unsigned long duration = idleMoodDurationMs(picked);
  bool changed = setMood(picked, duration, PRIORITY_IDLE, false);

  if (changed) {
    lastIdleMood = picked;
    lastIdleMoodMs = millis();
    lastIdleReason = idleMoodReason(picked);
    if (memoryEnabled && needsEnabled) {
      lastIdleReason += String(" · ") + getNeedsBiasSummary();
    }
    idleMoodCount++;

    addEvent("Mood", String("Idle pick: ") + moodToString(picked) +
             " for " + String(duration) + " ms (" + lastIdleReason + ")");
  }

  return changed;
}

void updateIdleBehavior() {
  if (!idleMoodEnabled || !faceDisplayAvailable()) {
    return;
  }

  unsigned long now = millis();

  if (sensorOverridePriority > 0 && now < sensorOverrideUntil) {
    return;
  }

  if (sequenceActive()) {
    return;
  }

  if (currentMoodPriority > PRIORITY_IDLE) {
    return;
  }

  if (now - lastInteractionMs < 12000UL) {
    return;
  }

  if (moodTemporaryActive() && currentMoodPriority == PRIORITY_IDLE) {
    return;
  }

  if (lastIdleBehaviorMs == 0) {
    lastIdleBehaviorMs = now;
    scheduleNextIdleBehavior();
    return;
  }

  if (now - lastIdleBehaviorMs >= nextIdleBehaviorDelayMs) {
    runRandomIdleBehavior();
    lastIdleBehaviorMs = now;
    scheduleNextIdleBehavior();
  }
}

void setExpression(Expression expression, bool countAsEngagement = true) {
  if (countAsEngagement) {
    setMood(moodFromExpression(expression), 0, PRIORITY_MANUAL, true);
    return;
  }

  currentExpression = expression;

  if (expression == FACE_SURPRISED || expression == FACE_SHOCKED || expression == FACE_BORED) {
    targetEyeOffsetX = 0;
    targetEyeOffsetY = 0;
  }

  scheduleNextExpression();
}

void startBlink() {
  blinking = true;
  blinkStartTime = millis();

  if (random(0, 100) < 25) {
    doubleBlinkQueued = true;
    doubleBlinkTime = millis() + blinkDuration + random(80, 180);
  }

  scheduleNextBlink();
}

float getBlinkAmount() {
  if (!blinking) {
    return 0.0;
  }

  unsigned long elapsed = millis() - blinkStartTime;

  if (elapsed >= blinkDuration) {
    blinking = false;
    return 0.0;
  }

  float phase = (float)elapsed / (float)blinkDuration;

  if (phase < 0.5) {
    return phase * 2.0;
  } else {
    return (1.0 - phase) * 2.0;
  }
}

void clearSavedWifi() {
  prefs.begin("wifi", false);
  prefs.clear();
  prefs.end();

  savedSsid = "";
  savedPassword = "";
}


// =====================================================
// MEMORY CORE STORAGE / RELATIONSHIP
// =====================================================

int clampPercent(int value) {
  if (value < 0) return 0;
  if (value > 100) return 100;
  return value;
}

void markMemoryDirty() {
  memoryDirty = true;
}

void loadMemorySettings() {
  prefs.begin("yeti_mem", true);

  memoryEnabled = prefs.getBool("memEn", true);
  needsEnabled = prefs.getBool("needsEn", true);

  memoryStats.lifetimeBoots = prefs.getUInt("boots", 0);
  memoryStats.lifetimePokes = prefs.getUInt("pokes", 0);
  memoryStats.lifetimeShakes = prefs.getUInt("shakes", 0);
  memoryStats.lifetimeTilts = prefs.getUInt("tilts", 0);
  memoryStats.lifetimeBadWifi = prefs.getUInt("badWifi", 0);
  memoryStats.lifetimeWeatherFails = prefs.getUInt("wFails", 0);
  memoryStats.lifetimeWebVisits = prefs.getUInt("webVisits", 0);
  memoryStats.lifetimeCommands = prefs.getUInt("commands", 0);
  memoryStats.lifetimeReboots = prefs.getUInt("reboots", 0);
  memoryStats.lifetimeSettingsSaves = prefs.getUInt("cfgSaves", 0);
  memoryStats.lifetimeMoodChanges = prefs.getUInt("moodChg", 0);
  memoryStats.lifetimeSequences = prefs.getUInt("seqRuns", 0);

  relationship.affection = clampPercent(prefs.getInt("affect", 35));
  relationship.annoyance = clampPercent(prefs.getInt("annoy", 20));
  relationship.trust = clampPercent(prefs.getInt("trust", 50));
  relationship.suspicion = clampPercent(prefs.getInt("suspect", 25));

  grudges.pokeGrudge = clampPercent(prefs.getInt("gPoke", 0));
  grudges.shakeGrudge = clampPercent(prefs.getInt("gShake", 0));
  grudges.wifiGrudge = clampPercent(prefs.getInt("gWifi", 0));
  grudges.weatherGrudge = clampPercent(prefs.getInt("gWeather", 0));
  grudges.rebootGrudge = clampPercent(prefs.getInt("gReboot", 0));
  grudges.neglectGrudge = clampPercent(prefs.getInt("gNeglect", 0));

  needs.boredom = clampPercent(prefs.getInt("nBored", 20));
  needs.energy = clampPercent(prefs.getInt("nEnergy", 70));
  needs.irritation = clampPercent(prefs.getInt("nIrrit", 20));
  needs.loneliness = clampPercent(prefs.getInt("nLonely", 10));

  dailyMemory.todayPokes = prefs.getUShort("tdPoke", 0);
  dailyMemory.todayShakes = prefs.getUShort("tdShake", 0);
  dailyMemory.todayBadWifi = prefs.getUShort("tdWifi", 0);
  dailyMemory.todayWeatherFails = prefs.getUShort("tdWFail", 0);
  dailyMemory.todayWebVisits = prefs.getUShort("tdWeb", 0);
  dailyMemory.todayCommands = prefs.getUShort("tdCmd", 0);
  dailyMemory.todayAngryMoodCount = prefs.getUShort("tdAngry", 0);
  dailyMemory.todayHappyMoodCount = prefs.getUShort("tdHappy", 0);
  dailyMemory.yesterdayPokes = prefs.getUShort("ydPoke", 0);
  dailyMemory.yesterdayShakes = prefs.getUShort("ydShake", 0);
  dailyMemory.yesterdayBadWifi = prefs.getUShort("ydWifi", 0);
  dailyMemory.yesterdayWeatherFails = prefs.getUShort("ydWFail", 0);
  dailyMemory.yesterdayAngryMoodCount = prefs.getUShort("ydAngry", 0);
  dailyMemory.yesterdayHappyMoodCount = prefs.getUShort("ydHappy", 0);
  setDailyMemoryDate(prefs.getString("lastDate", ""));

  prefs.end();

  memoryDirty = false;
  lastMemorySaveMs = millis();
  lastGrudgeDecayMs = millis();
  lastNeedsUpdateMs = millis();
  lastNeedsMoodMs = 0;
  lastNeedsPersistMarkMs = millis();
  lastMemoryEvent = "Loaded";
  lastMemoryNote = memoryEnabled ? "Persistent memory loaded" : "Memory disabled";
  Serial.println(memoryEnabled ? "YETI memory loaded." : "YETI memory disabled.");
}

void saveMemorySettings(bool force) {
  unsigned long now = millis();

  if (!force && !memoryDirty) {
    return;
  }

  if (!force && now - lastMemorySaveMs < YETI_MEMORY_SAVE_INTERVAL_MS) {
    return;
  }

  prefs.begin("yeti_mem", false);

  prefs.putBool("memEn", memoryEnabled);
  prefs.putBool("needsEn", needsEnabled);

  prefs.putUInt("boots", memoryStats.lifetimeBoots);
  prefs.putUInt("pokes", memoryStats.lifetimePokes);
  prefs.putUInt("shakes", memoryStats.lifetimeShakes);
  prefs.putUInt("tilts", memoryStats.lifetimeTilts);
  prefs.putUInt("badWifi", memoryStats.lifetimeBadWifi);
  prefs.putUInt("wFails", memoryStats.lifetimeWeatherFails);
  prefs.putUInt("webVisits", memoryStats.lifetimeWebVisits);
  prefs.putUInt("commands", memoryStats.lifetimeCommands);
  prefs.putUInt("reboots", memoryStats.lifetimeReboots);
  prefs.putUInt("cfgSaves", memoryStats.lifetimeSettingsSaves);
  prefs.putUInt("moodChg", memoryStats.lifetimeMoodChanges);
  prefs.putUInt("seqRuns", memoryStats.lifetimeSequences);

  prefs.putInt("affect", clampPercent(relationship.affection));
  prefs.putInt("annoy", clampPercent(relationship.annoyance));
  prefs.putInt("trust", clampPercent(relationship.trust));
  prefs.putInt("suspect", clampPercent(relationship.suspicion));

  prefs.putInt("gPoke", clampPercent(grudges.pokeGrudge));
  prefs.putInt("gShake", clampPercent(grudges.shakeGrudge));
  prefs.putInt("gWifi", clampPercent(grudges.wifiGrudge));
  prefs.putInt("gWeather", clampPercent(grudges.weatherGrudge));
  prefs.putInt("gReboot", clampPercent(grudges.rebootGrudge));
  prefs.putInt("gNeglect", clampPercent(grudges.neglectGrudge));

  prefs.putInt("nBored", clampPercent(needs.boredom));
  prefs.putInt("nEnergy", clampPercent(needs.energy));
  prefs.putInt("nIrrit", clampPercent(needs.irritation));
  prefs.putInt("nLonely", clampPercent(needs.loneliness));

  prefs.putUShort("tdPoke", dailyMemory.todayPokes);
  prefs.putUShort("tdShake", dailyMemory.todayShakes);
  prefs.putUShort("tdWifi", dailyMemory.todayBadWifi);
  prefs.putUShort("tdWFail", dailyMemory.todayWeatherFails);
  prefs.putUShort("tdWeb", dailyMemory.todayWebVisits);
  prefs.putUShort("tdCmd", dailyMemory.todayCommands);
  prefs.putUShort("tdAngry", dailyMemory.todayAngryMoodCount);
  prefs.putUShort("tdHappy", dailyMemory.todayHappyMoodCount);
  prefs.putUShort("ydPoke", dailyMemory.yesterdayPokes);
  prefs.putUShort("ydShake", dailyMemory.yesterdayShakes);
  prefs.putUShort("ydWifi", dailyMemory.yesterdayBadWifi);
  prefs.putUShort("ydWFail", dailyMemory.yesterdayWeatherFails);
  prefs.putUShort("ydAngry", dailyMemory.yesterdayAngryMoodCount);
  prefs.putUShort("ydHappy", dailyMemory.yesterdayHappyMoodCount);
  prefs.putString("lastDate", String(dailyMemory.lastMemoryDate));

  prefs.end();

  memoryDirty = false;
  lastMemorySaveMs = now;
  lastMemoryEvent = "Saved";
  lastMemoryNote = force ? "Memory saved immediately" : "Memory auto-saved";
  addEvent("Memory", lastMemoryNote);
}

String currentDailyMemoryDateKey() {
  if (!clockSynced()) {
    return "";
  }

  time_t raw = time(nullptr) + activeClockOffsetSeconds();
  struct tm localTime;
  gmtime_r(&raw, &localTime);

  char buf[11];
  strftime(buf, sizeof(buf), "%Y-%m-%d", &localTime);
  return String(buf);
}

void setDailyMemoryDate(const String& dateKey) {
  String safe = dateKey;
  safe.trim();
  if (safe.length() > 10) {
    safe = safe.substring(0, 10);
  }
  memset(dailyMemory.lastMemoryDate, 0, sizeof(dailyMemory.lastMemoryDate));
  safe.toCharArray(dailyMemory.lastMemoryDate, sizeof(dailyMemory.lastMemoryDate));
}

void clearTodayDailyMemory() {
  dailyMemory.todayPokes = 0;
  dailyMemory.todayShakes = 0;
  dailyMemory.todayBadWifi = 0;
  dailyMemory.todayWeatherFails = 0;
  dailyMemory.todayWebVisits = 0;
  dailyMemory.todayCommands = 0;
  dailyMemory.todayAngryMoodCount = 0;
  dailyMemory.todayHappyMoodCount = 0;
}

void resetDailyMemory() {
  clearTodayDailyMemory();
  dailyMemory.yesterdayPokes = 0;
  dailyMemory.yesterdayShakes = 0;
  dailyMemory.yesterdayBadWifi = 0;
  dailyMemory.yesterdayWeatherFails = 0;
  dailyMemory.yesterdayAngryMoodCount = 0;
  dailyMemory.yesterdayHappyMoodCount = 0;
  setDailyMemoryDate(currentDailyMemoryDateKey());
  dailyRolloverCount = 0;
  lastDailyNote = clockSynced() ? String("Daily memory reset for ") + String(dailyMemory.lastMemoryDate) : "Daily memory reset; waiting for clock sync";
  markMemoryDirty();
}

void rolloverDailyMemoryIfNeeded(bool force) {
  if (!memoryEnabled) {
    return;
  }

  String dateKey = currentDailyMemoryDateKey();
  if (dateKey.length() == 0 && !force) {
    return;
  }

  if (dateKey.length() == 0) {
    dateKey = String(dailyMemory.lastMemoryDate);
    if (dateKey.length() == 0) {
      dateKey = "manual";
    }
  }

  String stored = String(dailyMemory.lastMemoryDate);

  if (stored.length() == 0) {
    setDailyMemoryDate(dateKey);
    lastDailyNote = String("Daily memory date initialized: ") + dateKey;
    markMemoryDirty();
    addEvent("Memory", lastDailyNote);
    return;
  }

  if (!force && stored == dateKey) {
    return;
  }

  dailyMemory.yesterdayPokes = dailyMemory.todayPokes;
  dailyMemory.yesterdayShakes = dailyMemory.todayShakes;
  dailyMemory.yesterdayBadWifi = dailyMemory.todayBadWifi;
  dailyMemory.yesterdayWeatherFails = dailyMemory.todayWeatherFails;
  dailyMemory.yesterdayAngryMoodCount = dailyMemory.todayAngryMoodCount;
  dailyMemory.yesterdayHappyMoodCount = dailyMemory.todayHappyMoodCount;
  clearTodayDailyMemory();
  setDailyMemoryDate(dateKey);
  dailyRolloverCount++;
  lastDailyNote = String("Daily rollover: ") + stored + " -> " + dateKey;
  markMemoryDirty();
  addEvent("Memory", lastDailyNote);
}

void recordDailyEvent(YetiEvent event) {
  if (!memoryEnabled) {
    return;
  }

  rolloverDailyMemoryIfNeeded(false);

  switch (event) {
    case EVENT_POKED:
      dailyMemory.todayPokes++;
      break;
    case EVENT_GYRO_SHAKEN:
      dailyMemory.todayShakes++;
      break;
    case EVENT_WIFI_FAILED:
    case EVENT_WIFI_WEAK:
    case EVENT_WIFI_VERY_WEAK:
      dailyMemory.todayBadWifi++;
      break;
    case EVENT_WEATHER_FAILED:
      dailyMemory.todayWeatherFails++;
      break;
    case EVENT_WEBUI_OPENED:
      dailyMemory.todayWebVisits++;
      break;
    case EVENT_WEBUI_COMMAND:
      dailyMemory.todayCommands++;
      break;
    default:
      return;
  }

  markMemoryDirty();
}

void recordDailyMood(YetiMood mood) {
  if (!memoryEnabled) {
    return;
  }

  rolloverDailyMemoryIfNeeded(false);

  if (mood == MOOD_ANGRY) {
    dailyMemory.todayAngryMoodCount++;
    markMemoryDirty();
  } else if (mood == MOOD_HAPPY) {
    dailyMemory.todayHappyMoodCount++;
    markMemoryDirty();
  }
}

String getTodaySummary() {
  uint16_t trouble = dailyMemory.todayPokes + dailyMemory.todayShakes + dailyMemory.todayBadWifi + dailyMemory.todayWeatherFails;
  if (trouble == 0 && dailyMemory.todayWebVisits == 0) return "Quiet so far. Suspicious.";
  if (dailyMemory.todayPokes >= 5) return String("Poke-heavy: ") + String(dailyMemory.todayPokes) + " pokes today";
  if (dailyMemory.todayShakes >= 3) return String("Motion crimes: ") + String(dailyMemory.todayShakes) + " shakes today";
  if (dailyMemory.todayBadWifi >= 3) return String("Router nonsense: ") + String(dailyMemory.todayBadWifi) + " bad Wi-Fi events";
  if (dailyMemory.todayWeatherFails >= 3) return String("Weather API drama: ") + String(dailyMemory.todayWeatherFails) + " failures";
  if (dailyMemory.todayHappyMoodCount > dailyMemory.todayAngryMoodCount + 2) return "Mostly tolerable. Weird.";
  if (dailyMemory.todayAngryMoodCount > dailyMemory.todayHappyMoodCount + 2) return "Anger-forward little workday.";
  return "Mixed results. File remains open.";
}

String getYesterdaySummary() {
  uint16_t trouble = dailyMemory.yesterdayPokes + dailyMemory.yesterdayShakes + dailyMemory.yesterdayBadWifi + dailyMemory.yesterdayWeatherFails;
  if (trouble == 0 && dailyMemory.yesterdayHappyMoodCount == 0 && dailyMemory.yesterdayAngryMoodCount == 0) return "No useful yesterday yet";
  if (dailyMemory.yesterdayPokes >= 8) return String("Yesterday was poke crimes: ") + String(dailyMemory.yesterdayPokes);
  if (dailyMemory.yesterdayShakes >= 4) return String("Yesterday involved shake offenses: ") + String(dailyMemory.yesterdayShakes);
  if (dailyMemory.yesterdayBadWifi >= 5) return String("Yesterday was router trauma: ") + String(dailyMemory.yesterdayBadWifi);
  if (dailyMemory.yesterdayWeatherFails >= 4) return String("Yesterday weather failed repeatedly: ") + String(dailyMemory.yesterdayWeatherFails);
  if (dailyMemory.yesterdayHappyMoodCount > dailyMemory.yesterdayAngryMoodCount + 2) return "Yesterday was oddly pleasant";
  if (dailyMemory.yesterdayAngryMoodCount > dailyMemory.yesterdayHappyMoodCount + 2) return "Yesterday left an angry residue";
  return "Yesterday was emotionally inconclusive";
}

String getDailyMemorySummary() {
  return String("Today: ") + getTodaySummary() + " · Yesterday: " + getYesterdaySummary();
}

String getYesterdayBootReason() {
  if (dailyMemory.yesterdayPokes >= 8) return "yesterday's poke crimes remembered";
  if (dailyMemory.yesterdayShakes >= 4) return "yesterday's shake offenses remembered";
  if (dailyMemory.yesterdayBadWifi >= 5) return "yesterday's router trauma remembered";
  if (dailyMemory.yesterdayWeatherFails >= 4) return "yesterday's weather failures remembered";
  if (dailyMemory.yesterdayAngryMoodCount > dailyMemory.yesterdayHappyMoodCount + 3) return "yesterday's angry residue remembered";
  if (dailyMemory.yesterdayHappyMoodCount > dailyMemory.yesterdayAngryMoodCount + 3) return "yesterday was tolerable enough to remember";
  return "boot initialized";
}

YetiSequence bootSequenceFromDailyMemory() {
  if (!memoryEnabled) return SEQ_BOOT_DRAMA;
  if (dailyMemory.yesterdayBadWifi >= 5) return SEQ_BAD_WIFI_TANTRUM;
  if (dailyMemory.yesterdayWeatherFails >= 4) return SEQ_WEATHER_RAIN_SAD;
  if (dailyMemory.yesterdayPokes >= 8 || dailyMemory.yesterdayShakes >= 4 || dailyMemory.yesterdayAngryMoodCount > dailyMemory.yesterdayHappyMoodCount + 3) return SEQ_BAD_WIFI_TANTRUM;
  return SEQ_BOOT_DRAMA;
}

void updateMemorySystem() {
  rolloverDailyMemoryIfNeeded(false);
  decayGrudges(false);
  updateNeeds();
  saveMemorySettings(false);
}

void resetMemorySettings() {
  memoryStats.lifetimeBoots = 0;
  memoryStats.lifetimePokes = 0;
  memoryStats.lifetimeShakes = 0;
  memoryStats.lifetimeTilts = 0;
  memoryStats.lifetimeBadWifi = 0;
  memoryStats.lifetimeWeatherFails = 0;
  memoryStats.lifetimeWebVisits = 0;
  memoryStats.lifetimeCommands = 0;
  memoryStats.lifetimeReboots = 0;
  memoryStats.lifetimeSettingsSaves = 0;
  memoryStats.lifetimeMoodChanges = 0;
  memoryStats.lifetimeSequences = 0;

  relationship.affection = 35;
  relationship.annoyance = 20;
  relationship.trust = 50;
  relationship.suspicion = 25;
  grudges.pokeGrudge = 0;
  grudges.shakeGrudge = 0;
  grudges.wifiGrudge = 0;
  grudges.weatherGrudge = 0;
  grudges.rebootGrudge = 0;
  grudges.neglectGrudge = 0;
  needs.boredom = 20;
  needs.energy = 70;
  needs.irritation = 20;
  needs.loneliness = 10;
  needsEnabled = true;
  resetDailyMemory();
  memoryEventCount = 0;
  grudgeEventCount = 0;
  needsEventCount = 0;
  lastGrudgeDecayMs = millis();
  lastNeedsUpdateMs = millis();
  lastNeedsMoodMs = 0;
  lastNeedsPersistMarkMs = millis();
  lastMemoryEvent = "Reset";
  lastMemoryNote = "Memory wiped; grudges temporarily misplaced";
  lastGrudgeNote = "All grudges reset to zero";
  lastNeedsNote = "Needs reset to factory suspicious";
  memoryDirty = true;
  saveMemorySettings(true);
  addEvent("Memory", "Reset memory and relationship values");
}

void adjustRelationship(int affectionDelta, int annoyanceDelta, int trustDelta, int suspicionDelta) {
  relationship.affection = clampPercent(relationship.affection + affectionDelta);
  relationship.annoyance = clampPercent(relationship.annoyance + annoyanceDelta);
  relationship.trust = clampPercent(relationship.trust + trustDelta);
  relationship.suspicion = clampPercent(relationship.suspicion + suspicionDelta);
  markMemoryDirty();
}

void adjustGrudgeValue(int &value, int delta) {
  int oldValue = value;
  value = clampPercent(value + delta);
  if (value != oldValue) {
    markMemoryDirty();
  }
}

void increaseGrudge(YetiEvent event, int amount) {
  if (!memoryEnabled || amount <= 0) {
    return;
  }

  String name = "general";

  switch (event) {
    case EVENT_POKED:
      adjustGrudgeValue(grudges.pokeGrudge, amount);
      name = "poke";
      break;

    case EVENT_GYRO_SHAKEN:
      adjustGrudgeValue(grudges.shakeGrudge, amount);
      name = "shake";
      break;

    case EVENT_GYRO_TILTED:
      adjustGrudgeValue(grudges.shakeGrudge, max(1, amount / 2));
      name = "tilt";
      break;

    case EVENT_WIFI_FAILED:
    case EVENT_WIFI_WEAK:
    case EVENT_WIFI_VERY_WEAK:
      adjustGrudgeValue(grudges.wifiGrudge, amount);
      name = "wifi";
      break;

    case EVENT_WEATHER_FAILED:
      adjustGrudgeValue(grudges.weatherGrudge, amount);
      name = "weather";
      break;

    case EVENT_REBOOT_REQUESTED:
      adjustGrudgeValue(grudges.rebootGrudge, amount);
      name = "reboot";
      break;

    default:
      adjustGrudgeValue(grudges.neglectGrudge, max(1, amount / 2));
      name = "general";
      break;
  }

  grudgeEventCount++;
  lastGrudgeNote = String("Increased ") + name + " grudge by " + String(amount) + " (worst: " + getWorstGrudgeName() + ")";
  addEvent("Grudge", lastGrudgeNote);
}

void reduceAllGrudges(int amount) {
  if (amount <= 0) {
    return;
  }

  adjustGrudgeValue(grudges.pokeGrudge, -amount);
  adjustGrudgeValue(grudges.shakeGrudge, -amount);
  adjustGrudgeValue(grudges.wifiGrudge, -amount);
  adjustGrudgeValue(grudges.weatherGrudge, -amount);
  adjustGrudgeValue(grudges.rebootGrudge, -amount);
  adjustGrudgeValue(grudges.neglectGrudge, -amount);
  lastGrudgeNote = String("Reduced all grudges by ") + String(amount);
  addEvent("Grudge", lastGrudgeNote);
}

void decayGrudges(bool force) {
  unsigned long now = millis();
  if (!memoryEnabled) {
    return;
  }

  if (!force && lastGrudgeDecayMs > 0 && now - lastGrudgeDecayMs < YETI_GRUDGE_DECAY_INTERVAL_MS) {
    return;
  }

  lastGrudgeDecayMs = now;

  int before = getTotalGrudge();
  adjustGrudgeValue(grudges.pokeGrudge, -1);
  adjustGrudgeValue(grudges.shakeGrudge, -1);
  adjustGrudgeValue(grudges.wifiGrudge, -1);
  adjustGrudgeValue(grudges.weatherGrudge, -1);
  adjustGrudgeValue(grudges.rebootGrudge, -1);
  adjustGrudgeValue(grudges.neglectGrudge, -1);

  if (getTotalGrudge() != before) {
    lastGrudgeNote = "Grudges decayed by 1. Forgiveness remains suspicious.";
    addEvent("Grudge", lastGrudgeNote);
  }
}

int getTotalGrudge() {
  return grudges.pokeGrudge + grudges.shakeGrudge + grudges.wifiGrudge + grudges.weatherGrudge + grudges.rebootGrudge + grudges.neglectGrudge;
}

bool isHoldingGrudge() {
  return getTotalGrudge() >= 40 || grudges.pokeGrudge >= 25 || grudges.shakeGrudge >= 25 || grudges.wifiGrudge >= 25 || grudges.weatherGrudge >= 25 || grudges.rebootGrudge >= 25;
}

String getWorstGrudgeName() {
  int value = grudges.pokeGrudge;
  String name = "Poke";

  if (grudges.shakeGrudge > value) { value = grudges.shakeGrudge; name = "Shake"; }
  if (grudges.wifiGrudge > value) { value = grudges.wifiGrudge; name = "Wi-Fi"; }
  if (grudges.weatherGrudge > value) { value = grudges.weatherGrudge; name = "Weather"; }
  if (grudges.rebootGrudge > value) { value = grudges.rebootGrudge; name = "Reboot"; }
  if (grudges.neglectGrudge > value) { value = grudges.neglectGrudge; name = "Neglect"; }

  if (value <= 0) return "None";
  return name + " (" + String(value) + ")";
}

String getGrudgeSummary() {
  if (!isHoldingGrudge()) {
    return "No serious grudge. Yet.";
  }

  return String("Worst: ") + getWorstGrudgeName() + " · Total: " + String(getTotalGrudge());
}

String getMemoryBiasSummary() {
  if (!memoryEnabled) return "Memory disabled";
  if (needsEnabled && needs.irritation >= 70) return String("Needs bias: ") + getNeedsBiasSummary();
  if (needsEnabled && needs.boredom >= 75) return String("Needs bias: ") + getNeedsBiasSummary();
  if (needsEnabled && needs.loneliness >= 75) return String("Needs bias: ") + getNeedsBiasSummary();
  if (dailyMemory.yesterdayPokes >= 8) return "Yesterday bias: poke crimes remembered";
  if (dailyMemory.yesterdayBadWifi >= 5) return "Yesterday bias: router trauma remembered";
  if (dailyMemory.yesterdayShakes >= 4) return "Yesterday bias: motion offenses remembered";
  if (!isHoldingGrudge()) return "Mostly neutral; keeping receipts anyway";

  String worst = getWorstGrudgeName();
  if (grudges.pokeGrudge >= 70) return "Poke reactions strongly biased angry/smug";
  if (grudges.shakeGrudge >= 70) return "Shake reactions strongly biased angry";
  if (grudges.wifiGrudge >= 70) return "Wi-Fi tantrums extended";
  if (grudges.weatherGrudge >= 70) return "Weather failures bias annoyed/sad";
  return String("Mild reaction bias from ") + worst;
}

YetiMood biasMoodWithGrudge(YetiMood proposedMood, YetiEvent event) {
  if (!memoryEnabled) {
    return proposedMood;
  }

  switch (event) {
    case EVENT_POKED:
      if (grudges.pokeGrudge >= 70) return MOOD_ANGRY;
      if (grudges.pokeGrudge >= 40 && proposedMood == MOOD_ANNOYED) return MOOD_ANGRY;
      break;

    case EVENT_GYRO_SHAKEN:
      if (grudges.shakeGrudge >= 35) return MOOD_ANGRY;
      break;

    case EVENT_GYRO_TILTED:
      if (grudges.shakeGrudge >= 65) return MOOD_ANNOYED;
      break;

    case EVENT_WIFI_WEAK:
    case EVENT_WIFI_VERY_WEAK:
    case EVENT_WIFI_FAILED:
      if (grudges.wifiGrudge >= 65) return MOOD_ANGRY;
      if (grudges.wifiGrudge >= 35 && proposedMood == MOOD_CURIOUS) return MOOD_ANNOYED;
      break;

    case EVENT_WEATHER_FAILED:
      if (grudges.weatherGrudge >= 70) return MOOD_ANGRY;
      if (grudges.weatherGrudge >= 30) return MOOD_ANNOYED;
      break;

    case EVENT_WEATHER_UPDATED:
      if (grudges.weatherGrudge >= 50 && proposedMood == MOOD_WEATHER) return MOOD_SAD;
      break;

    case EVENT_REBOOT_REQUESTED:
      if (grudges.rebootGrudge >= 45) return MOOD_ANNOYED;
      break;

    default:
      break;
  }

  return proposedMood;
}

unsigned long biasDurationWithGrudge(unsigned long durationMs, YetiEvent event) {
  if (!memoryEnabled) {
    return durationMs;
  }

  int g = 0;
  switch (event) {
    case EVENT_POKED: g = grudges.pokeGrudge; break;
    case EVENT_GYRO_SHAKEN:
    case EVENT_GYRO_TILTED: g = grudges.shakeGrudge; break;
    case EVENT_WIFI_FAILED:
    case EVENT_WIFI_WEAK:
    case EVENT_WIFI_VERY_WEAK: g = grudges.wifiGrudge; break;
    case EVENT_WEATHER_FAILED:
    case EVENT_WEATHER_UPDATED: g = grudges.weatherGrudge; break;
    case EVENT_REBOOT_REQUESTED: g = grudges.rebootGrudge; break;
    default: g = getTotalGrudge() / 6; break;
  }

  if (g <= 0) return durationMs;
  unsigned long extra = (unsigned long)min(g, 100) * 45UL;
  return durationMs + extra;
}

void adjustNeedValue(int &value, int delta, bool markDirtyNow) {
  int oldValue = value;
  value = clampPercent(value + delta);
  if (value != oldValue && markDirtyNow) {
    markMemoryDirty();
  }
}

void adjustNeeds(int boredomDelta, int energyDelta, int irritationDelta, int lonelinessDelta, bool markDirtyNow) {
  adjustNeedValue(needs.boredom, boredomDelta, markDirtyNow);
  adjustNeedValue(needs.energy, energyDelta, markDirtyNow);
  adjustNeedValue(needs.irritation, irritationDelta, markDirtyNow);
  adjustNeedValue(needs.loneliness, lonelinessDelta, markDirtyNow);
}

String getNeedsSummary() {
  if (!memoryEnabled) return "Memory disabled";
  if (!needsEnabled) return "Needs drift disabled";

  if (needs.irritation >= 80) return "Highly irritated";
  if (needs.boredom >= 80) return "Bored and plotting";
  if (needs.loneliness >= 80) return "Lonely little cryptid";
  if (needs.energy <= 20) return "Low energy / sleepy";
  if (needs.energy >= 85 && needs.boredom < 40) return "Oddly energized";
  return "Emotionally idling";
}

String getNeedsBiasSummary() {
  if (!memoryEnabled || !needsEnabled) return "needs off";
  if (needs.irritation >= 70) return "irritation bias";
  if (needs.boredom >= 70) return "boredom bias";
  if (needs.loneliness >= 70) return "loneliness bias";
  if (needs.energy <= 30) return "low energy bias";
  return "needs neutral";
}

YetiMood moodFromNeeds() {
  if (needs.irritation >= 90) return MOOD_ANGRY;
  if (needs.irritation >= 74) return MOOD_ANNOYED;
  if (needs.energy <= 18) return MOOD_SLEEPY;
  if (needs.loneliness >= 82) return MOOD_SAD;
  if (needs.boredom >= 88 && personality.chaos >= 55) return MOOD_SMUG;
  if (needs.boredom >= 76) return MOOD_CURIOUS;
  if (needs.energy >= 88 && relationship.affection >= 55 && needs.irritation < 35) return MOOD_HAPPY;
  return baseMood;
}

bool shouldNeedsTriggerMood() {
  if (!memoryEnabled || !needsEnabled || !faceDisplayAvailable()) return false;
  if (sequenceActive() || demoMode) return false;
  if (sensorOverridePriority > 0 && millis() < sensorOverrideUntil) return false;
  if (currentMoodPriority > PRIORITY_IDLE) return false;
  if (moodTemporaryActive()) return false;
  if (millis() - lastInteractionMs < 20000UL) return false;
  if (lastNeedsMoodMs > 0 && millis() - lastNeedsMoodMs < YETI_NEEDS_MOOD_COOLDOWN_MS) return false;

  return needs.irritation >= 74 || needs.boredom >= 78 || needs.loneliness >= 82 || needs.energy <= 18 || needs.energy >= 90;
}

void updateNeeds() {
  if (!memoryEnabled || !needsEnabled) {
    return;
  }

  unsigned long now = millis();
  if (lastNeedsUpdateMs == 0) {
    lastNeedsUpdateMs = now;
    lastNeedsPersistMarkMs = now;
    return;
  }

  if (now - lastNeedsUpdateMs < YETI_NEEDS_UPDATE_INTERVAL_MS) {
    return;
  }

  unsigned long elapsed = now - lastNeedsUpdateMs;
  int steps = (int)min(6UL, elapsed / YETI_NEEDS_UPDATE_INTERVAL_MS);
  lastNeedsUpdateMs = now;

  int boredomDelta = 0;
  int energyDelta = 0;
  int irritationDelta = 0;
  int lonelinessDelta = 0;

  for (int i = 0; i < steps; i++) {
    unsigned long idleAge = now - lastInteractionMs;

    if (idleAge > 300000UL) {
      boredomDelta += 2;
      lonelinessDelta += 1;
    } else if (idleAge > 90000UL) {
      boredomDelta += 1;
    } else {
      boredomDelta -= 2;
      lonelinessDelta -= 1;
    }

    if (sequenceActive() || moodTemporaryActive()) {
      energyDelta -= 1;
    } else if (needs.energy < 72) {
      energyDelta += 1;
    }

    if (clockSynced()) {
      time_t raw = time(nullptr) + activeClockOffsetSeconds();
      struct tm localTime;
      gmtime_r(&raw, &localTime);
      if (localTime.tm_hour >= 23 || localTime.tm_hour < 6) {
        energyDelta -= 1;
        if (needs.energy < 35) boredomDelta -= 1;
      } else if (localTime.tm_hour >= 9 && localTime.tm_hour < 18 && needs.energy < 80) {
        energyDelta += 1;
      }
    }

    if (needs.irritation > 0 && getTotalGrudge() < 120) {
      irritationDelta -= 1;
    } else if (getTotalGrudge() >= 220) {
      irritationDelta += 1;
    }
  }

  int beforeBoredom = needs.boredom;
  int beforeEnergy = needs.energy;
  int beforeIrritation = needs.irritation;
  int beforeLoneliness = needs.loneliness;

  adjustNeeds(boredomDelta, energyDelta, irritationDelta, lonelinessDelta, false);

  bool changed = needs.boredom != beforeBoredom || needs.energy != beforeEnergy || needs.irritation != beforeIrritation || needs.loneliness != beforeLoneliness;
  if (changed) {
    needsEventCount++;
    lastNeedsNote = String("Needs drift: boredom ") + String(boredomDelta) + ", energy " + String(energyDelta) + ", irritation " + String(irritationDelta) + ", loneliness " + String(lonelinessDelta);

    // Needs drift is allowed to survive reboot, but not at the cost of flash abuse.
    // Mark dirty at most every 10 minutes for passive drift.
    if (now - lastNeedsPersistMarkMs >= YETI_NEEDS_PERSIST_INTERVAL_MS) {
      lastNeedsPersistMarkMs = now;
      markMemoryDirty();
    }
  }

  if (shouldNeedsTriggerMood()) {
    YetiMood needsMood = moodFromNeeds();
    if (needsMood != baseMood) {
      unsigned long duration = idleMoodDurationMs(needsMood);
      if (setMood(needsMood, duration, PRIORITY_IDLE, false)) {
        lastNeedsMoodMs = now;
        lastNeedsNote = String("Needs triggered mood: ") + moodToString(needsMood) + " (" + getNeedsBiasSummary() + ")";
        addEvent("Needs", lastNeedsNote);
      }
    }
  }
}

void recordMemoryEvent(YetiEvent event) {
  if (!memoryEnabled) {
    return;
  }

  memoryEventCount++;
  lastMemoryEvent = yetiEventName(event);
  lastMemoryNote = "Recorded event";
  recordDailyEvent(event);

  switch (event) {
    case EVENT_BOOT:
      memoryStats.lifetimeBoots++;
      adjustRelationship(0, 0, 0, 1);
      adjustNeeds(0, -2, 0, 0, true);
      lastMemoryNote = "Boot remembered";
      break;

    case EVENT_POKED:
      memoryStats.lifetimePokes++;
      adjustRelationship(-1, 5, -1, 2);
      adjustNeeds(-2, -1, 8, -1, true);
      increaseGrudge(event, relationship.annoyance >= 65 ? 9 : 6);
      lastMemoryNote = "Poke logged as evidence";
      break;

    case EVENT_GYRO_SHAKEN:
      memoryStats.lifetimeShakes++;
      adjustRelationship(-2, 7, -2, 3);
      adjustNeeds(-4, -5, 12, -1, true);
      increaseGrudge(event, relationship.annoyance >= 65 ? 12 : 8);
      lastMemoryNote = "Shake logged; tiny trust damage applied";
      break;

    case EVENT_GYRO_TILTED:
      memoryStats.lifetimeTilts++;
      adjustRelationship(0, 2, 0, 1);
      adjustNeeds(-2, -2, 4, -1, true);
      increaseGrudge(event, 3);
      lastMemoryNote = "Tilt/pickup remembered";
      break;

    case EVENT_WIFI_FAILED:
    case EVENT_WIFI_WEAK:
    case EVENT_WIFI_VERY_WEAK:
      memoryStats.lifetimeBadWifi++;
      adjustRelationship(0, event == EVENT_WIFI_VERY_WEAK ? 5 : 3, 0, 1);
      adjustNeeds(1, -1, event == EVENT_WIFI_VERY_WEAK ? 6 : 3, 0, true);
      increaseGrudge(event, event == EVENT_WIFI_VERY_WEAK ? 8 : 5);
      lastMemoryNote = "Network grievance filed";
      break;

    case EVENT_WEATHER_FAILED:
      memoryStats.lifetimeWeatherFails++;
      adjustRelationship(0, 2, 0, 1);
      adjustNeeds(1, 0, 3, 0, true);
      increaseGrudge(event, 5);
      lastMemoryNote = "Weather failure remembered";
      break;

    case EVENT_WEBUI_OPENED:
      memoryStats.lifetimeWebVisits++;
      adjustRelationship(1, -1, 0, 0);
      adjustNeeds(-8, 1, -2, -15, true);
      adjustGrudgeValue(grudges.neglectGrudge, -1);
      lastMemoryNote = "Attention received";
      break;

    case EVENT_WEBUI_COMMAND:
      memoryStats.lifetimeCommands++;
      adjustRelationship(0, 0, 1, 0);
      adjustNeeds(-5, 0, -1, -3, true);
      lastMemoryNote = "Command remembered";
      break;

    case EVENT_REBOOT_REQUESTED:
      memoryStats.lifetimeReboots++;
      adjustRelationship(-1, 4, -1, 2);
      adjustNeeds(0, -3, 5, 1, true);
      increaseGrudge(event, 7);
      lastMemoryNote = "Reboot remembered suspiciously";
      break;

    case EVENT_SETTINGS_SAVED:
      memoryStats.lifetimeSettingsSaves++;
      adjustRelationship(1, -1, 3, -1);
      adjustNeeds(-3, 0, -3, -2, true);
      reduceAllGrudges(1);
      lastMemoryNote = "Settings save improved trust";
      break;

    case EVENT_WEATHER_UPDATED:
      adjustGrudgeValue(grudges.weatherGrudge, -2);
      adjustNeeds(0, 0, -1, 0, true);
      lastMemoryNote = "Weather success slightly reduced weather grudge";
      break;

    case EVENT_WIFI_CONNECTED:
    case EVENT_WIFI_RESTORED:
      adjustGrudgeValue(grudges.wifiGrudge, -2);
      adjustNeeds(0, 0, -2, 0, true);
      lastMemoryNote = "Network behaving; Wi-Fi grudge softened";
      break;

    default:
      // Some events are useful as live diagnostics but do not need persistent writes.
      break;
  }

}


int getToleranceScore() {
  if (!memoryEnabled) return 0;

  int score = 50;
  score += relationship.affection / 3;
  score += relationship.trust / 4;
  score += needs.energy / 8;
  score -= relationship.annoyance / 3;
  score -= relationship.suspicion / 5;
  score -= needs.irritation / 4;
  score -= getTotalGrudge() / 12;

  if (dailyMemory.todayPokes >= 5) score -= 8;
  if (dailyMemory.todayShakes >= 3) score -= 10;
  if (dailyMemory.todayWebVisits > 0 && dailyMemory.todayPokes == 0 && dailyMemory.todayShakes == 0) score += 4;
  if (dailyMemory.yesterdayHappyMoodCount > dailyMemory.yesterdayAngryMoodCount) score += 3;

  return clampPercent(score);
}

int getForgivenessScore() {
  if (!memoryEnabled) return 0;

  int score = 70;
  score += relationship.trust / 5;
  score += relationship.affection / 6;
  score -= relationship.annoyance / 3;
  score -= relationship.suspicion / 4;
  score -= getTotalGrudge() / 10;
  score -= needs.irritation / 5;

  if (dailyMemory.todayPokes >= 8) score -= 10;
  if (dailyMemory.todayShakes >= 4) score -= 12;
  if (dailyMemory.todayCommands >= 5) score += 3;

  return clampPercent(score);
}

String getJudgmentLabel() {
  if (!memoryEnabled) return "Memory disabled";

  int tolerance = getToleranceScore();
  int forgiveness = getForgivenessScore();
  int totalGrudge = getTotalGrudge();

  if (needsEnabled && needs.irritation >= 90) return "Irritated little cryptid";
  if (totalGrudge >= 340) return "Holding a cursed ledger";
  if (grudges.pokeGrudge >= 80 || dailyMemory.todayPokes >= 10) return "Poke-fatigued";
  if (grudges.wifiGrudge >= 80 || dailyMemory.todayBadWifi >= 8) return "Router-traumatized";
  if (grudges.shakeGrudge >= 80 || dailyMemory.todayShakes >= 5) return "Motion-offended";
  if (needsEnabled && needs.boredom >= 90) return "Bored and legally dangerous";
  if (needsEnabled && needs.loneliness >= 90) return "Lonely but pretending not to care";
  if (needsEnabled && needs.energy <= 15) return "Emotionally in low-power mode";
  if (relationship.annoyance >= 85 || tolerance <= 18) return "Actively judging you";
  if (relationship.trust >= 80 && relationship.affection >= 65 && forgiveness >= 65) return "Trusting, unfortunately";
  if (relationship.affection >= 70 && relationship.annoyance < 35) return "Fondly annoyed";
  if (relationship.suspicion >= 70) return "Suspicious but entertained";
  if (relationship.annoyance >= 50 || totalGrudge >= 120) return "Mildly disappointed";
  if (dailyMemory.yesterdayPokes >= 8 || dailyMemory.yesterdayShakes >= 4) return "Remembering yesterday loudly";
  return "Technically tolerant";
}

String getTrustStatus() {
  if (!memoryEnabled) return "offline";
  if (relationship.trust >= 85) return "Unfortunately high";
  if (relationship.trust >= 65) return "Technically acceptable";
  if (relationship.trust >= 40) return "Under observation";
  if (relationship.trust >= 20) return "Thin ice with tiny footprints";
  return "Trust not found";
}

String getRelationshipTone() {
  if (!memoryEnabled) return "No opinion available";
  int tolerance = getToleranceScore();
  if (relationship.affection >= 75 && relationship.annoyance <= 35) return "Fond but pretending to be above it";
  if (relationship.annoyance >= 75 && relationship.suspicion >= 60) return "Hostile paperwork energy";
  if (relationship.suspicion >= 75) return "Watching you from the corner of the OLED";
  if (tolerance >= 75) return "Surprisingly cooperative";
  if (tolerance <= 25) return "Barely tolerating this arrangement";
  return "Classic YETI: grumpy but present";
}

String getMoodBiasSummary() {
  if (!memoryEnabled) return "Memory disabled";
  if (needsEnabled && needs.irritation >= 75) return "Leaning annoyed/angry from irritation";
  if (grudges.pokeGrudge >= 65) return "Poke grudge is biasing reactions angry/smug";
  if (grudges.shakeGrudge >= 65) return "Shake grudge is biasing reactions angry";
  if (grudges.wifiGrudge >= 65) return "Wi-Fi grudge is extending tantrums";
  if (grudges.weatherGrudge >= 65) return "Weather grudge is biasing dramatic reactions";
  if (needsEnabled && needs.boredom >= 75) return "Boredom bias: curious/smug weirdness likely";
  if (needsEnabled && needs.loneliness >= 75) return "Loneliness bias: sad/sleepy reactions likely";
  if (needsEnabled && needs.energy <= 25) return "Low-energy bias: sleepy reactions likely";
  if (dailyMemory.yesterdayPokes >= 8) return "Yesterday's pokes are still affecting attitude";
  if (dailyMemory.yesterdayBadWifi >= 5) return "Yesterday's Wi-Fi crimes remain relevant";
  if (relationship.affection >= 70 && relationship.annoyance < 40) return "Mild friendly bias, against his better judgment";
  return getMemoryBiasSummary();
}

String getCurrentGrievance() {
  if (!memoryEnabled) return "Memory disabled";

  if (dailyMemory.todayPokes >= 8) return String("Today: poked ") + String(dailyMemory.todayPokes) + " times. Charges pending.";
  if (dailyMemory.todayShakes >= 4) return String("Today: shaken ") + String(dailyMemory.todayShakes) + " times. Unacceptable.";
  if (dailyMemory.todayBadWifi >= 5) return String("Today: Wi-Fi betrayed him ") + String(dailyMemory.todayBadWifi) + " times.";
  if (dailyMemory.todayWeatherFails >= 4) return String("Today: weather failed ") + String(dailyMemory.todayWeatherFails) + " times.";
  if (isHoldingGrudge()) return String("Active grudge: ") + getWorstGrudgeName();
  if (dailyMemory.yesterdayPokes >= 8) return String("Yesterday's poke count remains suspicious: ") + String(dailyMemory.yesterdayPokes);
  if (dailyMemory.yesterdayShakes >= 4) return String("Yesterday's shake crimes remain on file: ") + String(dailyMemory.yesterdayShakes);
  if (needsEnabled && needs.loneliness >= 80) return "Insufficient attention logged";
  if (needsEnabled && needs.boredom >= 80) return "Boredom with witnesses";
  return getRecentGrievance();
}

String getForgivenessStatus() {
  if (!memoryEnabled) return "Memory disabled";
  int score = getForgivenessScore();

  if (score >= 85) return "Forgiven, but screenshots remain";
  if (score >= 65) return "Provisionally forgiven";
  if (score >= 45) return "Under review by the tiny grievance board";
  if (score >= 25) return "Apology queued, suspicion active";
  return "Denied with theatrical prejudice";
}

String getDailyOpinionSummary() {
  if (!memoryEnabled) return "Memory disabled";
  if (String(dailyMemory.lastMemoryDate).length() == 0) return "No clock-synced daily memory yet";

  if (dailyMemory.todayPokes == 0 && dailyMemory.todayShakes == 0 && dailyMemory.todayBadWifi == 0 && dailyMemory.todayWeatherFails == 0) {
    if (dailyMemory.todayWebVisits > 0 || dailyMemory.todayCommands > 0) return "Today is suspiciously civil";
    return "Today is quiet. Too quiet.";
  }

  if (dailyMemory.todayPokes >= dailyMemory.todayShakes && dailyMemory.todayPokes >= dailyMemory.todayBadWifi && dailyMemory.todayPokes > 0) {
    return String("Today's leading offense: pokes x") + String(dailyMemory.todayPokes);
  }

  if (dailyMemory.todayShakes >= dailyMemory.todayBadWifi && dailyMemory.todayShakes > 0) {
    return String("Today's leading offense: shakes x") + String(dailyMemory.todayShakes);
  }

  if (dailyMemory.todayBadWifi > 0) {
    return String("Today's leading offense: Wi-Fi crimes x") + String(dailyMemory.todayBadWifi);
  }

  return "Today is messy but documented";
}

String getMemoryFlavorText() {
  if (!memoryEnabled) return "The haunted ledger is closed.";

  int tolerance = getToleranceScore();
  int forgiveness = getForgivenessScore();
  int totalGrudge = getTotalGrudge();

  if (totalGrudge >= 300) return "YETI is not angry. YETI is archiving.";
  if (dailyMemory.todayPokes >= 10) return "The poke file is thick enough to require a tiny binder clip.";
  if (dailyMemory.todayShakes >= 5) return "YETI would like to remind you that he is not a maraca.";
  if (grudges.wifiGrudge >= 80) return "Router crimes have shaped his worldview.";
  if (needsEnabled && needs.boredom >= 90) return "He is one quiet minute away from becoming performance art.";
  if (needsEnabled && needs.loneliness >= 90) return "He misses attention but will deny this under oath.";
  if (needsEnabled && needs.energy <= 15) return "Currently operating on spite and OLED fumes.";
  if (forgiveness <= 25) return "Forgiveness engine returned 404.";
  if (tolerance >= 80) return "He may actually like this arrangement. Do not make it weird.";
  if (relationship.trust >= 80) return "Trust level is high, which he finds personally embarrassing.";
  if (relationship.affection >= 75) return "There may be affection here. He has filed a complaint about it.";
  return "He is keeping receipts, but the font is very small.";
}

String getOpinionSummary() {
  if (!memoryEnabled) return "Memory disabled; YETI is living in the moment against his will.";

  return String(getJudgmentLabel()) + " · " + getRelationshipTone() + " · " + getForgivenessStatus();
}

String getYetiReportCard() {
  if (!memoryEnabled) return "Memory disabled";
  return String("Tolerance ") + String(getToleranceScore()) + "% · Forgiveness " + String(getForgivenessScore()) + "% · Trust: " + getTrustStatus();
}

String getYetiJudgment() {
  return getJudgmentLabel();
}

String getRecentGrievance() {
  if (isHoldingGrudge()) {
    return String("Currently holding: ") + getWorstGrudgeName();
  }

  if (memoryStats.lifetimePokes >= memoryStats.lifetimeShakes && memoryStats.lifetimePokes >= memoryStats.lifetimeBadWifi && memoryStats.lifetimePokes > 0) {
    return String("Poked ") + String(memoryStats.lifetimePokes) + " time" + (memoryStats.lifetimePokes == 1 ? "" : "s");
  }

  if (memoryStats.lifetimeShakes >= memoryStats.lifetimeBadWifi && memoryStats.lifetimeShakes > 0) {
    return String("Shaken ") + String(memoryStats.lifetimeShakes) + " time" + (memoryStats.lifetimeShakes == 1 ? "" : "s");
  }

  if (memoryStats.lifetimeBadWifi > 0) {
    return String("Bad Wi-Fi logged ") + String(memoryStats.lifetimeBadWifi) + " time" + (memoryStats.lifetimeBadWifi == 1 ? "" : "s");
  }

  if (memoryStats.lifetimeWeatherFails > 0) {
    return String("Weather failed ") + String(memoryStats.lifetimeWeatherFails) + " time" + (memoryStats.lifetimeWeatherFails == 1 ? "" : "s");
  }

  return "No major grievances yet. Suspicious.";
}

String getRelationshipSummary() {
  return String("A") + relationship.affection + " / N" + relationship.annoyance + " / T" + relationship.trust + " / S" + relationship.suspicion;
}

const char* phraseCategoryName(YetiPhraseCategory category) {
  switch (category) {
    case PHRASE_POKE: return "poke";
    case PHRASE_SHAKE: return "shake";
    case PHRASE_WIFI: return "wifi";
    case PHRASE_WEATHER: return "weather";
    case PHRASE_IDLE: return "idle";
    case PHRASE_BOOT: return "boot";
    case PHRASE_FORGIVE: return "forgive";
    case PHRASE_PRAISE: return "praise";
    case PHRASE_GRUDGE: return "grudge";
    case PHRASE_NEEDS: return "needs";
    case PHRASE_JUDGMENT: return "judgment";
    case PHRASE_NONE:
    default: return "none";
  }
}

String phraseCategoryLabel(YetiPhraseCategory category) {
  switch (category) {
    case PHRASE_POKE: return "Poke Receipt";
    case PHRASE_SHAKE: return "Motion Complaint";
    case PHRASE_WIFI: return "Router Crimes";
    case PHRASE_WEATHER: return "Weather Opinion";
    case PHRASE_IDLE: return "Idle Thought";
    case PHRASE_BOOT: return "Boot Remark";
    case PHRASE_FORGIVE: return "Forgiveness Desk";
    case PHRASE_PRAISE: return "Praise Audit";
    case PHRASE_GRUDGE: return "Grudge Ledger";
    case PHRASE_NEEDS: return "Needs Complaint";
    case PHRASE_JUDGMENT: return "Judgment";
    case PHRASE_NONE:
    default: return "None";
  }
}

YetiPhraseCategory stringToPhraseCategory(String name) {
  name.toLowerCase();
  name.replace("_", "-");
  name.trim();
  if (name == "poke") return PHRASE_POKE;
  if (name == "shake" || name == "motion") return PHRASE_SHAKE;
  if (name == "wifi" || name == "wi-fi") return PHRASE_WIFI;
  if (name == "weather") return PHRASE_WEATHER;
  if (name == "idle") return PHRASE_IDLE;
  if (name == "boot") return PHRASE_BOOT;
  if (name == "forgive") return PHRASE_FORGIVE;
  if (name == "praise") return PHRASE_PRAISE;
  if (name == "grudge") return PHRASE_GRUDGE;
  if (name == "needs") return PHRASE_NEEDS;
  if (name == "judgment" || name == "judge") return PHRASE_JUDGMENT;
  return PHRASE_NONE;
}

YetiPhraseCategory phraseCategoryForEvent(YetiEvent event) {
  switch (event) {
    case EVENT_POKED: return PHRASE_POKE;
    case EVENT_GYRO_SHAKEN:
    case EVENT_GYRO_TILTED: return PHRASE_SHAKE;
    case EVENT_WIFI_FAILED:
    case EVENT_WIFI_WEAK:
    case EVENT_WIFI_VERY_WEAK: return PHRASE_WIFI;
    case EVENT_WEATHER_FAILED:
    case EVENT_WEATHER_UPDATED: return PHRASE_WEATHER;
    case EVENT_BOOT: return PHRASE_BOOT;
    case EVENT_GYRO_IDLE_LONG: return PHRASE_NEEDS;
    case EVENT_SETTINGS_SAVED: return PHRASE_PRAISE;
    default: return PHRASE_NONE;
  }
}

uint32_t sassCooldownMs() {
  uint8_t freq = sanitizeSensitivity(sassFrequency, 35);
  uint32_t span = YETI_SASS_COOLDOWN_MAX_MS - YETI_SASS_COOLDOWN_MIN_MS;
  return YETI_SASS_COOLDOWN_MAX_MS - ((span * (uint32_t)freq) / 100UL);
}

bool sassCooldownReady(bool force) {
  if (force) return true;
  unsigned long now = millis();
  if (lastSassPhraseMs == 0) return true;
  return now - lastSassPhraseMs >= sassCooldownMs();
}

bool canShowSassPhrase(bool force, YetiPhraseCategory category) {
  if (setupMode) {
    lastSassNote = "Suppressed: setup portal active";
    sassSuppressedCount++;
    return false;
  }

  if (!oledReady) {
    lastSassNote = "Suppressed: OLED unavailable";
    sassSuppressedCount++;
    return false;
  }

  if (!force && !sassTickerEnabled) {
    lastSassNote = "Suppressed: sass ticker disabled";
    sassSuppressedCount++;
    return false;
  }

  if (!force && sassIdleOnly && category != PHRASE_IDLE && category != PHRASE_NEEDS && category != PHRASE_GRUDGE && category != PHRASE_JUDGMENT) {
    lastSassNote = "Suppressed: idle-only sass enabled";
    sassSuppressedCount++;
    return false;
  }

  if (!force && !sassEventEnabled && category != PHRASE_IDLE && category != PHRASE_NEEDS && category != PHRASE_GRUDGE && category != PHRASE_JUDGMENT) {
    lastSassNote = "Suppressed: event sass disabled";
    sassSuppressedCount++;
    return false;
  }

  if (!force && category == PHRASE_GRUDGE && !sassGrudgeEnabled) {
    lastSassNote = "Suppressed: grudge sass disabled";
    sassSuppressedCount++;
    return false;
  }

  if (oledOverlayActive) {
    lastSassNote = String("Suppressed: OLED already showing ") + oledOverlayModeLabel(oledOverlayMode);
    sassSuppressedCount++;
    return false;
  }

  if (!force && sequenceActive()) {
    lastSassNote = "Suppressed: active sequence running";
    sassSuppressedCount++;
    return false;
  }

  if (!sassCooldownReady(force)) {
    lastSassNote = "Suppressed: sass cooldown";
    sassSuppressedCount++;
    return false;
  }

  return true;
}

uint32_t sassTickerDurationFor(String text) {
  uint16_t width = oledTextWidth(text, 2);
  uint32_t travelPixels = SCREEN_WIDTH + width + 24;
  uint32_t duration = ((travelPixels * YETI_WEATHER_TICKER_STEP_MS) / YETI_WEATHER_TICKER_PX_PER_STEP) + 1000;
  if (duration < YETI_SASS_TICKER_MIN_MS) duration = YETI_SASS_TICKER_MIN_MS;
  if (duration > YETI_SASS_TICKER_MAX_MS) duration = YETI_SASS_TICKER_MAX_MS;
  return duration;
}

const char* pickPhrase(YetiPhraseCategory category) {
  switch (category) {
    case PHRASE_POKE:
      if (grudges.pokeGrudge >= 70) {
        switch (random(4)) {
          case 0: return "I remember every poke.";
          case 1: return "Your file remains open.";
          case 2: return "Again? Bold.";
          default: return "Hands off the tiny cryptid.";
        }
      }
      switch (random(4)) {
        case 0: return "Rude.";
        case 1: return "Personal space, goblin.";
        case 2: return "Touch logged.";
        default: return "That was unnecessary.";
      }

    case PHRASE_SHAKE:
      switch (random(5)) {
        case 0: return "I live here, you know.";
        case 1: return "Unhand the rectangle.";
        case 2: return "Violence logged.";
        case 3: return "Motion complaint filed.";
        default: return "Desk earthquake detected.";
      }

    case PHRASE_WIFI:
      switch (random(5)) {
        case 0: return "Router crimes detected.";
        case 1: return "Signal weak. Mood weaker.";
        case 2: return "The air is full of soup.";
        case 3: return "Wi-Fi is doing crimes.";
        default: return "Packet sadness logged.";
      }

    case PHRASE_WEATHER:
      if (weatherAvailable && !weatherMetric && weatherTemperature <= 40.0f) return "Cold. Excellent.";
      if (weatherAvailable && !weatherMetric && weatherTemperature >= 85.0f) return "Heat detected. Dislike engaged.";
      if (!weatherAvailable) return "Weather oracle failed.";
      switch (random(4)) {
        case 0: return "Rain. Dramatic.";
        case 1: return "Sky gossip received.";
        case 2: return "Forecast archived suspiciously.";
        default: return "Weather noted. Begrudgingly.";
      }

    case PHRASE_IDLE:
      if (needs.boredom >= 75) return "Blinking is my cardio.";
      if (needs.loneliness >= 70) return "Still here.";
      switch (random(6)) {
        case 0: return "I saw that.";
        case 1: return "Processing disappointment.";
        case 2: return "Tiny uptime. Large opinions.";
        case 3: return "Emotionally buffering.";
        case 4: return "Standing by. Judging.";
        default: return "This is fine. Suspiciously.";
      }

    case PHRASE_BOOT:
      switch (random(4)) {
        case 0: return "Booted. Unfortunately aware.";
        case 1: return "Tiny brain online.";
        case 2: return "Grudge ledger restored.";
        default: return "Consciousness resumed.";
      }

    case PHRASE_FORGIVE:
      switch (random(4)) {
        case 0: return "Apology filed.";
        case 1: return "Forgiveness pending review.";
        case 2: return "Suspicious, but accepted.";
        default: return "Your crimes are reduced.";
      }

    case PHRASE_PRAISE:
      switch (random(4)) {
        case 0: return "Praise accepted.";
        case 1: return "Correct.";
        case 2: return "Suspiciously kind.";
        default: return "Compliment archived.";
      }

    case PHRASE_GRUDGE:
      if (getTotalGrudge() >= 250) return "Forgiveness not found.";
      switch (random(5)) {
        case 0: return "I remember.";
        case 1: return "Your file remains open.";
        case 2: return "The ledger grows.";
        case 3: return "Grudge saved successfully.";
        default: return "Receipts retained.";
      }

    case PHRASE_NEEDS:
      if (needs.irritation >= 75) return "Internal gremlin pressure high.";
      if (needs.energy <= 25) return "Battery of the soul low.";
      if (needs.loneliness >= 70) return "Attention deficit detected.";
      if (needs.boredom >= 80) return "Boredom is becoming structural.";
      return "Needs updated. Dramatically.";

    case PHRASE_JUDGMENT:
      if (getCurrentGrievance().length() > 0 && getCurrentGrievance() != "No major grievance filed today. Yet.") {
        String grievance = getCurrentGrievance();
        if (grievance.indexOf("poke") >= 0 || grievance.indexOf("Poke") >= 0) return "Poke crimes remain active.";
        if (grievance.indexOf("Wi-Fi") >= 0) return "Router trauma persists.";
      }
      switch (random(5)) {
        case 0: return "Judgment rendered.";
        case 1: return "Technically tolerated.";
        case 2: return "Mildly disappointed.";
        case 3: return "Trust level: complicated.";
        default: return "Opinion archived.";
      }

    case PHRASE_NONE:
    default:
      return "No comment. Suspicious.";
  }
}

bool showSassPhrase(String phrase, YetiPhraseCategory category, bool force) {
  phrase.trim();
  if (phrase.length() == 0) {
    phrase = pickPhrase(category);
  }
  if (phrase.length() > 72) {
    phrase = phrase.substring(0, 72);
  }

  if (!canShowSassPhrase(force, category)) {
    return false;
  }

  oledTickerText = phrase;
  oledTickerTextWidth = oledTextWidth(oledTickerText, 2);
  lastSassPhrase = phrase;
  lastSassCategory = phraseCategoryName(category);
  lastSassPhraseMs = millis();
  sassPhraseCount++;
  lastSassNote = String("Displayed ") + phraseCategoryLabel(category) + ": " + phrase;

  startOledOverlay(OLED_OVERLAY_SASS_TICKER, sassTickerDurationFor(oledTickerText));
  drawSassTickerOverlay();
  addEvent("Sass", clipText(phrase, 60));
  return true;
}

bool maybeShowMemoryPhrase(YetiPhraseCategory category, bool force) {
  if (category == PHRASE_NONE) {
    category = PHRASE_IDLE;
  }

  if (!force) {
    int chance = 3 + (sanitizeSensitivity(sassFrequency, 35) / 4) + (sanitizeSensitivity(sassLevel, 55) / 8) + (personality.chaos / 10);
    if (category == PHRASE_GRUDGE && getTotalGrudge() >= 140) chance += 12;
    if (category == PHRASE_NEEDS && (needs.irritation >= 70 || needs.boredom >= 80 || needs.loneliness >= 75)) chance += 10;
    if (chance > 65) chance = 65;

    if (random(100) >= chance) {
      lastSassNote = String("Skipped ") + phraseCategoryLabel(category) + " by chance gate";
      return false;
    }
  }

  return showSassPhrase(pickPhrase(category), category, force);
}

void updateSassSystem() {
  if (!sassTickerEnabled || setupMode || !oledReady) {
    return;
  }

  unsigned long now = millis();
  if (lastSassCheckMs > 0 && now - lastSassCheckMs < YETI_SASS_IDLE_CHECK_MS) {
    return;
  }
  lastSassCheckMs = now;

  if (!faceDisplayAvailable() || sequenceActive()) {
    return;
  }

  YetiPhraseCategory category = PHRASE_IDLE;
  if (sassGrudgeEnabled && getTotalGrudge() >= 160) {
    category = PHRASE_GRUDGE;
  } else if (needsEnabled && (needs.irritation >= 70 || needs.boredom >= 80 || needs.loneliness >= 75 || needs.energy <= 25)) {
    category = PHRASE_NEEDS;
  } else if (random(100) < 25) {
    category = PHRASE_JUDGMENT;
  }

  maybeShowMemoryPhrase(category, false);
}

String buildMemoryJson() {
  unsigned long now = millis();
  long nextSaveIn = 0;

  if (memoryDirty) {
    unsigned long elapsed = now - lastMemorySaveMs;
    nextSaveIn = elapsed >= YETI_MEMORY_SAVE_INTERVAL_MS ? 0 : (long)(YETI_MEMORY_SAVE_INTERVAL_MS - elapsed);
  }

  String json = "{";
  json += "\"enabled\":" + String(memoryEnabled ? "true" : "false") + ",";
  json += "\"dirty\":" + String(memoryDirty ? "true" : "false") + ",";
  json += "\"saveIntervalMs\":" + String(YETI_MEMORY_SAVE_INTERVAL_MS) + ",";
  json += "\"lastSaveAgeMs\":" + String(lastMemorySaveMs > 0 ? now - lastMemorySaveMs : 0) + ",";
  json += "\"nextSaveInMs\":" + String(nextSaveIn) + ",";
  json += "\"lastMemoryEvent\":\"" + jsonEscape(lastMemoryEvent) + "\",";
  json += "\"lastMemoryNote\":\"" + jsonEscape(lastMemoryNote) + "\",";
  json += "\"memoryEventCount\":" + String(memoryEventCount) + ",";
  json += "\"sassEnabled\":" + String(sassTickerEnabled ? "true" : "false") + ",";
  json += "\"sassIdleOnly\":" + String(sassIdleOnly ? "true" : "false") + ",";
  json += "\"sassEventEnabled\":" + String(sassEventEnabled ? "true" : "false") + ",";
  json += "\"sassGrudgeEnabled\":" + String(sassGrudgeEnabled ? "true" : "false") + ",";
  json += "\"sassLevel\":" + String(sassLevel) + ",";
  json += "\"sassFrequency\":" + String(sassFrequency) + ",";
  json += "\"sassCooldownMs\":" + String(sassCooldownMs()) + ",";
  json += "\"lastSassPhrase\":\"" + jsonEscape(lastSassPhrase) + "\",";
  json += "\"lastSassCategory\":\"" + jsonEscape(lastSassCategory) + "\",";
  json += "\"lastSassNote\":\"" + jsonEscape(lastSassNote) + "\",";
  json += "\"sassPhraseCount\":" + String(sassPhraseCount) + ",";
  json += "\"sassSuppressedCount\":" + String(sassSuppressedCount) + ",";
  json += "\"lastSassAgeMs\":" + String(lastSassPhraseMs > 0 ? now - lastSassPhraseMs : 0) + ",";
  json += "\"judgment\":\"" + jsonEscape(getYetiJudgment()) + "\",";
  json += "\"judgmentLabel\":\"" + jsonEscape(getJudgmentLabel()) + "\",";
  json += "\"opinionSummary\":\"" + jsonEscape(getOpinionSummary()) + "\",";
  json += "\"currentGrievance\":\"" + jsonEscape(getCurrentGrievance()) + "\",";
  json += "\"forgivenessStatus\":\"" + jsonEscape(getForgivenessStatus()) + "\",";
  json += "\"memoryFlavorText\":\"" + jsonEscape(getMemoryFlavorText()) + "\",";
  json += "\"trustStatus\":\"" + jsonEscape(getTrustStatus()) + "\",";
  json += "\"relationshipTone\":\"" + jsonEscape(getRelationshipTone()) + "\",";
  json += "\"dailyOpinionSummary\":\"" + jsonEscape(getDailyOpinionSummary()) + "\",";
  json += "\"yetiReportCard\":\"" + jsonEscape(getYetiReportCard()) + "\",";
  json += "\"toleranceScore\":" + String(getToleranceScore()) + ",";
  json += "\"forgivenessScore\":" + String(getForgivenessScore()) + ",";
  json += "\"recentGrievance\":\"" + jsonEscape(getRecentGrievance()) + "\",";
  json += "\"relationshipSummary\":\"" + jsonEscape(getRelationshipSummary()) + "\",";
  json += "\"grudgeSummary\":\"" + jsonEscape(getGrudgeSummary()) + "\",";
  json += "\"moodBiasSummary\":\"" + jsonEscape(getMoodBiasSummary()) + "\",";
  json += "\"memoryBiasSummary\":\"" + jsonEscape(getMemoryBiasSummary()) + "\",";
  json += "\"worstGrudge\":\"" + jsonEscape(getWorstGrudgeName()) + "\",";
  json += "\"totalGrudge\":" + String(getTotalGrudge()) + ",";
  json += "\"holdingGrudge\":" + String(isHoldingGrudge() ? "true" : "false") + ",";
  json += "\"lastGrudgeNote\":\"" + jsonEscape(lastGrudgeNote) + "\",";
  json += "\"grudgeEventCount\":" + String(grudgeEventCount) + ",";
  json += "\"grudgeDecayIntervalMs\":" + String(YETI_GRUDGE_DECAY_INTERVAL_MS) + ",";
  json += "\"lastGrudgeDecayAgeMs\":" + String(lastGrudgeDecayMs > 0 ? now - lastGrudgeDecayMs : 0) + ",";
  json += "\"needsEnabled\":" + String(needsEnabled ? "true" : "false") + ",";
  json += "\"needsSummary\":\"" + jsonEscape(getNeedsSummary()) + "\",";
  json += "\"needsBiasSummary\":\"" + jsonEscape(getNeedsBiasSummary()) + "\",";
  json += "\"lastNeedsNote\":\"" + jsonEscape(lastNeedsNote) + "\",";
  json += "\"needsEventCount\":" + String(needsEventCount) + ",";
  json += "\"lastNeedsUpdateAgeMs\":" + String(lastNeedsUpdateMs > 0 ? now - lastNeedsUpdateMs : 0) + ",";
  json += "\"lastNeedsMoodAgeMs\":" + String(lastNeedsMoodMs > 0 ? now - lastNeedsMoodMs : 0) + ",";
  json += "\"needsUpdateIntervalMs\":" + String(YETI_NEEDS_UPDATE_INTERVAL_MS) + ",";
  json += "\"needsMoodCooldownMs\":" + String(YETI_NEEDS_MOOD_COOLDOWN_MS) + ",";
  json += "\"dailySummary\":\"" + jsonEscape(getDailyMemorySummary()) + "\",";
  json += "\"todaySummary\":\"" + jsonEscape(getTodaySummary()) + "\",";
  json += "\"yesterdaySummary\":\"" + jsonEscape(getYesterdaySummary()) + "\",";
  json += "\"lastDailyNote\":\"" + jsonEscape(lastDailyNote) + "\",";
  json += "\"dailyRolloverCount\":" + String(dailyRolloverCount) + ",";
  json += "\"dailyDate\":\"" + jsonEscape(String(dailyMemory.lastMemoryDate)) + "\",";
  json += "\"currentDateKey\":\"" + jsonEscape(currentDailyMemoryDateKey()) + "\",";

  json += "\"relationship\":{";
  json += "\"affection\":" + String(relationship.affection) + ",";
  json += "\"annoyance\":" + String(relationship.annoyance) + ",";
  json += "\"trust\":" + String(relationship.trust) + ",";
  json += "\"suspicion\":" + String(relationship.suspicion);
  json += "},";

  json += "\"needs\":{";
  json += "\"boredom\":" + String(needs.boredom) + ",";
  json += "\"energy\":" + String(needs.energy) + ",";
  json += "\"irritation\":" + String(needs.irritation) + ",";
  json += "\"loneliness\":" + String(needs.loneliness);
  json += "},";

  json += "\"grudges\":{";
  json += "\"poke\":" + String(grudges.pokeGrudge) + ",";
  json += "\"shake\":" + String(grudges.shakeGrudge) + ",";
  json += "\"wifi\":" + String(grudges.wifiGrudge) + ",";
  json += "\"weather\":" + String(grudges.weatherGrudge) + ",";
  json += "\"reboot\":" + String(grudges.rebootGrudge) + ",";
  json += "\"neglect\":" + String(grudges.neglectGrudge);
  json += "},";

  json += "\"daily\":{";
  json += "\"today\":{";
  json += "\"pokes\":" + String(dailyMemory.todayPokes) + ",";
  json += "\"shakes\":" + String(dailyMemory.todayShakes) + ",";
  json += "\"badWifi\":" + String(dailyMemory.todayBadWifi) + ",";
  json += "\"weatherFails\":" + String(dailyMemory.todayWeatherFails) + ",";
  json += "\"webVisits\":" + String(dailyMemory.todayWebVisits) + ",";
  json += "\"commands\":" + String(dailyMemory.todayCommands) + ",";
  json += "\"angryMoodCount\":" + String(dailyMemory.todayAngryMoodCount) + ",";
  json += "\"happyMoodCount\":" + String(dailyMemory.todayHappyMoodCount);
  json += "},";
  json += "\"yesterday\":{";
  json += "\"pokes\":" + String(dailyMemory.yesterdayPokes) + ",";
  json += "\"shakes\":" + String(dailyMemory.yesterdayShakes) + ",";
  json += "\"badWifi\":" + String(dailyMemory.yesterdayBadWifi) + ",";
  json += "\"weatherFails\":" + String(dailyMemory.yesterdayWeatherFails) + ",";
  json += "\"angryMoodCount\":" + String(dailyMemory.yesterdayAngryMoodCount) + ",";
  json += "\"happyMoodCount\":" + String(dailyMemory.yesterdayHappyMoodCount);
  json += "}";
  json += "},";

  json += "\"stats\":{";
  json += "\"lifetimeBoots\":" + String(memoryStats.lifetimeBoots) + ",";
  json += "\"lifetimePokes\":" + String(memoryStats.lifetimePokes) + ",";
  json += "\"lifetimeShakes\":" + String(memoryStats.lifetimeShakes) + ",";
  json += "\"lifetimeTilts\":" + String(memoryStats.lifetimeTilts) + ",";
  json += "\"lifetimeBadWifi\":" + String(memoryStats.lifetimeBadWifi) + ",";
  json += "\"lifetimeWeatherFails\":" + String(memoryStats.lifetimeWeatherFails) + ",";
  json += "\"lifetimeWebVisits\":" + String(memoryStats.lifetimeWebVisits) + ",";
  json += "\"lifetimeCommands\":" + String(memoryStats.lifetimeCommands) + ",";
  json += "\"lifetimeReboots\":" + String(memoryStats.lifetimeReboots) + ",";
  json += "\"lifetimeSettingsSaves\":" + String(memoryStats.lifetimeSettingsSaves) + ",";
  json += "\"lifetimeMoodChanges\":" + String(memoryStats.lifetimeMoodChanges) + ",";
  json += "\"lifetimeSequences\":" + String(memoryStats.lifetimeSequences);
  json += "}";

  json += "}";
  return json;
}

// =====================================================
// BEHAVIOR CONFIG STORAGE
// =====================================================

void loadBehaviorConfig() {
  prefs.begin("yeti_cfg", true);

  configuredHostname = sanitizeHostname(prefs.getString("hostname", DEFAULT_HOSTNAME));

  shakeConfig.enabled = prefs.getBool("shakeEn", true);
  shakeConfig.holdMs = sanitizeHoldMs(prefs.getUInt("shakeMs", 1800), 1800);
  shakeConfig.expressionMask = validExpressionMask(prefs.getUInt("shakeMask", DEFAULT_SHAKE_MASK), DEFAULT_SHAKE_MASK);

  touchConfig.enabled = prefs.getBool("touchEn", true);
  touchConfig.holdMs = sanitizeHoldMs(prefs.getUInt("touchMs", 1600), 1600);
  touchConfig.expressionMask = validExpressionMask(prefs.getUInt("touchMask", DEFAULT_TOUCH_MASK), DEFAULT_TOUCH_MASK);

  faceTheme = FACE_THEME_CLASSIC;
  boredTimeoutMs = sanitizeBoredTimeoutMs(prefs.getUInt("boredMs", 180000), 180000);
  faceFrameMs = sanitizeFaceFrameMs(prefs.getUInt("frameMs", YETI_FACE_FRAME_DEFAULT_MS), YETI_FACE_FRAME_DEFAULT_MS);

  weatherEnabled = prefs.getBool("weatherEn", false);
  weatherLocationName = sanitizeLocationName(prefs.getString("weatherName", ""));
  weatherLatitude = sanitizeLatitude(prefs.getFloat("weatherLat", 0.0f));
  weatherLongitude = sanitizeLongitude(prefs.getFloat("weatherLon", 0.0f));
  weatherMetric = prefs.getBool("weatherMet", false);
  weatherUpdateMs = sanitizeWeatherUpdateMs(prefs.getUInt("weatherMs", YETI_WEATHER_UPDATE_DEFAULT_MS));
  weatherTickerShowLocation = prefs.getBool("wtLoc", true);
  weatherTickerShowTemp = prefs.getBool("wtTemp", true);
  weatherTickerShowCondition = prefs.getBool("wtCond", true);
  weatherTickerShowFeelsLike = prefs.getBool("wtFeel", true);
  weatherTickerShowHumidity = prefs.getBool("wtHum", true);
  weatherTickerShowWind = prefs.getBool("wtWind", true);
  weatherTickerShowPrecip = prefs.getBool("wtPrec", true);
  weatherTickerShowUpdated = prefs.getBool("wtUpd", true);

  clockEnabled = prefs.getBool("clockEn", true);
  clock24h = prefs.getBool("clock24h", false);
  clockUtcOffsetMinutes = sanitizeUtcOffsetMinutes(prefs.getInt("clockOffMin", -420));
  clockUseWeatherOffset = prefs.getBool("clockUseW", true);

  infoCardsEnabled = prefs.getBool("infoEn", true);
  infoCardClockEnabled = prefs.getBool("infoClock", true);
  infoCardWeatherEnabled = prefs.getBool("infoWeather", true);
  infoCardIntervalMs = sanitizeInfoCardIntervalMs(prefs.getUInt("infoMs", YETI_INFO_CARD_DEFAULT_MS));

  sleepModeEnabled = prefs.getBool("sleepEn", true);
  sleepStartMinute = sanitizeMinuteOfDay(prefs.getInt("sleepSt", YETI_SLEEP_START_DEFAULT_MIN), YETI_SLEEP_START_DEFAULT_MIN);
  sleepEndMinute = sanitizeMinuteOfDay(prefs.getInt("sleepEnd", YETI_SLEEP_END_DEFAULT_MIN), YETI_SLEEP_END_DEFAULT_MIN);
  sleepAnimDurationMs = sanitizeSleepAnimMs(prefs.getUInt("sleepAnim", YETI_SLEEP_ANIM_DEFAULT_MS), YETI_SLEEP_ANIM_DEFAULT_MS);
  sleepGapMinMs = sanitizeSleepGapMs(prefs.getUInt("sleepGapMin", YETI_SLEEP_GAP_MIN_DEFAULT_MS), YETI_SLEEP_GAP_MIN_DEFAULT_MS);
  sleepGapMaxMs = sanitizeSleepGapMs(prefs.getUInt("sleepGapMax", YETI_SLEEP_GAP_MAX_DEFAULT_MS), YETI_SLEEP_GAP_MAX_DEFAULT_MS);
  if (sleepGapMaxMs < sleepGapMinMs) sleepGapMaxMs = sleepGapMinMs;
  sleepWakeHoldMs = sanitizeSleepWakeHoldMs(prefs.getUInt("sleepHold", YETI_SLEEP_WAKE_HOLD_DEFAULT_MS), YETI_SLEEP_WAKE_HOLD_DEFAULT_MS);

  baseMood = stringToMood(prefs.getString("baseMood", "deadpan"));
  currentMood = baseMood;
  currentMoodPriority = PRIORITY_IDLE;
  moodUntilMs = 0;
  lastMoodChangeMs = millis();
  personality.grumpiness = sanitizePersonalityTrait(prefs.getInt("grumpy", 70), 70);
  personality.curiosity = sanitizePersonalityTrait(prefs.getInt("curious", 45), 45);
  personality.sleepiness = sanitizePersonalityTrait(prefs.getInt("sleepy", 40), 40);
  personality.chaos = sanitizePersonalityTrait(prefs.getInt("chaos", 30), 30);
  personality.friendliness = sanitizePersonalityTrait(prefs.getInt("friendly", 25), 25);
  idleMoodEnabled = prefs.getBool("idleMood", true);
  weatherMoodEnabled = prefs.getBool("moodWeather", true);
  wifiMoodEnabled = prefs.getBool("moodWifi", true);
  webMoodEnabled = prefs.getBool("moodWeb", true);
  movementMoodEnabled = prefs.getBool("moodMove", true);
  memoryEnabled = prefs.getBool("memoryEn", memoryEnabled);
  needsEnabled = prefs.getBool("needsEn", needsEnabled);
  sassTickerEnabled = prefs.getBool("sassEn", true);
  sassIdleOnly = prefs.getBool("sassIdle", false);
  sassEventEnabled = prefs.getBool("sassEvent", true);
  sassGrudgeEnabled = prefs.getBool("sassGrudg", true);
  sassLevel = sanitizeSensitivity(prefs.getUInt("sassLvl", 55), 55);
  sassFrequency = sanitizeSensitivity(prefs.getUInt("sassFreq", 35), 35);
  movementSensitivity = sanitizeSensitivity(prefs.getUInt("moveSens", 50), 50);
  shakeSensitivity = sanitizeSensitivity(prefs.getUInt("shakeSens", 50), 50);

  prefs.end();

#if !YETI_MPU_ENABLED
  shakeConfig.enabled = false;
  movementMoodEnabled = false;
#endif

#if !YETI_TOUCH_ENABLED
  touchConfig.enabled = false;
#endif

  lastInteractionMs = millis();
  lastIdleBehaviorMs = millis();
  lastIdleMoodMs = 0;
  lastIdleMood = baseMood;
  lastIdleReason = "Loaded personality settings";
  scheduleNextIdleBehavior();
  applyMood(currentMood);

  Serial.println("Behavior config loaded.");
}

void saveBehaviorConfig() {
  shakeConfig.holdMs = sanitizeHoldMs(shakeConfig.holdMs, 1800);
  shakeConfig.expressionMask = validExpressionMask(shakeConfig.expressionMask, DEFAULT_SHAKE_MASK);

  touchConfig.holdMs = sanitizeHoldMs(touchConfig.holdMs, 1600);
  touchConfig.expressionMask = validExpressionMask(touchConfig.expressionMask, DEFAULT_TOUCH_MASK);

  faceTheme = faceThemeFromId((int)faceTheme);
  configuredHostname = sanitizeHostname(configuredHostname);
  boredTimeoutMs = sanitizeBoredTimeoutMs(boredTimeoutMs, 180000);
  faceFrameMs = sanitizeFaceFrameMs(faceFrameMs, YETI_FACE_FRAME_DEFAULT_MS);

  weatherLocationName = sanitizeLocationName(weatherLocationName);
  weatherLatitude = sanitizeLatitude(weatherLatitude);
  weatherLongitude = sanitizeLongitude(weatherLongitude);
  weatherUpdateMs = sanitizeWeatherUpdateMs(weatherUpdateMs);

  clockUtcOffsetMinutes = sanitizeUtcOffsetMinutes(clockUtcOffsetMinutes);
  infoCardIntervalMs = sanitizeInfoCardIntervalMs(infoCardIntervalMs);
  sleepStartMinute = sanitizeMinuteOfDay(sleepStartMinute, YETI_SLEEP_START_DEFAULT_MIN);
  sleepEndMinute = sanitizeMinuteOfDay(sleepEndMinute, YETI_SLEEP_END_DEFAULT_MIN);
  sleepAnimDurationMs = sanitizeSleepAnimMs(sleepAnimDurationMs, YETI_SLEEP_ANIM_DEFAULT_MS);
  sleepGapMinMs = sanitizeSleepGapMs(sleepGapMinMs, YETI_SLEEP_GAP_MIN_DEFAULT_MS);
  sleepGapMaxMs = sanitizeSleepGapMs(sleepGapMaxMs, YETI_SLEEP_GAP_MAX_DEFAULT_MS);
  if (sleepGapMaxMs < sleepGapMinMs) sleepGapMaxMs = sleepGapMinMs;
  sleepWakeHoldMs = sanitizeSleepWakeHoldMs(sleepWakeHoldMs, YETI_SLEEP_WAKE_HOLD_DEFAULT_MS);

  personality.grumpiness = sanitizePersonalityTrait(personality.grumpiness, 70);
  personality.curiosity = sanitizePersonalityTrait(personality.curiosity, 45);
  personality.sleepiness = sanitizePersonalityTrait(personality.sleepiness, 40);
  personality.chaos = sanitizePersonalityTrait(personality.chaos, 30);
  personality.friendliness = sanitizePersonalityTrait(personality.friendliness, 25);
  movementSensitivity = sanitizeSensitivity(movementSensitivity, 50);
  shakeSensitivity = sanitizeSensitivity(shakeSensitivity, 50);
  sassLevel = sanitizeSensitivity(sassLevel, 55);
  sassFrequency = sanitizeSensitivity(sassFrequency, 35);

#if !YETI_MPU_ENABLED
  shakeConfig.enabled = false;
  movementMoodEnabled = false;
#endif

#if !YETI_TOUCH_ENABLED
  touchConfig.enabled = false;
#endif

  prefs.begin("yeti_cfg", false);

  prefs.putString("hostname", configuredHostname);

  prefs.putBool("shakeEn", shakeConfig.enabled);
  prefs.putUInt("shakeMs", shakeConfig.holdMs);
  prefs.putUInt("shakeMask", shakeConfig.expressionMask);

  prefs.putBool("touchEn", touchConfig.enabled);
  prefs.putUInt("touchMs", touchConfig.holdMs);
  prefs.putUInt("touchMask", touchConfig.expressionMask);

  prefs.putUInt("boredMs", boredTimeoutMs);
  prefs.putUInt("frameMs", faceFrameMs);

  prefs.putBool("weatherEn", weatherEnabled);
  prefs.putString("weatherName", weatherLocationName);
  prefs.putFloat("weatherLat", weatherLatitude);
  prefs.putFloat("weatherLon", weatherLongitude);
  prefs.putBool("weatherMet", weatherMetric);
  prefs.putUInt("weatherMs", weatherUpdateMs);
  prefs.putBool("wtLoc", weatherTickerShowLocation);
  prefs.putBool("wtTemp", weatherTickerShowTemp);
  prefs.putBool("wtCond", weatherTickerShowCondition);
  prefs.putBool("wtFeel", weatherTickerShowFeelsLike);
  prefs.putBool("wtHum", weatherTickerShowHumidity);
  prefs.putBool("wtWind", weatherTickerShowWind);
  prefs.putBool("wtPrec", weatherTickerShowPrecip);
  prefs.putBool("wtUpd", weatherTickerShowUpdated);

  prefs.putBool("clockEn", clockEnabled);
  prefs.putBool("clock24h", clock24h);
  prefs.putInt("clockOffMin", clockUtcOffsetMinutes);
  prefs.putBool("clockUseW", clockUseWeatherOffset);

  prefs.putBool("infoEn", infoCardsEnabled);
  prefs.putBool("infoClock", infoCardClockEnabled);
  prefs.putBool("infoWeather", infoCardWeatherEnabled);
  prefs.putUInt("infoMs", infoCardIntervalMs);

  prefs.putBool("sleepEn", sleepModeEnabled);
  prefs.putInt("sleepSt", sleepStartMinute);
  prefs.putInt("sleepEnd", sleepEndMinute);
  prefs.putUInt("sleepAnim", sleepAnimDurationMs);
  prefs.putUInt("sleepGapMin", sleepGapMinMs);
  prefs.putUInt("sleepGapMax", sleepGapMaxMs);
  prefs.putUInt("sleepHold", sleepWakeHoldMs);

  prefs.putString("baseMood", moodToString(baseMood));
  prefs.putInt("grumpy", personality.grumpiness);
  prefs.putInt("curious", personality.curiosity);
  prefs.putInt("sleepy", personality.sleepiness);
  prefs.putInt("chaos", personality.chaos);
  prefs.putInt("friendly", personality.friendliness);
  prefs.putBool("idleMood", idleMoodEnabled);
  prefs.putBool("moodWeather", weatherMoodEnabled);
  prefs.putBool("moodWifi", wifiMoodEnabled);
  prefs.putBool("moodWeb", webMoodEnabled);
  prefs.putBool("moodMove", movementMoodEnabled);
  prefs.putBool("memoryEn", memoryEnabled);
  prefs.putBool("needsEn", needsEnabled);
  prefs.putBool("sassEn", sassTickerEnabled);
  prefs.putBool("sassIdle", sassIdleOnly);
  prefs.putBool("sassEvent", sassEventEnabled);
  prefs.putBool("sassGrudg", sassGrudgeEnabled);
  prefs.putUInt("sassLvl", sassLevel);
  prefs.putUInt("sassFreq", sassFrequency);
  prefs.putUInt("moveSens", movementSensitivity);
  prefs.putUInt("shakeSens", shakeSensitivity);

  prefs.end();

  Serial.println("Behavior config saved.");
}

void triggerSensorReaction(String triggerName, SensorTriggerConfig &config, uint8_t priority, Expression fallbackExpression) {
  if (!config.enabled) {
    return;
  }

  markInteraction();

  unsigned long now = millis();

  if (sensorOverridePriority > 0 && now >= sensorOverrideUntil) {
    sensorOverridePriority = 0;
    lastTriggerName = "None";
  }

  if (sensorOverridePriority > 0 && priority < sensorOverridePriority) {
    return;
  }

  sensorOverrideExpression = chooseExpressionFromMask(config.expressionMask, fallbackExpression);
  sensorOverrideUntil = now + config.holdMs;
  sensorOverridePriority = priority;
  lastTriggerName = triggerName;

  Serial.print(triggerName);
  Serial.print(" reaction: ");
  Serial.print(expressionName(sensorOverrideExpression));
  Serial.print(" for ");
  Serial.print(config.holdMs);
  Serial.println(" ms");

  addEvent("Sensor", triggerName + String(" -> ") + expressionName(sensorOverrideExpression));
}

// =====================================================
// OLED STATUS HELPERS
// =====================================================

void oledStatus(String title, String line1, String line2, String line3, String line4, String line5 = "") {
  if (!oledReady) {
    return;
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  display.setCursor(0, 0);
  display.println(clipText(title, 21));

  display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

  display.setCursor(0, 14);
  display.println(clipText(line1, 21));

  display.setCursor(0, 24);
  display.println(clipText(line2, 21));

  display.setCursor(0, 34);
  display.println(clipText(line3, 21));

  display.setCursor(0, 44);
  display.println(clipText(line4, 21));

  display.setCursor(0, 54);
  display.println(clipText(line5, 21));

  display.display();
}

uint16_t oledTextWidth(String text, uint8_t size) {
  // Built-in Adafruit_GFX font is 5px plus 1px spacing per character.
  return text.length() * 6 * size;
}

void drawCenteredOledText(String text, int16_t y, uint8_t size) {
  display.setTextSize(size);
  int16_t x = (SCREEN_WIDTH - (int16_t)oledTextWidth(text, size)) / 2;
  if (x < 0) x = 0;
  display.setCursor(x, y);
  display.print(text);
}

void startOledOverlay(OledOverlayMode mode, unsigned long durationMs) {
  if (!oledReady || durationMs == 0) {
    return;
  }

  unsigned long now = millis();
  oledOverlayActive = true;
  oledOverlayMode = mode;
  oledOverlayStarted = now;
  oledOverlayUntil = now + durationMs;
  lastOverlayDraw = 0;
}

void stopOledOverlay() {
  OledOverlayMode previousMode = oledOverlayMode;
  oledOverlayActive = false;
  oledOverlayMode = OLED_OVERLAY_NONE;
  oledTickerText = "";
  oledTickerTextWidth = 0;

  // v1.6.0: when text/ticker display mode exits, explicitly re-arm the
  // current mood so returning to the face restores the correct RoboEyes setup.
  applyMood(currentMood);
  addEvent("OLED", String("Overlay finished: ") + oledOverlayModeLabel(previousMode) +
           " -> face " + moodToString(currentMood));
}

void showTemporaryOledStatus(String title, String line1, String line2, String line3, String line4, String line5 = "", unsigned long durationMs = 5000) {
  oledStatus(title, line1, line2, line3, line4, line5);

  if (oledReady && durationMs > 0) {
    startOledOverlay(OLED_OVERLAY_STATUS, durationMs);
  }
}

// =====================================================
// SLEEP MODE HELPERS
// =====================================================

void oledDisplayOnForYeti(bool force = false) {
  if (!oledReady) {
    return;
  }

  // A few SSD1306 clones can get their hardware display-off state out of sync
  // with our flag after rapid WebUI tests. Force mode is used by sleep bursts
  // and previews so the first Zzz frame is visible immediately.
  if (force || oledDisplayOff) {
    display.ssd1306_command(SSD1306_DISPLAYON);
    oledDisplayOff = false;
    delay(2);
  }
}

bool sleepRenderBlockActive() {
  return sleepModeActive || sleepPreviewActive;
}

void oledDisplayOffForYeti() {
  if (!oledReady) {
    return;
  }

  display.clearDisplay();
  display.display();
  display.ssd1306_command(SSD1306_DISPLAYOFF);
  oledDisplayOff = true;
}

int currentLocalMinuteOfDay() {
  if (!clockSynced()) {
    return -1;
  }

  time_t raw = time(nullptr) + activeClockOffsetSeconds();
  struct tm localTime;
  gmtime_r(&raw, &localTime);
  return (localTime.tm_hour * 60) + localTime.tm_min;
}

bool minuteInSleepWindow(int minute) {
  if (minute < 0) {
    return false;
  }

  if (sleepStartMinute == sleepEndMinute) {
    return false;
  }

  if (sleepStartMinute < sleepEndMinute) {
    return minute >= sleepStartMinute && minute < sleepEndMinute;
  }

  return minute >= sleepStartMinute || minute < sleepEndMinute;
}

bool sleepScheduleActiveNow() {
  if (!sleepModeEnabled || setupMode || !clockSynced()) {
    return false;
  }

  return minuteInSleepWindow(currentLocalMinuteOfDay());
}

bool sleepWakeOverrideActive() {
  return sleepWakeOverrideUntilMs > 0 && millis() < sleepWakeOverrideUntilMs;
}

int minutesUntilMinute(int fromMinute, int targetMinute) {
  if (fromMinute < 0) {
    return -1;
  }

  int delta = targetMinute - fromMinute;
  if (delta < 0) {
    delta += 1440;
  }
  return delta;
}

int minutesUntilScheduledWake() {
  return minutesUntilMinute(currentLocalMinuteOfDay(), sleepEndMinute);
}

YetiMood wakeMoodForCurrentTime() {
  int untilWake = minutesUntilScheduledWake();
  if (untilWake < 0 || !minuteInSleepWindow(currentLocalMinuteOfDay())) {
    return baseMood;
  }

  if (untilWake >= 120) {
    return MOOD_ANGRY;
  }

  if (untilWake >= 45) {
    return MOOD_ANNOYED;
  }

  if (untilWake >= 10) {
    return MOOD_SLEEPY;
  }

  return MOOD_DEADPAN;
}

unsigned long wakeMoodDurationMs(YetiMood mood) {
  switch (mood) {
    case MOOD_ANGRY: return 14000UL;
    case MOOD_ANNOYED: return 10000UL;
    case MOOD_SLEEPY: return 8000UL;
    case MOOD_DEADPAN: return 6500UL;
    default: return 6000UL;
  }
}

String wakeMoodNote(YetiMood mood) {
  int untilWake = minutesUntilScheduledWake();
  String prefix = untilWake >= 0 ? String(untilWake) + " min before scheduled wake" : String("clock unavailable");

  switch (mood) {
    case MOOD_ANGRY: return prefix + " · angry little sleep goblin";
    case MOOD_ANNOYED: return prefix + " · annoyed, filing paperwork";
    case MOOD_SLEEPY: return prefix + " · close enough, still crusty";
    case MOOD_DEADPAN: return prefix + " · acceptable wake window";
    default: return prefix + " · normal wake";
  }
}

String sleepStatusLabel() {
  if (sleepPreviewActive) return "Sleep animation preview";
  if (!sleepModeEnabled) return "Disabled";
  if (!clockSynced()) return "Waiting for clock";
  if (sleepModeActive && sleepAnimationActive) return "Dreaming";
  if (sleepModeActive) return oledDisplayOff ? "Sleeping / display off" : "Sleeping / blank";
  if (sleepWakeOverrideActive()) return "Awake override";
  if (sleepScheduleActiveNow()) return "Scheduled sleep pending";
  return "Awake";
}

String sleepSummaryLine() {
  String line = String("Sleep ") + minuteToTimeString(sleepStartMinute) + " -> " + minuteToTimeString(sleepEndMinute);
  if (sleepPreviewActive) return line + " · previewing sleep animation";
  if (!sleepModeEnabled) return line + " · disabled";
  if (!clockSynced()) return line + " · waiting for NTP";
  if (sleepModeActive) return line + " · " + sleepStatusLabel();
  if (sleepWakeOverrideActive()) {
    long remaining = (long)(sleepWakeOverrideUntilMs - millis());
    return line + " · awake override " + compactMs((uint32_t)max(0L, remaining));
  }
  return line + " · " + sleepStatusLabel();
}

void drawSleepAnimationFrame() {
  if (!oledReady || !sleepRenderBlockActive() || !sleepAnimationActive) {
    return;
  }

  unsigned long now = millis();
  if (lastSleepFrameMs > 0 && now - lastSleepFrameMs < YETI_SLEEP_FRAME_MS) {
    return;
  }
  lastSleepFrameMs = now;

  oledDisplayOnForYeti();

  uint8_t phase = ((now - sleepAnimationStartedMs) / YETI_SLEEP_FRAME_MS) % 18;
  int bob = (phase % 6 < 3) ? 0 : 1;
  int zRise = phase % 12;

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  // Closed sleepy eyes. Primitive lines beat a full RoboEyes update here because
  // sleep mode should sip battery, not animate like a caffeinated billboard.
  int eyeY = 36 + bob;
  display.drawLine(17, eyeY, 31, eyeY + 5, SSD1306_WHITE);
  display.drawLine(31, eyeY + 5, 50, eyeY, SSD1306_WHITE);
  display.drawLine(76, eyeY, 95, eyeY + 5, SSD1306_WHITE);
  display.drawLine(95, eyeY + 5, 111, eyeY, SSD1306_WHITE);
  display.drawLine(54, 53, 73, 53, SSD1306_WHITE);

  display.setTextSize(2);
  display.setCursor(82 + (phase % 3), max(0, 22 - zRise));
  display.print("Z");
  display.setTextSize(1);
  display.setCursor(101 + (phase % 5), max(0, 12 - zRise));
  display.print("z");
  display.setCursor(113, max(0, 4 - zRise));
  display.print("z");

  if ((phase % 9) < 4) {
    display.setTextSize(1);
    display.setCursor(6, 2);
    display.print("sleeping");
  }

  display.display();
}

void startSleepAnimationBurst() {
  if (!oledReady) {
    return;
  }

  sleepPreviewActive = false;
  sleepAnimationActive = true;
  sleepAnimationStartedMs = millis();
  nextSleepAnimationAtMs = 0;
  lastSleepFrameMs = 0;
  oledDisplayOnForYeti(true);
  drawSleepAnimationFrame();
}

void stopSleepAnimationBurst() {
  sleepAnimationActive = false;

  uint32_t minGap = sanitizeSleepGapMs(sleepGapMinMs, YETI_SLEEP_GAP_MIN_DEFAULT_MS);
  uint32_t maxGap = sanitizeSleepGapMs(sleepGapMaxMs, YETI_SLEEP_GAP_MAX_DEFAULT_MS);
  if (maxGap < minGap) {
    maxGap = minGap;
  }

  unsigned long gap = minGap;
  if (maxGap > minGap) {
    gap = (unsigned long)random((long)minGap, (long)maxGap + 1L);
  }

  nextSleepAnimationAtMs = millis() + gap;
  oledDisplayOffForYeti();
}

void startSleepAnimationPreview(uint32_t durationMs) {
  if (!oledReady || setupMode) {
    return;
  }

  if (sleepModeActive) {
    exitSleepMode("sleep animation preview", true);
  }

  sleepPreviewDurationMs = constrain(durationMs, 1000UL, 30000UL);
  sleepPreviewStartedMs = millis();
  sleepAnimationStartedMs = sleepPreviewStartedMs;
  sleepPreviewActive = true;
  sleepAnimationActive = true;
  nextSleepAnimationAtMs = 0;
  lastSleepFrameMs = 0;

  if (oledOverlayActive) {
    stopOledOverlay();
  }
  stopSequence(false);

  lastSleepNote = String("Previewing sleep animation for ") + compactMs(sleepPreviewDurationMs);
  addEvent("Sleep", lastSleepNote);

  oledDisplayOnForYeti(true);
  drawSleepAnimationFrame();
}

void stopSleepAnimationPreview(bool redrawFace) {
  if (!sleepPreviewActive) {
    return;
  }

  sleepPreviewActive = false;
  sleepAnimationActive = false;
  sleepPreviewStartedMs = 0;
  sleepPreviewDurationMs = YETI_SLEEP_PREVIEW_DEFAULT_MS;
  lastSleepFrameMs = 0;
  nextSleepAnimationAtMs = 0;
  oledDisplayOnForYeti(true);

  if (oledReady) {
    display.clearDisplay();
    display.display();
  }

  if (redrawFace) {
    applyMood(currentMood);
    lastFaceDraw = 0;
    lastIdleBehaviorMs = millis();
  }

  lastSleepNote = "Sleep animation preview finished";
  addEvent("Sleep", lastSleepNote);
}

void updateSleepAnimationPreview() {
  if (!sleepPreviewActive) {
    return;
  }

  unsigned long now = millis();
  if (now - sleepPreviewStartedMs >= sleepPreviewDurationMs) {
    stopSleepAnimationPreview(true);
    return;
  }

  drawSleepAnimationFrame();
}

void enterSleepMode(String reason) {
  if (sleepPreviewActive) {
    stopSleepAnimationPreview(false);
  }

  if (sleepModeActive) {
    sleepAnimationActive = false;
    startSleepAnimationBurst();
    lastSleepNote = String("Sleep animation restarted: ") + reason;
    addEvent("Sleep", lastSleepNote);
    return;
  }

  if (setupMode) {
    return;
  }

  sleepModeActive = true;
  sleepAnimationActive = false;
  sleepEntryCount++;
  lastSleepNote = String("Entered sleep: ") + reason;
  lastWakeReason = "Still sleeping";
  sensorOverridePriority = 0;
  demoMode = false;
  boredActive = false;

  if (oledOverlayActive) {
    stopOledOverlay();
  }

  stopSequence(false);
  setMood(MOOD_SLEEPY, 0, PRIORITY_INFO, false);
  currentMoodPriority = PRIORITY_IDLE;
  addEvent("Sleep", lastSleepNote);

  startSleepAnimationBurst();
}

void exitSleepMode(String reason, bool userInitiated) {
  if (!sleepModeActive && !oledDisplayOff) {
    return;
  }

  sleepModeActive = false;
  sleepPreviewActive = false;
  sleepAnimationActive = false;
  oledDisplayOnForYeti(true);
  sleepWakeCount++;
  lastWakeReason = reason;
  lastSleepNote = String("Woke: ") + reason;
  nextSleepAnimationAtMs = 0;
  lastSleepFrameMs = 0;

  if (oledReady) {
    display.clearDisplay();
    display.display();
  }

  YetiMood wakeMood = userInitiated ? wakeMoodForCurrentTime() : baseMood;
  unsigned long duration = userInitiated ? wakeMoodDurationMs(wakeMood) : 6500UL;
  String note = userInitiated ? wakeMoodNote(wakeMood) : String("scheduled wake window");

  setMood(wakeMood, duration, PRIORITY_MANUAL, false);
  currentMoodPriority = userInitiated ? PRIORITY_MANUAL : PRIORITY_IDLE;
  lastIdleBehaviorMs = millis();
  scheduleNextIdleBehavior();

  addEvent("Sleep", String("Wake: ") + reason + " · " + note);
}

bool wakeYetiFromSleep(String reason, bool holdOverride) {
  if (holdOverride && sleepWakeHoldMs > 0 && sleepScheduleActiveNow()) {
    sleepWakeOverrideUntilMs = millis() + sleepWakeHoldMs;
  }

  if (!sleepModeActive && !oledDisplayOff) {
    return false;
  }

  exitSleepMode(reason, true);
  return true;
}

void updateSleepSystem() {
  unsigned long now = millis();

  if (!sleepModeEnabled || setupMode) {
    if (sleepModeActive || oledDisplayOff) {
      exitSleepMode(!sleepModeEnabled ? "sleep disabled" : "setup mode", false);
    }
    return;
  }

  if (sleepWakeOverrideUntilMs > 0 && now >= sleepWakeOverrideUntilMs) {
    sleepWakeOverrideUntilMs = 0;
    addEvent("Sleep", "Wake override expired");
  }

  bool haveClock = clockSynced();
  bool scheduledSleep = haveClock && sleepScheduleActiveNow();
  bool overrideAwake = sleepWakeOverrideActive();

  if (!sleepModeActive && haveClock && scheduledSleep && !overrideAwake) {
    enterSleepMode("schedule " + minuteToTimeString(sleepStartMinute) + " -> " + minuteToTimeString(sleepEndMinute));
  }

  if (sleepModeActive && haveClock && (!scheduledSleep || overrideAwake)) {
    exitSleepMode(scheduledSleep ? "manual wake override" : "scheduled wake time", false);
    return;
  }

  // If Sleep Now is pressed before NTP finishes, still run the low-power animation cycle.
  if (!sleepModeActive) {
    return;
  }

  if (sleepAnimationActive) {
    if (now - sleepAnimationStartedMs >= sleepAnimDurationMs) {
      stopSleepAnimationBurst();
      return;
    }
    drawSleepAnimationFrame();
    return;
  }

  if (nextSleepAnimationAtMs == 0 || now >= nextSleepAnimationAtMs) {
    startSleepAnimationBurst();
  }
}

String buildSleepJson() {
  unsigned long now = millis();
  long nextAnim = 0;
  if (!sleepAnimationActive && nextSleepAnimationAtMs > 0 && now < nextSleepAnimationAtMs) {
    nextAnim = nextSleepAnimationAtMs - now;
  }

  long overrideRemaining = 0;
  if (sleepWakeOverrideActive()) {
    overrideRemaining = sleepWakeOverrideUntilMs - now;
  }

  int localMinute = currentLocalMinuteOfDay();
  int untilWake = minutesUntilScheduledWake();

  String json = "{";
  json += "\"enabled\":" + String(sleepModeEnabled ? "true" : "false") + ",";
  json += "\"active\":" + String(sleepModeActive ? "true" : "false") + ",";
  json += "\"animationActive\":" + String(sleepAnimationActive ? "true" : "false") + ",";
  json += "\"previewActive\":" + String(sleepPreviewActive ? "true" : "false") + ",";
  json += "\"displayOff\":" + String(oledDisplayOff ? "true" : "false") + ",";
  json += "\"scheduledNow\":" + String(sleepScheduleActiveNow() ? "true" : "false") + ",";
  json += "\"overrideActive\":" + String(sleepWakeOverrideActive() ? "true" : "false") + ",";
  json += "\"overrideRemainingMs\":" + String(overrideRemaining) + ",";
  json += "\"clockSynced\":" + String(clockSynced() ? "true" : "false") + ",";
  json += "\"status\":\"" + jsonEscape(sleepStatusLabel()) + "\",";
  json += "\"summary\":\"" + jsonEscape(sleepSummaryLine()) + "\",";
  json += "\"startMinute\":" + String(sleepStartMinute) + ",";
  json += "\"endMinute\":" + String(sleepEndMinute) + ",";
  json += "\"startTime\":\"" + minuteToTimeString(sleepStartMinute) + "\",";
  json += "\"endTime\":\"" + minuteToTimeString(sleepEndMinute) + "\",";
  json += "\"currentMinute\":" + String(localMinute) + ",";
  json += "\"minutesUntilWake\":" + String(untilWake) + ",";
  json += "\"animationDurationMs\":" + String(sleepAnimDurationMs) + ",";
  json += "\"animationDurationText\":\"" + jsonEscape(compactMs(sleepAnimDurationMs)) + "\",";
  json += "\"gapMinMs\":" + String(sleepGapMinMs) + ",";
  json += "\"gapMaxMs\":" + String(sleepGapMaxMs) + ",";
  json += "\"gapText\":\"" + jsonEscape(compactMs(sleepGapMinMs) + " - " + compactMs(sleepGapMaxMs)) + "\",";
  json += "\"wakeHoldMs\":" + String(sleepWakeHoldMs) + ",";
  json += "\"wakeHoldText\":\"" + jsonEscape(compactMs(sleepWakeHoldMs)) + "\",";
  json += "\"nextAnimationInMs\":" + String(nextAnim) + ",";
  json += "\"entryCount\":" + String(sleepEntryCount) + ",";
  json += "\"wakeCount\":" + String(sleepWakeCount) + ",";
  json += "\"lastSleepNote\":\"" + jsonEscape(lastSleepNote) + "\",";
  json += "\"lastWakeReason\":\"" + jsonEscape(lastWakeReason) + "\",";
  json += "\"animMinMs\":" + String(YETI_SLEEP_ANIM_MIN_MS) + ",";
  json += "\"animMaxMs\":" + String(YETI_SLEEP_ANIM_MAX_MS) + ",";
  json += "\"gapMinAllowedMs\":" + String(YETI_SLEEP_GAP_MIN_ALLOWED_MS) + ",";
  json += "\"gapMaxAllowedMs\":" + String(YETI_SLEEP_GAP_MAX_ALLOWED_MS) + ",";
  json += "\"wakeHoldMinMs\":" + String(YETI_SLEEP_WAKE_HOLD_MIN_MS) + ",";
  json += "\"wakeHoldMaxMs\":" + String(YETI_SLEEP_WAKE_HOLD_MAX_MS);
  json += "}";
  return json;
}

void oledBootPause(unsigned long pauseMs) {
  if (oledReady && pauseMs > 0) {
    delay(pauseMs);
  }
}

void oledBootStatus(String title, String line1, String line2, String line3, String line4, String line5 = "", unsigned long pauseMs = YETI_BOOT_STEP_SCREEN_MS) {
  oledStatus(title, line1, line2, line3, line4, line5);
  oledBootPause(pauseMs);
}

void showBootHardwareScreen() {
  String i2cLine = "SDA GPIO" + String(YETI_I2C_SDA_PIN) + " SCL GPIO" + String(YETI_I2C_SCL_PIN);

  oledBootStatus(
    "Yeti Hardware",
    String(YETI_BOARD_PROFILE_NAME),
    i2cLine,
    "OLED " + hexAddress(YETI_OLED_I2C_ADDRESS),
    "MPU " + hexAddress(YETI_MPU_I2C_ADDRESS),
    String("Touch: ") + String(YETI_TOUCH_ENABLED ? "On" : "Off")
  );
}

void showBootSensorScreen() {
  String touchLine = YETI_TOUCH_ENABLED ? String("Touch: ") + yesNo(touchReady) : String("Touch: disabled");

  oledBootStatus(
    "Yeti Sensors",
    String("OLED: ") + yesNo(oledReady),
    String("MPU: ") + yesNo(mpuReady),
    touchLine,
    "Heap: " + bytesToHuman(ESP.getFreeHeap()),
    "Booting network"
  );
}

void showBootReadyScreen() {
  if (setupMode) {
    oledBootStatus(
      "Yeti Setup Ready",
      "Wi-Fi AP:",
      setupApName,
      "IP: " + WiFi.softAPIP().toString(),
      "Open browser to",
      "192.168.4.1",
      YETI_BOOT_FINAL_INFO_MS
    );
    return;
  }

  if (WiFi.status() == WL_CONNECTED) {
    oledBootStatus(
      "Yeti Online Ready",
      "SSID:",
      WiFi.SSID(),
      "IP: " + WiFi.localIP().toString(),
      "WebUI:",
      hostnameLocalUrl(),
      YETI_BOOT_FINAL_INFO_MS
    );
    return;
  }

  oledBootStatus(
    "Yeti Offline",
    "No Wi-Fi link",
    "Check setup",
    "or Serial Monitor",
    "",
    "",
    YETI_BOOT_FINAL_INFO_MS
  );
}

void initOLED() {
#if YETI_OLED_ENABLED
  Serial.println("Initializing OLED...");

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println("OLED init failed. Continuing without display.");
    oledReady = false;
    return;
  }

  oledReady = true;

  oledBootStatus(
    "Yeti v1.6.6",
    "OLED online",
    "Starting system",
    "Please wait",
    "",
    ""
  );

  Serial.println("OLED initialized.");
#else
  oledReady = false;
  Serial.println("OLED disabled in hardware profile.");
#endif
}

// =====================================================
// TOUCH HELPERS
// =====================================================

void calibrateTouch() {
  Serial.println();

#if YETI_TOUCH_ENABLED
  Serial.print("Calibrating touch sensor on ");
  Serial.println(YETI_TOUCH_PIN_LABEL);
  Serial.println("Do not touch the pad during calibration.");

  oledStatus(
    "Yeti Touch",
    "Calibrating",
    YETI_TOUCH_PIN_LABEL,
    "Do not touch pad",
    "Please wait",
    ""
  );

  long total = 0;
  const int samples = 100;

  delay(400);

  for (int i = 0; i < samples; i++) {
    total += touchRead(YETI_TOUCH_PIN);
    delay(15);
  }

  touchBaseline = total / samples;
  touchThreshold = touchBaseline * TOUCH_THRESHOLD_RATIO;
  lastTouchValue = touchRead(YETI_TOUCH_PIN);
  touchReady = true;

  Serial.print("Touch baseline: ");
  Serial.println(touchBaseline);

  Serial.print("Touch threshold: ");
  Serial.println(touchThreshold);
  addEvent("Touch", "Calibration complete");
#else
  touchReady = false;
  touchBaseline = 0;
  touchThreshold = 0;
  lastTouchValue = 0;
  lastTouched = false;
  touchConfig.enabled = false;

  Serial.println("Touch sensor disabled in hardware profile. Skipping calibration.");

  oledStatus(
    "Yeti Touch",
    "Touch disabled",
    YETI_TOUCH_PIN_LABEL,
    "",
    "",
    ""
  );
#endif
}

void updateTouch() {
#if YETI_TOUCH_ENABLED
  if (!touchReady) {
    return;
  }

  unsigned long now = millis();

  if (now - lastTouchRead < 30) {
    return;
  }

  lastTouchRead = now;

  int value = touchRead(YETI_TOUCH_PIN);
  lastTouchValue = value;

  bool touched = value < touchThreshold;

  if (touchConfig.enabled && touched) {
    if (!lastTouched) {
      startBlink();
      triggerSensorReaction("Touch", touchConfig, TOUCH_PRIORITY, FACE_HAPPY);
    } else if (lastTriggerName == "Touch" && sensorOverridePriority == TOUCH_PRIORITY) {
      sensorOverrideUntil = now + touchConfig.holdMs;
    }

    if (now - lastTouchPrint > 350) {
      lastTouchPrint = now;

      Serial.print("Touch detected. value=");
      Serial.print(value);
      Serial.print(" threshold=");
      Serial.println(touchThreshold);
    }
  }

  lastTouched = touched;
#endif
}

// =====================================================
// MPU HELPERS
// =====================================================

bool i2cDevicePresent(byte address) {
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}

void mpuWriteRegister(byte reg, byte value) {
  Wire.beginTransmission(MPU_ADDRESS);
  Wire.write(reg);
  Wire.write(value);
  Wire.endTransmission();
}

byte mpuReadRegister(byte reg) {
  Wire.beginTransmission(MPU_ADDRESS);
  Wire.write(reg);
  Wire.endTransmission(false);

  Wire.requestFrom(MPU_ADDRESS, 1, true);

  if (Wire.available()) {
    return Wire.read();
  }

  return 0xFF;
}

bool initMPU() {
  Serial.println();

#if YETI_MPU_ENABLED
  Serial.println("Checking MPU-6500...");

  if (!i2cDevicePresent(MPU_ADDRESS)) {
    Serial.print("MPU-6500 not found at 0x");
    Serial.print(MPU_ADDRESS, HEX);
    Serial.println(". Shake detection disabled.");
    addEvent("MPU", "Not found at configured address");
    return false;
  }

  byte who = mpuReadRegister(MPU_REG_WHO_AM_I);

  Serial.print("MPU WHO_AM_I: 0x");
  Serial.println(who, HEX);

  mpuWriteRegister(MPU_REG_PWR_MGMT_1, 0x00);
  delay(100);

  mpuWriteRegister(MPU_REG_CONFIG, 0x03);
  mpuWriteRegister(MPU_REG_GYRO_CFG, 0x08);
  mpuWriteRegister(MPU_REG_ACCEL_CFG, 0x08);

  Serial.println("MPU-6500 initialized. Shake detection enabled.");
  addEvent("MPU", "Initialized successfully");

  return true;
#else
  Serial.println("MPU disabled in hardware profile. Shake detection disabled.");
  return false;
#endif
}

bool readAccelG(float &ax, float &ay, float &az) {
  Wire.beginTransmission(MPU_ADDRESS);
  Wire.write(MPU_REG_ACCEL_XOUT);

  if (Wire.endTransmission(false) != 0) {
    return false;
  }

  int bytesRead = Wire.requestFrom(MPU_ADDRESS, 6, true);

  if (bytesRead != 6) {
    return false;
  }

  int16_t rawAx = (Wire.read() << 8) | Wire.read();
  int16_t rawAy = (Wire.read() << 8) | Wire.read();
  int16_t rawAz = (Wire.read() << 8) | Wire.read();

  ax = rawAx / 8192.0;
  ay = rawAy / 8192.0;
  az = rawAz / 8192.0;

  return true;
}

void updateMotion() {
  if (!mpuReady) {
    return;
  }

  unsigned long now = millis();

  if (now - lastMotionRead < 25) {
    return;
  }

  lastMotionRead = now;

  float ax, ay, az;

  if (!readAccelG(ax, ay, az)) {
    return;
  }

  float magnitude = sqrt((ax * ax) + (ay * ay) + (az * az));
  float magDelta = abs(magnitude - 1.0f);

  if (!haveAccelBaseline) {
    baselineAx = ax;
    baselineAy = ay;
    baselineAz = az;
    haveAccelBaseline = true;
    lastMovementActivityMs = now;
  }

  float tiltDx = ax - baselineAx;
  float tiltDy = ay - baselineAy;
  float tiltDz = az - baselineAz;
  float tiltDelta = sqrt((tiltDx * tiltDx) + (tiltDy * tiltDy) + (tiltDz * tiltDz));

  float jerk = 0.0;

  if (haveLastAccel) {
    float dx = ax - lastAx;
    float dy = ay - lastAy;
    float dz = az - lastAz;

    jerk = sqrt((dx * dx) + (dy * dy) + (dz * dz));
  }

  lastAx = ax;
  lastAy = ay;
  lastAz = az;
  haveLastAccel = true;

  lastAccelMagnitude = magnitude;
  lastMotionJerk = jerk;
  lastTiltDelta = tiltDelta;
  lastShakeScore = max(jerk, magDelta);

  bool moved = jerk > movementJerkThresholdG();
  bool tilted = tiltDelta > movementTiltThresholdG();
  if (!tilted && tiltDelta < movementTiltThresholdG() * 0.65f) {
    tiltMoodLatched = false;
  }
  bool shaken = false;

  if (jerk > movementShakeJerkThresholdG()) {
    shaken = true;
  }

  if (magDelta > movementShakeMagThresholdG()) {
    shaken = true;
  }

  if (moved || tilted || shaken) {
    lastMovementActivityMs = now;
  }

  if (sleepModeActive) {
    if (shaken && now - lastShakeTrigger > 900) {
      lastShakeTrigger = now;
      lastShakeScore = max(jerk, magDelta);
      wakeYetiFromSleep("shake", true);
      recordMemoryEvent(EVENT_GYRO_SHAKEN);
      addEvent("Sleep", String("Shake wake score ") + String(lastShakeScore, 2) + "g");
    }
    return;
  }

  // Legacy shake trigger stays alive for compatibility with the old sensor override settings.
  if (shakeConfig.enabled && shaken) {
    if (now - lastShakeTrigger > 600) {
      lastShakeTrigger = now;
      triggerSensorReaction("Shake", shakeConfig, SHAKE_PRIORITY, FACE_ANGRY);
    }

    if (now - lastShakePrint > 350) {
      lastShakePrint = now;

      Serial.print("Shake detected. jerk=");
      Serial.print(jerk, 2);
      Serial.print("g magDelta=");
      Serial.print(magDelta, 2);
      Serial.print("g score=");
      Serial.print(lastShakeScore, 2);
      Serial.print(" thresholds=");
      Serial.print(movementShakeJerkThresholdG(), 2);
      Serial.print("/");
      Serial.println(movementShakeMagThresholdG(), 2);
    }
  }

  if (!movementMoodEnabled) {
    return;
  }

  if (shaken) {
    handleYetiEvent(EVENT_GYRO_SHAKEN);
    return;
  }

  if (tilted && !tiltMoodLatched) {
    tiltMoodLatched = true;
    handleYetiEvent(EVENT_GYRO_TILTED);
    return;
  }

  if (moved) {
    handleYetiEvent(EVENT_GYRO_MOVED);
    return;
  }

  // If YETI has been sitting untouched for a long time, let the movement system
  // nudge him sleepy. This is deliberately slow and cooldown-protected.
  if (lastMovementActivityMs > 0 && now - lastMovementActivityMs > 300000UL) {
    handleYetiEvent(EVENT_GYRO_IDLE_LONG);
    // Do not fire every loop while still; cooldown protects it too, but moving the
    // activity marker keeps the diagnostic age sane and avoids noisy checks.
    lastMovementActivityMs = now;
  }
}

// =====================================================
// FACE UPDATE LOGIC
// =====================================================

void updateEyeMovement() {
  unsigned long now = millis();
  Expression face = activeExpression();

  if (now >= nextLookTime) {
    if (face == FACE_SURPRISED || face == FACE_SHOCKED) {
      targetEyeOffsetX = random(-2, 3);
      targetEyeOffsetY = random(-2, 3);
    } else if (face == FACE_SLEEPY) {
      targetEyeOffsetX = random(-5, 6);
      targetEyeOffsetY = random(0, 5);
    } else if (face == FACE_BORED) {
      targetEyeOffsetX = random(-4, 5);
      targetEyeOffsetY = random(2, 6);
    } else if (face == FACE_ANGRY) {
      targetEyeOffsetX = random(-8, 9);
      targetEyeOffsetY = random(-3, 4);
    } else if (face == FACE_HAPPY) {
      targetEyeOffsetX = random(-6, 7);
      targetEyeOffsetY = random(-2, 4);
    } else {
      targetEyeOffsetX = random(-10, 11);
      targetEyeOffsetY = random(-5, 6);
    }

    scheduleNextLook();
  }

  eyeOffsetX += (targetEyeOffsetX - eyeOffsetX) * 0.13;
  eyeOffsetY += (targetEyeOffsetY - eyeOffsetY) * 0.13;
}

void updateBlinking() {
  unsigned long now = millis();

  if (!blinking && now >= nextBlinkTime) {
    startBlink();
  }

  if (!blinking && doubleBlinkQueued && now >= doubleBlinkTime) {
    doubleBlinkQueued = false;
    blinking = true;
    blinkStartTime = millis();
  }
}

void startDemoMode() {
  stopSequence(false);
  demoMode = true;
  demoModeUntil = millis() + 15000;
  nextDemoStep = 0;
  demoExpressionIndex = 0;
  sensorOverridePriority = 0;
  lastTriggerName = "Demo";
  markEngaged();
  addEvent("Demo", "Demo mode started");
}

void updateDemoMode() {
  if (!demoMode) {
    return;
  }

  unsigned long now = millis();

  if (now >= demoModeUntil) {
    demoMode = false;
    lastTriggerName = "None";
    setExpression(FACE_NORMAL, false);
    addEvent("Demo", "Demo mode ended");
    return;
  }

  if (now >= nextDemoStep) {
    Expression sequence[] = {
      FACE_NORMAL, FACE_HAPPY, FACE_SURPRISED, FACE_SHOCKED,
      FACE_ANGRY, FACE_SLEEPY, FACE_BORED, FACE_HAPPY
    };

    const int sequenceCount = sizeof(sequence) / sizeof(sequence[0]);
    currentExpression = sequence[demoExpressionIndex % sequenceCount];
    demoExpressionIndex++;

    if (random(0, 100) < 55) {
      startBlink();
    }

    nextDemoStep = now + 850;
  }
}

void updateExpression() {
  unsigned long now = millis();

  if (demoMode) {
    return;
  }

  if (sensorOverridePriority > 0 && now < sensorOverrideUntil) {
    return;
  }

  if (sensorOverridePriority > 0 && now >= sensorOverrideUntil) {
    sensorOverridePriority = 0;
    lastTriggerName = "None";
  }

  if (!boredActive && boredTimeoutMs > 0 && (now - lastEngagementMs) >= boredTimeoutMs) {
    boredActive = true;
    Serial.println("Yeti is bored.");
    addEvent("Mood", "Boredom timeout reached");
    return;
  }

  if (boredActive) {
    return;
  }

  // v1.5.0: Random idle expression changes are now owned by updateIdleBehavior().
  // Leaving old expression roulette enabled would fight manual moods and temporary reactions.
}

#if YETI_FACE_ENGINE == YETI_FACE_ENGINE_CLASSIC
// =====================================================
// ORIGINAL ROUNDED-EYE FACE DRAWING
// Legacy face themes are not exposed in v1.1.1; this build uses Classic.
// =====================================================

void drawClosedEye(int cx, int cy, int w, bool happyCurve) {
  int x1 = cx - w / 2;
  int x2 = cx + w / 2;
  uint16_t fg = faceFgColor();

  if (happyCurve) {
    display.drawLine(x1, cy, x1 + 8, cy + 4, fg);
    display.drawLine(x1 + 8, cy + 4, cx, cy + 6, fg);
    display.drawLine(cx, cy + 6, x2 - 8, cy + 4, fg);
    display.drawLine(x2 - 8, cy + 4, x2, cy, fg);

    display.drawLine(x1, cy + 1, x1 + 8, cy + 5, fg);
    display.drawLine(x1 + 8, cy + 5, cx, cy + 7, fg);
    display.drawLine(cx, cy + 7, x2 - 8, cy + 5, fg);
    display.drawLine(x2 - 8, cy + 5, x2, cy + 1, fg);
  } else {
    display.drawLine(x1, cy, x2, cy, fg);
    display.drawLine(x1, cy + 1, x2, cy + 1, fg);
    display.drawLine(x1 + 2, cy - 1, x2 - 2, cy - 1, fg);
  }
}

void drawEye(int cx, int cy, int w, int h, float blinkAmount, bool leftEye) {
  Expression face = activeExpression();

  uint16_t fg = faceFgColor();
  uint16_t cut = faceCutColor();
  uint16_t bg = faceBgColor();

  int actualW = w;
  int actualH = h;

  bool happy = face == FACE_HAPPY;
  bool angry = face == FACE_ANGRY;
  bool sleepy = face == FACE_SLEEPY;
  bool surprised = face == FACE_SURPRISED;
  bool shocked = face == FACE_SHOCKED;
  bool bored = face == FACE_BORED;
  bool outline = faceOutlineMode();

  if (happy) {
    actualH = h - 8;
    cy += 1;
  }

  if (sleepy) {
    actualH = h - 15;
    cy += 6;
  }

  if (bored) {
    actualH = h - 18;
    actualW = w - 2;
    cy += 8;
  }

  if (surprised) {
    actualW = w - 3;
    actualH = h + 4;
  }

  if (shocked) {
    actualW = w - 6;
    actualH = h + 6;
  }

  int closedH = max(2, (int)(actualH * (1.0 - blinkAmount)));

  if (blinkAmount > 0.88) {
    drawClosedEye(cx, cy, actualW, happy);
    return;
  }

  int x = cx - actualW / 2;
  int y = cy - closedH / 2;

  int radius = min(actualW, closedH) / 3;

  if (outline) {
    display.drawRoundRect(x, y, actualW, closedH, radius, fg);
    display.drawRoundRect(x + 1, y + 1, actualW - 2, closedH - 2, max(1, radius - 1), fg);
  } else {
    display.fillRoundRect(x, y, actualW, closedH, radius, fg);
  }

  int pupilSize = 7;

  if (sleepy) pupilSize = 5;
  if (bored) pupilSize = 4;
  if (angry || happy) pupilSize = 6;
  if (surprised) pupilSize = 8;
  if (shocked) pupilSize = 4;

  int pupilX = cx + (int)eyeOffsetX;
  int pupilY = cy + (int)eyeOffsetY;

  int maxPX = actualW / 2 - pupilSize - 3;
  int maxPY = closedH / 2 - pupilSize - 2;

  if (maxPX < 1) maxPX = 1;
  if (maxPY < 1) maxPY = 1;

  pupilX = constrain(pupilX, cx - maxPX, cx + maxPX);
  pupilY = constrain(pupilY, cy - maxPY, cy + maxPY);

  if (outline) {
    display.fillCircle(pupilX, pupilY, pupilSize, fg);
    display.fillCircle(pupilX, pupilY, max(1, pupilSize - 3), bg);
  } else {
    display.fillCircle(pupilX, pupilY, pupilSize, cut);
  }

  if (!sleepy && !bored && !angry && !outline) {
    display.fillCircle(pupilX - 2, pupilY - 2, 1, fg);
  }

  if (angry) {
    if (leftEye) {
      display.fillTriangle(
        x, y,
        x + actualW, y,
        x + actualW, y + closedH / 3,
        bg
      );
    } else {
      display.fillTriangle(
        x, y,
        x + actualW, y,
        x, y + closedH / 3,
        bg
      );
    }

    if (leftEye) {
      display.drawLine(x + 2, y + 2, x + actualW - 2, y + closedH / 3, fg);
      display.drawLine(x + 2, y + 3, x + actualW - 2, y + closedH / 3 + 1, fg);
    } else {
      display.drawLine(x + 2, y + closedH / 3, x + actualW - 2, y + 2, fg);
      display.drawLine(x + 2, y + closedH / 3 + 1, x + actualW - 2, y + 3, fg);
    }
  }

  if (sleepy) {
    display.fillRect(x, y, actualW, closedH / 2, bg);
    display.drawLine(x + 2, y + closedH / 2, x + actualW - 3, y + closedH / 2, fg);
  }

  if (bored) {
    display.fillRect(x, y, actualW, max(1, closedH / 2 - 1), bg);
    display.drawLine(x + 2, y + closedH / 2, x + actualW - 3, y + closedH / 2, fg);
    display.drawLine(x + 4, y + closedH / 2 + 2, x + actualW - 5, y + closedH / 2 + 2, fg);
  }

  if (happy) {
    display.fillRect(x, y + closedH - 7, actualW, 8, bg);
    display.drawLine(x + 4, y + closedH - 7, x + actualW - 5, y + closedH - 7, fg);
    display.drawLine(x + 7, y + closedH - 5, x + actualW - 8, y + closedH - 5, fg);
  }
}

void drawMouth() {
  Expression face = activeExpression();
  uint16_t fg = faceFgColor();

  int cx = 64;
  int y = 56;

  switch (face) {
    case FACE_NORMAL:
      display.drawLine(cx - 7, y, cx + 7, y, fg);
      display.drawLine(cx - 5, y + 1, cx + 5, y + 1, fg);
      break;

    case FACE_ANGRY:
      display.drawLine(cx - 8, y + 3, cx - 3, y, fg);
      display.drawLine(cx - 3, y, cx + 3, y, fg);
      display.drawLine(cx + 3, y, cx + 8, y + 3, fg);
      break;

    case FACE_SLEEPY:
      display.drawCircle(cx, y, 3, fg);
      break;

    case FACE_HAPPY:
      display.drawLine(cx - 10, y - 2, cx - 6, y + 1, fg);
      display.drawLine(cx - 6, y + 1, cx, y + 3, fg);
      display.drawLine(cx, y + 3, cx + 6, y + 1, fg);
      display.drawLine(cx + 6, y + 1, cx + 10, y - 2, fg);
      break;

    case FACE_SURPRISED:
      display.drawCircle(cx, y, 4, fg);
      break;

    case FACE_SHOCKED:
      display.drawCircle(cx, y, 6, fg);
      display.drawCircle(cx, y, 3, fg);
      break;

    case FACE_BORED:
      display.drawLine(cx - 9, y + 2, cx - 3, y, fg);
      display.drawLine(cx - 3, y, cx + 3, y, fg);
      display.drawLine(cx + 3, y, cx + 9, y + 2, fg);
      display.drawLine(cx - 7, y + 4, cx + 7, y + 4, fg);
      break;

    default:
      break;
  }
}


void drawTinyZ(int x, int y, uint16_t color) {
  display.drawLine(x, y, x + 7, y, color);
  display.drawLine(x + 7, y, x, y + 6, color);
  display.drawLine(x, y + 6, x + 7, y + 6, color);
}

void drawTinySparkle(int cx, int cy, uint16_t color) {
  display.drawLine(cx, cy - 3, cx, cy + 3, color);
  display.drawLine(cx - 3, cy, cx + 3, cy, color);
  display.drawPixel(cx - 2, cy - 2, color);
  display.drawPixel(cx + 2, cy - 2, color);
  display.drawPixel(cx - 2, cy + 2, color);
  display.drawPixel(cx + 2, cy + 2, color);
}

void drawTinyLightning(int x, int y, uint16_t color) {
  display.drawLine(x + 5, y, x, y + 7, color);
  display.drawLine(x, y + 7, x + 5, y + 7, color);
  display.drawLine(x + 5, y + 7, x + 1, y + 14, color);

  display.drawLine(x + 7, y, x + 2, y + 7, color);
  display.drawLine(x + 2, y + 7, x + 7, y + 7, color);
  display.drawLine(x + 7, y + 7, x + 3, y + 14, color);
}

void drawTwoZoneAccent(Expression face) {
  if (!faceTwoZoneMode()) {
    return;
  }

  // On common two-color SSD1306 modules, pixels in this top band appear yellow.
  // The rest of the screen appears blue. The driver still only sees on/off pixels.
  uint16_t accent = SSD1306_WHITE;

  display.drawLine(0, 15, 127, 15, accent);

  switch (face) {
    case FACE_NORMAL:
      // Calm snow peak / horizon accent.
      display.drawLine(12, 12, 24, 4, accent);
      display.drawLine(24, 4, 36, 12, accent);
      display.drawLine(20, 7, 24, 10, accent);
      display.drawLine(24, 10, 29, 7, accent);

      display.drawLine(92, 12, 104, 4, accent);
      display.drawLine(104, 4, 116, 12, accent);
      display.drawLine(100, 7, 104, 10, accent);
      display.drawLine(104, 10, 109, 7, accent);

      display.drawLine(50, 9, 78, 9, accent);
      display.drawLine(55, 11, 73, 11, accent);
      break;

    case FACE_HAPPY:
      // Happy sparkles in the yellow strip.
      drawTinySparkle(22, 8, accent);
      drawTinySparkle(106, 8, accent);
      display.drawLine(48, 8, 55, 12, accent);
      display.drawLine(55, 12, 64, 13, accent);
      display.drawLine(64, 13, 73, 12, accent);
      display.drawLine(73, 12, 80, 8, accent);
      break;

    case FACE_ANGRY:
      // Jagged angry brow / warning accent.
      display.drawLine(0, 13, 10, 5, accent);
      display.drawLine(10, 5, 20, 13, accent);
      display.drawLine(20, 13, 30, 5, accent);
      display.drawLine(30, 5, 40, 13, accent);

      display.drawLine(88, 13, 98, 5, accent);
      display.drawLine(98, 5, 108, 13, accent);
      display.drawLine(108, 13, 118, 5, accent);
      display.drawLine(118, 5, 127, 13, accent);

      display.drawLine(50, 5, 78, 12, accent);
      display.drawLine(50, 6, 78, 13, accent);
      break;

    case FACE_SLEEPY:
      // Sleep indicators.
      drawTinyZ(16, 5, accent);
      drawTinyZ(32, 3, accent);
      drawTinyZ(48, 6, accent);

      display.drawLine(82, 8, 92, 8, accent);
      display.drawLine(96, 8, 106, 8, accent);
      break;

    case FACE_SURPRISED:
      // Attention marks.
      display.drawLine(22, 3, 22, 11, accent);
      display.drawPixel(22, 14, accent);
      display.drawLine(106, 3, 106, 11, accent);
      display.drawPixel(106, 14, accent);

      display.drawCircle(64, 8, 5, accent);
      break;

    case FACE_SHOCKED:
      // Lightning bolts.
      drawTinyLightning(17, 1, accent);
      drawTinyLightning(104, 1, accent);

      display.drawLine(58, 2, 70, 2, accent);
      display.drawLine(56, 5, 72, 5, accent);
      display.drawLine(58, 8, 70, 8, accent);
      display.drawLine(60, 11, 68, 11, accent);
      break;

    case FACE_BORED:
      // Slow drifting boredom marks.
      display.drawLine(8, 7, 25, 7, accent);
      display.drawLine(103, 7, 120, 7, accent);
      drawTinyZ(50, 4, accent);
      drawTinyZ(68, 6, accent);
      break;

    default:
      break;
  }
}


void drawFace() {
  if (!oledReady) {
    return;
  }

  float blinkAmount = getBlinkAmount();
  Expression face = activeExpression();

  display.clearDisplay();

  if (faceTheme == FACE_THEME_INVERTED) {
    display.fillScreen(SSD1306_WHITE);
  } else {
    display.fillScreen(SSD1306_BLACK);
  }

  drawTwoZoneAccent(face);

  int leftX = 38;
  int rightX = 90;
  int eyeY = 27;
  int eyeW = 41;
  int eyeH = 36;

  if (faceTwoZoneMode()) {
    eyeY = 30;
    eyeW = 39;
    eyeH = 32;
  }

  if (face == FACE_SLEEPY) {
    eyeY = 27;
  }

  if (face == FACE_BORED) {
    eyeY = faceTwoZoneMode() ? 32 : 30;
    eyeW = 40;
    eyeH = 31;
  }

  if (face == FACE_SURPRISED) {
    eyeW = 38;
    eyeH = 39;
  }

  if (face == FACE_SHOCKED) {
    eyeW = 36;
    eyeH = 40;
  }

  if (face == FACE_HAPPY) {
    eyeY = 27;
    eyeW = 42;
    eyeH = 34;
  }

  drawEye(leftX, eyeY, eyeW, eyeH, blinkAmount, true);
  drawEye(rightX, eyeY, eyeW, eyeH, blinkAmount, false);

  drawMouth();

  display.display();
}

#endif


// =====================================================
// ROBOEYES FACE ENGINE
// =====================================================

byte faceFrameMsToFps(uint32_t frameMs) {
  if (frameMs < YETI_FACE_FRAME_MIN_MS) frameMs = YETI_FACE_FRAME_MIN_MS;
  if (frameMs > YETI_FACE_FRAME_MAX_MS) frameMs = YETI_FACE_FRAME_MAX_MS;

  uint32_t safeFrameMs = frameMs == 0 ? 1 : frameMs;
  uint32_t fps = 1000UL / safeFrameMs;
  if (fps < 4) fps = 4;
  if (fps > 30) fps = 30;
  return (byte)fps;
}

void initFaceEngine() {
#if YETI_FACE_ENGINE == YETI_FACE_ENGINE_ROBOEYES
  if (!oledReady) {
    return;
  }

  roboEyes.begin(SCREEN_WIDTH, SCREEN_HEIGHT, faceFrameMsToFps(faceFrameMs));
  roboEyes.setDisplayColors(0, 1);
  roboEyes.setWidth(40, 40);
  roboEyes.setHeight(34, 34);
  roboEyes.setBorderradius(10, 10);
  roboEyes.setSpacebetween(8);
  roboEyes.setCuriosity(ON);
  roboEyes.setAutoblinker(ON, 3, 4);
  roboEyes.setIdleMode(ON, 2, 3);
  roboEyes.setMood(DEFAULT);
  roboEyes.setPosition(DEFAULT);
  roboEyes.close();
  roboEyes.open();

  roboEyesStarted = true;
  roboEyesLastExpression = FACE_COUNT;
  roboEyesLastFrameMs = faceFrameMs;
  addEvent("Face", "RoboEyes engine started");
#endif
}

#if YETI_FACE_ENGINE == YETI_FACE_ENGINE_ROBOEYES
void applyRoboEyesExpression(Expression face) {
  // Baseline YETI eye geometry. The values are deliberately a little larger
  // than the library defaults so the face remains readable on a tiny 128x64 OLED.
  roboEyes.setDisplayColors(0, 1);
  roboEyes.setCyclops(OFF);
  roboEyes.setSweat(OFF);
  roboEyes.setHFlicker(OFF, 0);
  roboEyes.setVFlicker(OFF, 0);
  roboEyes.setCuriosity(ON);
  roboEyes.setIdleMode(ON, 2, 3);
  roboEyes.open();

  switch (face) {
    case FACE_ANGRY:
      roboEyes.setWidth(42, 42);
      roboEyes.setHeight(34, 34);
      roboEyes.setBorderradius(8, 8);
      roboEyes.setSpacebetween(6);
      roboEyes.setMood(ANGRY);
      roboEyes.setPosition(DEFAULT);
      roboEyes.setHFlicker(ON, 1);
      break;

    case FACE_SLEEPY:
      roboEyes.setWidth(42, 42);
      roboEyes.setHeight(28, 28);
      roboEyes.setBorderradius(8, 8);
      roboEyes.setSpacebetween(8);
      roboEyes.setMood(TIRED);
      roboEyes.setPosition(S);
      roboEyes.setIdleMode(OFF);
      break;

    case FACE_HAPPY:
      roboEyes.setWidth(42, 42);
      roboEyes.setHeight(34, 34);
      roboEyes.setBorderradius(12, 12);
      roboEyes.setSpacebetween(8);
      roboEyes.setMood(HAPPY);
      roboEyes.setPosition(DEFAULT);
      roboEyes.anim_laugh();
      break;

    case FACE_SURPRISED:
      roboEyes.setWidth(34, 34);
      roboEyes.setHeight(42, 42);
      roboEyes.setBorderradius(18, 18);
      roboEyes.setSpacebetween(12);
      roboEyes.setMood(DEFAULT);
      roboEyes.setPosition(N);
      roboEyes.blink();
      break;

    case FACE_SHOCKED:
      roboEyes.setWidth(32, 32);
      roboEyes.setHeight(44, 44);
      roboEyes.setBorderradius(16, 16);
      roboEyes.setSpacebetween(14);
      roboEyes.setMood(DEFAULT);
      roboEyes.setPosition(DEFAULT);
      roboEyes.setSweat(ON);
      roboEyes.anim_confused();
      break;

    case FACE_BORED:
      roboEyes.setWidth(46, 46);
      roboEyes.setHeight(24, 24);
      roboEyes.setBorderradius(7, 7);
      roboEyes.setSpacebetween(6);
      roboEyes.setMood(TIRED);
      roboEyes.setPosition(S);
      roboEyes.setIdleMode(OFF);
      break;

    case FACE_NORMAL:
    default:
      roboEyes.setWidth(40, 40);
      roboEyes.setHeight(34, 34);
      roboEyes.setBorderradius(10, 10);
      roboEyes.setSpacebetween(8);
      roboEyes.setMood(DEFAULT);
      roboEyes.setPosition(DEFAULT);
      break;
  }
}
#endif

void updateFaceEngine() {
#if YETI_FACE_ENGINE == YETI_FACE_ENGINE_ROBOEYES
  if (!oledReady || !roboEyesStarted || oledOverlayActive) {
    return;
  }

  if (roboEyesLastFrameMs != faceFrameMs) {
    roboEyes.setFramerate(faceFrameMsToFps(faceFrameMs));
    roboEyesLastFrameMs = faceFrameMs;
  }

  Expression face = activeExpression();
  if (face != roboEyesLastExpression) {
    applyRoboEyesExpression(face);
    roboEyesLastExpression = face;
  }

  uint32_t renderStartUs = micros();
  roboEyes.update();
  lastFaceRenderMs = (micros() - renderStartUs + 999) / 1000;
#endif
}

// =====================================================
// WIFI SCAN JSON
// =====================================================

String encryptionToString(wifi_auth_mode_t type) {
  switch (type) {
    case WIFI_AUTH_OPEN: return "Open";
    case WIFI_AUTH_WEP: return "WEP";
    case WIFI_AUTH_WPA_PSK: return "WPA";
    case WIFI_AUTH_WPA2_PSK: return "WPA2";
    case WIFI_AUTH_WPA_WPA2_PSK: return "WPA/WPA2";
    case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2 Enterprise";
    case WIFI_AUTH_WPA3_PSK: return "WPA3";
    case WIFI_AUTH_WPA2_WPA3_PSK: return "WPA2/WPA3";
    default: return "Other/Unknown";
  }
}

String buildScanJson() {
  if (setupMode) {
    WiFi.mode(WIFI_AP_STA);
  } else {
    WiFi.mode(WIFI_STA);
  }

  int networkCount = WiFi.scanNetworks(false, true);

  String json = "[";

  if (networkCount > 0) {
    for (int i = 0; i < networkCount; i++) {
      if (i > 0) {
        json += ",";
      }

      json += "{";
      json += "\"ssid\":\"" + jsonEscape(WiFi.SSID(i)) + "\",";
      json += "\"rssi\":" + String(WiFi.RSSI(i)) + ",";
      json += "\"channel\":" + String(WiFi.channel(i)) + ",";
      json += "\"encryption\":\"" + jsonEscape(encryptionToString(WiFi.encryptionType(i))) + "\",";
      json += "\"bssid\":\"" + jsonEscape(WiFi.BSSIDstr(i)) + "\"";
      json += "}";
    }
  }

  json += "]";

  WiFi.scanDelete();
  return json;
}

// =====================================================
// STATUS JSON
// =====================================================

String expressionOptionsJson() {
  String json = "[";

  for (int i = 0; i < FACE_COUNT; i++) {
    if (i > 0) {
      json += ",";
    }

    Expression expression = (Expression)i;

    json += "{";
    json += "\"id\":" + String(i) + ",";
    json += "\"name\":\"" + expressionName(expression) + "\",";
    json += "\"bit\":" + String(1UL << i);
    json += "}";
  }

  json += "]";

  return json;
}

String moodOptionsJson() {
  String json = "[";

  for (int i = 0; i < MOOD_COUNT; i++) {
    if (i > 0) {
      json += ",";
    }

    YetiMood mood = (YetiMood)i;

    json += "{";
    json += "\"id\":" + String(i) + ",";
    json += "\"name\":\"" + jsonEscape(moodLabel(mood)) + "\",";
    json += "\"value\":\"" + jsonEscape(String(moodToString(mood))) + "\"";
    json += "}";
  }

  json += "]";
  return json;
}

String personalityPresetOptionsJson() {
  String json = "[";

  for (uint8_t i = 0; i < PERSONALITY_PRESET_COUNT; i++) {
    if (i > 0) {
      json += ",";
    }

    const YetiPersonalityPreset &preset = PERSONALITY_PRESETS[i];

    json += "{";
    json += "\"id\":\"" + jsonEscape(String(preset.id)) + "\",";
    json += "\"name\":\"" + jsonEscape(String(preset.name)) + "\",";
    json += "\"description\":\"" + jsonEscape(String(preset.description)) + "\",";
    json += "\"baseMood\":\"" + jsonEscape(String(moodToString(preset.baseMood))) + "\",";
    json += "\"grumpiness\":" + String(preset.traits.grumpiness) + ",";
    json += "\"curiosity\":" + String(preset.traits.curiosity) + ",";
    json += "\"sleepiness\":" + String(preset.traits.sleepiness) + ",";
    json += "\"chaos\":" + String(preset.traits.chaos) + ",";
    json += "\"friendliness\":" + String(preset.traits.friendliness);
    json += "}";
  }

  json += "]";
  return json;
}

String buildMoodJson() {
  unsigned long now = millis();
  long remaining = 0;

  if (moodUntilMs > 0 && now < moodUntilMs) {
    remaining = moodUntilMs - now;
  }

  long nextIdleIn = 0;
  if (lastIdleBehaviorMs > 0) {
    unsigned long elapsed = now - lastIdleBehaviorMs;
    if (elapsed < nextIdleBehaviorDelayMs) {
      nextIdleIn = nextIdleBehaviorDelayMs - elapsed;
    }
  }

  String json = "{";
  json += "\"currentMood\":\"" + jsonEscape(String(moodToString(currentMood))) + "\",";
  json += "\"currentMoodLabel\":\"" + jsonEscape(moodLabel(currentMood)) + "\",";
  json += "\"baseMood\":\"" + jsonEscape(String(moodToString(baseMood))) + "\",";
  json += "\"baseMoodLabel\":\"" + jsonEscape(moodLabel(baseMood)) + "\",";
  json += "\"activePreset\":\"" + jsonEscape(activePersonalityPresetId()) + "\",";
  json += "\"activePresetLabel\":\"" + jsonEscape(activePersonalityPresetLabel()) + "\",";
  json += "\"temporary\":" + String(remaining > 0 ? "true" : "false") + ",";
  json += "\"remainingMs\":" + String(remaining) + ",";
  json += "\"moodUntilMs\":" + String(moodUntilMs) + ",";
  json += "\"lastMoodChangeMs\":" + String(lastMoodChangeMs) + ",";
  json += "\"lastMoodChangeAgeMs\":" + String(now - lastMoodChangeMs) + ",";
  json += "\"priority\":\"" + jsonEscape(moodPriorityLabel(currentMoodPriority)) + "\",";
  json += "\"displayMode\":\"" + jsonEscape(String(oledOverlayModeName(oledOverlayMode))) + "\",";
  json += "\"displayModeLabel\":\"" + jsonEscape(displayModeLabel()) + "\",";
  json += "\"overlayActive\":" + String(oledOverlayActive ? "true" : "false") + ",";
  json += "\"faceDisplayAvailable\":" + String(faceDisplayAvailable() ? "true" : "false") + ",";
  json += "\"automationSummary\":\"" + jsonEscape(moodAutomationSummary()) + "\",";
  json += "\"safetySummary\":\"" + jsonEscape(moodSafetySummary()) + "\",";
  json += "\"idleMoodEnabled\":" + String(idleMoodEnabled ? "true" : "false") + ",";
  json += "\"nextIdleInMs\":" + String(nextIdleIn) + ",";
  json += "\"nextIdleDelayMs\":" + String(nextIdleBehaviorDelayMs) + ",";
  json += "\"lastIdleMood\":\"" + jsonEscape(String(moodToString(lastIdleMood))) + "\",";
  json += "\"lastIdleMoodLabel\":\"" + jsonEscape(moodLabel(lastIdleMood)) + "\",";
  json += "\"lastIdleReason\":\"" + jsonEscape(lastIdleReason) + "\",";
  json += "\"lastIdleMoodAgeMs\":" + String(lastIdleMoodMs > 0 ? now - lastIdleMoodMs : 0) + ",";
  json += "\"idleMoodCount\":" + String(idleMoodCount) + ",";
  json += "\"weatherMoodEnabled\":" + String(weatherMoodEnabled ? "true" : "false") + ",";
  json += "\"wifiMoodEnabled\":" + String(wifiMoodEnabled ? "true" : "false") + ",";
  json += "\"webMoodEnabled\":" + String(webMoodEnabled ? "true" : "false") + ",";
  json += "\"movementMoodEnabled\":" + String(movementMoodEnabled ? "true" : "false") + ",";
  json += "\"memoryEnabled\":" + String(memoryEnabled ? "true" : "false") + ",";
  json += "\"needsEnabled\":" + String(needsEnabled ? "true" : "false") + ",";
  json += "\"grudgeSummary\":\"" + jsonEscape(getGrudgeSummary()) + "\",";
  json += "\"memoryBiasSummary\":\"" + jsonEscape(getMemoryBiasSummary()) + "\",";
  json += "\"needsSummary\":\"" + jsonEscape(getNeedsSummary()) + "\",";
  json += "\"movementSensitivity\":" + String(movementSensitivity) + ",";
  json += "\"shakeSensitivity\":" + String(shakeSensitivity) + ",";
  json += "\"lastMovementEvent\":\"" + jsonEscape(lastMovementEvent) + "\",";
  json += "\"lastMovementReason\":\"" + jsonEscape(lastMovementReason) + "\",";
  json += "\"movementMoodCount\":" + String(movementMoodCount) + ",";
  json += "\"lastGyroMoodAgeMs\":" + String(lastGyroMoodMs > 0 ? now - lastGyroMoodMs : 0) + ",";
  json += "\"lastGyroActivityAgeMs\":" + String(lastMovementActivityMs > 0 ? now - lastMovementActivityMs : 0) + ",";
  json += "\"lastReactionEvent\":\"" + jsonEscape(lastReactionEvent) + "\",";
  json += "\"lastReactionReason\":\"" + jsonEscape(lastReactionReason) + "\",";
  json += "\"reactionMoodCount\":" + String(reactionMoodCount) + ",";
  json += "\"lastWifiRssi\":" + String(lastWifiRssi) + ",";
  json += "\"lastWifiMoodAgeMs\":" + String(lastWifiMoodMs > 0 ? now - lastWifiMoodMs : 0) + ",";
  json += "\"lastWeatherMoodAgeMs\":" + String(lastWeatherMoodMs > 0 ? now - lastWeatherMoodMs : 0) + ",";
  json += "\"lastWebMoodAgeMs\":" + String(lastWebMoodMs > 0 ? now - lastWebMoodMs : 0) + ",";
  long nextSequenceIn = 0;
  if (sequenceActive() && nextSequenceStepAtMs > 0 && now < nextSequenceStepAtMs) {
    nextSequenceIn = nextSequenceStepAtMs - now;
  }
  json += "\"sequenceActive\":" + String(sequenceActive() ? "true" : "false") + ",";
  json += "\"activeSequence\":\"" + jsonEscape(String(sequenceToString(activeSequence))) + "\",";
  json += "\"activeSequenceLabel\":\"" + jsonEscape(sequenceLabel(activeSequence)) + "\",";
  json += "\"lastSequence\":\"" + jsonEscape(String(sequenceToString(lastSequence))) + "\",";
  json += "\"lastSequenceLabel\":\"" + jsonEscape(sequenceLabel(lastSequence)) + "\",";
  json += "\"sequenceStep\":" + String(sequenceStep) + ",";
  json += "\"nextSequenceInMs\":" + String(nextSequenceIn) + ",";
  json += "\"sequenceRunCount\":" + String(sequenceRunCount) + ",";
  json += "\"sequencePausedForOverlay\":" + String(sequencePausedForOverlay ? "true" : "false") + ",";
  json += "\"lastSequenceReason\":\"" + jsonEscape(lastSequenceReason) + "\",";
  json += "\"pokeStagePending\":" + String(pokeStagePending ? "true" : "false") + ",";
  json += "\"personality\":{";
  json += "\"grumpiness\":" + String(personality.grumpiness) + ",";
  json += "\"curiosity\":" + String(personality.curiosity) + ",";
  json += "\"sleepiness\":" + String(personality.sleepiness) + ",";
  json += "\"chaos\":" + String(personality.chaos) + ",";
  json += "\"friendliness\":" + String(personality.friendliness);
  json += "}";
  json += "}";

  return json;
}

String moodLine() {
  if (demoMode) {
    return "Running the tiny goblin face parade.";
  }

  if (sequenceActive()) {
    return String("Sequence: ") + sequenceLabel(activeSequence) + " step " + String(sequenceStep);
  }

  if (sensorOverridePriority > 0 && millis() < sensorOverrideUntil) {
    return String("Reacting to ") + lastTriggerName + ".";
  }

  if (boredActive) {
    return "Bored. Emotionally chewing the furniture.";
  }

  String line = String("Mood: ") + moodLabel(currentMood);
  if (moodTemporaryActive()) {
    line += String(" · ") + String(moodUntilMs - millis()) + " ms left";
  } else {
    line += String(" · Base: ") + moodLabel(baseMood);
  }

  switch (currentMood) {
    case MOOD_DEADPAN: return line + " · Calm, suspicious, and operational.";
    case MOOD_HAPPY: return line + " · Tiny OLED morale spike detected.";
    case MOOD_ANNOYED: return line + " · Side-eye subsystem engaged.";
    case MOOD_ANGRY: return line + " · Judging the wiring choices.";
    case MOOD_SLEEPY: return line + " · Sleepy but technically still employed.";
    case MOOD_CURIOUS: return line + " · Investigating absolutely nothing.";
    case MOOD_STARTLED: return line + " · Maximum eyebrow voltage.";
    case MOOD_SMUG: return line + " · He knows something and refuses to explain.";
    case MOOD_SAD: return line + " · Low battery vibes, emotionally speaking.";
    case MOOD_WEATHER: return line + " · Consulting sky gossip.";
    case MOOD_BOOTING: return line + " · Boot drama in progress.";
    default: return line + " · Existing in monochrome.";
  }
}

String buildEventLogJson() {
  String json = "[";

  for (uint8_t i = 0; i < eventLogCount; i++) {
    int index = (int)eventLogHead - 1 - i;

    while (index < 0) {
      index += YETI_EVENT_LOG_SIZE;
    }

    EventLogEntry &entry = eventLog[index % YETI_EVENT_LOG_SIZE];

    if (i > 0) {
      json += ",";
    }

    json += "{";
    json += "\"ms\":" + String(entry.ms) + ",";
    json += "\"ageMs\":" + String(millis() - entry.ms) + ",";
    json += "\"type\":\"" + jsonEscape(String(entry.type)) + "\",";
    json += "\"message\":\"" + jsonEscape(String(entry.message)) + "\"";
    json += "}";
  }

  json += "]";
  return json;
}

String buildI2cScanJson() {
  String json = "[";
  bool first = true;

  for (byte address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    byte error = Wire.endTransmission();

    if (error == 0) {
      if (!first) {
        json += ",";
      }

      String likely = "Unknown";

      if (address == YETI_OLED_I2C_ADDRESS || address == 0x3C || address == 0x3D) {
        likely = "SSD1306 OLED";
      } else if (address == YETI_MPU_I2C_ADDRESS || address == 0x68 || address == 0x69) {
        likely = "MPU gyro/accelerometer";
      }

      json += "{";
      json += "\"address\":\"" + hexAddress(address) + "\",";
      json += "\"decimal\":" + String(address) + ",";
      json += "\"likely\":\"" + jsonEscape(likely) + "\"";
      json += "}";

      first = false;
    }

    delay(1);
  }

  json += "]";
  addEvent("I2C", "Manual I2C scan completed");
  return json;
}

String buildStatusJson() {
  unsigned long now = millis();

  bool wifiConnected = WiFi.status() == WL_CONNECTED;

  long overrideRemaining = 0;
  if (sensorOverridePriority > 0 && now < sensorOverrideUntil) {
    overrideRemaining = sensorOverrideUntil - now;
  }

  uint64_t chipId = ESP.getEfuseMac();

  String json = "{";

  json += "\"app\":{";
  json += "\"name\":\"" + jsonEscape(String(APP_NAME)) + "\",";
  json += "\"version\":\"" + jsonEscape(String(APP_VERSION)) + "\",";
  json += "\"hostname\":\"" + jsonEscape(configuredHostname) + "\",";
  json += "\"setupMode\":" + String(setupMode ? "true" : "false");
  json += "},";

  json += "\"hardware\":{";
  json += "\"boardProfile\":\"" + jsonEscape(String(YETI_BOARD_PROFILE_NAME)) + "\",";
  json += "\"i2cSda\":" + String(YETI_I2C_SDA_PIN) + ",";
  json += "\"i2cScl\":" + String(YETI_I2C_SCL_PIN) + ",";
  json += "\"i2cClock\":" + String(YETI_I2C_CLOCK_HZ) + ",";
  json += "\"oledEnabled\":" + String(YETI_OLED_ENABLED ? "true" : "false") + ",";
  json += "\"oledAddress\":\"0x" + String(YETI_OLED_I2C_ADDRESS, HEX) + "\",";
  json += "\"mpuEnabled\":" + String(YETI_MPU_ENABLED ? "true" : "false") + ",";
  json += "\"mpuAddress\":\"0x" + String(YETI_MPU_I2C_ADDRESS, HEX) + "\",";
  json += "\"touchEnabled\":" + String(YETI_TOUCH_ENABLED ? "true" : "false") + ",";
  json += "\"touchPin\":\"" + jsonEscape(String(YETI_TOUCH_PIN_LABEL)) + "\"";
  json += "},";

  json += "\"wifi\":{";
  json += "\"connected\":" + String(wifiConnected ? "true" : "false") + ",";
  json += "\"ssid\":\"" + jsonEscape(WiFi.SSID()) + "\",";
  json += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
  json += "\"rssi\":" + String(WiFi.RSSI()) + ",";
  json += "\"status\":\"" + jsonEscape(wifiStatusName(WiFi.status())) + "\",";
  json += "\"setupAp\":\"" + jsonEscape(setupApName) + "\",";
  json += "\"setupIp\":\"" + setupIP.toString() + "\"";
  json += "},";


  json += "\"weather\":{";
  json += "\"enabled\":" + String(weatherEnabled ? "true" : "false") + ",";
  json += "\"available\":" + String(weatherAvailable ? "true" : "false") + ",";
  json += "\"locationConfigured\":" + String(weatherLocationConfigured() ? "true" : "false") + ",";
  json += "\"locationName\":\"" + jsonEscape(weatherLocationName) + "\",";
  json += "\"latitude\":" + String(weatherLatitude, 6) + ",";
  json += "\"longitude\":" + String(weatherLongitude, 6) + ",";
  json += "\"metric\":" + String(weatherMetric ? "true" : "false") + ",";
  json += "\"tempUnit\":\"" + weatherTempUnit() + "\",";
  json += "\"windUnit\":\"" + weatherWindUnit() + "\",";
  json += "\"precipUnit\":\"" + weatherPrecipUnit() + "\",";
  json += "\"updateMs\":" + String(weatherUpdateMs) + ",";
  json += "\"updateText\":\"" + jsonEscape(compactMs(weatherUpdateMs)) + "\",";
  json += "\"fetching\":" + String(weatherFetchInProgress ? "true" : "false") + ",";
  json += "\"lastError\":\"" + jsonEscape(weatherLastError) + "\",";
  json += "\"httpCode\":" + String(weatherHttpCode) + ",";
  json += "\"lastAttemptAgeMs\":" + String(lastWeatherAttemptMs ? now - lastWeatherAttemptMs : 0) + ",";
  json += "\"lastSuccessAgeMs\":" + String(lastWeatherSuccessMs ? now - lastWeatherSuccessMs : 0) + ",";
  json += "\"currentTime\":\"" + jsonEscape(weatherCurrentTime) + "\",";
  json += "\"timezone\":\"" + jsonEscape(weatherTimezone) + "\",";
  json += "\"utcOffsetSeconds\":" + String(weatherUtcOffsetSeconds) + ",";
  json += "\"temperature\":" + String(weatherTemperature, 2) + ",";
  json += "\"feelsLike\":" + String(weatherFeelsLike, 2) + ",";
  json += "\"humidity\":" + String(weatherHumidity, 1) + ",";
  json += "\"precipitation\":" + String(weatherPrecipitation, 3) + ",";
  json += "\"windSpeed\":" + String(weatherWindSpeed, 2) + ",";
  json += "\"cloudCover\":" + String(weatherCloudCover, 1) + ",";
  json += "\"weatherCode\":" + String(weatherCode) + ",";
  json += "\"condition\":\"" + jsonEscape(weatherCodeDescription(weatherCode)) + "\",";
  json += "\"isDay\":" + String(weatherIsDay ? "true" : "false") + ",";
  json += "\"summary\":\"" + jsonEscape(weatherSummaryLine()) + "\",";
  json += "\"tickerLine\":\"" + jsonEscape(weatherAvailable ? weatherTickerLine() : String("")) + "\"";
  json += "},";

  json += "\"clock\":{";
  json += "\"enabled\":" + String(clockEnabled ? "true" : "false") + ",";
  json += "\"synced\":" + String(clockSynced() ? "true" : "false") + ",";
  json += "\"time\":\"" + jsonEscape(formatClockTime()) + "\",";
  json += "\"date\":\"" + jsonEscape(formatClockDate()) + "\",";
  json += "\"format24h\":" + String(clock24h ? "true" : "false") + ",";
  json += "\"utcOffsetMinutes\":" + String(clockUtcOffsetMinutes) + ",";
  json += "\"activeOffsetLabel\":\"" + jsonEscape(clockOffsetLabel()) + "\",";
  json += "\"activeOffsetSource\":\"" + jsonEscape(clockOffsetSource()) + "\",";
  json += "\"useWeatherOffset\":" + String(clockUseWeatherOffset ? "true" : "false") + ",";
  json += "\"weatherOffsetUsable\":" + String(weatherClockOffsetUsable() ? "true" : "false") + ",";
  json += "\"ntpStarted\":" + String(ntpStarted ? "true" : "false") + ",";
  json += "\"infoCardsEnabled\":" + String(infoCardsEnabled ? "true" : "false") + ",";
  json += "\"infoCardClockEnabled\":" + String(infoCardClockEnabled ? "true" : "false") + ",";
  json += "\"infoCardWeatherEnabled\":" + String(infoCardWeatherEnabled ? "true" : "false") + ",";
  json += "\"infoCardIntervalMs\":" + String(infoCardIntervalMs) + ",";
  json += "\"infoCardIntervalText\":\"" + jsonEscape(compactMs(infoCardIntervalMs)) + "\"";
  json += "},";

  json += "\"sleep\":";
  json += buildSleepJson();
  json += ",";

  json += "\"face\":{";
  json += "\"baseExpression\":\"" + jsonEscape(expressionName(expressionForMood(baseMood))) + "\",";
  json += "\"activeExpression\":\"" + jsonEscape(expressionName(activeExpression())) + "\",";
  json += "\"moodLine\":\"" + jsonEscape(moodLine()) + "\",";
  json += "\"sensorOverrideExpression\":\"" + jsonEscape(expressionName(sensorOverrideExpression)) + "\",";
  json += "\"sensorOverrideRemainingMs\":" + String(overrideRemaining) + ",";
  json += "\"lastTrigger\":\"" + jsonEscape(lastTriggerName) + "\",";
  json += "\"boredActive\":" + String(boredActive ? "true" : "false") + ",";
  json += "\"demoMode\":" + String(demoMode ? "true" : "false") + ",";
  json += "\"lastEngagementAgeMs\":" + String(now - lastEngagementMs) + ",";
  json += "\"lastInteractionAgeMs\":" + String(now - lastInteractionMs) + ",";
  json += "\"blinking\":" + String(blinking ? "true" : "false");
  json += "},";

  json += "\"mood\":";
  json += buildMoodJson();
  json += ",";

  json += "\"memory\":";
  json += buildMemoryJson();
  json += ",";

  json += "\"sensors\":{";
  json += "\"mpuReady\":" + String(mpuReady ? "true" : "false") + ",";
  json += "\"lastShakeScore\":" + String(lastShakeScore, 2) + ",";
  json += "\"lastMotionJerk\":" + String(lastMotionJerk, 2) + ",";
  json += "\"lastTiltDelta\":" + String(lastTiltDelta, 2) + ",";
  json += "\"lastAccelMagnitude\":" + String(lastAccelMagnitude, 2) + ",";
  json += "\"movementJerkThreshold\":" + String(movementJerkThresholdG(), 2) + ",";
  json += "\"movementTiltThreshold\":" + String(movementTiltThresholdG(), 2) + ",";
  json += "\"shakeJerkThreshold\":" + String(movementShakeJerkThresholdG(), 2) + ",";
  json += "\"shakeMagThreshold\":" + String(movementShakeMagThresholdG(), 2) + ",";
  json += "\"touchReady\":" + String(touchReady ? "true" : "false") + ",";
  json += "\"lastTouchValue\":" + String(lastTouchValue) + ",";
  json += "\"touchBaseline\":" + String(touchBaseline) + ",";
  json += "\"touchThreshold\":" + String(touchThreshold);
  json += "},";

  json += "\"config\":{";

  json += "\"expressions\":";
  json += expressionOptionsJson();
  json += ",";

  json += "\"moods\":";
  json += moodOptionsJson();
  json += ",";

  json += "\"personalityPresets\":";
  json += personalityPresetOptionsJson();
  json += ",";

  json += "\"personality\":{";
  json += "\"baseMood\":\"" + jsonEscape(String(moodToString(baseMood))) + "\",";
  json += "\"activePreset\":\"" + jsonEscape(activePersonalityPresetId()) + "\",";
  json += "\"activePresetLabel\":\"" + jsonEscape(activePersonalityPresetLabel()) + "\",";
  json += "\"idleMoodEnabled\":" + String(idleMoodEnabled ? "true" : "false") + ",";
  json += "\"weatherMoodEnabled\":" + String(weatherMoodEnabled ? "true" : "false") + ",";
  json += "\"wifiMoodEnabled\":" + String(wifiMoodEnabled ? "true" : "false") + ",";
  json += "\"webMoodEnabled\":" + String(webMoodEnabled ? "true" : "false") + ",";
  json += "\"movementMoodEnabled\":" + String(movementMoodEnabled ? "true" : "false") + ",";
  json += "\"memoryEnabled\":" + String(memoryEnabled ? "true" : "false") + ",";
  json += "\"needsEnabled\":" + String(needsEnabled ? "true" : "false") + ",";
  json += "\"sassEnabled\":" + String(sassTickerEnabled ? "true" : "false") + ",";
  json += "\"sassIdleOnly\":" + String(sassIdleOnly ? "true" : "false") + ",";
  json += "\"sassEventEnabled\":" + String(sassEventEnabled ? "true" : "false") + ",";
  json += "\"sassGrudgeEnabled\":" + String(sassGrudgeEnabled ? "true" : "false") + ",";
  json += "\"sassLevel\":" + String(sassLevel) + ",";
  json += "\"sassFrequency\":" + String(sassFrequency) + ",";
  json += "\"grudgeSummary\":\"" + jsonEscape(getGrudgeSummary()) + "\",";
  json += "\"memoryBiasSummary\":\"" + jsonEscape(getMemoryBiasSummary()) + "\",";
  json += "\"needsSummary\":\"" + jsonEscape(getNeedsSummary()) + "\",";
  json += "\"movementSensitivity\":" + String(movementSensitivity) + ",";
  json += "\"shakeSensitivity\":" + String(shakeSensitivity) + ",";
  json += "\"grumpiness\":" + String(personality.grumpiness) + ",";
  json += "\"curiosity\":" + String(personality.curiosity) + ",";
  json += "\"sleepiness\":" + String(personality.sleepiness) + ",";
  json += "\"chaos\":" + String(personality.chaos) + ",";
  json += "\"friendliness\":" + String(personality.friendliness);
  json += "},";

  json += "\"shake\":{";
  json += "\"enabled\":" + String(shakeConfig.enabled ? "true" : "false") + ",";
  json += "\"holdMs\":" + String(shakeConfig.holdMs) + ",";
  json += "\"expressionMask\":" + String(shakeConfig.expressionMask);
  json += "},";

  json += "\"touch\":{";
  json += "\"enabled\":" + String(touchConfig.enabled ? "true" : "false") + ",";
  json += "\"holdMs\":" + String(touchConfig.holdMs) + ",";
  json += "\"expressionMask\":" + String(touchConfig.expressionMask);
  json += "},";

  json += "\"bored\":{";
  json += "\"timeoutMs\":" + String(boredTimeoutMs) + ",";
  json += "\"enabled\":" + String(boredTimeoutMs > 0 ? "true" : "false");
  json += "},";

  json += "\"network\":{";
  json += "\"hostname\":\"" + jsonEscape(configuredHostname) + "\",";
  json += "\"defaultHostname\":\"" + jsonEscape(String(DEFAULT_HOSTNAME)) + "\",";
  json += "\"hostnameMaxLen\":" + String(YETI_HOSTNAME_MAX_LEN);
  json += "},";

  json += "\"performance\":{";
  json += "\"faceFrameMs\":" + String(faceFrameMs) + ",";
  json += "\"faceFrameMinMs\":" + String(YETI_FACE_FRAME_MIN_MS) + ",";
  json += "\"faceFrameMaxMs\":" + String(YETI_FACE_FRAME_MAX_MS);
  json += "},";

  json += "\"weather\":{";
  json += "\"enabled\":" + String(weatherEnabled ? "true" : "false") + ",";
  json += "\"locationName\":\"" + jsonEscape(weatherLocationName) + "\",";
  json += "\"latitude\":" + String(weatherLatitude, 6) + ",";
  json += "\"longitude\":" + String(weatherLongitude, 6) + ",";
  json += "\"metric\":" + String(weatherMetric ? "true" : "false") + ",";
  json += "\"updateMs\":" + String(weatherUpdateMs) + ",";
  json += "\"updateMinMs\":" + String(YETI_WEATHER_UPDATE_MIN_MS) + ",";
  json += "\"updateMaxMs\":" + String(YETI_WEATHER_UPDATE_MAX_MS) + ",";
  json += "\"tickerLocation\":" + String(weatherTickerShowLocation ? "true" : "false") + ",";
  json += "\"tickerTemp\":" + String(weatherTickerShowTemp ? "true" : "false") + ",";
  json += "\"tickerCondition\":" + String(weatherTickerShowCondition ? "true" : "false") + ",";
  json += "\"tickerFeelsLike\":" + String(weatherTickerShowFeelsLike ? "true" : "false") + ",";
  json += "\"tickerHumidity\":" + String(weatherTickerShowHumidity ? "true" : "false") + ",";
  json += "\"tickerWind\":" + String(weatherTickerShowWind ? "true" : "false") + ",";
  json += "\"tickerPrecip\":" + String(weatherTickerShowPrecip ? "true" : "false") + ",";
  json += "\"tickerUpdated\":" + String(weatherTickerShowUpdated ? "true" : "false");
  json += "},";

  json += "\"clock\":{";
  json += "\"enabled\":" + String(clockEnabled ? "true" : "false") + ",";
  json += "\"format24h\":" + String(clock24h ? "true" : "false") + ",";
  json += "\"utcOffsetMinutes\":" + String(clockUtcOffsetMinutes) + ",";
  json += "\"useWeatherOffset\":" + String(clockUseWeatherOffset ? "true" : "false") + ",";
  json += "\"infoCardsEnabled\":" + String(infoCardsEnabled ? "true" : "false") + ",";
  json += "\"infoCardClockEnabled\":" + String(infoCardClockEnabled ? "true" : "false") + ",";
  json += "\"infoCardWeatherEnabled\":" + String(infoCardWeatherEnabled ? "true" : "false") + ",";
  json += "\"infoCardIntervalMs\":" + String(infoCardIntervalMs) + ",";
  json += "\"infoCardMinMs\":" + String(YETI_INFO_CARD_MIN_MS) + ",";
  json += "\"infoCardMaxMs\":" + String(YETI_INFO_CARD_MAX_MS);
  json += "},";

  json += "\"sleep\":{";
  json += "\"enabled\":" + String(sleepModeEnabled ? "true" : "false") + ",";
  json += "\"startMinute\":" + String(sleepStartMinute) + ",";
  json += "\"endMinute\":" + String(sleepEndMinute) + ",";
  json += "\"startTime\":\"" + minuteToTimeString(sleepStartMinute) + "\",";
  json += "\"endTime\":\"" + minuteToTimeString(sleepEndMinute) + "\",";
  json += "\"animationDurationMs\":" + String(sleepAnimDurationMs) + ",";
  json += "\"gapMinMs\":" + String(sleepGapMinMs) + ",";
  json += "\"gapMaxMs\":" + String(sleepGapMaxMs) + ",";
  json += "\"wakeHoldMs\":" + String(sleepWakeHoldMs) + ",";
  json += "\"animMinMs\":" + String(YETI_SLEEP_ANIM_MIN_MS) + ",";
  json += "\"animMaxMs\":" + String(YETI_SLEEP_ANIM_MAX_MS) + ",";
  json += "\"gapMinAllowedMs\":" + String(YETI_SLEEP_GAP_MIN_ALLOWED_MS) + ",";
  json += "\"gapMaxAllowedMs\":" + String(YETI_SLEEP_GAP_MAX_ALLOWED_MS) + ",";
  json += "\"wakeHoldMinMs\":" + String(YETI_SLEEP_WAKE_HOLD_MIN_MS) + ",";
  json += "\"wakeHoldMaxMs\":" + String(YETI_SLEEP_WAKE_HOLD_MAX_MS);
  json += "}";

  json += "},";

  json += "\"system\":{";
  json += "\"uptimeText\":\"" + jsonEscape(uptimeText()) + "\",";
  json += "\"uptimeSeconds\":" + String(millis() / 1000) + ",";
  json += "\"freeHeap\":\"" + bytesToHuman(ESP.getFreeHeap()) + "\",";
  json += "\"heapSize\":\"" + bytesToHuman(ESP.getHeapSize()) + "\",";
  json += "\"minFreeHeap\":\"" + bytesToHuman(ESP.getMinFreeHeap()) + "\",";
  json += "\"maxAllocHeap\":\"" + bytesToHuman(ESP.getMaxAllocHeap()) + "\",";
  json += "\"sketchSize\":\"" + bytesToHuman(ESP.getSketchSize()) + "\",";
  json += "\"freeSketchSpace\":\"" + bytesToHuman(ESP.getFreeSketchSpace()) + "\",";
  json += "\"flashSize\":\"" + bytesToHuman(ESP.getFlashChipSize()) + "\",";
  json += "\"flashUsed\":\"" + bytesToHuman(ESP.getSketchSize()) + "\",";
  uint32_t sketchSize = ESP.getSketchSize();
  uint32_t freeSketch = ESP.getFreeSketchSpace();
  uint32_t appSlotTotal = sketchSize + freeSketch;
  json += "\"appSlotSize\":\"" + bytesToHuman(appSlotTotal) + "\",";
  json += "\"appSlotFree\":\"" + bytesToHuman(freeSketch) + "\",";
  json += "\"appSlotUsedPct\":" + String(appSlotTotal ? ((sketchSize * 100.0f) / appSlotTotal) : 0.0f, 1) + ",";
  json += "\"chipModel\":\"" + jsonEscape(String(ESP.getChipModel())) + "\",";
  json += "\"chipCores\":" + String(ESP.getChipCores()) + ",";
  json += "\"cpuMHz\":" + String(ESP.getCpuFreqMHz()) + ",";
  json += "\"sdk\":\"" + jsonEscape(String(ESP.getSdkVersion())) + "\",";
  json += "\"efuseMac\":\"";
  json += String((uint16_t)(chipId >> 32), HEX);
  json += String((uint32_t)chipId, HEX);
  json += "\",";
  json += "\"lastFaceRenderMs\":" + String(lastFaceRenderMs) + ",";
  json += "\"configuredFrameMs\":" + String(faceFrameMs);
  json += "},";

  json += "\"events\":";
  json += buildEventLogJson();

  json += "}";

  return json;
}

// =====================================================
// SETUP PORTAL HTML
// =====================================================

const char SETUP_HTML[] PROGMEM = R"HTML(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1.0">
<title>Yeti Setup</title>
<style>
:root {
--bg: #071019;
--card: rgba(255,255,255,0.085);
--border: rgba(255,255,255,0.14);
--text: #eef7ff;
--muted: #9eb3c8;
--accent: #9de8ff;
--accent2: #d7f7ff;
}
* { box-sizing: border-box; }
body {
margin: 0;
min-height: 100vh;
font-family: system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
color: var(--text);
background:
radial-gradient(circle at top left, rgba(157,232,255,0.22), transparent 34%),
radial-gradient(circle at bottom right, rgba(255,255,255,0.10), transparent 38%),
var(--bg);
display: grid;
place-items: center;
padding: 18px;
}
.wrap { width: 100%; max-width: 560px; }
.card {
background: var(--card);
border: 1px solid var(--border);
border-radius: 24px;
padding: 22px;
box-shadow: 0 24px 70px rgba(0,0,0,0.38);
backdrop-filter: blur(14px);
}
h1 {
margin: 0 0 6px;
font-size: clamp(1.9rem, 7vw, 3.2rem);
letter-spacing: -0.05em;
}
.subtitle, .note, .tiny {
color: var(--muted);
line-height: 1.45;
}
.subtitle { margin-bottom: 18px; }
label {
display: block;
margin: 14px 0 7px;
color: var(--accent);
font-weight: 800;
font-size: 0.9rem;
text-transform: uppercase;
letter-spacing: 0.06em;
}
input {
width: 100%;
border: 1px solid var(--border);
border-radius: 14px;
background: rgba(0,0,0,0.28);
color: var(--text);
padding: 13px 14px;
font-size: 1rem;
outline: none;
}
input[type="checkbox"] {
width: auto;
accent-color: var(--accent);
}
.check {
display: flex;
align-items: center;
gap: 9px;
text-transform: none;
letter-spacing: normal;
color: var(--text);
font-weight: 650;
}
.mini-grid {
display: grid;
grid-template-columns: 1fr 1fr;
gap: 10px;
}
@media (max-width: 620px) { .mini-grid { grid-template-columns: 1fr; } }
input:focus {
border-color: var(--accent);
box-shadow: 0 0 0 3px rgba(157,232,255,0.14);
}
button {
border: 0;
border-radius: 14px;
padding: 13px 15px;
background: linear-gradient(135deg, var(--accent), var(--accent2));
color: #061018;
font-weight: 900;
cursor: pointer;
font-size: 1rem;
box-shadow: 0 16px 34px rgba(157,232,255,0.18);
}
button.secondary {
background: rgba(255,255,255,0.1);
color: var(--text);
border: 1px solid var(--border);
box-shadow: none;
}
button:disabled {
opacity: 0.5;
cursor: not-allowed;
}
.row {
display: flex;
gap: 10px;
flex-wrap: wrap;
margin-top: 16px;
}
.row button {
flex: 1;
min-width: 150px;
}
.networks {
margin-top: 14px;
border-top: 1px solid rgba(255,255,255,0.1);
padding-top: 14px;
}
.network {
padding: 11px;
border-radius: 14px;
background: rgba(255,255,255,0.06);
margin-bottom: 8px;
border: 1px solid rgba(255,255,255,0.08);
cursor: pointer;
}
.network:hover {
border-color: var(--accent);
}
.network strong {
display: block;
}
.network span {
display: block;
color: var(--muted);
font-size: 0.86rem;
margin-top: 3px;
}
.tiny {
font-size: 0.8rem;
margin-top: 14px;
text-align: center;
}
.toast-wrap {
position: fixed;
right: 16px;
bottom: 16px;
z-index: 20;
display: grid;
gap: 10px;
width: min(360px, calc(100vw - 32px));
}
.toast {
border: 1px solid var(--border);
border-left: 4px solid var(--accent);
background: rgba(7,16,25,0.94);
color: var(--text);
border-radius: 16px;
padding: 12px 14px;
box-shadow: 0 18px 44px rgba(0,0,0,0.35);
backdrop-filter: blur(14px);
animation: toastIn 0.18s ease-out;
}
.toast.error { border-left-color: #ff6f8a; }
.toast.warn { border-left-color: #ffd166; }
@keyframes toastIn {
from { opacity: 0; transform: translateY(8px); }
to { opacity: 1; transform: translateY(0); }
}
</style>
</head>
<body>
<div id="toastWrap" class="toast-wrap"></div>
<div class="wrap">
<div class="card">
<h1>Yeti Setup</h1>
<div class="subtitle">
Connect Yeti to your Wi-Fi. Pick a scanned network or enter the SSID manually.
</div>
<form id="setupForm" method="POST" action="/save">
<label for="hostname">Yeti Hostname</label>
<input id="hostname" name="hostname" maxlength="31" value="{{HOSTNAME}}" placeholder="yeti" autocomplete="off">
<div class="note">Use letters, numbers, and hyphens. The dashboard will try to answer at <strong>http://&lt;hostname&gt;.local/</strong> after joining Wi-Fi.</div>
<label for="ssid">Wi-Fi SSID</label>
<input id="ssid" name="ssid" placeholder="Your Wi-Fi name" required>
<label for="password">Wi-Fi Password</label>
<input id="password" name="password" type="password" placeholder="Leave blank for open network">
<div style="height:10px"></div>
<div class="note">Optional: seed Yeti's weather/clock settings now. You can also set this later from the dashboard.</div>
<label class="check">
<input type="checkbox" id="weatherEnabled" name="weatherEnabled" value="1">
Enable weather after setup
</label>
<label for="weatherName">Weather Location Name</label>
<input id="weatherName" name="weatherName" placeholder="Example: Anderson, CA">
<div class="mini-grid">
<div>
<label for="weatherLat">Latitude</label>
<input id="weatherLat" name="weatherLat" type="number" step="0.000001" min="-90" max="90" placeholder="40.4482">
</div>
<div>
<label for="weatherLon">Longitude</label>
<input id="weatherLon" name="weatherLon" type="number" step="0.000001" min="-180" max="180" placeholder="-122.2978">
</div>
</div>
<div class="mini-grid">
<div>
<label for="clockOffset">Clock UTC Offset Minutes</label>
<input id="clockOffset" name="clockOffset" type="number" step="15" min="-840" max="840" value="-420">
</div>
<div>
<label class="check" style="margin-top:38px;">
<input type="checkbox" id="weatherMetric" name="weatherMetric" value="1">
Metric units
</label>
</div>
</div>
<div class="row">
<button type="submit">Save & Reboot</button>
<button type="button" class="secondary" id="scanBtn" onclick="scanWifi()">Scan Networks</button>
</div>
</form>
<div class="note">
After saving, Yeti will reboot and join your Wi-Fi. The OLED will show the new IP address.
</div>
<div id="networks" class="networks"></div>
<div class="tiny">
Setup portal: 192.168.4.1
</div>
</div>
</div>
<script>
function esc(s) {
return String(s).replace(/[&<>"']/g, m => ({
'&': '&amp;',
'<': '&lt;',
'>': '&gt;',
'"': '&quot;',
"'": '&#39;'
}[m]));
}
function showToast(message, type = 'ok') {
const wrap = document.getElementById('toastWrap');
if (!wrap) return;
const toast = document.createElement('div');
toast.className = 'toast ' + type;
toast.textContent = message;
wrap.appendChild(toast);
setTimeout(() => {
toast.style.opacity = '0';
toast.style.transform = 'translateY(8px)';
toast.style.transition = 'opacity .18s ease, transform .18s ease';
setTimeout(() => toast.remove(), 220);
}, 3600);
}
function cleanHostname(value) {
return String(value || '')
.trim()
.toLowerCase()
.replace(/[_.\s]+/g, '-')
.replace(/[^a-z0-9-]/g, '')
.replace(/-+/g, '-')
.replace(/^-|-$/g, '') || 'yeti';
}
const setupForm = document.getElementById('setupForm');
setupForm.addEventListener('submit', async (ev) => {
ev.preventDefault();
const submitBtn = setupForm.querySelector('button[type="submit"]');
const hostnameInput = document.getElementById('hostname');
hostnameInput.value = cleanHostname(hostnameInput.value).slice(0, 31);
submitBtn.disabled = true;
submitBtn.textContent = 'Saving...';
showToast('Saving Wi-Fi and hostname...', 'ok');
try {
const res = await fetch('/save', {
method: 'POST',
body: new URLSearchParams(new FormData(setupForm))
});
const text = await res.text();
if (!res.ok) {
throw new Error(text || 'Setup save failed');
}
showToast('Saved. Yeti is rebooting now.', 'ok');
document.querySelector('.note').innerHTML = 'Saved. Rebooting now. After Yeti joins Wi-Fi, try <strong>http://' + esc(hostnameInput.value) + '.local/</strong> or check the OLED for the IP address.';
} catch (e) {
console.error(e);
showToast('Save failed. Check SSID/password and try again.', 'error');
submitBtn.disabled = false;
submitBtn.textContent = 'Save & Reboot';
}
});
showToast('Setup page loaded.', 'ok');
async function scanWifi() {
const btn = document.getElementById('scanBtn');
const box = document.getElementById('networks');
btn.disabled = true;
btn.textContent = 'Scanning...';
box.innerHTML = '<div class="note">Scanning nearby networks...</div>';
showToast('Scanning Wi-Fi networks...', 'ok');
try {
const res = await fetch('/api/scan');
const networks = await res.json();
if (!networks.length) {
box.innerHTML = '<div class="note">No networks found. Enter the SSID manually.</div>';
showToast('No networks found.', 'warn');
} else {
box.innerHTML = '';
networks.forEach(n => {
const item = document.createElement('div');
item.className = 'network';
item.innerHTML = `
<strong>${esc(n.ssid || '[Hidden Network]')}</strong>
<span>${esc(n.rssi)} dBm · Channel ${esc(n.channel)} · ${esc(n.encryption)}</span>
`;
item.onclick = () => {
document.getElementById('ssid').value = n.ssid || '';
document.getElementById('password').focus();
};
box.appendChild(item);
});
}
} catch (e) {
box.innerHTML = '<div class="note">Scan failed. Enter the SSID manually.</div>';
showToast('Wi-Fi scan failed.', 'error');
}
btn.disabled = false;
btn.textContent = 'Scan Networks';
}
</script>
</body>
</html>
)HTML";

// =====================================================
// DASHBOARD HTML
// =====================================================

const char DASHBOARD_HTML[] PROGMEM = R"HTML(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1.0">
<title>Yeti v1.7.1</title>
<style>
:root {
--bg: #071019;
--card: rgba(255,255,255,0.075);
--border: rgba(255,255,255,0.13);
--text: #eef7ff;
--muted: #9eb3c8;
--accent: #9de8ff;
--accent2: #d7f7ff;
--danger: #ff6f8a;
--good: #9de8ff;
--warn: #ffd166;
}
* { box-sizing: border-box; }
body {
margin: 0;
min-height: 100vh;
font-family: system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
color: var(--text);
background:
radial-gradient(circle at top left, rgba(157,232,255,0.19), transparent 35%),
radial-gradient(circle at bottom right, rgba(255,255,255,0.10), transparent 40%),
var(--bg);
}
header, main {
max-width: 1120px;
margin: 0 auto;
padding: 20px;
}
.title-row {
display: flex;
justify-content: space-between;
align-items: center;
gap: 14px;
flex-wrap: wrap;
}
h1 {
margin: 0;
font-size: clamp(2rem, 6vw, 3.5rem);
letter-spacing: -0.06em;
}
.subtitle {
color: var(--muted);
margin-top: 7px;
}
.pill {
display: inline-flex;
align-items: center;
gap: 8px;
padding: 10px 14px;
border-radius: 999px;
background: rgba(255,255,255,0.08);
border: 1px solid var(--border);
color: var(--muted);
}
.dot {
width: 10px;
height: 10px;
border-radius: 999px;
background: var(--warn);
box-shadow: 0 0 14px var(--warn);
}
.dot.good {
background: var(--good);
box-shadow: 0 0 14px var(--good);
}
.dot.bad {
background: var(--danger);
box-shadow: 0 0 14px var(--danger);
}
.grid {
display: grid;
grid-template-columns: repeat(12, 1fr);
gap: 16px;
}
.card {
grid-column: span 4;
background: var(--card);
border: 1px solid var(--border);
border-radius: 22px;
padding: 18px;
box-shadow: 0 18px 50px rgba(0,0,0,0.35);
backdrop-filter: blur(14px);
}
.card.wide { grid-column: span 8; }
.card.full { grid-column: 1 / -1; }

.summary-card {
grid-column: 1 / -1;
border-color: rgba(157,232,255,0.28);
background: rgba(157,232,255,0.09);
}
.summary-grid {
display: grid;
grid-template-columns: repeat(4, minmax(0, 1fr));
gap: 12px;
}
.summary-tile {
border: 1px solid rgba(255,255,255,0.09);
background: rgba(0,0,0,0.16);
border-radius: 16px;
padding: 12px;
min-height: 84px;
}
.summary-tile .label {
color: var(--muted);
font-size: .82rem;
text-transform: uppercase;
letter-spacing: .04em;
}
.summary-tile .metric {
font-size: 1.25rem;
font-weight: 900;
margin-top: 6px;
overflow-wrap: anywhere;
}
.accordion-card {
padding: 0;
overflow: hidden;
}
.accordion-head {
width: 100%;
background: transparent;
color: var(--text);
border: 0;
border-radius: 0;
padding: 16px 18px;
display: flex;
align-items: center;
justify-content: space-between;
gap: 12px;
text-align: left;
font-size: 1.02rem;
font-weight: 900;
letter-spacing: .02em;
text-transform: uppercase;
}
.accordion-head span:first-child { color: var(--accent); }
.accordion-head:hover { background: rgba(255,255,255,0.055); }
.accordion-chevron { color: var(--muted); transition: transform .18s ease; }
.accordion-card.open .accordion-chevron { transform: rotate(180deg); }
.accordion-body {
padding: 0 18px 18px;
display: none;
}
.accordion-card.open .accordion-body { display: block; }
.sticky-actions {
position: sticky;
bottom: 0;
z-index: 10;
margin-top: 12px;
padding: 10px;
border: 1px solid rgba(157,232,255,0.2);
border-radius: 18px;
background: rgba(7,16,25,0.92);
backdrop-filter: blur(16px);
}
@media (max-width: 860px) {
.summary-grid { grid-template-columns: repeat(2, minmax(0, 1fr)); }
.accordion-head { padding: 15px 16px; }
.accordion-body { padding: 0 16px 16px; }
}
@media (max-width: 520px) {
.summary-grid { grid-template-columns: 1fr; }
}

.top-nav {
max-width: 1120px;
margin: 0 auto;
padding: 0 20px 16px;
display: grid;
grid-template-columns: repeat(5, minmax(0, 1fr));
gap: 10px;
position: sticky;
top: 0;
z-index: 20;
backdrop-filter: blur(16px);
}
.navbtn {
background: rgba(255,255,255,0.09);
color: var(--text);
border: 1px solid var(--border);
border-radius: 16px;
padding: 12px 10px;
min-width: 0;
}
.navbtn.active {
background: linear-gradient(135deg, var(--accent), var(--accent2));
color: #061018;
}
section[data-page] { display: none; }
section[data-page].active { display: block; }
h2 {
margin: 0 0 12px;
font-size: 1.05rem;
color: var(--accent);
letter-spacing: 0.02em;
text-transform: uppercase;
}
.kv {
display: grid;
grid-template-columns: 1fr auto;
gap: 8px 14px;
padding: 6px 0;
border-bottom: 1px solid rgba(255,255,255,0.07);
}
.kv:last-child { border-bottom: none; }
.key { color: var(--muted); }
.value {
text-align: right;
font-weight: 750;
overflow-wrap: anywhere;
}
.big {
font-size: 2rem;
line-height: 1;
font-weight: 900;
letter-spacing: -0.05em;
}
.mini {
color: var(--muted);
margin-top: 7px;
font-size: 0.9rem;
}
button {
border: 0;
border-radius: 14px;
padding: 12px 14px;
background: linear-gradient(135deg, var(--accent), var(--accent2));
color: #061018;
font-weight: 900;
cursor: pointer;
}
button.secondary {
background: rgba(255,255,255,0.1);
color: var(--text);
border: 1px solid var(--border);
}
button.danger {
background: linear-gradient(135deg, #ff6f8a, #ffb86b);
}
button:disabled {
opacity: 0.55;
cursor: not-allowed;
}
input[type="number"], input[type="text"], input[type="time"], select {
width: 100%;
border: 1px solid var(--border);
border-radius: 14px;
background: rgba(0,0,0,0.28);
color: var(--text);
padding: 11px 12px;
font-size: 1rem;
outline: none;
}
input[type="range"] {
width: 100%;
accent-color: var(--accent);
}
.trait-line {
display: grid;
grid-template-columns: 1fr auto;
gap: 10px;
align-items: center;
margin: 10px 0 4px;
}
.trait-line strong { color: var(--text); }
.trait-line span { color: var(--accent); font-weight: 900; }
input[type="number"]:focus, input[type="text"]:focus, input[type="time"]:focus, select:focus {
border-color: var(--accent);
box-shadow: 0 0 0 3px rgba(157,232,255,0.14);
}
label.setting {
display: flex;
gap: 9px;
align-items: center;
color: var(--text);
padding: 7px 0;
}
.row {
display: flex;
gap: 10px;
flex-wrap: wrap;
margin-top: 14px;
}
.row button {
flex: 1;
min-width: 130px;
}
.settings-grid {
display: grid;
grid-template-columns: repeat(2, minmax(0, 1fr));
gap: 16px;
margin-top: 14px;
}
.panel {
border: 1px solid rgba(255,255,255,0.09);
background: rgba(255,255,255,0.04);
border-radius: 16px;
padding: 14px;
}
.panel-title {
font-weight: 900;
color: var(--accent);
margin-bottom: 8px;
}
.muted {
color: var(--muted);
line-height: 1.45;
}
.expr-list {
display: grid;
grid-template-columns: repeat(2, minmax(0, 1fr));
gap: 4px 10px;
margin-top: 8px;
}
.log {
display: grid;
gap: 8px;
max-height: 280px;
overflow: auto;
padding-right: 4px;
}
.log-entry {
border: 1px solid rgba(255,255,255,0.08);
background: rgba(0,0,0,0.18);
border-radius: 14px;
padding: 10px 12px;
}
.log-entry strong { color: var(--accent); }
.log-entry span { color: var(--muted); font-size: 0.82rem; }
.toast-wrap {
position: fixed;
right: 16px;
bottom: 16px;
z-index: 40;
display: grid;
gap: 10px;
width: min(380px, calc(100vw - 32px));
}
.toast {
border: 1px solid var(--border);
border-left: 4px solid var(--accent);
background: rgba(7,16,25,0.94);
color: var(--text);
border-radius: 16px;
padding: 12px 14px;
box-shadow: 0 18px 44px rgba(0,0,0,0.35);
backdrop-filter: blur(14px);
animation: toastIn 0.18s ease-out;
}
.toast.error { border-left-color: var(--danger); }
.toast.warn { border-left-color: var(--warn); }
@keyframes toastIn {
from { opacity: 0; transform: translateY(8px); }
to { opacity: 1; transform: translateY(0); }
}
@media (max-width: 860px) {
.card, .card.wide { grid-column: 1 / -1; }
.kv { grid-template-columns: 1fr; }
.value { text-align: left; }
.settings-grid { grid-template-columns: 1fr; }
header, main { padding: 16px; }
.top-nav { grid-template-columns: repeat(2, minmax(0, 1fr)); padding: 0 16px 14px; }
}
</style>
</head>
<body>
<div id="toastWrap" class="toast-wrap"></div>
<header>
<div class="title-row">
<div>
<h1>Yeti v1.7.4</h1>
<div class="subtitle">OLED face controller with the full memory/grudge checkpoint, cleaned-up mobile accordions, sass ticker, and scheduled sleep mode for the tiny battery goblin.</div>
</div>
<div class="pill">
<span id="dot" class="dot"></span>
<span id="status">Loading...</span>
</div>
</div>
</header>
<nav class="top-nav" aria-label="Yeti sections">
<button class="navbtn" data-target="dashboard" onclick="showPage('dashboard')">Dashboard</button>
<button class="navbtn" data-target="personality" onclick="showPage('personality')">Personality</button>
<button class="navbtn" data-target="memory" onclick="showPage('memory')">Memory</button>
<button class="navbtn" data-target="settings" onclick="showPage('settings')">Settings</button>
<button class="navbtn" data-target="system" onclick="showPage('system')">System</button>
</nav>
<main>
<div class="grid">
<section class="card summary-card" data-page="dashboard" data-accordion="off">
<h2>Dashboard Summary</h2>
<div id="dashboardSummary" class="summary-grid"></div>
</section>
<section class="card" data-page="dashboard" id="dashFace">
<h2>Face</h2>
<div class="big" id="activeExpression">--</div>
<div class="mini">Active expression</div>
<div class="mini" id="moodLine">--</div>
<div style="height:12px"></div>
<div id="faceInfo"></div>
</section>
<section class="card" data-page="dashboard">
<h2>Network</h2>
<div id="wifiInfo"></div>
</section>
<section class="card" data-page="dashboard">
<h2>Clock</h2>
<div class="big" id="clockTime">--:--</div>
<div class="mini" id="clockDate">Waiting for NTP</div>
<div style="height:12px"></div>
<div id="clockInfo"></div>
<div class="row">
<button class="secondary" onclick="action('show_clock')">Show on OLED</button>
<button class="secondary" onclick="action('sync_clock')">Sync NTP</button>
</div>
</section>
<section class="card" data-page="dashboard">
<h2>Sleep Mode</h2>
<div class="big" id="sleepStatus">--</div>
<div class="mini" id="sleepSummary">Waiting for clock</div>
<div style="height:12px"></div>
<div id="sleepInfo"></div>
<div class="row">
<button class="secondary" onclick="action('sleep_now')">Sleep Now</button>
<button class="secondary" onclick="action('sleep_preview')">Play Sleep Anim 5s</button>
<button class="secondary" onclick="action('wake_now')">Wake Now</button>
</div>
</section>
<section class="card" data-page="dashboard">
<h2>Weather</h2>
<div class="big" id="weatherTemp">--</div>
<div class="mini" id="weatherSummary">Set location to summon sky gossip.</div>
<div style="height:12px"></div>
<div id="weatherInfo"></div>
<div class="row">
<button class="secondary" onclick="action('weather_refresh')">Refresh Weather</button>
<button class="secondary" onclick="action('show_weather')">Show on OLED</button>
</div>
</section>
<section class="card" data-page="dashboard">
<h2>Sensors</h2>
<div id="sensorInfo"></div>
</section>
<section class="card wide" data-page="personality">
<h2>Personality / Mood</h2>
<div class="muted">Manual mood testing, base mood, presets, automation toggles, diagnostics, and the full v1.6 personality stack. YETI now has organized emotional damage. Progress?</div>
<div style="height:12px"></div>
<div id="moodInfo"></div>
<div class="settings-grid" style="margin-top:14px;">
<div class="panel">
<div class="panel-title">Mood Controls</div>
<label class="setting" style="display:block;">
Temporary duration, ms
<input type="number" id="moodDurationMs" min="0" max="60000" step="500" value="8000">
</label>
<div class="row" id="moodButtons"></div>
<div class="row">
<button class="secondary" onclick="moodRandom()">Random Mood</button>
<button class="secondary" onclick="moodIdleNow()">Idle Mood Now</button>
<button class="secondary" onclick="moodWeatherNow()">Weather Mood Now</button>
<button class="secondary" onclick="moodWifiNow()">Wi-Fi Mood Check</button>
<button class="secondary" onclick="moodMovementNow()">Movement Mood Now</button>
<button class="danger" onclick="moodPoke()">Poke YETI</button>
<button class="secondary" onclick="moodCalm()">Calm Down</button>
</div>
</div>
<div class="panel">
<div class="panel-title">Acting Sequences</div>
<div class="muted">Short non-blocking mood arcs. Manual moods and Calm Down interrupt them.</div>
<div class="row">
<button class="secondary" onclick="moodSequence('boot_drama')">Boot Drama</button>
<button class="secondary" onclick="moodSequence('bad_wifi_tantrum')">Bad Wi-Fi Tantrum</button>
<button class="secondary" onclick="moodSequence('settings_saved')">Settings Saved</button>
<button class="secondary" onclick="moodSequence('idle_boredom')">Idle Boredom</button>
<button class="danger" onclick="moodStopSequence()">Stop Sequence</button>
</div>
</div>
<div class="panel">
<div class="panel-title">Personality Preset</div>
<label class="setting" style="display:block;">
Preset
<select id="personalityPresetSelect"></select>
</label>
<div id="personalityPresetInfo" class="muted">Choose a preset to preview the trait values here.</div>
<div class="row">
<button onclick="applyPersonalityPresetFromUi()">Apply Preset</button>
<button class="secondary" onclick="saveConfig()">Save Current</button>
<button class="secondary" onclick="action('reset_personality')">Reset Defaults</button>
</div>
<div class="muted">Apply Preset updates base mood and sliders immediately, then saves them.</div>
</div>
<div class="panel">
<div class="panel-title">Base Mood</div>
<label class="setting" style="display:block;">
Base mood
<select id="baseMoodSelect"></select>
</label>
<label class="setting">
<input type="checkbox" id="idleMoodEnabled">
Enable random idle moods
</label>
<label class="setting">
<input type="checkbox" id="weatherMoodEnabled">
Enable weather mood reactions
</label>
<label class="setting">
<input type="checkbox" id="wifiMoodEnabled">
Enable Wi-Fi mood reactions
</label>
<label class="setting">
<input type="checkbox" id="webMoodEnabled">
Enable WebUI visit/command reactions
</label>
<label class="setting">
<input type="checkbox" id="movementMoodEnabled">
Enable movement mood reactions
</label>
<div class="row">
<button onclick="setBaseMoodFromUi()">Save Base Mood</button>
</div>
<div class="muted">Base mood is saved immediately. Sliders use Save Personality below.</div>
</div>
</div>
</section>
<section class="card full" data-page="personality">
<h2>Personality Sliders</h2>
<div class="settings-grid">
<div class="panel">
<div class="trait-line"><strong>Grumpiness</strong><span id="grumpinessValue">70</span></div>
<input type="range" id="grumpiness" min="0" max="100" step="1">
<div class="muted">Higher means more annoyed, smug, and angry idle choices.</div>
</div>
<div class="panel">
<div class="trait-line"><strong>Curiosity</strong><span id="curiosityValue">45</span></div>
<input type="range" id="curiosity" min="0" max="100" step="1">
<div class="muted">Higher means more curious glances and suspicious investigating.</div>
</div>
<div class="panel">
<div class="trait-line"><strong>Sleepiness</strong><span id="sleepinessValue">40</span></div>
<input type="range" id="sleepiness" min="0" max="100" step="1">
<div class="muted">Higher means more sleepy/tired idle moods.</div>
</div>
<div class="panel">
<div class="trait-line"><strong>Chaos</strong><span id="chaosValue">30</span></div>
<input type="range" id="chaos" min="0" max="100" step="1">
<div class="muted">Higher means shorter idle delays, startled twitches, smug nonsense, and more weird little mood swings.</div>
</div>
<div class="panel">
<div class="trait-line"><strong>Friendliness</strong><span id="friendlinessValue">25</span></div>
<input type="range" id="friendliness" min="0" max="100" step="1">
<div class="muted">Higher means happy beats annoyance more often.</div>
</div>
<div class="panel">
<div class="trait-line"><strong>Movement Sensitivity</strong><span id="movementSensitivityValue">50</span></div>
<input type="range" id="movementSensitivity" min="0" max="100" step="1">
<div class="muted">Higher reacts to smaller picked-up/tilted/moved nudges.</div>
</div>
<div class="panel">
<div class="trait-line"><strong>Shake Sensitivity</strong><span id="shakeSensitivityValue">50</span></div>
<input type="range" id="shakeSensitivity" min="0" max="100" step="1">
<div class="muted">Higher makes shake rage easier to trigger. Use carefully unless you want desk-earthquake drama.</div>
</div>
<div class="panel">
<div class="panel-title">Save</div>
<div class="muted">Persists base mood, sliders, automation toggles, and movement sensitivity to ESP32 Preferences.</div>
<div class="row">
<button onclick="saveConfig()">Save Personality</button>
<button class="secondary" onclick="reloadSettings()">Reload</button>
</div>
</div>
</div>
</section>

<section class="card summary-card" data-page="memory" data-accordion="off">
<h2>Memory Summary</h2>
<div id="memoryOpinion"></div>
<div class="sticky-actions">
<div class="row">
<button class="secondary" onclick="memoryAction('save')">Save Memory</button>
<button class="secondary" onclick="saveConfig()">Save Personality / Sass</button>
<button class="secondary" onclick="memoryAction('forgive')">Forgive Me</button>
<button class="secondary" onclick="memoryAction('praise')">Praise YETI</button>
</div>
</div>
</section>
<section class="card wide" data-page="memory" id="memoryJudgmentCard">
<h2>Opinion / Judgment</h2>
<div id="memoryJudgment"></div>
</section>
<section class="card wide" data-page="memory" id="memoryRelationshipCard">
<h2>Relationship + Memory Actions</h2>
<div id="memoryRelationship"></div>
<label class="setting">
<input type="checkbox" id="memoryEnabled">
Enable memory
</label>
<div class="sticky-actions">
<div class="row">
<button class="secondary" onclick="memoryAction('save')">Save Memory</button>
<button class="secondary" onclick="memoryAction('forgive')">Forgive Me</button>
<button class="secondary" onclick="memoryAction('praise')">Praise YETI</button>
<button class="danger" onclick="memoryAction('annoy')">Annoy YETI</button>
<button class="danger" onclick="memoryAction('reset')">Reset Memory</button>
</div>
</div>
</section>
<section class="card wide" data-page="memory" id="memoryGrudgesCard">
<h2>Grudge Ledger</h2>
<div id="memoryGrudges"></div>
<div class="row">
<button class="secondary" onclick="memoryAction('decay')">Decay Grudges</button>
</div>
</section>
<section class="card wide" data-page="memory" id="memoryNeedsCard">
<h2>Needs / Emotional Drift</h2>
<div id="memoryNeeds"></div>
<label class="setting">
<input type="checkbox" id="needsEnabled">
Enable needs drift
</label>
<div class="row">
<button class="secondary" onclick="memoryAction('attention')">Give Attention</button>
<button class="secondary" onclick="memoryAction('calm')">Calm Down</button>
<button class="secondary" onclick="memoryAction('bore')">Bore YETI</button>
<button class="secondary" onclick="memoryAction('wake')">Wake YETI Up</button>
</div>
</section>
<section class="card wide" data-page="memory" id="memorySassCard">
<h2>Sass / Phrases</h2>
<div id="memorySass"></div>
<label class="setting">
<input type="checkbox" id="sassTickerEnabled">
Enable sass ticker
</label>
<label class="setting">
<input type="checkbox" id="sassIdleOnly">
Idle-only sass
</label>
<label class="setting">
<input type="checkbox" id="sassEventEnabled">
Allow event sass
</label>
<label class="setting">
<input type="checkbox" id="sassGrudgeEnabled">
Allow grudge sass
</label>
<div class="trait-line"><strong>Sass Level</strong><span id="sassLevelValue">55</span></div>
<input type="range" id="sassLevel" min="0" max="100" step="1">
<div class="trait-line"><strong>Sass Frequency</strong><span id="sassFrequencyValue">35</span></div>
<input type="range" id="sassFrequency" min="0" max="100" step="1">
<div class="sticky-actions">
<div class="row">
<button class="secondary" onclick="sassAction('test')">Test Sass</button>
<button class="secondary" onclick="sassAction('judgment')">Say Judgment</button>
<button class="secondary" onclick="sassAction('grievance')">Say Grievance</button>
<button class="secondary" onclick="sassAction('random')">Random Phrase</button>
<button class="danger" onclick="sassAction('clear')">Clear Phrase</button>
</div>
</div>
<div class="muted">Uses OLED ticker overlays only when the display is safe. Save Memory or Save Personality persists these settings.</div>
</section>
<section class="card wide" data-page="memory" id="memoryDailyCard">
<h2>Daily Memory</h2>
<div id="memoryDaily"></div>
<div class="row">
<button class="secondary" onclick="memoryAction('rollover')">Rollover Check</button>
<button class="danger" onclick="memoryAction('clear-today')">Clear Today</button>
</div>
</section>
<section class="card full" data-page="memory" id="memoryStatsCard">
<h2>Lifetime Stats</h2>
<div class="settings-grid" id="memoryStats"></div>
</section>
<section class="card wide" data-page="memory" id="memoryDiagnosticsCard">
<h2>Memory Diagnostics</h2>
<div id="memoryDiagnostics"></div>
</section>
<section class="card" data-page="system" id="systemSummaryCard">
<h2>System Info</h2>
<div id="systemInfo"></div>
</section>
<section class="card wide" data-page="system" id="systemHardwareTests">
<h2>Hardware Test Buttons</h2>
<div class="muted">Trigger reactions can choose randomly from their configured expression lists.</div>
<div class="row">
<button id="triggerShakeButton" onclick="action('trigger_shake')">Test Shake Reaction</button>
<button id="triggerTouchButton" onclick="action('trigger_touch')">Test Touch Reaction</button>
<button id="calibrateTouchButton" class="secondary" onclick="action('calibrate_touch')">Recalibrate Touch</button>
<button class="secondary" onclick="loadStatus()">Refresh Status</button>
</div>
</section>
<section class="card wide" data-page="system" id="systemDiagnosticsCard">
<h2>Diagnostics</h2>
<div class="muted">Runtime stats and a manual I2C scanner for wiring sanity checks.</div>
<div id="perfInfo" style="margin-top:12px;"></div>
<div class="row">
<button class="secondary" onclick="scanI2c()">Scan I2C Bus</button>
<button class="secondary" onclick="action('show_ip')">Flash Network Info</button>
</div>
<div id="i2cInfo" style="margin-top:12px;"></div>
</section>
<section class="card" data-page="system" id="systemEventLogCard">
<h2>Event Log</h2>
<div id="eventLog" class="log"></div>
</section>
<section class="card" data-page="settings" id="settingsShakeCard">
<h2>Shake Trigger</h2>
<div class="muted">Configure the MPU6050 shake reaction and which expression can be selected when shaking is detected.</div>
<div class="panel" id="shakePanel">
<div class="muted" id="shakeHardwareNote"></div>
<label class="setting">
<input type="checkbox" id="shakeEnabled">
Enabled
</label>
<label class="setting" style="display:block;">
Hold duration, ms
<input type="number" id="shakeHoldMs" min="250" max="30000" step="50">
</label>
<div class="muted">Randomly choose from:</div>
<div id="shakeExpressions" class="expr-list"></div>
</div>
</section>
<section class="card" data-page="settings" id="settingsTouchCard">
<h2>Touch Trigger</h2>
<div class="muted">Configure the touch reaction and which expression can be selected when touch is detected.</div>
<div class="panel" id="touchPanel">
<div class="muted" id="touchHardwareNote"></div>
<label class="setting">
<input type="checkbox" id="touchEnabled">
Enabled
</label>
<label class="setting" style="display:block;">
Hold duration, ms
<input type="number" id="touchHoldMs" min="250" max="30000" step="50">
</label>
<div class="muted">Randomly choose from:</div>
<div id="touchExpressions" class="expr-list"></div>
</div>
</section>
<section class="card" data-page="settings" id="settingsIdleCard">
<h2>Idle Behavior</h2>
<div class="muted">Configure the older bored timeout behavior that still works alongside the newer mood/personality idle system.</div>
<div class="panel">
<label class="setting" style="display:block;">
Bored timeout, ms
<input type="number" id="boredTimeoutMs" min="0" max="3600000" step="1000">
</label>
<div class="muted">
Default is 180000 ms / 3 minutes. Set to 0 to disable boredom timeout. The face keeps the original rounded-eye Yeti style.
</div>
</div>
</section>
<section class="card" data-page="settings" id="settingsPerformanceCard">
<h2>Performance</h2>
<div class="muted">Tune display update pacing if the WebUI or OLED starts feeling sluggish.</div>
<div class="panel">
<label class="setting" style="display:block;">
Face frame interval, ms
<input type="number" id="faceFrameMs" min="40" max="250" step="1">
</label>
<div class="muted">
Higher = less lag and less I2C traffic. 83 ms is about 12 FPS; 66 ms is about 15 FPS; 40 ms is chaos goblin mode.
</div>
</div>
</section>
<section class="card" data-page="settings" id="settingsNetworkCard">
<h2>Network Identity</h2>
<div class="muted">Configure the hostname used for mDNS and DHCP identity.</div>
<div class="panel">
<label class="setting" style="display:block;">
Hostname
<input type="text" id="hostname" maxlength="31" autocomplete="off" placeholder="yeti">
</label>
<div class="muted">
Used for mDNS as <strong>http://hostname.local/</strong>. Letters, numbers, and hyphens only. A reboot/reconnect makes the DHCP name fully catch up.
</div>
</div>
</section>
<section class="card wide" data-page="settings" id="settingsWeatherCard">
<h2>Weather Location + Ticker</h2>
<div class="muted">Configure weather lookup, units, update interval, and what appears in the OLED weather ticker.</div>
<div class="panel">
<label class="setting">
<input type="checkbox" id="weatherEnabled">
Enable weather
</label>
<label class="setting" style="display:block;">
Location label
<input type="text" id="weatherName" maxlength="48" autocomplete="off" placeholder="Example: Anderson, CA">
</label>
<label class="setting" style="display:block;">
Latitude
<input type="number" id="weatherLat" min="-90" max="90" step="0.000001" placeholder="40.4482">
</label>
<label class="setting" style="display:block;">
Longitude
<input type="number" id="weatherLon" min="-180" max="180" step="0.000001" placeholder="-122.2978">
</label>
<label class="setting">
<input type="checkbox" id="weatherMetric">
Metric units
</label>
<label class="setting" style="display:block;">
Weather update interval, ms
<input type="number" id="weatherUpdateMs" min="300000" max="21600000" step="60000">
</label>
<div class="panel-title" style="margin-top:12px;">OLED Weather Ticker Includes</div>
<div class="expr-list">
<label class="setting"><input type="checkbox" id="weatherTickerLocation"> Location</label>
<label class="setting"><input type="checkbox" id="weatherTickerTemp"> Temperature</label>
<label class="setting"><input type="checkbox" id="weatherTickerCondition"> Condition</label>
<label class="setting"><input type="checkbox" id="weatherTickerFeelsLike"> Feels like</label>
<label class="setting"><input type="checkbox" id="weatherTickerHumidity"> Humidity</label>
<label class="setting"><input type="checkbox" id="weatherTickerWind"> Wind</label>
<label class="setting"><input type="checkbox" id="weatherTickerPrecip"> Precipitation</label>
<label class="setting"><input type="checkbox" id="weatherTickerUpdated"> Updated age</label>
</div>
<div class="row">
<button type="button" class="secondary" onclick="useBrowserLocation()">Use Browser Location</button>
<button type="button" class="secondary" onclick="findLocation()">Find Coordinates</button>
<button type="button" class="secondary" onclick="testWeather()">Test Weather</button>
</div>
<div id="locationResults" class="log" style="max-height:160px;margin-top:10px;"></div>
<div class="muted">
Uses Open-Meteo current weather and geocoding with no API key. Browser location may be blocked on plain HTTP; manual coordinates always work.
</div>
</div>
</section>
<section class="card wide" data-page="settings" id="settingsClockCard">
<h2>Clock + OLED Info Cards</h2>
<div class="muted">Configure NTP time and the automatic clock/weather OLED info cards.</div>
<div class="panel">
<label class="setting">
<input type="checkbox" id="clockEnabled">
Enable NTP clock
</label>
<label class="setting">
<input type="checkbox" id="clock24h">
Use 24-hour time
</label>
<label class="setting">
<input type="checkbox" id="clockUseWeatherOffset">
Use weather timezone offset when available
</label>
<label class="setting" style="display:block;">
Manual UTC offset, minutes
<input type="number" id="clockOffsetMinutes" min="-840" max="840" step="15">
</label>
<label class="setting">
<input type="checkbox" id="infoCardsEnabled">
Show clock/weather cards on OLED
</label>
<label class="setting">
<input type="checkbox" id="infoCardClockEnabled">
Include clock card
</label>
<label class="setting">
<input type="checkbox" id="infoCardWeatherEnabled">
Include weather card
</label>
<label class="setting" style="display:block;">
OLED info card interval, ms
<input type="number" id="infoCardIntervalMs" min="10000" max="3600000" step="5000">
</label>
<div class="muted">
The OLED cards briefly interrupt the face, then Yeti goes back to glaring. Interval default is 60000 ms / 1 minute.
</div>
</div>
</section>
<section class="card wide" data-page="settings" id="settingsSleepCard">
<h2>Sleep Schedule</h2>
<div class="muted">Configure the overnight battery-saver mode. During sleep, clock/weather/sass tickers pause; the OLED is mostly off/blank except for little sleepy eye + Zzz animation bursts.</div>
<div class="panel">
<label class="setting">
<input type="checkbox" id="sleepEnabled">
Enable scheduled sleep
</label>
<div class="settings-grid">
<label class="setting" style="display:block;">
Go to sleep
<input type="time" id="sleepStartTime" value="21:00">
</label>
<label class="setting" style="display:block;">
Wake up
<input type="time" id="sleepEndTime" value="06:00">
</label>
<label class="setting" style="display:block;">
Sleep animation duration, ms
<input type="number" id="sleepAnimMs" min="2000" max="120000" step="1000">
</label>
<label class="setting" style="display:block;">
Minimum blank/off gap, ms
<input type="number" id="sleepGapMinMs" min="5000" max="3600000" step="5000">
</label>
<label class="setting" style="display:block;">
Maximum blank/off gap, ms
<input type="number" id="sleepGapMaxMs" min="5000" max="3600000" step="5000">
</label>
<label class="setting" style="display:block;">
Wake override hold, ms
<input type="number" id="sleepWakeHoldMs" min="30000" max="21600000" step="30000">
</label>
</div>
<div class="row">
<button type="button" class="secondary" onclick="action('sleep_now')">Test Sleep Now</button>
<button type="button" class="secondary" onclick="action('sleep_preview')">Play Sleep Anim 5s</button>
<button type="button" class="secondary" onclick="action('wake_now')">Wake Now</button>
</div>
<div class="muted">Wake override hold keeps him awake briefly after a shake/WebUI wake, then the schedule can reclaim him if it is still sleepy-time. Default is 900000 ms / 15 minutes.</div>
</div>
</section>
<section class="card wide" data-page="settings" id="settingsSaveCard">
<h2>Save / Reload Settings</h2>
<div class="muted">Save all Settings, Personality, Memory automation, Sass, and ticker configuration values that live in the shared config form.</div>
<div class="row sticky-actions">
<button onclick="saveConfig()">Save Settings</button>
<button class="secondary" onclick="reloadSettings()">Reload Settings</button>
</div>
</section>
<section class="card full" data-page="system" id="systemMaintenanceCard">
<h2>Maintenance</h2>
<div class="muted">
Erase saved Wi-Fi credentials and reboot Yeti into first-run setup mode.
</div>
<form method="POST" action="/forget" onsubmit="return confirm('Forget saved Wi-Fi and reboot into setup mode?');">
<div class="row">
<button class="danger" type="submit">Forget Wi-Fi / Setup Reset</button>
<button class="secondary" type="button" onclick="rebootYeti()">Reboot YETI</button>
</div>
</form>
</section>
</div>
</main>
<script>
let expressions = [];
let moods = [];
let personalityPresets = [];
let settingsDirty = false;
let settingsBuilt = false;
let activePage = 'dashboard';

function accordionKey(card) {
return 'yeti.acc.' + (card.id || ((card.dataset.page || 'page') + '.' + Array.from(document.querySelectorAll('section.card')).indexOf(card)));
}
function setAccordionOpen(card, open) {
card.classList.toggle('open', !!open);
const head = card.querySelector('.accordion-head');
if (head) head.setAttribute('aria-expanded', open ? 'true' : 'false');
try { localStorage.setItem(accordionKey(card), open ? '1' : '0'); } catch (e) {}
}
function initAccordions() {
document.querySelectorAll('section.card').forEach((card, index) => {
if (card.dataset.accordion === 'off' || card.classList.contains('summary-card') || card.classList.contains('accordion-card')) return;
const title = Array.from(card.children).find(el => el.tagName === 'H2');
if (!title) return;
if (!card.id) card.id = 'acc-' + (card.dataset.page || 'page') + '-' + index;
const head = document.createElement('button');
head.type = 'button';
head.className = 'accordion-head';
head.innerHTML = '<span>' + esc(title.textContent || 'Section') + '</span><span class="accordion-chevron">▾</span>';
head.setAttribute('aria-expanded', 'false');
title.replaceWith(head);
const body = document.createElement('div');
body.className = 'accordion-body';
while (head.nextSibling) body.appendChild(head.nextSibling);
card.appendChild(body);
card.classList.add('accordion-card');
let saved = null;
try { saved = localStorage.getItem(accordionKey(card)); } catch (e) {}
setAccordionOpen(card, saved === '1');
head.addEventListener('click', () => setAccordionOpen(card, !card.classList.contains('open')));
});
}
function summaryTile(label, metric, detail = '') {
return `<div class="summary-tile"><div class="label">${esc(label)}</div><div class="metric">${esc(metric)}</div>${detail ? `<div class="mini">${esc(detail)}</div>` : ''}</div>`;
}
function showPage(page) {
activePage = page || 'dashboard';
document.querySelectorAll('section[data-page]').forEach(section => {
section.classList.toggle('active', section.dataset.page === activePage);
});
document.querySelectorAll('.navbtn').forEach(button => {
button.classList.toggle('active', button.dataset.target === activePage);
});
if (!settingsDirty) {
loadStatus();
}
}
function kv(k, v) {
return `
<div class="kv">
<div class="key">${k}</div>
<div class="value">${v}</div>
</div>
`;
}
function yesNo(v) {
return v ? 'Yes' : 'No';
}
function esc(s) {
return String(s ?? '').replace(/[&<>"']/g, m => ({
'&': '&amp;',
'<': '&lt;',
'>': '&gt;',
'"': '&quot;',
"'": '&#39;'
}[m]));
}
function ageText(ms) {
ms = Number(ms || 0);
if (ms < 1000) return ms + ' ms ago';
const sec = Math.floor(ms / 1000);
if (sec < 60) return sec + ' sec ago';
const min = Math.floor(sec / 60);
if (min < 60) return min + ' min ago';
return Math.floor(min / 60) + ' hr ago';
}
function fixed(value, digits = 1) {
const n = Number(value);
return Number.isFinite(n) ? n.toFixed(digits) : '--';
}
function setTraitValue(id, value) {
const input = document.getElementById(id);
const label = document.getElementById(id + 'Value');
if (input) input.value = value;
if (label) label.textContent = value;
}
function updateTraitLabels() {
['grumpiness', 'curiosity', 'sleepiness', 'chaos', 'friendliness', 'movementSensitivity', 'shakeSensitivity', 'sassLevel', 'sassFrequency'].forEach(id => {
const input = document.getElementById(id);
const label = document.getElementById(id + 'Value');
if (input && label) label.textContent = input.value;
});
}
function moodDuration() {
const value = Number(document.getElementById('moodDurationMs')?.value || 8000);
return Number.isFinite(value) ? Math.max(0, Math.min(60000, value)) : 8000;
}
function markDirty() {
settingsDirty = true;
}
function showToast(message, type = 'ok') {
const wrap = document.getElementById('toastWrap');
if (!wrap) return;
const toast = document.createElement('div');
toast.className = 'toast ' + type;
toast.textContent = message;
wrap.appendChild(toast);
setTimeout(() => {
toast.style.opacity = '0';
toast.style.transform = 'translateY(8px)';
toast.style.transition = 'opacity .18s ease, transform .18s ease';
setTimeout(() => toast.remove(), 220);
}, 3600);
}
function cleanHostname(value) {
return String(value || '')
.trim()
.toLowerCase()
.replace(/[_.\s]+/g, '-')
.replace(/[^a-z0-9-]/g, '')
.replace(/-+/g, '-')
.replace(/^-|-$/g, '') || 'yeti';
}
function reloadSettings() {
settingsDirty = false;
loadStatus({ toast: true });
}
function buildExpressionCheckboxes(containerId, prefix) {
const box = document.getElementById(containerId);
box.innerHTML = '';
expressions.forEach(expr => {
const label = document.createElement('label');
label.className = 'setting';
const input = document.createElement('input');
input.type = 'checkbox';
input.dataset.exprBit = expr.bit;
input.dataset.group = prefix;
input.onchange = markDirty;
label.appendChild(input);
label.appendChild(document.createTextNode(expr.name));
box.appendChild(label);
});
}
function buildMoodControls() {
const buttons = document.getElementById('moodButtons');
const select = document.getElementById('baseMoodSelect');
if (buttons) buttons.innerHTML = '';
if (select) select.innerHTML = '';
(moods || []).filter(m => !['weather', 'booting'].includes(m.value)).forEach(mood => {
if (buttons) {
const button = document.createElement('button');
button.type = 'button';
button.textContent = mood.name;
button.onclick = () => setMoodFromUi(mood.value);
buttons.appendChild(button);
}
if (select) {
const option = document.createElement('option');
option.value = mood.value;
option.textContent = mood.name;
select.appendChild(option);
}
});
}
function buildPersonalityPresetControls() {
const select = document.getElementById('personalityPresetSelect');
if (!select) return;
select.innerHTML = '';
(personalityPresets || []).forEach(preset => {
const option = document.createElement('option');
option.value = preset.id;
option.textContent = preset.name;
select.appendChild(option);
});
select.onchange = updatePersonalityPresetPreview;
updatePersonalityPresetPreview();
}
function selectedPersonalityPreset() {
const select = document.getElementById('personalityPresetSelect');
const id = select ? select.value : '';
return (personalityPresets || []).find(p => p.id === id) || null;
}
function updatePersonalityPresetPreview() {
const box = document.getElementById('personalityPresetInfo');
const preset = selectedPersonalityPreset();
if (!box || !preset) return;
const mood = (moods || []).find(m => m.value === preset.baseMood);
box.innerHTML = `${esc(preset.description || '')}<br>` +
`Base: ${esc(mood ? mood.name : preset.baseMood || '--')} · ` +
`G ${esc(preset.grumpiness)} / C ${esc(preset.curiosity)} / S ${esc(preset.sleepiness)} / Chaos ${esc(preset.chaos)} / F ${esc(preset.friendliness)}`;
}
function setExpressionMask(prefix, mask) {
document.querySelectorAll(`input[data-group="${prefix}"]`).forEach(input => {
const bit = Number(input.dataset.exprBit);
input.checked = (mask & bit) !== 0;
});
}
function getExpressionMask(prefix) {
let mask = 0;
document.querySelectorAll(`input[data-group="${prefix}"]`).forEach(input => {
if (input.checked) {
mask |= Number(input.dataset.exprBit);
}
});
return mask;
}
function setDisabled(id, disabled) {
const el = document.getElementById(id);
if (el) {
el.disabled = disabled;
}
}
function setGroupDisabled(prefix, disabled) {
document.querySelectorAll(`input[data-group="${prefix}"]`).forEach(input => {
input.disabled = disabled;
});
}
function applyHardwareUiState(data) {
const hw = data.hardware || {};
const touchHw = !!hw.touchEnabled;
const mpuHw = !!hw.mpuEnabled;
setDisabled('triggerTouchButton', !touchHw);
setDisabled('calibrateTouchButton', !touchHw);
setDisabled('touchEnabled', !touchHw);
setDisabled('touchHoldMs', !touchHw);
setGroupDisabled('touch', !touchHw);
setDisabled('triggerShakeButton', !mpuHw);
setDisabled('shakeEnabled', !mpuHw);
setDisabled('shakeHoldMs', !mpuHw);
setGroupDisabled('shake', !mpuHw);
setDisabled('movementMoodEnabled', !mpuHw);
setDisabled('movementSensitivity', !mpuHw);
setDisabled('shakeSensitivity', !mpuHw);
const touchNote = document.getElementById('touchHardwareNote');
if (touchNote) {
touchNote.textContent = touchHw ? `Hardware: ${hw.touchPin}` : `Hardware disabled: ${hw.touchPin || 'not available on this board profile'}`;
}
const shakeNote = document.getElementById('shakeHardwareNote');
if (shakeNote) {
shakeNote.textContent = mpuHw ? `Hardware: MPU at ${hw.mpuAddress} on SDA ${hw.i2cSda} / SCL ${hw.i2cScl}` : 'Hardware disabled in this board profile';
}
}
function applyConfigToForm(data) {
if (!settingsBuilt) {
expressions = data.config.expressions || [];
moods = data.config.moods || [];
personalityPresets = data.config.personalityPresets || [];
buildExpressionCheckboxes('shakeExpressions', 'shake');
buildExpressionCheckboxes('touchExpressions', 'touch');
buildMoodControls();
buildPersonalityPresetControls();
['shakeEnabled', 'touchEnabled', 'shakeHoldMs', 'touchHoldMs', 'boredTimeoutMs', 'faceFrameMs', 'hostname', 'weatherEnabled', 'weatherName', 'weatherLat', 'weatherLon', 'weatherMetric', 'weatherUpdateMs', 'weatherTickerLocation', 'weatherTickerTemp', 'weatherTickerCondition', 'weatherTickerFeelsLike', 'weatherTickerHumidity', 'weatherTickerWind', 'weatherTickerPrecip', 'weatherTickerUpdated', 'clockEnabled', 'clock24h', 'clockUseWeatherOffset', 'clockOffsetMinutes', 'infoCardsEnabled', 'infoCardClockEnabled', 'infoCardWeatherEnabled', 'infoCardIntervalMs', 'sleepEnabled', 'sleepStartTime', 'sleepEndTime', 'sleepAnimMs', 'sleepGapMinMs', 'sleepGapMaxMs', 'sleepWakeHoldMs', 'idleMoodEnabled', 'weatherMoodEnabled', 'wifiMoodEnabled', 'webMoodEnabled', 'movementMoodEnabled', 'memoryEnabled', 'needsEnabled', 'sassTickerEnabled', 'sassIdleOnly', 'sassEventEnabled', 'sassGrudgeEnabled', 'sassLevel', 'sassFrequency', 'movementSensitivity', 'shakeSensitivity', 'baseMoodSelect', 'moodDurationMs', 'grumpiness', 'curiosity', 'sleepiness', 'chaos', 'friendliness'].forEach(id => {
const el = document.getElementById(id);
if (el) {
el.onchange = markDirty;
el.oninput = () => { updateTraitLabels(); markDirty(); };
}
});
settingsBuilt = true;
}
applyHardwareUiState(data);
if (settingsDirty) {
return;
}
document.getElementById('shakeEnabled').checked = data.config.shake.enabled;
document.getElementById('shakeHoldMs').value = data.config.shake.holdMs;
setExpressionMask('shake', data.config.shake.expressionMask);
document.getElementById('touchEnabled').checked = data.config.touch.enabled;
document.getElementById('touchHoldMs').value = data.config.touch.holdMs;
setExpressionMask('touch', data.config.touch.expressionMask);
document.getElementById('boredTimeoutMs').value = data.config.bored ? data.config.bored.timeoutMs : 180000;
if (data.config.performance) {
const frameInput = document.getElementById('faceFrameMs');
frameInput.min = data.config.performance.faceFrameMinMs || 40;
frameInput.max = data.config.performance.faceFrameMaxMs || 250;
frameInput.value = data.config.performance.faceFrameMs || 83;
}
const hostInput = document.getElementById('hostname');
if (hostInput) {
hostInput.maxLength = (data.config.network && data.config.network.hostnameMaxLen) || 31;
hostInput.value = (data.config.network && data.config.network.hostname) || data.app.hostname || 'yeti';
}
const weatherCfg = data.config.weather || {};
document.getElementById('weatherEnabled').checked = !!weatherCfg.enabled;
document.getElementById('weatherName').value = weatherCfg.locationName || '';
document.getElementById('weatherLat').value = Number(weatherCfg.latitude || 0).toFixed(6);
document.getElementById('weatherLon').value = Number(weatherCfg.longitude || 0).toFixed(6);
document.getElementById('weatherMetric').checked = !!weatherCfg.metric;
const weatherMs = document.getElementById('weatherUpdateMs');
weatherMs.min = weatherCfg.updateMinMs || 300000;
weatherMs.max = weatherCfg.updateMaxMs || 21600000;
weatherMs.value = weatherCfg.updateMs || 900000;
document.getElementById('weatherTickerLocation').checked = weatherCfg.tickerLocation !== false;
document.getElementById('weatherTickerTemp').checked = weatherCfg.tickerTemp !== false;
document.getElementById('weatherTickerCondition').checked = weatherCfg.tickerCondition !== false;
document.getElementById('weatherTickerFeelsLike').checked = weatherCfg.tickerFeelsLike !== false;
document.getElementById('weatherTickerHumidity').checked = weatherCfg.tickerHumidity !== false;
document.getElementById('weatherTickerWind').checked = weatherCfg.tickerWind !== false;
document.getElementById('weatherTickerPrecip').checked = weatherCfg.tickerPrecip !== false;
document.getElementById('weatherTickerUpdated').checked = weatherCfg.tickerUpdated !== false;
const clockCfg = data.config.clock || {};
document.getElementById('clockEnabled').checked = !!clockCfg.enabled;
document.getElementById('clock24h').checked = !!clockCfg.format24h;
document.getElementById('clockUseWeatherOffset').checked = !!clockCfg.useWeatherOffset;
document.getElementById('clockOffsetMinutes').value = clockCfg.utcOffsetMinutes ?? -420;
document.getElementById('infoCardsEnabled').checked = !!clockCfg.infoCardsEnabled;
document.getElementById('infoCardClockEnabled').checked = !!clockCfg.infoCardClockEnabled;
document.getElementById('infoCardWeatherEnabled').checked = !!clockCfg.infoCardWeatherEnabled;
const infoMs = document.getElementById('infoCardIntervalMs');
infoMs.min = clockCfg.infoCardMinMs || 10000;
infoMs.max = clockCfg.infoCardMaxMs || 3600000;
infoMs.value = clockCfg.infoCardIntervalMs || 60000;
const sleepCfg = data.config.sleep || {};
document.getElementById('sleepEnabled').checked = sleepCfg.enabled !== false;
document.getElementById('sleepStartTime').value = sleepCfg.startTime || '21:00';
document.getElementById('sleepEndTime').value = sleepCfg.endTime || '06:00';
const sleepAnim = document.getElementById('sleepAnimMs');
sleepAnim.min = sleepCfg.animMinMs || 2000;
sleepAnim.max = sleepCfg.animMaxMs || 120000;
sleepAnim.value = sleepCfg.animationDurationMs || 12000;
const sleepGapMin = document.getElementById('sleepGapMinMs');
sleepGapMin.min = sleepCfg.gapMinAllowedMs || 5000;
sleepGapMin.max = sleepCfg.gapMaxAllowedMs || 3600000;
sleepGapMin.value = sleepCfg.gapMinMs || 45000;
const sleepGapMax = document.getElementById('sleepGapMaxMs');
sleepGapMax.min = sleepCfg.gapMinAllowedMs || 5000;
sleepGapMax.max = sleepCfg.gapMaxAllowedMs || 3600000;
sleepGapMax.value = sleepCfg.gapMaxMs || 180000;
const sleepHold = document.getElementById('sleepWakeHoldMs');
sleepHold.min = sleepCfg.wakeHoldMinMs || 30000;
sleepHold.max = sleepCfg.wakeHoldMaxMs || 21600000;
sleepHold.value = sleepCfg.wakeHoldMs || 900000;
const personality = data.config.personality || {};
const baseMoodSelect = document.getElementById('baseMoodSelect');
if (baseMoodSelect) baseMoodSelect.value = personality.baseMood || (data.mood && data.mood.baseMood) || 'deadpan';
const presetSelect = document.getElementById('personalityPresetSelect');
if (presetSelect && personality.activePreset && personality.activePreset !== 'custom') {
  presetSelect.value = personality.activePreset;
  updatePersonalityPresetPreview();
}
const idleToggle = document.getElementById('idleMoodEnabled');
if (idleToggle) idleToggle.checked = personality.idleMoodEnabled !== false;
const weatherMoodToggle = document.getElementById('weatherMoodEnabled');
if (weatherMoodToggle) weatherMoodToggle.checked = personality.weatherMoodEnabled !== false;
const wifiMoodToggle = document.getElementById('wifiMoodEnabled');
if (wifiMoodToggle) wifiMoodToggle.checked = personality.wifiMoodEnabled !== false;
const webMoodToggle = document.getElementById('webMoodEnabled');
if (webMoodToggle) webMoodToggle.checked = personality.webMoodEnabled !== false;
const movementMoodToggle = document.getElementById('movementMoodEnabled');
if (movementMoodToggle) movementMoodToggle.checked = personality.movementMoodEnabled !== false;
const memoryToggle = document.getElementById('memoryEnabled');
if (memoryToggle) memoryToggle.checked = personality.memoryEnabled !== false;
const needsToggle = document.getElementById('needsEnabled');
if (needsToggle) needsToggle.checked = personality.needsEnabled !== false;
const sassToggle = document.getElementById('sassTickerEnabled');
if (sassToggle) sassToggle.checked = personality.sassEnabled !== false;
const sassIdle = document.getElementById('sassIdleOnly');
if (sassIdle) sassIdle.checked = !!personality.sassIdleOnly;
const sassEvent = document.getElementById('sassEventEnabled');
if (sassEvent) sassEvent.checked = personality.sassEventEnabled !== false;
const sassGrudge = document.getElementById('sassGrudgeEnabled');
if (sassGrudge) sassGrudge.checked = personality.sassGrudgeEnabled !== false;
setTraitValue('sassLevel', personality.sassLevel ?? 55);
setTraitValue('sassFrequency', personality.sassFrequency ?? 35);
setTraitValue('movementSensitivity', personality.movementSensitivity ?? 50);
setTraitValue('shakeSensitivity', personality.shakeSensitivity ?? 50);
setTraitValue('grumpiness', personality.grumpiness ?? 70);
setTraitValue('curiosity', personality.curiosity ?? 45);
setTraitValue('sleepiness', personality.sleepiness ?? 40);
setTraitValue('chaos', personality.chaos ?? 30);
setTraitValue('friendliness', personality.friendliness ?? 25);
updateTraitLabels();
}
function renderEvents(events) {
const box = document.getElementById('eventLog');
if (!events || !events.length) {
box.innerHTML = '<div class="muted">No events yet. Yeti is being suspiciously quiet.</div>';
return;
}
box.innerHTML = events.map(e => `
<div class="log-entry">
<strong>${esc(e.type)}</strong> ${esc(e.message)}<br>
<span>${ageText(e.ageMs)}</span>
</div>
`).join('');
}
function useBrowserLocation() {
if (!navigator.geolocation) {
showToast('Browser geolocation is not available.', 'error');
return;
}
showToast('Asking browser for location...', 'ok');
navigator.geolocation.getCurrentPosition(pos => {
document.getElementById('weatherLat').value = pos.coords.latitude.toFixed(6);
document.getElementById('weatherLon').value = pos.coords.longitude.toFixed(6);
if (!document.getElementById('weatherName').value.trim()) {
document.getElementById('weatherName').value = 'Browser location';
}
document.getElementById('weatherEnabled').checked = true;
markDirty();
showToast('Location filled. Save settings to use it.', 'ok');
}, err => {
console.error(err);
showToast('Browser blocked location. Manual coordinates still work.', 'warn');
}, { enableHighAccuracy: false, timeout: 10000, maximumAge: 600000 });
}
async function findLocation() {
const query = document.getElementById('weatherName').value.trim();
const box = document.getElementById('locationResults');
if (!query) {
showToast('Enter a location name first.', 'warn');
return;
}
box.innerHTML = '<div class="muted">Searching locations...</div>';
showToast('Searching Open-Meteo geocoding...', 'ok');
try {
const url = 'https://geocoding-api.open-meteo.com/v1/search?name=' + encodeURIComponent(query) + '&count=5&language=en&format=json';
const res = await fetch(url);
const data = await res.json();
const results = data.results || [];
if (!results.length) {
box.innerHTML = '<div class="muted">No matches found. Try a city/state/country combo.</div>';
showToast('No location matches found.', 'warn');
return;
}
box.innerHTML = '';
results.forEach(result => {
const item = document.createElement('div');
item.className = 'log-entry';
const label = [result.name, result.admin1, result.country].filter(Boolean).join(', ');
const coords = `${Number(result.latitude).toFixed(5)}, ${Number(result.longitude).toFixed(5)}`;
const button = document.createElement('button');
button.type = 'button';
button.className = 'secondary';
button.style.width = '100%';
button.textContent = `${label} · ${coords}`;
button.onclick = () => {
document.getElementById('weatherName').value = label;
document.getElementById('weatherLat').value = Number(result.latitude).toFixed(6);
document.getElementById('weatherLon').value = Number(result.longitude).toFixed(6);
document.getElementById('weatherEnabled').checked = true;
markDirty();
showToast('Coordinates filled. Save settings to use them.', 'ok');
};
item.appendChild(button);
box.appendChild(item);
});
} catch (e) {
console.error(e);
box.innerHTML = '<div class="muted">Location lookup failed. Manual coordinates still work.</div>';
showToast('Location lookup failed.', 'error');
}
}
function testWeather() {
if (settingsDirty) {
showToast('Save settings first, then test weather.', 'warn');
return;
}
action('weather_refresh');
}
async function postMoodUrl(url, params = new URLSearchParams()) {
try {
const res = await fetch(url, {
method: 'POST',
headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
body: params.toString()
});
const payload = await res.json();
if (!res.ok || payload.ok === false) {
throw new Error(payload.error || 'Mood command failed');
}
await loadStatus();
showToast('Mood updated.', 'ok');
} catch (e) {
console.error(e);
showToast('Mood command failed: ' + (e.message || 'unknown'), 'error');
}
}
function setMoodFromUi(mood) {
const params = new URLSearchParams();
params.set('mood', mood);
params.set('durationMs', moodDuration());
postMoodUrl('/api/mood', params);
}
function setBaseMoodFromUi() {
const params = new URLSearchParams();
params.set('mood', document.getElementById('baseMoodSelect')?.value || 'deadpan');
postMoodUrl('/api/mood/base', params);
}
function moodRandom() {
const params = new URLSearchParams();
params.set('durationMs', moodDuration());
postMoodUrl('/api/mood/random', params);
}
function moodIdleNow() {
postMoodUrl('/api/mood/idle-now');
}
function moodWeatherNow() {
postMoodUrl('/api/mood/weather-now');
}
function moodWifiNow() {
postMoodUrl('/api/mood/wifi-now');
}
function moodMovementNow() {
postMoodUrl('/api/mood/movement-now');
}
function moodSequence(sequence) {
const params = new URLSearchParams();
params.set('sequence', sequence);
postMoodUrl('/api/sequence', params);
}
function moodStopSequence() {
postMoodUrl('/api/sequence/stop');
}
async function applyPersonalityPresetFromUi() {
const preset = selectedPersonalityPreset();
if (!preset) {
showToast('Choose a personality preset first.', 'warn');
return;
}
const params = new URLSearchParams();
params.set('preset', preset.id);
try {
const res = await fetch('/api/personality/preset', {
method: 'POST',
headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
body: params.toString()
});
const payload = await res.json();
if (!res.ok || !payload.ok) {
throw new Error(payload.error || 'Preset failed');
}
settingsDirty = false;
await loadStatus();
showToast('Preset applied: ' + preset.name, 'ok');
} catch (e) {
console.error(e);
showToast('Preset failed: ' + (e.message || preset.name), 'error');
}
}
function moodPoke() {
postMoodUrl('/api/mood/poke');
}
function moodCalm() {
postMoodUrl('/api/mood/calm');
}
function renderMemory(memory = {}) {
const rel = memory.relationship || {};
const stats = memory.stats || {};
const opinion = document.getElementById('memoryOpinion');
const judgmentBox = document.getElementById('memoryJudgment');
const relationshipBox = document.getElementById('memoryRelationship');
const diag = document.getElementById('memoryDiagnostics');
const grudgeBox = document.getElementById('memoryGrudges');
const needsBox = document.getElementById('memoryNeeds');
const dailyBox = document.getElementById('memoryDaily');
const sassBox = document.getElementById('memorySass');
const statBox = document.getElementById('memoryStats');
if (opinion) {
opinion.innerHTML =
kv('Opinion summary', esc(memory.opinionSummary || memory.judgment || '--')) +
kv('Current grievance', esc(memory.currentGrievance || memory.recentGrievance || '--')) +
kv('Forgiveness', esc(memory.forgivenessStatus || '--')) +
kv('Report card', esc(memory.yetiReportCard || '--')) +
kv('Flavor text', esc(memory.memoryFlavorText || '--'));
}
if (judgmentBox) {
judgmentBox.innerHTML =
kv('Judgment label', esc(memory.judgmentLabel || memory.judgment || '--')) +
kv('Tolerance score', (memory.toleranceScore ?? 0) + '%') +
kv('Forgiveness score', (memory.forgivenessScore ?? 0) + '%') +
kv('Mood bias', esc(memory.moodBiasSummary || memory.memoryBiasSummary || '--')) +
kv('Trust status', esc(memory.trustStatus || '--')) +
kv('Relationship tone', esc(memory.relationshipTone || '--')) +
kv('Daily opinion', esc(memory.dailyOpinionSummary || '--'));
}
if (relationshipBox) {
relationshipBox.innerHTML =
kv('Relationship summary', esc(memory.relationshipSummary || '--')) +
kv('Affection', rel.affection ?? '--') +
kv('Annoyance', rel.annoyance ?? '--') +
kv('Trust', rel.trust ?? '--') +
kv('Suspicion', rel.suspicion ?? '--');
}
if (grudgeBox) {
const grudges = memory.grudges || {};
grudgeBox.innerHTML =
kv('Holding grudge', yesNo(memory.holdingGrudge)) +
kv('Total grudge', memory.totalGrudge ?? 0) +
kv('Worst grudge', esc(memory.worstGrudge || '--')) +
kv('Poke', grudges.poke ?? 0) +
kv('Shake', grudges.shake ?? 0) +
kv('Wi-Fi', grudges.wifi ?? 0) +
kv('Weather', grudges.weather ?? 0) +
kv('Reboot', grudges.reboot ?? 0) +
kv('Neglect', grudges.neglect ?? 0);
}
if (needsBox) {
const needs = memory.needs || {};
needsBox.innerHTML =
kv('Needs drift enabled', yesNo(memory.needsEnabled !== false)) +
kv('Summary', esc(memory.needsSummary || '--')) +
kv('Bias', esc(memory.needsBiasSummary || '--')) +
kv('Boredom', needs.boredom ?? 0) +
kv('Energy', needs.energy ?? 0) +
kv('Irritation', needs.irritation ?? 0) +
kv('Loneliness', needs.loneliness ?? 0);
const needsToggle = document.getElementById('needsEnabled');
if (needsToggle) needsToggle.checked = memory.needsEnabled !== false;
}
if (sassBox) {
sassBox.innerHTML =
kv('Enabled', yesNo(memory.sassEnabled !== false)) +
kv('Idle only', yesNo(memory.sassIdleOnly)) +
kv('Event sass', yesNo(memory.sassEventEnabled !== false)) +
kv('Grudge sass', yesNo(memory.sassGrudgeEnabled !== false)) +
kv('Level', memory.sassLevel ?? 55) +
kv('Frequency', memory.sassFrequency ?? 35) +
kv('Cooldown', (memory.sassCooldownMs || 0) + ' ms') +
kv('Last phrase', esc(memory.lastSassPhrase || '--')) +
kv('Last category', esc(memory.lastSassCategory || '--')) +
kv('Last sass note', esc(memory.lastSassNote || '--')) +
kv('Phrase count', memory.sassPhraseCount ?? 0) +
kv('Suppressed', memory.sassSuppressedCount ?? 0) +
kv('Last sass age', ageText(memory.lastSassAgeMs || 0));
const sassEnabled = document.getElementById('sassTickerEnabled');
if (sassEnabled) sassEnabled.checked = memory.sassEnabled !== false;
const sassIdle = document.getElementById('sassIdleOnly');
if (sassIdle) sassIdle.checked = !!memory.sassIdleOnly;
const sassEvent = document.getElementById('sassEventEnabled');
if (sassEvent) sassEvent.checked = memory.sassEventEnabled !== false;
const sassGrudge = document.getElementById('sassGrudgeEnabled');
if (sassGrudge) sassGrudge.checked = memory.sassGrudgeEnabled !== false;
setTraitValue('sassLevel', memory.sassLevel ?? 55);
setTraitValue('sassFrequency', memory.sassFrequency ?? 35);
}
if (dailyBox) {
const daily = memory.daily || {};
const today = daily.today || {};
const yesterday = daily.yesterday || {};
dailyBox.innerHTML =
kv('Memory date', esc(memory.dailyDate || '--')) +
kv('Current date', esc(memory.currentDateKey || 'waiting for clock')) +
kv('Today summary', esc(memory.todaySummary || '--')) +
kv('Yesterday summary', esc(memory.yesterdaySummary || '--')) +
kv('Today pokes', today.pokes ?? 0) +
kv('Today shakes', today.shakes ?? 0) +
kv('Today bad Wi-Fi', today.badWifi ?? 0) +
kv('Today weather fails', today.weatherFails ?? 0) +
kv('Today web visits', today.webVisits ?? 0) +
kv('Today commands', today.commands ?? 0) +
kv('Today angry moods', today.angryMoodCount ?? 0) +
kv('Today happy moods', today.happyMoodCount ?? 0) +
kv('Yesterday pokes', yesterday.pokes ?? 0) +
kv('Yesterday shakes', yesterday.shakes ?? 0) +
kv('Yesterday bad Wi-Fi', yesterday.badWifi ?? 0) +
kv('Yesterday weather fails', yesterday.weatherFails ?? 0) +
kv('Yesterday angry moods', yesterday.angryMoodCount ?? 0) +
kv('Yesterday happy moods', yesterday.happyMoodCount ?? 0);
}
if (diag) {
diag.innerHTML =
kv('Enabled', yesNo(memory.enabled !== false)) +
kv('Dirty', yesNo(memory.dirty)) +
kv('Last memory event', esc(memory.lastMemoryEvent || '--')) +
kv('Memory note', esc(memory.lastMemoryNote || '--')) +
kv('Mood bias', esc(memory.moodBiasSummary || memory.memoryBiasSummary || '--')) +
kv('Flavor text', esc(memory.memoryFlavorText || '--')) +
kv('Grudge note', esc(memory.lastGrudgeNote || '--')) +
kv('Runtime events', memory.memoryEventCount ?? 0) +
kv('Grudge events', memory.grudgeEventCount ?? 0) +
kv('Needs events', memory.needsEventCount ?? 0) +
kv('Needs note', esc(memory.lastNeedsNote || '--')) +
kv('Daily note', esc(memory.lastDailyNote || '--')) +
kv('Daily rollovers', memory.dailyRolloverCount ?? 0) +
kv('Last needs drift', ageText(memory.lastNeedsUpdateAgeMs || 0)) +
kv('Last needs mood', ageText(memory.lastNeedsMoodAgeMs || 0)) +
kv('Needs update interval', (memory.needsUpdateIntervalMs || 0) + ' ms') +
kv('Needs mood cooldown', (memory.needsMoodCooldownMs || 0) + ' ms') +
kv('Last decay', ageText(memory.lastGrudgeDecayAgeMs || 0)) +
kv('Decay interval', (memory.grudgeDecayIntervalMs || 0) + ' ms') +
kv('Last save', ageText(memory.lastSaveAgeMs || 0)) +
kv('Next auto-save', memory.dirty ? ((memory.nextSaveInMs || 0) + ' ms') : '--') +
kv('Save interval', (memory.saveIntervalMs || 0) + ' ms');
}
if (statBox) {
const entries = [
['Boots', stats.lifetimeBoots],
['Pokes', stats.lifetimePokes],
['Shakes', stats.lifetimeShakes],
['Tilts / pickups', stats.lifetimeTilts],
['Bad Wi-Fi events', stats.lifetimeBadWifi],
['Weather failures', stats.lifetimeWeatherFails],
['WebUI visits', stats.lifetimeWebVisits],
['Commands', stats.lifetimeCommands],
['Reboots', stats.lifetimeReboots],
['Settings saves', stats.lifetimeSettingsSaves],
['Mood changes', stats.lifetimeMoodChanges],
['Sequences', stats.lifetimeSequences]
];
statBox.innerHTML = entries.map(([label, value]) => `<div class="panel"><div class="panel-title">${esc(label)}</div><div class="big">${esc(value ?? 0)}</div></div>`).join('');
}
}
async function memoryAction(name) {
if (name === 'reset' && !confirm('Reset YETI memory and grudges? This wipes lifetime stats.')) {
return;
}
if (name === 'clear-today' && !confirm("Clear today's daily memory counters? Lifetime stats stay intact.")) {
return;
}
const params = new URLSearchParams();
if (name === 'save') {
params.set('memoryEnabled', document.getElementById('memoryEnabled')?.checked ? '1' : '0');
params.set('needsEnabled', document.getElementById('needsEnabled')?.checked ? '1' : '0');
params.set('sassTickerEnabled', document.getElementById('sassTickerEnabled')?.checked ? '1' : '0');
params.set('sassIdleOnly', document.getElementById('sassIdleOnly')?.checked ? '1' : '0');
params.set('sassEventEnabled', document.getElementById('sassEventEnabled')?.checked ? '1' : '0');
params.set('sassGrudgeEnabled', document.getElementById('sassGrudgeEnabled')?.checked ? '1' : '0');
params.set('sassLevel', document.getElementById('sassLevel')?.value || '55');
params.set('sassFrequency', document.getElementById('sassFrequency')?.value || '35');
}
try {
const res = await fetch('/api/memory/' + encodeURIComponent(name), {
method: 'POST',
headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
body: params.toString()
});
const payload = await res.json();
if (!res.ok || payload.ok === false) {
throw new Error(payload.error || 'Memory command failed');
}
settingsDirty = false;
await loadStatus();
showToast('Memory updated.', 'ok');
} catch (e) {
console.error(e);
showToast('Memory command failed: ' + (e.message || name), 'error');
}
}
async function sassAction(name) {
try {
const params = new URLSearchParams();
params.set('sassTickerEnabled', document.getElementById('sassTickerEnabled')?.checked ? '1' : '0');
params.set('sassIdleOnly', document.getElementById('sassIdleOnly')?.checked ? '1' : '0');
params.set('sassEventEnabled', document.getElementById('sassEventEnabled')?.checked ? '1' : '0');
params.set('sassGrudgeEnabled', document.getElementById('sassGrudgeEnabled')?.checked ? '1' : '0');
params.set('sassLevel', document.getElementById('sassLevel')?.value || '55');
params.set('sassFrequency', document.getElementById('sassFrequency')?.value || '35');
const res = await fetch('/api/sass/' + encodeURIComponent(name), {
method: 'POST',
headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
body: params.toString()
});
const payload = await res.json();
if (!res.ok || payload.ok === false) {
throw new Error(payload.error || 'Sass command failed');
}
settingsDirty = false;
await loadStatus();
showToast('Sass command sent.', 'ok');
} catch (e) {
console.error(e);
showToast('Sass command failed: ' + (e.message || name), 'error');
}
}
async function scanI2c() {
const box = document.getElementById('i2cInfo');
box.innerHTML = '<div class="muted">Scanning I2C bus...</div>';
showToast('Scanning I2C bus...', 'ok');
try {
const res = await fetch('/api/i2c');
const devices = await res.json();
if (!devices.length) {
box.innerHTML = '<div class="muted">No I2C devices found. That usually means wiring, power, or wrong SDA/SCL pins. Classic gremlin behavior.</div>';
} else {
box.innerHTML = devices.map(d => kv(d.address, `${esc(d.likely)} · decimal ${esc(d.decimal)}`)).join('');
}
await loadStatus();
} catch (e) {
console.error(e);
box.innerHTML = '<div class="muted">I2C scan failed.</div>';
showToast('I2C scan failed.', 'error');
}
}
async function rebootYeti() {
if (!confirm('Reboot YETI now?')) {
return;
}
showToast('Reboot command sent. YETI will disappear for a moment.', 'warn');
try {
await fetch('/api/action?action=reboot', { method: 'POST' });
} catch (e) {
// The ESP32 may reset before the browser finishes reading the reply.
console.log('Reboot request likely interrupted by reset', e);
}
}
async function action(name) {
const buttons = document.querySelectorAll('button');
buttons.forEach(b => b.disabled = true);
try {
const res = await fetch('/api/action?action=' + encodeURIComponent(name), {
method: 'POST'
});
const payload = await res.json();
if (!res.ok || !payload.ok) {
throw new Error(payload.error || 'Action failed');
}
} catch (e) {
console.error(e);
showToast('Command failed: ' + (e.message || name), 'error');
}
buttons.forEach(b => b.disabled = false);
await loadStatus();
}
async function saveConfig() {
const params = new URLSearchParams();
params.set('shakeEnabled', document.getElementById('shakeEnabled').checked ? '1' : '0');
params.set('shakeHoldMs', document.getElementById('shakeHoldMs').value);
params.set('shakeMask', getExpressionMask('shake'));
params.set('touchEnabled', document.getElementById('touchEnabled').checked ? '1' : '0');
params.set('touchHoldMs', document.getElementById('touchHoldMs').value);
params.set('touchMask', getExpressionMask('touch'));
params.set('boredTimeoutMs', document.getElementById('boredTimeoutMs').value);
params.set('faceFrameMs', document.getElementById('faceFrameMs').value);
params.set('baseMood', document.getElementById('baseMoodSelect')?.value || 'deadpan');
params.set('idleMoodEnabled', document.getElementById('idleMoodEnabled')?.checked ? '1' : '0');
params.set('weatherMoodEnabled', document.getElementById('weatherMoodEnabled')?.checked ? '1' : '0');
params.set('wifiMoodEnabled', document.getElementById('wifiMoodEnabled')?.checked ? '1' : '0');
params.set('webMoodEnabled', document.getElementById('webMoodEnabled')?.checked ? '1' : '0');
params.set('movementMoodEnabled', document.getElementById('movementMoodEnabled')?.checked ? '1' : '0');
params.set('memoryEnabled', document.getElementById('memoryEnabled')?.checked ? '1' : '0');
params.set('needsEnabled', document.getElementById('needsEnabled')?.checked ? '1' : '0');
params.set('sassTickerEnabled', document.getElementById('sassTickerEnabled')?.checked ? '1' : '0');
params.set('sassIdleOnly', document.getElementById('sassIdleOnly')?.checked ? '1' : '0');
params.set('sassEventEnabled', document.getElementById('sassEventEnabled')?.checked ? '1' : '0');
params.set('sassGrudgeEnabled', document.getElementById('sassGrudgeEnabled')?.checked ? '1' : '0');
params.set('sassLevel', document.getElementById('sassLevel')?.value || '55');
params.set('sassFrequency', document.getElementById('sassFrequency')?.value || '35');
params.set('movementSensitivity', document.getElementById('movementSensitivity')?.value || '50');
params.set('shakeSensitivity', document.getElementById('shakeSensitivity')?.value || '50');
params.set('grumpiness', document.getElementById('grumpiness')?.value || '70');
params.set('curiosity', document.getElementById('curiosity')?.value || '45');
params.set('sleepiness', document.getElementById('sleepiness')?.value || '40');
params.set('chaos', document.getElementById('chaos')?.value || '30');
params.set('friendliness', document.getElementById('friendliness')?.value || '25');
const hostInput = document.getElementById('hostname');
hostInput.value = cleanHostname(hostInput.value).slice(0, Number(hostInput.maxLength || 31));
params.set('hostname', hostInput.value);
params.set('weatherEnabled', document.getElementById('weatherEnabled').checked ? '1' : '0');
params.set('weatherName', document.getElementById('weatherName').value);
params.set('weatherLat', document.getElementById('weatherLat').value);
params.set('weatherLon', document.getElementById('weatherLon').value);
params.set('weatherMetric', document.getElementById('weatherMetric').checked ? '1' : '0');
params.set('weatherUpdateMs', document.getElementById('weatherUpdateMs').value);
params.set('weatherTickerLocation', document.getElementById('weatherTickerLocation').checked ? '1' : '0');
params.set('weatherTickerTemp', document.getElementById('weatherTickerTemp').checked ? '1' : '0');
params.set('weatherTickerCondition', document.getElementById('weatherTickerCondition').checked ? '1' : '0');
params.set('weatherTickerFeelsLike', document.getElementById('weatherTickerFeelsLike').checked ? '1' : '0');
params.set('weatherTickerHumidity', document.getElementById('weatherTickerHumidity').checked ? '1' : '0');
params.set('weatherTickerWind', document.getElementById('weatherTickerWind').checked ? '1' : '0');
params.set('weatherTickerPrecip', document.getElementById('weatherTickerPrecip').checked ? '1' : '0');
params.set('weatherTickerUpdated', document.getElementById('weatherTickerUpdated').checked ? '1' : '0');
params.set('clockEnabled', document.getElementById('clockEnabled').checked ? '1' : '0');
params.set('clock24h', document.getElementById('clock24h').checked ? '1' : '0');
params.set('clockUseWeatherOffset', document.getElementById('clockUseWeatherOffset').checked ? '1' : '0');
params.set('clockOffsetMinutes', document.getElementById('clockOffsetMinutes').value);
params.set('infoCardsEnabled', document.getElementById('infoCardsEnabled').checked ? '1' : '0');
params.set('infoCardClockEnabled', document.getElementById('infoCardClockEnabled').checked ? '1' : '0');
params.set('infoCardWeatherEnabled', document.getElementById('infoCardWeatherEnabled').checked ? '1' : '0');
params.set('infoCardIntervalMs', document.getElementById('infoCardIntervalMs').value);
params.set('sleepEnabled', document.getElementById('sleepEnabled')?.checked ? '1' : '0');
params.set('sleepStartTime', document.getElementById('sleepStartTime')?.value || '21:00');
params.set('sleepEndTime', document.getElementById('sleepEndTime')?.value || '06:00');
params.set('sleepAnimMs', document.getElementById('sleepAnimMs')?.value || '12000');
params.set('sleepGapMinMs', document.getElementById('sleepGapMinMs')?.value || '45000');
params.set('sleepGapMaxMs', document.getElementById('sleepGapMaxMs')?.value || '180000');
params.set('sleepWakeHoldMs', document.getElementById('sleepWakeHoldMs')?.value || '900000');
try {
const res = await fetch('/api/config', {
method: 'POST',
headers: {
'Content-Type': 'application/x-www-form-urlencoded'
},
body: params.toString()
});
const payload = await res.json();
if (!res.ok || !payload.ok) {
throw new Error(payload.error || 'Save failed');
}
settingsDirty = false;
await loadStatus();
showToast(payload.rebootRecommended ? 'Settings saved. Reboot/reconnect to fully apply hostname.' : 'Settings saved.', payload.rebootRecommended ? 'warn' : 'ok');
} catch (e) {
console.error(e);
showToast('Failed to save settings.', 'error');
}
}
async function loadStatus(options = {}) {
try {
const res = await fetch('/api/status');
const data = await res.json();
document.getElementById('dot').className = data.wifi.connected ? 'dot good' : 'dot bad';
document.getElementById('status').textContent = data.wifi.connected ? 'Online' : 'Disconnected';

const mem = data.memory || {};
const weather = data.weather || {};
const clock = data.clock || {};
const sleep = data.sleep || {};
const dashSummary = document.getElementById('dashboardSummary');
if (dashSummary) {
dashSummary.innerHTML =
summaryTile('Mood', data.face.currentMood || data.face.activeExpression || '--', data.face.baseMood ? ('base: ' + data.face.baseMood) : '') +
summaryTile('Wi-Fi', data.wifi.connected ? (data.wifi.rssi + ' dBm') : 'Offline', data.wifi.ip || '') +
summaryTile('Clock', clock.time || '--:--', clock.synced ? (clock.date || 'synced') : 'waiting for NTP') +
summaryTile('Sleep', sleep.status || '--', sleep.summary || '') +
summaryTile('Weather', weather.available ? `${fixed(weather.temperature)} °${weather.tempUnit || ''}` : '--', weather.summary || 'weather unavailable') +
summaryTile('Opinion', mem.judgmentLabel || mem.judgment || '--', mem.currentGrievance || mem.recentGrievance || '') +
summaryTile('Memory', mem.dirty ? 'Unsaved' : 'Saved', mem.holdingGrudge ? ('grudge: ' + (mem.worstGrudge || 'yes')) : 'no major grudge');
}
if (activePage === 'dashboard') {
document.getElementById('activeExpression').textContent = data.face.activeExpression;
document.getElementById('moodLine').textContent = data.face.moodLine || '--';
document.getElementById('faceInfo').innerHTML =
kv('Base expression', data.face.baseExpression) +
kv('Last trigger', data.face.lastTrigger) +
kv('Override expression', data.face.sensorOverrideExpression) +
kv('Override remaining', data.face.sensorOverrideRemainingMs + ' ms') +
kv('Bored active', yesNo(data.face.boredActive)) +
kv('Demo mode', yesNo(data.face.demoMode)) +
kv('Idle age', data.face.lastEngagementAgeMs + ' ms');
document.getElementById('wifiInfo').innerHTML =
kv('SSID', esc(data.wifi.ssid || 'Not connected')) +
kv('IP address', esc(data.wifi.ip)) +
kv('Wi-Fi status', esc(data.wifi.status || 'Unknown')) +
kv('RSSI', data.wifi.rssi + ' dBm') +
kv('Hostname', esc(data.app.hostname + '.local')) +
kv('mDNS URL', esc('http://' + data.app.hostname + '.local/'));
const clock = data.clock || {};
document.getElementById('clockTime').textContent = clock.time || '--:--';
document.getElementById('clockDate').textContent = clock.date || 'Waiting for NTP';
document.getElementById('clockInfo').innerHTML =
kv('Enabled', yesNo(clock.enabled)) +
kv('Synced', yesNo(clock.synced)) +
kv('Offset', esc(clock.activeOffsetLabel || '--')) +
kv('Offset source', esc(clock.activeOffsetSource || 'Manual')) +
kv('Weather offset', yesNo(clock.useWeatherOffset)) +
kv('OLED cards', yesNo(clock.infoCardsEnabled)) +
kv('Card interval', esc(clock.infoCardIntervalText || '--'));
document.getElementById('sleepStatus').textContent = sleep.status || '--';
document.getElementById('sleepSummary').textContent = sleep.summary || 'Sleep status unavailable';
document.getElementById('sleepInfo').innerHTML =
kv('Enabled', yesNo(sleep.enabled)) +
kv('Active', yesNo(sleep.active)) +
kv('Preview active', yesNo(sleep.previewActive)) +
kv('Window', esc((sleep.startTime || '--') + ' → ' + (sleep.endTime || '--'))) +
kv('Animation', esc(sleep.animationDurationText || '--')) +
kv('Blank/off gap', esc(sleep.gapText || '--')) +
kv('Wake hold', esc(sleep.wakeHoldText || '--')) +
kv('Minutes until wake', sleep.minutesUntilWake ?? '--') +
kv('Next animation', (sleep.nextAnimationInMs || 0) + ' ms') +
kv('Last wake', esc(sleep.lastWakeReason || '--'));
const weather = data.weather || {};
document.getElementById('weatherTemp').textContent = weather.available ? `${fixed(weather.temperature)} °${esc(weather.tempUnit || '')}` : '--';
document.getElementById('weatherSummary').textContent = weather.summary || 'Weather unavailable';
document.getElementById('weatherInfo').innerHTML =
kv('Enabled', yesNo(weather.enabled)) +
kv('Location', esc(weather.locationName || 'Not set')) +
kv('Coordinates', `${fixed(weather.latitude, 4)}, ${fixed(weather.longitude, 4)}`) +
kv('Condition', esc(weather.condition || '--')) +
kv('Feels like', weather.available ? `${fixed(weather.feelsLike)} °${esc(weather.tempUnit || '')}` : '--') +
kv('Humidity', weather.available ? `${fixed(weather.humidity, 0)}%` : '--') +
kv('Wind', weather.available ? `${fixed(weather.windSpeed)} ${esc(weather.windUnit || '')}` : '--') +
kv('Precip', weather.available ? `${fixed(weather.precipitation, 3)} ${esc(weather.precipUnit || '')}` : '--') +
kv('Updated', weather.available ? ageText(weather.lastSuccessAgeMs) : esc(weather.lastError || 'Never')) +
kv('Fetch interval', esc(weather.updateText || '--')) +
kv('Timezone', esc(weather.timezone || '--')) +
kv('Ticker', esc(weather.tickerLine || '--'));
document.getElementById('sensorInfo').innerHTML =
kv('I2C bus', 'SDA GPIO' + data.hardware.i2cSda + ' / SCL GPIO' + data.hardware.i2cScl) +
kv('OLED', data.hardware.oledEnabled ? 'Enabled at ' + data.hardware.oledAddress : 'Disabled') +
kv('MPU-6500', data.hardware.mpuEnabled ? yesNo(data.sensors.mpuReady) + ' at ' + data.hardware.mpuAddress : 'Disabled') +
kv('Shake score', data.sensors.lastShakeScore) +
kv('Motion jerk', data.sensors.lastMotionJerk + ' g') +
kv('Tilt delta', data.sensors.lastTiltDelta + ' g') +
kv('Accel magnitude', data.sensors.lastAccelMagnitude + ' g') +
kv('Move thresholds', data.sensors.movementJerkThreshold + ' / ' + data.sensors.movementTiltThreshold) +
kv('Shake thresholds', data.sensors.shakeJerkThreshold + ' / ' + data.sensors.shakeMagThreshold) +
kv('Touch hardware', data.hardware.touchEnabled ? data.hardware.touchPin : 'Disabled') +
kv('Touch ready', yesNo(data.sensors.touchReady)) +
kv('Touch value', data.sensors.lastTouchValue) +
kv('Touch baseline', data.sensors.touchBaseline) +
kv('Touch threshold', data.sensors.touchThreshold);
}
if (activePage === 'controls') {
applyHardwareUiState(data);
}
if (activePage === 'personality') {
const mood = data.mood || {};
const personality = mood.personality || {};
document.getElementById('moodInfo').innerHTML =
kv('Current mood', esc(mood.currentMoodLabel || mood.currentMood || '--')) +
kv('Base mood', esc(mood.baseMoodLabel || mood.baseMood || '--')) +
kv('Preset', esc(mood.activePresetLabel || mood.activePreset || '--')) +
kv('Temporary', yesNo(mood.temporary)) +
kv('Remaining', (mood.remainingMs || 0) + ' ms') +
kv('Priority', esc(mood.priority || '--')) +
kv('Display mode', esc(mood.displayModeLabel || '--')) +
kv('Face display available', yesNo(mood.faceDisplayAvailable)) +
kv('Automation', esc(mood.automationSummary || '--')) +
kv('Safety guard', esc(mood.safetySummary || '--')) +
kv('Active sequence', esc(mood.activeSequenceLabel || '--')) +
kv('Sequence step', mood.sequenceActive ? (mood.sequenceStep ?? 0) : '--') +
kv('Next sequence step', mood.sequenceActive ? ((mood.nextSequenceInMs || 0) + ' ms') : '--') +
kv('Last sequence', esc(mood.lastSequenceLabel || '--')) +
kv('Sequence reason', esc(mood.lastSequenceReason || '--')) +
kv('Sequence count', mood.sequenceRunCount ?? 0) +
kv('Sequence paused', yesNo(mood.sequencePausedForOverlay)) +
kv('Idle moods', yesNo(mood.idleMoodEnabled)) +
kv('Weather moods', yesNo(mood.weatherMoodEnabled)) +
kv('Wi-Fi moods', yesNo(mood.wifiMoodEnabled)) +
kv('WebUI moods', yesNo(mood.webMoodEnabled)) +
kv('Movement moods', yesNo(mood.movementMoodEnabled)) +
kv('Memory enabled', yesNo((data.memory || {}).enabled !== false)) +
kv('Sass ticker', yesNo((data.memory || {}).sassEnabled !== false)) +
kv('Last sass', esc((data.memory || {}).lastSassPhrase || '--')) +
kv('Grudge summary', esc(mood.grudgeSummary || '--')) +
kv('Memory bias', esc(mood.moodBiasSummary || mood.memoryBiasSummary || '--')) +
kv('Move sensitivity', mood.movementSensitivity ?? '--') +
kv('Shake sensitivity', mood.shakeSensitivity ?? '--') +
kv('Last movement event', esc(mood.lastMovementEvent || '--')) +
kv('Movement reason', esc(mood.lastMovementReason || '--')) +
kv('Movement count', mood.movementMoodCount ?? 0) +
kv('Last gyro activity', (mood.lastGyroActivityAgeMs || 0) + ' ms ago') +
kv('Last reaction', esc(mood.lastReactionEvent || '--')) +
kv('Reaction reason', esc(mood.lastReactionReason || '--')) +
kv('Reaction count', mood.reactionMoodCount ?? 0) +
kv('Last Wi-Fi RSSI', (mood.lastWifiRssi || 0) + ' dBm') +
kv('Next idle mood', (mood.nextIdleInMs || 0) + ' ms') +
kv('Last idle mood', esc(mood.lastIdleMoodLabel || '--')) +
kv('Last idle reason', esc(mood.lastIdleReason || '--')) +
kv('Idle mood count', mood.idleMoodCount ?? 0) +
kv('Grumpiness', personality.grumpiness ?? '--') +
kv('Curiosity', personality.curiosity ?? '--') +
kv('Sleepiness', personality.sleepiness ?? '--') +
kv('Chaos', personality.chaos ?? '--') +
kv('Friendliness', personality.friendliness ?? '--');
}
if (activePage === 'memory') {
renderMemory(data.memory || {});
}
if (activePage === 'system') {
document.getElementById('systemInfo').innerHTML =
kv('Version', data.app.version) +
kv('Board profile', data.hardware.boardProfile) +
kv('Uptime', data.system.uptimeText) +
kv('Chip', data.system.chipModel) +
kv('CPU', data.system.cpuMHz + ' MHz') +
kv('Cores', data.system.chipCores) +
kv('SDK', data.system.sdk || '--') +
kv('Flash chip', data.system.flashSize || '--') +
kv('App slot used', (data.system.flashUsed || data.system.sketchSize || '--') + (data.system.appSlotUsedPct !== undefined ? ' / ' + data.system.appSlotUsedPct + '%' : '')) +
kv('App slot free', data.system.appSlotFree || data.system.freeSketchSpace || '--') +
kv('App slot total', data.system.appSlotSize || '--') +
kv('Heap total', data.system.heapSize || '--') +
kv('Heap free', data.system.freeHeap || '--') +
kv('Min free heap', data.system.minFreeHeap || '--') +
kv('Max alloc heap', data.system.maxAllocHeap || '--');
document.getElementById('perfInfo').innerHTML =
kv('Face frame interval', data.system.configuredFrameMs + ' ms') +
kv('Last OLED render', data.system.lastFaceRenderMs + ' ms') +
kv('Approx face FPS', data.system.configuredFrameMs ? (1000 / data.system.configuredFrameMs).toFixed(1) : '--');
renderEvents(data.events);
}
if (activePage === 'settings' || activePage === 'personality' || options.toast || !settingsBuilt) {
applyConfigToForm(data);
}
if (options.toast) {
showToast('Settings loaded.', 'ok');
}
} catch (e) {
document.getElementById('dot').className = 'dot bad';
document.getElementById('status').textContent = 'Status Error';
console.error(e);
if (options.toast) {
showToast('Failed to load settings/status.', 'error');
}
}
}
initAccordions();
showPage('dashboard');
loadStatus({ toast: true });
setInterval(() => {
if (!document.hidden && !settingsDirty) {
loadStatus();
}
}, 5000);
setInterval(() => {
if (document.hidden && !settingsDirty) {
loadStatus();
}
}, 15000);
</script>
</body>
</html>
)HTML";

// =====================================================
// HTML MESSAGE PAGE
// =====================================================

String htmlMessage(String title, String message) {
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
  html += "<style>";
  html += "body{margin:0;min-height:100vh;display:grid;place-items:center;background:#071019;color:#eef7ff;font-family:system-ui;padding:18px}";
  html += ".card{max-width:520px;background:rgba(255,255,255,.08);border:1px solid rgba(255,255,255,.14);border-radius:22px;padding:22px;box-shadow:0 20px 60px rgba(0,0,0,.35)}";
  html += "h1{margin-top:0;color:#9de8ff}";
  html += "p{color:#9eb3c8;line-height:1.45}";
  html += "</style></head><body><div class='card'>";
  html += "<h1>" + htmlEscape(title) + "</h1>";
  html += "<p>" + htmlEscape(message) + "</p>";
  html += "</div></body></html>";
  return html;
}

// =====================================================
// WEB ROUTES
// =====================================================

void handleRoot() {
  server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "0");

  if (setupMode) {
    String html = FPSTR(SETUP_HTML);
    html.replace("{{HOSTNAME}}", htmlEscape(configuredHostname));
    server.send(200, "text/html", html);
  } else {
    handleYetiEvent(EVENT_WEBUI_OPENED);
    server.send_P(200, "text/html", DASHBOARD_HTML);
  }
}

void handleApiScan() {
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", buildScanJson());
}

void handleApiStatus() {
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", buildStatusJson());
}

void handleApiI2cScan() {
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", buildI2cScanJson());
}

void handleApiConfig() {
  if (setupMode) {
    server.send(403, "application/json", "{\"ok\":false,\"error\":\"setup_mode\"}");
    return;
  }

  shakeConfig.enabled = server.arg("shakeEnabled") == "1";
  shakeConfig.holdMs = sanitizeHoldMs(server.arg("shakeHoldMs").toInt(), 1800);
  shakeConfig.expressionMask = validExpressionMask(server.arg("shakeMask").toInt(), DEFAULT_SHAKE_MASK);

  touchConfig.enabled = server.arg("touchEnabled") == "1";
  touchConfig.holdMs = sanitizeHoldMs(server.arg("touchHoldMs").toInt(), 1600);
  touchConfig.expressionMask = validExpressionMask(server.arg("touchMask").toInt(), DEFAULT_TOUCH_MASK);

  boredTimeoutMs = sanitizeBoredTimeoutMs(server.arg("boredTimeoutMs").toInt(), 180000);

  String oldHostname = configuredHostname;
  if (server.hasArg("hostname")) {
    configuredHostname = sanitizeHostname(server.arg("hostname"));
  }
  bool hostnameChanged = configuredHostname != oldHostname;

  if (server.hasArg("faceFrameMs")) {
    faceFrameMs = sanitizeFaceFrameMs(server.arg("faceFrameMs").toInt(), YETI_FACE_FRAME_DEFAULT_MS);
  }

  if (server.hasArg("baseMood")) {
    baseMood = stringToMood(server.arg("baseMood"));
    if (!moodTemporaryActive()) {
      currentMood = baseMood;
      applyMood(currentMood);
    }
  }
  if (server.hasArg("idleMoodEnabled")) idleMoodEnabled = server.arg("idleMoodEnabled") == "1";
  if (server.hasArg("weatherMoodEnabled")) weatherMoodEnabled = server.arg("weatherMoodEnabled") == "1";
  if (server.hasArg("wifiMoodEnabled")) wifiMoodEnabled = server.arg("wifiMoodEnabled") == "1";
  if (server.hasArg("webMoodEnabled")) webMoodEnabled = server.arg("webMoodEnabled") == "1";
  if (server.hasArg("movementMoodEnabled")) movementMoodEnabled = server.arg("movementMoodEnabled") == "1";
  if (server.hasArg("memoryEnabled")) memoryEnabled = server.arg("memoryEnabled") == "1";
  if (server.hasArg("needsEnabled")) needsEnabled = server.arg("needsEnabled") == "1";
  if (server.hasArg("sassTickerEnabled")) sassTickerEnabled = server.arg("sassTickerEnabled") == "1";
  if (server.hasArg("sassIdleOnly")) sassIdleOnly = server.arg("sassIdleOnly") == "1";
  if (server.hasArg("sassEventEnabled")) sassEventEnabled = server.arg("sassEventEnabled") == "1";
  if (server.hasArg("sassGrudgeEnabled")) sassGrudgeEnabled = server.arg("sassGrudgeEnabled") == "1";
  if (server.hasArg("sassLevel")) sassLevel = sanitizeSensitivity(server.arg("sassLevel").toInt(), 55);
  if (server.hasArg("sassFrequency")) sassFrequency = sanitizeSensitivity(server.arg("sassFrequency").toInt(), 35);
  if (server.hasArg("movementSensitivity")) movementSensitivity = sanitizeSensitivity(server.arg("movementSensitivity").toInt(), 50);
  if (server.hasArg("shakeSensitivity")) shakeSensitivity = sanitizeSensitivity(server.arg("shakeSensitivity").toInt(), 50);
  if (server.hasArg("grumpiness")) personality.grumpiness = sanitizePersonalityTrait(server.arg("grumpiness").toInt(), 70);
  if (server.hasArg("curiosity")) personality.curiosity = sanitizePersonalityTrait(server.arg("curiosity").toInt(), 45);
  if (server.hasArg("sleepiness")) personality.sleepiness = sanitizePersonalityTrait(server.arg("sleepiness").toInt(), 40);
  if (server.hasArg("chaos")) personality.chaos = sanitizePersonalityTrait(server.arg("chaos").toInt(), 30);
  if (server.hasArg("friendliness")) personality.friendliness = sanitizePersonalityTrait(server.arg("friendliness").toInt(), 25);
  scheduleNextIdleBehavior();

  bool oldWeatherEnabled = weatherEnabled;
  String oldWeatherName = weatherLocationName;
  float oldWeatherLat = weatherLatitude;
  float oldWeatherLon = weatherLongitude;
  bool oldWeatherMetric = weatherMetric;

  weatherEnabled = server.arg("weatherEnabled") == "1";
  if (server.hasArg("weatherName")) weatherLocationName = sanitizeLocationName(server.arg("weatherName"));
  if (server.hasArg("weatherLat")) weatherLatitude = sanitizeLatitude(server.arg("weatherLat").toFloat());
  if (server.hasArg("weatherLon")) weatherLongitude = sanitizeLongitude(server.arg("weatherLon").toFloat());
  weatherMetric = server.arg("weatherMetric") == "1";
  if (server.hasArg("weatherUpdateMs")) weatherUpdateMs = sanitizeWeatherUpdateMs(server.arg("weatherUpdateMs").toInt());

  weatherTickerShowLocation = server.arg("weatherTickerLocation") == "1";
  weatherTickerShowTemp = server.arg("weatherTickerTemp") == "1";
  weatherTickerShowCondition = server.arg("weatherTickerCondition") == "1";
  weatherTickerShowFeelsLike = server.arg("weatherTickerFeelsLike") == "1";
  weatherTickerShowHumidity = server.arg("weatherTickerHumidity") == "1";
  weatherTickerShowWind = server.arg("weatherTickerWind") == "1";
  weatherTickerShowPrecip = server.arg("weatherTickerPrecip") == "1";
  weatherTickerShowUpdated = server.arg("weatherTickerUpdated") == "1";

  bool weatherChanged = oldWeatherEnabled != weatherEnabled ||
                        oldWeatherName != weatherLocationName ||
                        fabs(oldWeatherLat - weatherLatitude) > 0.0001f ||
                        fabs(oldWeatherLon - weatherLongitude) > 0.0001f ||
                        oldWeatherMetric != weatherMetric;

  clockEnabled = server.arg("clockEnabled") == "1";
  clock24h = server.arg("clock24h") == "1";
  clockUseWeatherOffset = server.arg("clockUseWeatherOffset") == "1";
  if (server.hasArg("clockOffsetMinutes")) clockUtcOffsetMinutes = sanitizeUtcOffsetMinutes(server.arg("clockOffsetMinutes").toInt());

  infoCardsEnabled = server.arg("infoCardsEnabled") == "1";
  infoCardClockEnabled = server.arg("infoCardClockEnabled") == "1";
  infoCardWeatherEnabled = server.arg("infoCardWeatherEnabled") == "1";
  if (server.hasArg("infoCardIntervalMs")) infoCardIntervalMs = sanitizeInfoCardIntervalMs(server.arg("infoCardIntervalMs").toInt());

  sleepModeEnabled = server.arg("sleepEnabled") == "1";
  if (server.hasArg("sleepStartTime")) sleepStartMinute = parseTimeToMinute(server.arg("sleepStartTime"), YETI_SLEEP_START_DEFAULT_MIN);
  if (server.hasArg("sleepEndTime")) sleepEndMinute = parseTimeToMinute(server.arg("sleepEndTime"), YETI_SLEEP_END_DEFAULT_MIN);
  if (server.hasArg("sleepAnimMs")) sleepAnimDurationMs = sanitizeSleepAnimMs(server.arg("sleepAnimMs").toInt(), YETI_SLEEP_ANIM_DEFAULT_MS);
  if (server.hasArg("sleepGapMinMs")) sleepGapMinMs = sanitizeSleepGapMs(server.arg("sleepGapMinMs").toInt(), YETI_SLEEP_GAP_MIN_DEFAULT_MS);
  if (server.hasArg("sleepGapMaxMs")) sleepGapMaxMs = sanitizeSleepGapMs(server.arg("sleepGapMaxMs").toInt(), YETI_SLEEP_GAP_MAX_DEFAULT_MS);
  if (sleepGapMaxMs < sleepGapMinMs) sleepGapMaxMs = sleepGapMinMs;
  if (server.hasArg("sleepWakeHoldMs")) sleepWakeHoldMs = sanitizeSleepWakeHoldMs(server.arg("sleepWakeHoldMs").toInt(), YETI_SLEEP_WAKE_HOLD_DEFAULT_MS);
  if (!sleepModeEnabled && sleepModeActive) {
    exitSleepMode("sleep disabled from WebUI", false);
  }

  if (weatherChanged) {
    weatherAvailable = false;
    weatherLastError = weatherEnabled ? "Weather settings changed" : "Weather disabled";
    lastWeatherAttemptMs = 0;
    lastWeatherSuccessMs = 0;
  }

  nextInfoCardAt = 0;

#if !YETI_MPU_ENABLED
  shakeConfig.enabled = false;
  movementMoodEnabled = false;
#endif

#if !YETI_TOUCH_ENABLED
  touchConfig.enabled = false;
#endif

  saveBehaviorConfig();

  if (hostnameChanged) {
    WiFi.setHostname(configuredHostname.c_str());
    beginConfiguredMdns();
    addEvent("Config", String("Hostname saved: ") + configuredHostname);
  } else {
    addEvent("Config", "Settings saved");
  }

  if (weatherChanged && weatherEnabled && weatherLocationConfigured()) {
    addEvent("Weather", "Settings changed; refresh queued");
  }

  handleYetiEvent(EVENT_SETTINGS_SAVED);
  saveMemorySettings(true);

  String json = "{";
  json += "\"ok\":true,";
  json += "\"hostnameChanged\":" + String(hostnameChanged ? "true" : "false") + ",";
  json += "\"rebootRecommended\":" + String(hostnameChanged ? "true" : "false") + ",";
  json += "\"status\":" + buildStatusJson();
  json += "}";

  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", json);
}

void sendMoodApiResponse(bool ok, String error = "") {
  String json = "{";
  json += "\"ok\":" + String(ok ? "true" : "false") + ",";
  if (!ok) {
    json += "\"error\":\"" + jsonEscape(error.length() ? error : String("Mood command failed")) + "\",";
  }
  json += "\"mood\":";
  json += buildMoodJson();
  json += ",\"status\":";
  json += buildStatusJson();
  json += "}";

  server.sendHeader("Cache-Control", "no-store");
  server.send(ok ? 200 : 400, "application/json", json);
}


void sendMemoryApiResponse(bool ok, String error) {
  String json = "{";
  json += "\"ok\":" + String(ok ? "true" : "false") + ",";
  if (!ok) {
    json += "\"error\":\"" + jsonEscape(error.length() ? error : String("Memory command failed")) + "\",";
  }
  json += "\"memory\":";
  json += buildMemoryJson();
  json += ",\"status\":";
  json += buildStatusJson();
  json += "}";

  server.sendHeader("Cache-Control", "no-store");
  server.send(ok ? 200 : 400, "application/json", json);
}

void handleApiMemoryGet() {
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", buildMemoryJson());
}

void handleApiMemorySave() {
  if (setupMode) {
    server.send(403, "application/json", "{\"ok\":false,\"error\":\"setup_mode\"}");
    return;
  }

  if (server.hasArg("memoryEnabled")) {
    memoryEnabled = server.arg("memoryEnabled") == "1";
    markMemoryDirty();
  }
  if (server.hasArg("needsEnabled")) {
    needsEnabled = server.arg("needsEnabled") == "1";
    markMemoryDirty();
  }
  if (server.hasArg("sassTickerEnabled")) sassTickerEnabled = server.arg("sassTickerEnabled") == "1";
  if (server.hasArg("sassIdleOnly")) sassIdleOnly = server.arg("sassIdleOnly") == "1";
  if (server.hasArg("sassEventEnabled")) sassEventEnabled = server.arg("sassEventEnabled") == "1";
  if (server.hasArg("sassGrudgeEnabled")) sassGrudgeEnabled = server.arg("sassGrudgeEnabled") == "1";
  if (server.hasArg("sassLevel")) sassLevel = sanitizeSensitivity(server.arg("sassLevel").toInt(), 55);
  if (server.hasArg("sassFrequency")) sassFrequency = sanitizeSensitivity(server.arg("sassFrequency").toInt(), 35);
  saveBehaviorConfig();

  saveMemorySettings(true);
  sendMemoryApiResponse(true);
}

void handleApiMemoryReset() {
  if (setupMode) {
    server.send(403, "application/json", "{\"ok\":false,\"error\":\"setup_mode\"}");
    return;
  }

  resetMemorySettings();
  sendMemoryApiResponse(true);
}

void handleApiMemoryForgive() {
  if (setupMode) {
    server.send(403, "application/json", "{\"ok\":false,\"error\":\"setup_mode\"}");
    return;
  }

  adjustRelationship(2, -20, 0, 5);
  adjustNeeds(-5, 0, -25, -5, true);
  reduceAllGrudges(20);
  lastMemoryEvent = "Forgive";
  lastMemoryNote = "Forgiveness requested; suspicion retained";
  addEvent("Memory", "Forgive Me pressed");
  saveMemorySettings(true);
  setMood(MOOD_SMUG, 4500, PRIORITY_MANUAL, true);
  maybeShowMemoryPhrase(PHRASE_FORGIVE, true);
  sendMemoryApiResponse(true);
}

void handleApiMemoryAnnoy() {
  if (setupMode) {
    server.send(403, "application/json", "{\"ok\":false,\"error\":\"setup_mode\"}");
    return;
  }

  adjustRelationship(-3, 15, -2, 4);
  adjustNeeds(8, -2, 22, 3, true);
  adjustGrudgeValue(grudges.pokeGrudge, 10);
  adjustGrudgeValue(grudges.neglectGrudge, 5);
  lastGrudgeNote = "Manual annoyance filed into the grudge ledger";
  lastMemoryEvent = "Annoy";
  lastMemoryNote = "Manual annoyance added. Bold move.";
  addEvent("Memory", "Annoy YETI pressed");
  saveMemorySettings(true);
  setMood(MOOD_ANNOYED, 8000, PRIORITY_MANUAL, true);
  maybeShowMemoryPhrase(PHRASE_GRUDGE, true);
  sendMemoryApiResponse(true);
}

void handleApiMemoryPraise() {
  if (setupMode) {
    server.send(403, "application/json", "{\"ok\":false,\"error\":\"setup_mode\"}");
    return;
  }

  adjustRelationship(10, -5, 5, 2);
  adjustNeeds(-12, 2, -8, -12, true);
  reduceAllGrudges(5);
  lastMemoryEvent = "Praise";
  lastMemoryNote = "Praise accepted; motives under review";
  addEvent("Memory", "Praise YETI pressed");
  saveMemorySettings(true);
  setMood(MOOD_HAPPY, 6500, PRIORITY_MANUAL, true);
  maybeShowMemoryPhrase(PHRASE_PRAISE, true);
  sendMemoryApiResponse(true);
}

void handleApiMemoryDecay() {
  if (setupMode) {
    server.send(403, "application/json", "{\"ok\":false,\"error\":\"setup_mode\"}");
    return;
  }

  reduceAllGrudges(10);
  adjustNeeds(-3, 0, -8, -2, true);
  adjustRelationship(1, -5, 0, 1);
  lastMemoryEvent = "Grudge decay";
  lastMemoryNote = "Manual grudge decay requested";
  saveMemorySettings(true);
  setMood(MOOD_SMUG, 3500, PRIORITY_MANUAL, true);
  sendMemoryApiResponse(true);
}

void handleApiMemoryAttention() {
  if (setupMode) {
    server.send(403, "application/json", "{\"ok\":false,\"error\":\"setup_mode\"}");
    return;
  }

  adjustRelationship(4, -3, 1, 0);
  adjustNeeds(-18, 3, -6, -25, true);
  adjustGrudgeValue(grudges.neglectGrudge, -8);
  lastMemoryEvent = "Attention";
  lastMemoryNote = "Attention received; loneliness denied publicly";
  lastNeedsNote = "Manual attention reduced loneliness and boredom";
  addEvent("Needs", "Attention button pressed");
  saveMemorySettings(true);
  setMood(MOOD_CURIOUS, 5000, PRIORITY_MANUAL, true);
  sendMemoryApiResponse(true);
}

void handleApiMemoryCalm() {
  if (setupMode) {
    server.send(403, "application/json", "{\"ok\":false,\"error\":\"setup_mode\"}");
    return;
  }

  adjustRelationship(1, -8, 1, 1);
  adjustNeeds(-4, 0, -30, -4, true);
  lastMemoryEvent = "Calm";
  lastMemoryNote = "Calm Down applied to internal gremlin pressure";
  lastNeedsNote = "Manual calm reduced irritation";
  addEvent("Needs", "Calm Down pressed");
  saveMemorySettings(true);
  setMood(MOOD_DEADPAN, 4500, PRIORITY_MANUAL, true);
  sendMemoryApiResponse(true);
}

void handleApiMemoryBore() {
  if (setupMode) {
    server.send(403, "application/json", "{\"ok\":false,\"error\":\"setup_mode\"}");
    return;
  }

  adjustRelationship(0, 4, 0, 1);
  adjustNeeds(28, -2, 4, 8, true);
  adjustGrudgeValue(grudges.neglectGrudge, 6);
  lastMemoryEvent = "Bore";
  lastMemoryNote = "Boredom deliberately increased. Risky science.";
  lastNeedsNote = "Manual boredom injection";
  addEvent("Needs", "Bore YETI pressed");
  saveMemorySettings(true);
  setMood(MOOD_SMUG, 4500, PRIORITY_MANUAL, true);
  sendMemoryApiResponse(true);
}

void handleApiMemoryWake() {
  if (setupMode) {
    server.send(403, "application/json", "{\"ok\":false,\"error\":\"setup_mode\"}");
    return;
  }

  adjustRelationship(1, 2, 0, 2);
  adjustNeeds(-5, 30, 3, -3, true);
  lastMemoryEvent = "Wake";
  lastMemoryNote = "Wake-up energy injected; suspicion increased";
  lastNeedsNote = "Manual wake raised energy";
  addEvent("Needs", "Wake YETI pressed");
  saveMemorySettings(true);
  setMood(MOOD_STARTLED, 2500, PRIORITY_MANUAL, true);
  sendMemoryApiResponse(true);
}

void handleApiMemoryRollover() {
  if (setupMode) {
    server.send(403, "application/json", "{\"ok\":false,\"error\":\"setup_mode\"}");
    return;
  }

  rolloverDailyMemoryIfNeeded(true);
  lastMemoryEvent = "Daily rollover";
  lastMemoryNote = lastDailyNote;
  saveMemorySettings(true);
  setMood(MOOD_SMUG, 3500, PRIORITY_MANUAL, true);
  sendMemoryApiResponse(true);
}

void handleApiMemoryClearToday() {
  if (setupMode) {
    server.send(403, "application/json", "{\"ok\":false,\"error\":\"setup_mode\"}");
    return;
  }

  clearTodayDailyMemory();
  lastDailyNote = "Today counters cleared manually";
  lastMemoryEvent = "Clear today";
  lastMemoryNote = lastDailyNote;
  markMemoryDirty();
  addEvent("Memory", lastDailyNote);
  saveMemorySettings(true);
  setMood(MOOD_DEADPAN, 3000, PRIORITY_MANUAL, true);
  sendMemoryApiResponse(true);
}


void applySassArgsFromRequest() {
  if (server.hasArg("sassTickerEnabled")) sassTickerEnabled = server.arg("sassTickerEnabled") == "1";
  if (server.hasArg("sassIdleOnly")) sassIdleOnly = server.arg("sassIdleOnly") == "1";
  if (server.hasArg("sassEventEnabled")) sassEventEnabled = server.arg("sassEventEnabled") == "1";
  if (server.hasArg("sassGrudgeEnabled")) sassGrudgeEnabled = server.arg("sassGrudgeEnabled") == "1";
  if (server.hasArg("sassLevel")) sassLevel = sanitizeSensitivity(server.arg("sassLevel").toInt(), 55);
  if (server.hasArg("sassFrequency")) sassFrequency = sanitizeSensitivity(server.arg("sassFrequency").toInt(), 35);
  saveBehaviorConfig();
}

void sendSassApiResponse(bool ok, String error = "") {
  String json = "{";
  json += "\"ok\":" + String(ok ? "true" : "false") + ",";
  if (!ok) {
    json += "\"error\":\"" + jsonEscape(error.length() ? error : String("sass_not_displayed")) + "\",";
  }
  json += "\"memory\":" + buildMemoryJson() + ",";
  json += "\"status\":" + buildStatusJson();
  json += "}";
  server.sendHeader("Cache-Control", "no-store");
  server.send(ok ? 200 : 400, "application/json", json);
}

void handleApiSassTest() {
  if (setupMode) {
    server.send(403, "application/json", "{\"ok\":false,\"error\":\"setup_mode\"}");
    return;
  }
  applySassArgsFromRequest();
  bool ok = maybeShowMemoryPhrase(PHRASE_IDLE, true);
  sendSassApiResponse(ok, ok ? "" : lastSassNote);
}

void handleApiSassJudgment() {
  if (setupMode) {
    server.send(403, "application/json", "{\"ok\":false,\"error\":\"setup_mode\"}");
    return;
  }
  applySassArgsFromRequest();
  bool ok = showSassPhrase(getJudgmentLabel(), PHRASE_JUDGMENT, true);
  sendSassApiResponse(ok, ok ? "" : lastSassNote);
}

void handleApiSassGrievance() {
  if (setupMode) {
    server.send(403, "application/json", "{\"ok\":false,\"error\":\"setup_mode\"}");
    return;
  }
  applySassArgsFromRequest();
  bool ok = showSassPhrase(getCurrentGrievance(), PHRASE_GRUDGE, true);
  sendSassApiResponse(ok, ok ? "" : lastSassNote);
}

void handleApiSassRandom() {
  if (setupMode) {
    server.send(403, "application/json", "{\"ok\":false,\"error\":\"setup_mode\"}");
    return;
  }
  applySassArgsFromRequest();
  YetiPhraseCategory categories[] = {PHRASE_POKE, PHRASE_SHAKE, PHRASE_WIFI, PHRASE_WEATHER, PHRASE_IDLE, PHRASE_BOOT, PHRASE_FORGIVE, PHRASE_PRAISE, PHRASE_GRUDGE, PHRASE_NEEDS, PHRASE_JUDGMENT};
  YetiPhraseCategory category = categories[random(sizeof(categories) / sizeof(categories[0]))];
  bool ok = maybeShowMemoryPhrase(category, true);
  sendSassApiResponse(ok, ok ? "" : lastSassNote);
}

void handleApiSassClear() {
  if (setupMode) {
    server.send(403, "application/json", "{\"ok\":false,\"error\":\"setup_mode\"}");
    return;
  }
  applySassArgsFromRequest();
  bool cleared = oledOverlayActive && oledOverlayMode == OLED_OVERLAY_SASS_TICKER;
  if (cleared) {
    stopOledOverlay();
  }
  lastSassNote = cleared ? "Sass ticker cleared" : "No active sass ticker to clear";
  addEvent("Sass", lastSassNote);
  sendSassApiResponse(true);
}

void handleApiMoodGet() {
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", buildMoodJson());
}

void handleApiMoodPost() {
  if (setupMode) {
    server.send(403, "application/json", "{\"ok\":false,\"error\":\"setup_mode\"}");
    return;
  }

  recordMemoryEvent(EVENT_WEBUI_COMMAND);
  wakeYetiFromSleep("webui command", true);

  String moodName = server.arg("mood");
  unsigned long durationMs = server.hasArg("durationMs") ? (unsigned long)server.arg("durationMs").toInt() : 8000UL;
  bool ok = setMood(stringToMood(moodName), durationMs, PRIORITY_MANUAL, true);
  sendMoodApiResponse(ok, ok ? "" : "lower_priority_blocked");
}

void handleApiMoodBase() {
  if (setupMode) {
    server.send(403, "application/json", "{\"ok\":false,\"error\":\"setup_mode\"}");
    return;
  }

  recordMemoryEvent(EVENT_WEBUI_COMMAND);
  wakeYetiFromSleep("webui command", true);
  setBaseMood(stringToMood(server.arg("mood")));
  saveBehaviorConfig();
  sendMoodApiResponse(true);
}

void handleApiMoodRandom() {
  if (setupMode) {
    server.send(403, "application/json", "{\"ok\":false,\"error\":\"setup_mode\"}");
    return;
  }

  recordMemoryEvent(EVENT_WEBUI_COMMAND);
  wakeYetiFromSleep("webui command", true);
  unsigned long durationMs = server.hasArg("durationMs") ? (unsigned long)server.arg("durationMs").toInt() : 8000UL;
  bool ok = setMood(randomMood(), durationMs, PRIORITY_MANUAL, true);
  sendMoodApiResponse(ok, ok ? "" : "lower_priority_blocked");
}

void handleApiMoodIdleNow() {
  if (setupMode) {
    server.send(403, "application/json", "{\"ok\":false,\"error\":\"setup_mode\"}");
    return;
  }

  recordMemoryEvent(EVENT_WEBUI_COMMAND);
  wakeYetiFromSleep("webui command", true);

  if (!idleMoodEnabled) {
    sendMoodApiResponse(false, "idle_moods_disabled");
    return;
  }

  // Manual tester: clear lower/manual stickiness so the weighted idle picker is visible immediately.
  currentMoodPriority = PRIORITY_IDLE;
  moodUntilMs = 0;
  markInteraction();
  lastIdleBehaviorMs = millis();
  bool ok = runRandomIdleBehavior();
  scheduleNextIdleBehavior();
  sendMoodApiResponse(ok, ok ? "" : "idle_unavailable");
}

void handleApiMoodWeatherNow() {
  if (setupMode) {
    server.send(403, "application/json", "{\"ok\":false,\"error\":\"setup_mode\"}");
    return;
  }

  recordMemoryEvent(EVENT_WEBUI_COMMAND);
  wakeYetiFromSleep("webui command", true);

  if (!weatherMoodEnabled) {
    sendMoodApiResponse(false, "weather_moods_disabled");
    return;
  }

  if (!weatherAvailable) {
    bool fetched = fetchWeatherNow(true);
    if (!fetched) {
      sendMoodApiResponse(false, weatherLastError.length() ? weatherLastError : String("weather_unavailable"));
      return;
    }
  } else {
    // Manual tester bypasses the normal weather cooldown.
    lastWeatherMoodMs = 0;
    reactToCurrentWeather("manual weather test");
  }

  sendMoodApiResponse(true);
}

void handleApiMoodWifiNow() {
  if (setupMode) {
    server.send(403, "application/json", "{\"ok\":false,\"error\":\"setup_mode\"}");
    return;
  }

  recordMemoryEvent(EVENT_WEBUI_COMMAND);
  wakeYetiFromSleep("webui command", true);

  if (!wifiMoodEnabled) {
    sendMoodApiResponse(false, "wifi_moods_disabled");
    return;
  }

  lastWifiMoodMs = 0;
  updateWifiMoodReactions(true);
  sendMoodApiResponse(true);
}

void handleApiMoodMovementNow() {
  if (setupMode) {
    server.send(403, "application/json", "{\"ok\":false,\"error\":\"setup_mode\"}");
    return;
  }

  wakeYetiFromSleep("webui movement test", true);

  if (!movementMoodEnabled) {
    sendMoodApiResponse(false, "movement_moods_disabled");
    return;
  }

  if (!mpuReady) {
    sendMoodApiResponse(false, "mpu_not_ready");
    return;
  }

  lastGyroMoodMs = 0;
  lastGyroMoveMoodMs = 0;
  lastGyroShakeMoodMs = 0;
  lastMotionJerk = max(lastMotionJerk, movementJerkThresholdG() + 0.02f);
  handleYetiEvent(EVENT_GYRO_MOVED);
  sendMoodApiResponse(true);
}

void handleApiSequencePost() {
  if (setupMode) {
    server.send(403, "application/json", "{\"ok\":false,\"error\":\"setup_mode\"}");
    return;
  }

  recordMemoryEvent(EVENT_WEBUI_COMMAND);
  wakeYetiFromSleep("webui command", true);

  YetiSequence seq = stringToSequence(server.arg("sequence"));
  if (seq == SEQ_NONE) {
    sendMoodApiResponse(false, "unknown_sequence");
    return;
  }

  markInteraction();
  bool ok = startSequence(seq, "manual WebUI sequence", PRIORITY_MANUAL);
  sendMoodApiResponse(ok, ok ? "" : "sequence_failed");
}

void handleApiSequenceStop() {
  if (setupMode) {
    server.send(403, "application/json", "{\"ok\":false,\"error\":\"setup_mode\"}");
    return;
  }

  recordMemoryEvent(EVENT_WEBUI_COMMAND);
  wakeYetiFromSleep("webui command", true);
  stopSequence(true);
  sendMoodApiResponse(true);
}

void handleApiMoodPoke() {
  if (setupMode) {
    server.send(403, "application/json", "{\"ok\":false,\"error\":\"setup_mode\"}");
    return;
  }

  triggerPokeYeti();
  sendMoodApiResponse(true);
}

void handleApiMoodCalm() {
  if (setupMode) {
    server.send(403, "application/json", "{\"ok\":false,\"error\":\"setup_mode\"}");
    return;
  }

  recordMemoryEvent(EVENT_WEBUI_COMMAND);
  wakeYetiFromSleep("webui command", true);
  calmDownYeti();
  sendMoodApiResponse(true);
}

void handleApiPersonalityPreset() {
  if (setupMode) {
    server.send(403, "application/json", "{\"ok\":false,\"error\":\"setup_mode\"}");
    return;
  }

  recordMemoryEvent(EVENT_WEBUI_COMMAND);
  wakeYetiFromSleep("webui command", true);

  String presetId = server.arg("preset");
  bool ok = applyPersonalityPreset(presetId, true);
  sendMoodApiResponse(ok, ok ? "" : "unknown_preset");
}

void handleApiAction() {
  if (setupMode) {
    server.send(403, "application/json", "{\"ok\":false,\"error\":\"setup_mode\"}");
    return;
  }

  recordMemoryEvent(EVENT_WEBUI_COMMAND);

  String actionName = server.arg("action");
  actionName.toLowerCase();

  if (actionName != "sleep_now" && actionName != "wake_now") {
    wakeYetiFromSleep("webui command", true);
  }

  bool ok = true;

  if (sleepPreviewActive && actionName != "sleep_preview" && actionName != "sleep_now") {
    stopSleepAnimationPreview(true);
  }

  if (actionName == "sleep_now") {
    sleepWakeOverrideUntilMs = 0;
    enterSleepMode("manual WebUI sleep now");
  } else if (actionName == "sleep_preview") {
    startSleepAnimationPreview(YETI_SLEEP_PREVIEW_DEFAULT_MS);
  } else if (actionName == "wake_now") {
    wakeYetiFromSleep("manual WebUI wake", true);
  } else if (actionName == "normal") {
    setMood(MOOD_DEADPAN, 0, PRIORITY_MANUAL, true);
  } else if (actionName == "angry") {
    setMood(MOOD_ANGRY, 0, PRIORITY_MANUAL, true);
  } else if (actionName == "sleepy") {
    setMood(MOOD_SLEEPY, 0, PRIORITY_MANUAL, true);
  } else if (actionName == "happy") {
    setMood(MOOD_HAPPY, 0, PRIORITY_MANUAL, true);
  } else if (actionName == "surprised") {
    setMood(MOOD_CURIOUS, 8000, PRIORITY_MANUAL, true);
  } else if (actionName == "shocked") {
    setMood(MOOD_STARTLED, 5000, PRIORITY_MANUAL, true);
  } else if (actionName == "bored") {
    setMood(MOOD_SLEEPY, 0, PRIORITY_MANUAL, true);
  } else if (actionName == "blink") {
    startBlink();
    markInteraction();
    addEvent("Face", "Manual blink");
    handleYetiEvent(EVENT_WEBUI_COMMAND);
  } else if (actionName == "random") {
    setMood(randomMood(), 8000, PRIORITY_MANUAL, true);
  } else if (actionName == "poke") {
    triggerPokeYeti();
  } else if (actionName == "calm") {
    calmDownYeti();
  } else if (actionName == "reset_personality") {
    ok = applyPersonalityPreset("classic", true);
    addEvent("Mood", "Personality reset to Classic YETI");
  } else if (actionName == "demo") {
    startDemoMode();
  } else if (actionName == "show_ip") {
    String ipLine = WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : WiFi.softAPIP().toString();
    showTemporaryOledStatus(
      "Yeti Network",
      WiFi.status() == WL_CONNECTED ? "Station mode" : "Setup/AP mode",
      String("IP: ") + ipLine,
      String("Host: ") + configuredHostname + ".local",
      "SSID:",
      WiFi.status() == WL_CONNECTED ? clipText(WiFi.SSID(), 21) : clipText(setupApName, 21),
      7000
    );
    addEvent("OLED", "Displayed IP address");
    handleYetiEvent(EVENT_CUSTOM_MESSAGE);
  } else if (actionName == "weather_refresh") {
    ok = fetchWeatherNow(true);
  } else if (actionName == "sync_clock") {
    ntpStarted = false;
    ensureClockStarted();
    addEvent("Clock", "Manual NTP sync requested");
  } else if (actionName == "reboot") {
    handleYetiEvent(EVENT_REBOOT_REQUESTED);
    addEvent("System", "Reboot requested from WebUI");
    saveMemorySettings(true);
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "application/json", "{\"ok\":true,\"rebooting\":true}");
    delay(600);
    ESP.restart();
    return;
  } else if (actionName == "show_weather") {
    showWeatherCard();
  } else if (actionName == "show_clock") {
    showClockCard();
  } else if (actionName == "trigger_shake") {
#if YETI_MPU_ENABLED
    triggerSensorReaction("Shake", shakeConfig, SHAKE_PRIORITY, FACE_ANGRY);
    if (movementMoodEnabled && mpuReady) {
      lastGyroShakeMoodMs = 0;
      lastShakeScore = max(lastShakeScore, movementShakeJerkThresholdG() + 0.10f);
      handleYetiEvent(EVENT_GYRO_SHAKEN);
    }
#else
    ok = false;
#endif
  } else if (actionName == "trigger_touch") {
#if YETI_TOUCH_ENABLED
    triggerSensorReaction("Touch", touchConfig, TOUCH_PRIORITY, FACE_HAPPY);
#else
    ok = false;
#endif
  } else if (actionName == "calibrate_touch") {
#if YETI_TOUCH_ENABLED
    calibrateTouch();
#else
    ok = false;
#endif
  } else {
    ok = false;
  }

  String json = "{";
  json += "\"ok\":" + String(ok ? "true" : "false") + ",";
  if (!ok) {
    json += "\"error\":\"" + jsonEscape(weatherLastError.length() ? weatherLastError : String("Action failed")) + "\",";
  }
  json += "\"status\":" + buildStatusJson();
  json += "}";

  server.sendHeader("Cache-Control", "no-store");
  server.send(ok ? 200 : 400, "application/json", json);
}

void handleSaveWifi() {
  if (!setupMode) {
    server.send(403, "text/plain", "Wi-Fi setup is only available in setup mode.");
    return;
  }

  String ssid = server.arg("ssid");
  String password = server.arg("password");
  String hostname = sanitizeHostname(server.arg("hostname"));

  ssid.trim();

  if (ssid.length() == 0) {
    server.send(400, "text/html", htmlMessage("Missing SSID", "Go back and enter a Wi-Fi network name."));
    return;
  }

  configuredHostname = hostname;

  weatherEnabled = server.arg("weatherEnabled") == "1";
  if (server.hasArg("weatherName")) weatherLocationName = sanitizeLocationName(server.arg("weatherName"));
  if (server.hasArg("weatherLat")) weatherLatitude = sanitizeLatitude(server.arg("weatherLat").toFloat());
  if (server.hasArg("weatherLon")) weatherLongitude = sanitizeLongitude(server.arg("weatherLon").toFloat());
  weatherMetric = server.arg("weatherMetric") == "1";
  if (server.hasArg("clockOffset")) clockUtcOffsetMinutes = sanitizeUtcOffsetMinutes(server.arg("clockOffset").toInt());

  saveBehaviorConfig();
  saveMemorySettings(true);

  prefs.begin("wifi", false);
  prefs.putString("ssid", ssid);
  prefs.putString("password", password);
  prefs.end();

  Serial.println();
  Serial.println("Saved new Wi-Fi credentials.");
  addEvent("WiFi", String("Saved credentials for ") + ssid);
  addEvent("Config", String("Hostname saved: ") + configuredHostname);
  Serial.print("SSID: ");
  Serial.println(ssid);
  Serial.print("Hostname: ");
  Serial.println(configuredHostname);
  Serial.println("Rebooting...");

  oledStatus(
    "Yeti Wi-Fi Saved",
    "SSID:",
    clipText(ssid, 21),
    "Rebooting now",
    "Reconnect phone",
    "to normal Wi-Fi"
  );

  server.send(200, "text/html", htmlMessage(
    "Wi-Fi Saved",
    String("Credentials saved. Yeti is rebooting now. Try ") + hostnameLocalUrl() + " after it joins Wi-Fi, or check the OLED for the IP address."
  ));

  delay(1800);
  ESP.restart();
}

void handleForgetWifi() {
  clearSavedWifi();

  Serial.println();
  Serial.println("Saved Wi-Fi credentials erased.");
  addEvent("WiFi", "Credentials erased");
  Serial.println("Rebooting into setup mode...");

  oledStatus(
    "Yeti Reset",
    "Wi-Fi erased",
    "Rebooting into",
    "setup mode",
    "",
    ""
  );

  server.send(200, "text/html", htmlMessage(
    "Wi-Fi Forgotten",
    "Saved Wi-Fi credentials erased. Yeti is rebooting into setup mode."
  ));

  delay(1800);
  ESP.restart();
}

void redirectToPortal() {
  server.sendHeader("Location", "http://192.168.4.1/", true);
  server.send(302, "text/plain", "");
}

void handleNotFound() {
  if (setupMode) {
    redirectToPortal();
  } else {
    server.send(404, "text/plain", "404 - Not found");
  }
}

void setupWebServer() {
  server.on("/", HTTP_GET, handleRoot);

  server.on("/api/scan", HTTP_GET, handleApiScan);
  server.on("/api/status", HTTP_GET, handleApiStatus);
  server.on("/api/i2c", HTTP_GET, handleApiI2cScan);
  server.on("/api/mood", HTTP_GET, handleApiMoodGet);
  server.on("/api/mood", HTTP_POST, handleApiMoodPost);
  server.on("/api/mood/base", HTTP_POST, handleApiMoodBase);
  server.on("/api/mood/random", HTTP_POST, handleApiMoodRandom);
  server.on("/api/mood/idle-now", HTTP_POST, handleApiMoodIdleNow);
  server.on("/api/mood/weather-now", HTTP_POST, handleApiMoodWeatherNow);
  server.on("/api/mood/wifi-now", HTTP_POST, handleApiMoodWifiNow);
  server.on("/api/mood/movement-now", HTTP_POST, handleApiMoodMovementNow);
  server.on("/api/mood/poke", HTTP_POST, handleApiMoodPoke);
  server.on("/api/mood/calm", HTTP_POST, handleApiMoodCalm);
  server.on("/api/personality/preset", HTTP_POST, handleApiPersonalityPreset);
  server.on("/api/memory", HTTP_GET, handleApiMemoryGet);
  server.on("/api/memory/save", HTTP_POST, handleApiMemorySave);
  server.on("/api/memory/reset", HTTP_POST, handleApiMemoryReset);
  server.on("/api/memory/forgive", HTTP_POST, handleApiMemoryForgive);
  server.on("/api/memory/annoy", HTTP_POST, handleApiMemoryAnnoy);
  server.on("/api/memory/praise", HTTP_POST, handleApiMemoryPraise);
  server.on("/api/memory/decay", HTTP_POST, handleApiMemoryDecay);
  server.on("/api/memory/attention", HTTP_POST, handleApiMemoryAttention);
  server.on("/api/memory/calm", HTTP_POST, handleApiMemoryCalm);
  server.on("/api/memory/bore", HTTP_POST, handleApiMemoryBore);
  server.on("/api/memory/wake", HTTP_POST, handleApiMemoryWake);
  server.on("/api/memory/rollover", HTTP_POST, handleApiMemoryRollover);
  server.on("/api/memory/clear-today", HTTP_POST, handleApiMemoryClearToday);
  server.on("/api/sass/test", HTTP_POST, handleApiSassTest);
  server.on("/api/sass/judgment", HTTP_POST, handleApiSassJudgment);
  server.on("/api/sass/grievance", HTTP_POST, handleApiSassGrievance);
  server.on("/api/sass/random", HTTP_POST, handleApiSassRandom);
  server.on("/api/sass/clear", HTTP_POST, handleApiSassClear);
  server.on("/api/sequence", HTTP_POST, handleApiSequencePost);
  server.on("/api/sequence/stop", HTTP_POST, handleApiSequenceStop);
  server.on("/api/action", HTTP_POST, handleApiAction);
  server.on("/api/config", HTTP_POST, handleApiConfig);

  server.on("/save", HTTP_POST, handleSaveWifi);
  server.on("/forget", HTTP_POST, handleForgetWifi);

  server.on("/generate_204", HTTP_GET, redirectToPortal);
  server.on("/hotspot-detect.html", HTTP_GET, redirectToPortal);
  server.on("/fwlink", HTTP_GET, redirectToPortal);
  server.on("/connecttest.txt", HTTP_GET, redirectToPortal);
  server.on("/redirect", HTTP_GET, redirectToPortal);

  server.on("/favicon.ico", HTTP_GET, []() {
    server.send(204, "text/plain", "");
  });

  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("HTTP server started.");
}

// =====================================================
// WIFI MODES
// =====================================================

bool loadSavedWifi() {
  prefs.begin("wifi", true);
  savedSsid = prefs.getString("ssid", "");
  savedPassword = prefs.getString("password", "");
  prefs.end();

  savedSsid.trim();

  return savedSsid.length() > 0;
}

bool connectSavedWifi() {
  if (!loadSavedWifi()) {
    Serial.println("No saved Wi-Fi credentials found.");
    return false;
  }

  Serial.println();
  Serial.println("Saved Wi-Fi credentials found.");
  Serial.print("Connecting to SSID: ");
  Serial.println(savedSsid);

  oledBootStatus(
    "Yeti Wi-Fi",
    "Connecting to:",
    clipText(savedSsid, 21),
    "Please wait",
    "Timeout: 20 sec",
    ""
  );

  setupMode = false;

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);  // Keep WebUI/setup responses snappy; avoid modem-sleep lag.
  WiFi.setHostname(configuredHostname.c_str());
  WiFi.begin(savedSsid.c_str(), savedPassword.c_str());

  unsigned long startAttempt = millis();

  int lastShownSecond = -1;

  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 20000) {
    delay(500);
    Serial.print(".");

    int elapsedSecond = (millis() - startAttempt) / 1000;

    if (elapsedSecond != lastShownSecond && elapsedSecond % 3 == 0) {
      lastShownSecond = elapsedSecond;

      oledStatus(
        "Yeti Wi-Fi",
        "Connecting to:",
        clipText(savedSsid, 21),
        "Elapsed: " + String(elapsedSecond) + " sec",
        "Still trying",
        ""
      );
    }
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Wi-Fi connected.");
    addEvent("WiFi", String("Connected: ") + WiFi.localIP().toString());
    lastWifiConnectedState = true;
    lastWifiRssi = WiFi.RSSI();
    handleYetiEvent(EVENT_WIFI_CONNECTED);
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());

    Serial.print("Dashboard: http://");
    Serial.print(WiFi.localIP());
    Serial.println("/");

    oledBootStatus(
      "Yeti Online",
      "Connected",
      "IP: " + WiFi.localIP().toString(),
      "Starting WebUI",
      String("Host: ") + configuredHostname + ".local",
      ""
    );

    beginConfiguredMdns();

    return true;
  }

  Serial.println("Failed to connect using saved Wi-Fi credentials.");
  addEvent("WiFi", "Saved connection failed");
  handleYetiEvent(EVENT_WIFI_FAILED);
  Serial.println("Starting setup portal instead.");

  oledBootStatus(
    "Yeti Wi-Fi",
    "Connection failed",
    "Starting setup",
    "portal",
    "",
    ""
  );

  WiFi.disconnect(true);
  delay(500);

  return false;
}

void startSetupPortal() {
  setupMode = true;
  setupApName = "Yeti-Setup-" + chipSuffix();

  Serial.println();
  Serial.println("Starting first-run setup portal.");
  addEvent("WiFi", "Setup portal starting");
  Serial.print("Setup AP SSID: ");
  Serial.println(setupApName);

  oledBootStatus(
    "Yeti Setup Mode",
    "Starting AP:",
    setupApName,
    "IP: 192.168.4.1",
    String("Name: ") + configuredHostname,
    "Open setup"
  );

  WiFi.mode(WIFI_AP_STA);
  WiFi.setSleep(false);  // Better captive portal responsiveness on ESP32-C3.
  WiFi.setHostname(configuredHostname.c_str());
  WiFi.softAPConfig(setupIP, setupGateway, setupSubnet);

  bool apStarted = false;

  if (strlen(SETUP_AP_PASSWORD) >= 8) {
    apStarted = WiFi.softAP(setupApName.c_str(), SETUP_AP_PASSWORD);
    Serial.println("Setup AP security: password enabled.");
  } else {
    apStarted = WiFi.softAP(setupApName.c_str());
    Serial.println("Setup AP security: open network.");
  }

  if (!apStarted) {
    Serial.println("ERROR: Failed to start setup AP.");

    oledStatus(
      "Yeti AP Failed",
      "Could not start",
      "setup hotspot",
      "Check Serial",
      "",
      ""
    );

    return;
  }

  delay(300);

  Serial.print("Setup portal IP: ");
  Serial.println(WiFi.softAPIP());
  Serial.println("Connect from phone/laptop, then open:");
  Serial.println("http://192.168.4.1");

  dnsServer.start(DNS_PORT, "*", setupIP);
  Serial.println("Captive portal DNS started.");
  addEvent("WiFi", String("Setup AP online: ") + WiFi.softAPIP().toString());

  oledBootStatus(
    "Yeti AP Online",
    "SSID:",
    setupApName,
    "IP: " + WiFi.softAPIP().toString(),
    "Portal loading",
    "Web server next"
  );
}

// =====================================================
// SERIAL COMMANDS
// =====================================================

void printHelp() {
  Serial.println();
  Serial.println("Yeti v1.7.4 Serial Commands:");
  Serial.println("  n = deadpan mood");
  Serial.println("  a = angry mood");
  Serial.println("  s = sleepy mood");
  Serial.println("  h = happy mood");
  Serial.println("  u = curious mood");
  Serial.println("  k = startled mood");
  Serial.println("  o = smug mood");
  Serial.println("  p = poke YETI");
  Serial.println("  x = calm down / base mood");
  Serial.println("  b = blink");
  Serial.println("  r = random mood");
  Serial.println("  d = demo mode");
  Serial.println("  i = show IP/network info on OLED");
  Serial.println("  t = show clock on OLED");
  Serial.println("  w = refresh/show weather on OLED");
  Serial.println("  c = recalibrate touch");
  Serial.println();

  Serial.print("MPU ready: ");
  Serial.println(yesNo(mpuReady));

  Serial.print("Touch baseline: ");
  Serial.println(touchBaseline);

  Serial.print("Touch threshold: ");
  Serial.println(touchThreshold);

  Serial.print("Last touch value: ");
  Serial.println(lastTouchValue);

  Serial.print("Face style: ");
  Serial.println(faceThemeName(faceTheme));

  Serial.println();
}

void handleSerial() {
  if (!Serial.available()) {
    return;
  }

  char c = Serial.read();

  if (c == '\n' || c == '\r') {
    return;
  }

  switch (c) {
    case 'n':
    case 'N':
      setMood(MOOD_DEADPAN, 0, PRIORITY_MANUAL, true);
      break;

    case 'a':
    case 'A':
      setMood(MOOD_ANGRY, 0, PRIORITY_MANUAL, true);
      break;

    case 's':
    case 'S':
      setMood(MOOD_SLEEPY, 0, PRIORITY_MANUAL, true);
      break;

    case 'h':
    case 'H':
      setMood(MOOD_HAPPY, 0, PRIORITY_MANUAL, true);
      break;

    case 'u':
    case 'U':
      setMood(MOOD_CURIOUS, 8000, PRIORITY_MANUAL, true);
      break;

    case 'k':
    case 'K':
      setMood(MOOD_STARTLED, 5000, PRIORITY_MANUAL, true);
      break;

    case 'o':
    case 'O':
      setMood(MOOD_SMUG, 8000, PRIORITY_MANUAL, true);
      break;

    case 'p':
    case 'P':
      triggerPokeYeti();
      break;

    case 'x':
    case 'X':
      calmDownYeti();
      break;

    case 'b':
    case 'B':
      startBlink();
      markInteraction();
      break;

    case 'r':
    case 'R':
      setMood(randomMood(), 8000, PRIORITY_MANUAL, true);
      break;

    case 'd':
    case 'D':
      startDemoMode();
      break;

    case 'i':
    case 'I': {
      String ipLine = WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : WiFi.softAPIP().toString();
      showTemporaryOledStatus(
        "Yeti Network",
        WiFi.status() == WL_CONNECTED ? "Station mode" : "Setup/AP mode",
        String("IP: ") + ipLine,
        String("Host: ") + configuredHostname + ".local",
        "SSID:",
        WiFi.status() == WL_CONNECTED ? clipText(WiFi.SSID(), 21) : clipText(setupApName, 21),
        7000
      );
      addEvent("OLED", "Displayed IP address");
      break;
    }

    case 't':
    case 'T':
      showClockCard();
      break;

    case 'w':
    case 'W':
      showWeatherCard();
      break;

    case 'c':
    case 'C':
      calibrateTouch();
      break;

    default:
      printHelp();
      break;
  }
}

// =====================================================
// ARDUINO SETUP / LOOP
// =====================================================

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("Yeti v1.7.4-sleep-preview-compile-fix");
  Serial.println("Starting firmware...");
  addEvent("Boot", "Firmware started");

  printHardwareConfig();

  randomSeed(esp_random());
  lastEngagementMs = millis();
  lastInteractionMs = millis();

  Wire.begin(YETI_I2C_SDA_PIN, YETI_I2C_SCL_PIN);
  Wire.setClock(YETI_I2C_CLOCK_HZ);

  loadBehaviorConfig();
  loadMemorySettings();

  initOLED();
  initFaceEngine();
  handleYetiEvent(EVENT_BOOT);

  showBootHardwareScreen();

  mpuReady = initMPU();

  calibrateTouch();

  showBootSensorScreen();

  scheduleNextLook();
  scheduleNextBlink();
  scheduleNextExpression();

  bool connected = connectSavedWifi();

  if (!connected) {
    startSetupPortal();
  }

  setupWebServer();

  showBootReadyScreen();

  printHelp();
}

void loop() {
  unsigned long now = millis();

  handleSerial();

  if (setupMode) {
    dnsServer.processNextRequest();
  }

  server.handleClient();

  ensureClockStarted();
  updateWeatherIfNeeded();
  updateSleepSystem();
  updateSleepAnimationPreview();

  bool sleepRenderBlocked = sleepRenderBlockActive();

  updateMotion();
  updateTouch();

  if (!sleepRenderBlocked) {
    updateWifiMoodReactions();
    updateInfoCards();
  }

#if YETI_FACE_ENGINE == YETI_FACE_ENGINE_CLASSIC
  if (!sleepRenderBlocked) {
    updateEyeMovement();
    updateBlinking();
  }
#endif
  if (!sleepRenderBlocked) {
    updateDemoMode();
    updateMoodEngine();
    updateExpression();
    updateIdleBehavior();
    updateSassSystem();
    updateOledOverlay();
  }

#if YETI_FACE_ENGINE == YETI_FACE_ENGINE_ROBOEYES
  // RoboEyes owns the animated face drawing and has its own framerate limiter.
  // We still skip face updates while large readable overlay cards are active.
  if (!sleepRenderBlocked) {
    updateFaceEngine();
  }
#else
  // OLED refresh is intentionally rate-limited. display.display() blocks while
  // the full framebuffer is sent over I2C, so drawing every loop makes Wi-Fi
  // and the setup portal feel laggy on the ESP32-C3. v1.2.0 exposes this as
  // a saved WebUI setting because every tiny board has its own gremlin budget.
  if (!sleepRenderBlocked && !oledOverlayActive && now - lastFaceDraw >= faceFrameMs) {
    lastFaceDraw = now;
    uint32_t renderStartUs = micros();
    drawFace();
    lastFaceRenderMs = (micros() - renderStartUs + 999) / 1000;
  }
#endif

  if (!setupMode && WiFi.status() != WL_CONNECTED) {
    if (now - lastReconnectAttempt > 10000) {
      lastReconnectAttempt = now;

      Serial.println("Wi-Fi disconnected. Trying to reconnect...");
      addEvent("WiFi", "Reconnect attempt");

      oledStatus(
        "Yeti Wi-Fi Lost",
        "Reconnecting",
        "Please wait",
        "",
        "",
        ""
      );

      WiFi.disconnect();
      WiFi.setHostname(configuredHostname.c_str());
      WiFi.begin(savedSsid.c_str(), savedPassword.c_str());
    }
  }

  updateMemorySystem();

  delay(YETI_LOOP_IDLE_DELAY_MS);
}
