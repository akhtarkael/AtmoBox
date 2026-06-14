#include <Wire.h>
#include <U8g2lib.h>
#include <OneButton.h>
#include <DHT.h>
#include <math.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <time.h>
#include <UniversalTelegramBot.h>
#include <HTTPClient.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <Preferences.h>

// ===================== KREDENSIAL WiFi =====================
#define WIFI_SSID   "Lutfi_1"
#define WIFI_PASS   "sifu_123"

// ===================== KREDENSIAL BOT =====================
#define BOT_TOKEN   "8721794179:AAE7Q8tcVWbLpxxlj10HVSbEPvZL7FsehK8"
#define CHAT_ID     "1360961353"

// ===================== OPENWEATHERMAP API =====================
#define OWM_API_KEY  "6704ee9157d7a5f00c9d381ac0f1debc"
#define OWM_CITY     "Surabaya,id"
String owmWeather    = "Memuat...";
float  owmTemp       = 0.0;
unsigned long lastWeatherMs = 0;
const unsigned long WEATHER_INTERVAL = 600000UL;

// ===================== PIN ESP32-C3 =====================
#define MQ135_PIN    2
#define I2C_SDA_PIN  5
#define I2C_SCL_PIN  4
#define BUZZER_PIN  10
#define BUTTON_PIN   1
#define DHT_PIN      3

// ===================== OBJEK HARDWARE =====================
#define DHT_TYPE DHT11
DHT dht(DHT_PIN, DHT_TYPE);

#define SCREEN_W 128
#define SCREEN_H  64
// [FIX FPS] Ganti SW_I2C -> HW_I2C agar oled.sendBuffer() jauh lebih cepat
// SW_I2C ~100kHz = 80-100ms/frame (max ~10fps)
// HW_I2C 800kHz  = ~10ms/frame   (max ~30fps)
U8G2_SH1106_128X64_NONAME_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE);

OneButton btn(BUTTON_PIN, true, true);

WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);

Preferences prefs;

// ===================== KONSTANTA =====================
#define INTERVAL_BACA     2000
#define BUFFER_SIZE       60
#define AUTO_SLEEP_MS     25000
#define LOG_INTERVAL_MS   30000
#define LOG_FILE          "/datalog.csv"
#define CALIB_FILE_KEY    "ro_value"
#define MAX_LOG_SIZE      80000

// ===================== AQI / ALERT 4 LEVEL =====================
#define AQI_BAIK      400
#define AQI_SEDANG    700
#define AQI_WASPADA   1000
#define AQI_BAHAYA    1500

const char* aqiLabel[] = { "BAIK", "SEDANG", "WASPADA", "BAHAYA!" };
const char* aqiEmoji[] = { "(OK)", "(!!)", "(!?)", "(!!!!)" };

int getAQILevel(float ppm) {
  if (ppm < AQI_BAIK)    return 0;
  if (ppm < AQI_SEDANG)  return 1;
  if (ppm < AQI_WASPADA) return 2;
  return 3;
}

// ===================== NADA =====================
#define NOTE_E4  330
#define NOTE_F4  349
#define NOTE_FS4 370
#define NOTE_G4  392
#define NOTE_GS4 415
#define NOTE_A4  440
#define NOTE_AS4 466
#define NOTE_B4  494
#define NOTE_C5  523
#define NOTE_CS5 554
#define NOTE_D5  587
#define NOTE_DS5 622
#define NOTE_E5  659
#define NOTE_F5  698
#define NOTE_FS5 740
#define NOTE_G5  784
#define NOTE_GS5 831
#define NOTE_A5  880
#define NOTE_AS5 932
#define NOTE_B5  988
#define NOTE_C6  1047
#define NOTE_CS6 1109
#define NOTE_D6  1175
#define NOTE_DS6 1245
#define NOTE_E6  1319
#define NOTE_F6  1397
#define NOTE_FS6 1480
#define NOTE_G6  1568
#define NOTE_GS6 1661
#define NOTE_A6  1760
#define NOTE_AS6 1865
#define NOTE_B6  1976
#define NOTE_C7  2093
#define NOTE_CS7 2217
#define NOTE_D7  2349
#define NOTE_DS7 2489
#define NOTE_E7  2637
#define NOTE_F7  2794
#define NOTE_FS7 2960
#define NOTE_G7  3136
#define NOTE_GS7 3322
#define NOTE_A7  3520
#define NOTE_AS7 3729
#define NOTE_B7  3951

// ===== SUPER MARIO BROS OVERWORLD THEME =====
const int marioTempo = 200;
const int marioWholenote = (60000 * 4) / marioTempo;

const int marioMelody[] = {
  NOTE_E5,8, NOTE_E5,8, 0,8, NOTE_E5,8, 0,8, NOTE_C5,8, NOTE_E5,8, 0,8, NOTE_G5,4, 0,4, NOTE_G4,4, 0,4,
  NOTE_C5,4, 0,8, NOTE_G4,4, 0,8, NOTE_E4,4, 0,8, NOTE_A4,4, NOTE_B4,4, NOTE_AS4,8, NOTE_A4,4,
  NOTE_G4,8, NOTE_E5,8, NOTE_G5,8, NOTE_A5,4, NOTE_F5,8, NOTE_G5,8, 0,8, NOTE_E5,4, NOTE_C5,8, NOTE_D5,8, NOTE_B4,4, 0,4,
  NOTE_C5,4, 0,8, NOTE_G4,4, 0,8, NOTE_E4,4, 0,8, NOTE_A4,4, NOTE_B4,4, NOTE_AS4,8, NOTE_A4,4,
  NOTE_G4,8, NOTE_E5,8, NOTE_G5,8, NOTE_A5,4, NOTE_F5,8, NOTE_G5,8, 0,8, NOTE_E5,4, NOTE_C5,8, NOTE_D5,8, NOTE_B4,4, 0,4,
  0,4, NOTE_G5,8, NOTE_FS5,8, NOTE_F5,8, NOTE_DS5,4, NOTE_E5,8, 0,8, NOTE_GS4,8, NOTE_A4,8, NOTE_C5,8, 0,8, NOTE_A4,8, NOTE_C5,8, NOTE_D5,8,
  0,4, NOTE_G5,8, NOTE_FS5,8, NOTE_F5,8, NOTE_DS5,4, NOTE_E5,8, 0,8, NOTE_C6,8, 0,8, NOTE_C6,8, NOTE_C6,4, 0,4
};
#define MARIO_LEN (sizeof(marioMelody) / sizeof(marioMelody[0]) / 2)

// ===================== STATE & GLOBAL =====================
enum State { BOOTING, IDLE_SENSING, ALERT_MODE, CHECK_MODE, GAME_MODE };
State currentState = BOOTING;
enum FaceType { FACE_SMILE, FACE_HOT, FACE_MASK, FACE_NONE };
FaceType currentFace = FACE_NONE;

// ===================== KALMAN FILTER =====================
float kf_x = 0.0f, kf_p = 1.0f, kf_q = 2.0f, kf_r = 5.0f,  kf_k = 0.0f; // [FIX] kf_r 20→5 respons lebih cepat

float kalmanUpdate(float measurement) {
  kf_p = kf_p + kf_q;
  kf_k = kf_p / (kf_p + kf_r);
  kf_x = kf_x + kf_k * (measurement - kf_x);
  kf_p = (1.0f - kf_k) * kf_p;
  return kf_x;
}

// ===================== RING BUFFER =====================
float ppmBuffer[BUFFER_SIZE]          = {0};
float tempBuffer[BUFFER_SIZE]         = {0};
float humBuffer[BUFFER_SIZE]          = {0};
unsigned long timeBuffer[BUFFER_SIZE] = {0};
int  bufferIndex = 0;
bool bufferFull  = false;

// ===================== SENSOR VALUES =====================
float currentPPM   = 0;
float currentTemp  = 0;
float currentHum   = 0;
float predictedPPM = 0;
bool  dht_ok       = false;
float sensorRO = 40000.0f;

// ===================== KONTROL =====================
bool showDataPage   = false;
bool isIdleMode     = false;
bool forceRedraw    = true;
bool silentMode     = false;
bool sendStartupMsg = false;
bool ntpSynced          = false;
unsigned long lastNtpRetryMs    = 0;
unsigned long lastWifiCheckMs   = 0;

// [FIX NONBLOCK] Semua operasi network (Telegram, Weather) dijalankan
// di FreeRTOS task terpisah agar main loop tidak pernah blocking
volatile bool netBusy           = false;
volatile bool doSendStartup     = false;
volatile bool doFetchWeather    = false;
volatile bool doSendTelegram    = false;
volatile bool doCheckTelegram   = false;
volatile bool doReconnectWiFi   = false;  // [FIX] trigger reconnect WiFi di networkTask
volatile bool doNtpSync         = false;  // [FIX] trigger NTP sync di networkTask
String        netSendMsg        = "";
unsigned long lastTelegramQueueMs = 0;

unsigned long lastActivityTime  = 0;
unsigned long lastTelegramAlertMs        = 0;
unsigned long lastLogMs                  = 0;
unsigned long lastTelegramCheckMs        = 0;
unsigned long lastSensorMs               = 0;

// ===========================================================
// ★ FLAPPY BIRD - VARIABEL (ENHANCED V2)
// ===========================================================
// [FIX LAYOUT] Arena lebih kecil (skor bar + ground lebih tipis)
// supaya ruang bermain lebih leluasa dan burung tidak start terlalu bawah
#define GAME_GROUND_Y    (SCREEN_H - 6)   // ground lebih tipis (was -9)
#define GAME_PIPE_W      8                // pipa sedikit lebih tipis (was 10)
#define GAME_GAP_H       26               // celah lebih lebar (was 22)
#define GAME_PIPE_SPACE  72               // jarak antar pipa (was 76)

// Burung — start di tengah area bermain yang tersedia
float birdY     = 28.0f;  // [FIX LAYOUT] lebih ke atas dari 27
float birdV     = 0.0f;
int   birdWing  = 0;
unsigned long lastWingMs = 0;

// Pipa 1
float pipeX    = 128.0f; // [FIX PHYSICS] float agar delta time smooth
int   pipeGapY = 27;

// Pipa 2
float pipe2X    = 128.0f + GAME_PIPE_SPACE; // [FIX PHYSICS] float
int   pipe2GapY = 27;

// Skor & state
int   gameScore     = 0;
int   gameHighScore = 0;
int   pipeSpeed     = 3;
bool  gameOver      = false;
bool  gameStarted   = false;
bool  gameNewRecord = false;
unsigned long gameToneEndMs    = 0; // [FIX FPS] tone non-blocking di game
unsigned long lastGamePhysicsMs = 0; // [FIX PHYSICS] delta time untuk physics
int   groundOffset  = 0;

bool gameJump = false;

#define PIPE_SPEED_MIN   3
#define PIPE_SPEED_MAX   6

// [FIX PHYSICS] Konstanta physics dalam satuan pixel/detik
// Dulu: pixel/frame (bergantung FPS). Sekarang: pixel/detik (konsisten di FPS berapapun)
#define GRAVITY_PPS      120.0f   // [FIX2] diperlambat lagi agar tidak terlalu berat (was 220)
#define JUMP_VEL        -110.0f   // [FIX4] dikurangi agar tidak teleport ke atas (was -190)
#define PIPE_VEL_MIN     50.0f    // pixel/detik kecepatan pipa awal
#define PIPE_VEL_STEP    10.0f    // tambahan kecepatan per level
#define SCORE_PER_LEVEL  5

// ===========================================================
// FORWARD DECLARATIONS
// ===========================================================
void sendTelegramMessage(String msg);
void runAutoCalibration();

// ===========================================================
// HELPER
// ===========================================================
void _tone(uint32_t freq) { if (freq > 0 && !silentMode) tone(BUZZER_PIN, freq); }
void _noTone() { noTone(BUZZER_PIN); }

void smartDelay(unsigned long ms) {
  unsigned long start = millis();
  while (millis() - start < ms) { btn.tick(); delay(10); }
}

void printCentered(int y, const char* txt, bool inverted = false) {
  int w = oled.getStrWidth(txt);
  int x = (SCREEN_W - w) / 2;
  if (x < 0) x = 0;
  if (inverted) {
    int h = oled.getMaxCharHeight();
    oled.setDrawColor(1); oled.drawBox(x-2, y-h, w+4, h+2); oled.setDrawColor(0);
  } else { oled.setDrawColor(1); }
  oled.drawStr(x, y, txt); oled.setDrawColor(1);
}

// ===========================================================
// SPIFFS LOGGING
// ===========================================================
void spiffsInit() {
  if (!SPIFFS.begin(true)) {
    Serial.println("[SPIFFS] Mount gagal, format...");
    SPIFFS.format(); SPIFFS.begin(true);
  }
  if (!SPIFFS.exists(LOG_FILE)) {
    File f = SPIFFS.open(LOG_FILE, FILE_WRITE);
    if (f) { f.println("timestamp,ppm,temp,hum,aqi"); f.close(); }
  }
  Serial.printf("[SPIFFS] Total:%dKB Used:%dKB\n",
    SPIFFS.totalBytes()/1024, SPIFFS.usedBytes()/1024);
}

void logDataToSPIFFS() {
  if (SPIFFS.exists(LOG_FILE)) {
    File check = SPIFFS.open(LOG_FILE, FILE_READ);
    size_t sz = check.size(); check.close();
    if (sz > MAX_LOG_SIZE) {
      SPIFFS.remove(LOG_FILE);
      File f = SPIFFS.open(LOG_FILE, FILE_WRITE);
      if (f) { f.println("timestamp,ppm,temp,hum,aqi"); f.close(); }
    }
  }
  File f = SPIFFS.open(LOG_FILE, FILE_APPEND);
  if (!f) return;
  struct tm ti; char ts[20] = "0";
  if (getLocalTime(&ti))
    sprintf(ts, "%04d-%02d-%02d %02d:%02d:%02d",
      ti.tm_year+1900, ti.tm_mon+1, ti.tm_mday,
      ti.tm_hour, ti.tm_min, ti.tm_sec);
  f.printf("%s,%.1f,%.1f,%.0f,%s\n",
    ts, (float)currentPPM, (float)currentTemp, (float)currentHum,
    aqiLabel[getAQILevel(currentPPM)]);
  f.close();
}

String getLogSummary() {
  File f = SPIFFS.open(LOG_FILE, FILE_READ);
  if (!f) return "Log kosong.";
  size_t sz = f.size();
  String result = "";
  if (sz > 512) f.seek(sz - 512);
  while (f.available()) result += (char)f.read();
  f.close();
  int nl = 0, idx = result.length() - 1;
  while (idx >= 0 && nl < 10) { if (result[idx] == '\n') nl++; idx--; }
  return result.substring(idx + 2);
}

// ===========================================================
// KALIBRASI
// ===========================================================
void loadCalibration() {
  prefs.begin("atmobadge", false);
  sensorRO       = prefs.getFloat(CALIB_FILE_KEY, 40000.0f);
  gameHighScore  = prefs.getInt("hiscore", 0);
  prefs.end();
  Serial.printf("[CALIB] RO: %.1f  |  High Score: %d\n", sensorRO, gameHighScore);
}

void saveHighScore() {
  prefs.begin("atmobadge", false);
  prefs.putInt("hiscore", gameHighScore);
  prefs.end();
}

void runAutoCalibration() {
  oled.clearBuffer(); oled.setFont(u8g2_font_5x8_tr);
  printCentered(20, "Kalibrasi RO...");
  printCentered(32, "Udara harus bersih!");
  printCentered(44, "Jangan gerakkan alat");
  oled.sendBuffer();
  smartDelay(5000);

  long sumMV = 0;
  for (int i = 0; i < 50; i++) { btn.tick(); sumMV += analogReadMilliVolts(MQ135_PIN); delay(20); }
  float voltage = (sumMV / 50.0f) / 1000.0f;
  if (voltage < 0.1f) voltage = 0.1f;

  float RS_clean = 10000.0f * (3.3f - voltage) / voltage;
  if (RS_clean > 5000.0f && RS_clean < 200000.0f) {
    sensorRO = RS_clean;
    prefs.begin("atmobadge", false);
    prefs.putFloat(CALIB_FILE_KEY, sensorRO);
    prefs.end();
    char buf[40]; sprintf(buf, "RO=%.0f tersimpan", sensorRO);
    oled.clearBuffer(); printCentered(32, buf); oled.sendBuffer();
    Serial.printf("[CALIB] RO baru: %.1f\n", sensorRO);
    sendTelegramMessage("Kalibrasi selesai. RO = " + String(sensorRO, 0));
    delay(2000);
  } else {
    oled.clearBuffer(); printCentered(32, "Kalibrasi gagal!"); oled.sendBuffer();
    delay(2000);
  }
}

// ===========================================================
// TELEGRAM
// ===========================================================
// [FIX NONBLOCK] Tidak langsung kirim — masuk ke queue,
// dieksekusi oleh networkTask di background
void sendTelegramMessage(String msg) {
  if (WiFi.status() != WL_CONNECTED) return;
  netSendMsg    = msg;
  doSendTelegram = true;
}

// handleTelegramCommands() sudah dipindah ke networkTask background
// Fungsi ini dikosongkan agar tidak ada blocking di main loop
void handleTelegramCommands() { }

// ===========================================================
// CUACA - OpenWeatherMap (pakai API key yang sudah aktif)
// Parsing pakai ArduinoJson agar robust
// ===========================================================
void fetchWeather() {
  if (WiFi.status() != WL_CONNECTED) { owmWeather = "No WiFi"; return; }

  WiFiClient client;
  client.setTimeout(5);    // [FIX] satuan detik, bukan ms — 5 detik cukup
  HTTPClient http;
  http.setTimeout(5000);   // [FIX] 5 detik, lebih cepat fail daripada nunggu 10 detik

  String url = "http://api.openweathermap.org/data/2.5/weather?q="
               + String(OWM_CITY)
               + "&appid=" + String(OWM_API_KEY)
               + "&units=metric";

  Serial.println("[OWM] Fetching: " + url);

  if (!http.begin(client, url)) {
    Serial.println("[OWM] http.begin() gagal");
    owmWeather = "Gagal"; return;
  }

  int code = http.GET();
  Serial.printf("[OWM] HTTP code: %d\n", code);

  if (code == HTTP_CODE_OK) {
    String payload = http.getString();
    Serial.println("[OWM] Payload: " + payload.substring(0, 200));

    // Parse pakai ArduinoJson — lebih robust dari indexOf manual
    StaticJsonDocument<1024> doc;
    DeserializationError err = deserializeJson(doc, payload);

    if (!err) {
      owmTemp = doc["main"]["temp"].as<float>();
      String w = doc["weather"][0]["main"].as<String>();

      if      (w == "Clear")        owmWeather = "Cerah";
      else if (w == "Clouds")       owmWeather = "Berawan";
      else if (w == "Rain")         owmWeather = "Hujan";
      else if (w == "Drizzle")      owmWeather = "Gerimis";
      else if (w == "Thunderstorm") owmWeather = "Badai";
      else if (w == "Snow")         owmWeather = "Salju";
      else if (w == "Mist")         owmWeather = "Kabut";
      else if (w == "Fog")          owmWeather = "Kabut";
      else if (w == "Haze")         owmWeather = "Berkabut";
      else                          owmWeather = w;

      Serial.printf("[OWM] Cuaca: %s, Suhu: %.1f\n", owmWeather.c_str(), owmTemp);
    } else {
      Serial.printf("[OWM] JSON parse error: %s\n", err.c_str());
      owmWeather = "Gagal";
    }
  } else {
    owmWeather = "Gagal";
    Serial.printf("[OWM] HTTP error: %d\n", code);
  }

  http.end();
}

// ===========================================================
// SENSOR UPDATE
// ===========================================================
void updateHardwareSensor() {
  unsigned long now = millis();
  if (now - lastSensorMs < INTERVAL_BACA) return;
  lastSensorMs = now;

  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (!isnan(t) && !isnan(h)) { currentTemp = t; currentHum = h; dht_ok = true; }
  else dht_ok = false;

  long sumMV = 0;
  for (int i = 0; i < 10; i++) { btn.tick(); sumMV += analogReadMilliVolts(MQ135_PIN); }
  float voltage = (sumMV / 10.0f) / 1000.0f;
  if (voltage < 0.1f) voltage = 0.1f;

  float RS = 10000.0f * (3.3f - voltage) / voltage;
  if (RS <= 0) RS = 1;

  float ratio = RS / sensorRO;
  float T = constrain((float)currentTemp, 10.0f, 45.0f);
  float H = constrain((float)currentHum, 20.0f, 90.0f);
  float corr = (-0.00035f*(T*T)) + (0.0202f*T) + (-0.000833f*(H*H)) + (-0.02718f*H) + 1.39538f;
  if (corr < 0.1f) corr = 0.1f;
  float rC = ratio / corr;

  float ppmRaw = (rC > 0) ? 116.6020682f * powf(rC, -2.769034857f) : 0;
  ppmRaw = constrain(ppmRaw, 0, 5000);

  if (kf_x == 0) kf_x = ppmRaw;
  currentPPM = kalmanUpdate(ppmRaw);
  currentPPM = constrain(currentPPM, 0, 5000);

  ppmBuffer[bufferIndex]  = currentPPM;
  tempBuffer[bufferIndex] = currentTemp;
  humBuffer[bufferIndex]  = currentHum;
  timeBuffer[bufferIndex] = now / 1000;
  bufferIndex++;
  if (bufferIndex >= BUFFER_SIZE) { bufferIndex = 0; bufferFull = true; }

  if (bufferFull) {
    float sX=0, sY=0, sXY=0, sX2=0;
    unsigned long t0 = timeBuffer[bufferIndex];
    for (int i = 0; i < BUFFER_SIZE; i++) {
      float x = (float)(timeBuffer[i] - t0), y = ppmBuffer[i];
      sX+=x; sY+=y; sXY+=x*y; sX2+=x*x;
    }
    float d = BUFFER_SIZE*sX2 - sX*sX;
    if (fabsf(d) > 0.001f) {
      float m  = (BUFFER_SIZE*sXY - sX*sY) / d;
      float c2 = (sY - m*sX) / BUFFER_SIZE;
      predictedPPM = constrain(m*((now/1000.0f)+300.0f-t0)+c2, 0, 5000);
    }
  }

  if (now - lastLogMs > LOG_INTERVAL_MS) { lastLogMs = now; logDataToSPIFFS(); }

  int lv = getAQILevel(currentPPM);
  if (lv >= 2 && currentState == IDLE_SENSING) {
    currentState = ALERT_MODE; isIdleMode = false; lastActivityTime = now;
    if (now - lastTelegramAlertMs > 300000) {
      lastTelegramAlertMs = now;
      sendTelegramMessage(
        "*Peringatan " + String(aqiLabel[lv]) + "!*\n"
        "Gas: `" + String(currentPPM, 0) + " PPM`\n"
        "Suhu: `" + String(currentTemp, 1) + " C`\n"
        "AQI Level: *" + String(lv) + "/3*"
      );
    }
  } else if (lv < 2 && currentState == ALERT_MODE) {
    currentState = IDLE_SENSING; forceRedraw = true;
  }
}

// ===========================================================
// WAJAH & UI
// ===========================================================
void drawSmileFace(int frame) {
  oled.drawRFrame(28,17,72,46,10); oled.drawRFrame(29,18,70,44,9);
  if (frame == 0) {
    oled.drawDisc(48,33,6); oled.drawDisc(80,33,6);
    oled.setDrawColor(0); oled.drawDisc(49,34,3); oled.drawDisc(81,34,3); oled.setDrawColor(1);
    oled.drawPixel(47,31); oled.drawPixel(79,31);
  } else {
    oled.drawLine(42,33,54,33); oled.drawLine(74,33,86,33);
    oled.drawLine(43,34,53,34); oled.drawLine(75,34,85,34);
    oled.drawPixel(48,35); oled.drawPixel(80,35);
  }
  oled.drawPixel(64,40); oled.drawPixel(63,41); oled.drawPixel(65,41);
  oled.drawPixel(42,50);oled.drawPixel(43,52);oled.drawPixel(45,54);oled.drawPixel(48,55);
  oled.drawPixel(52,57);oled.drawPixel(56,58);oled.drawPixel(60,58);oled.drawPixel(64,58);
  oled.drawPixel(68,57);oled.drawPixel(72,55);oled.drawPixel(75,54);oled.drawPixel(77,52);oled.drawPixel(78,50);
  oled.drawPixel(44,53);oled.drawPixel(47,56);oled.drawPixel(51,57);oled.drawPixel(55,58);
  oled.drawPixel(59,59);oled.drawPixel(64,59);oled.drawPixel(69,58);oled.drawPixel(73,56);oled.drawPixel(76,53);
  oled.drawPixel(33,43);oled.drawPixel(35,43);oled.drawPixel(34,45);
  oled.drawPixel(93,43);oled.drawPixel(95,43);oled.drawPixel(94,45);
}

void drawHotFace(int frame) {
  oled.drawRFrame(34,19,60,42,9); oled.drawRFrame(35,20,58,40,8);
  oled.drawLine(44,25,56,29);oled.drawLine(44,26,56,30);
  oled.drawLine(72,29,84,25);oled.drawLine(72,30,84,26);
  if (frame == 0) {
    oled.drawDisc(51,32,5); oled.drawDisc(79,32,5);
    oled.setDrawColor(0); oled.drawDisc(52,33,2); oled.drawDisc(80,33,2); oled.setDrawColor(1);
    oled.drawPixel(50,31); oled.drawPixel(78,31);
  } else {
    oled.drawDisc(51,32,5); oled.drawDisc(79,32,5);
    oled.setDrawColor(0); oled.drawBox(46,27,11,5); oled.drawBox(74,27,11,5); oled.setDrawColor(1);
  }
  oled.drawLine(52,52,76,52);oled.drawLine(51,51,52,52);oled.drawLine(76,52,77,51);
  oled.drawPixel(38,28);oled.drawPixel(38,29);oled.drawPixel(37,30);oled.drawPixel(38,30);oled.drawPixel(39,30);
}

void drawMaskFace(int frame) {
  oled.drawRFrame(34,19,60,42,9); oled.drawRFrame(35,20,58,40,8);
  oled.drawLine(44,27,56,24);oled.drawLine(44,28,56,25);
  oled.drawLine(72,24,84,27);oled.drawLine(72,25,84,28);
  if (frame == 0) {
    oled.drawDisc(51,32,5); oled.drawDisc(79,32,5);
    oled.setDrawColor(0); oled.drawDisc(51,32,2); oled.drawDisc(79,32,2); oled.setDrawColor(1);
    oled.drawPixel(52,31); oled.drawPixel(80,31);
  } else {
    oled.drawLine(46,32,56,32); oled.drawLine(74,32,84,32);
  }
  oled.drawRBox(42,44,44,12,3);
  oled.drawLine(42,45,36,40);oled.drawLine(86,45,92,40);
  oled.drawLine(42,54,36,58);oled.drawLine(86,54,92,58);
  oled.setDrawColor(0);
  oled.drawLine(43,49,85,49);oled.drawLine(43,53,85,53);oled.drawLine(58,46,70,46);
  oled.setDrawColor(1);
}

void drawFacePage(int frame) {
  oled.clearBuffer();
  struct tm timeinfo; char ck[6] = "--:--";
  if (getLocalTime(&timeinfo)) sprintf(ck, "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
  oled.setFont(u8g2_font_5x8_tr); oled.drawStr(2, 11, ck);

  int lv = getAQILevel(currentPPM);
  const char* lvl = aqiLabel[lv];
  if (lv >= 3) {
    oled.drawRBox(81,1,46,12,2); oled.setDrawColor(0);
    oled.drawStr(84,10,lvl); oled.setDrawColor(1);
  } else {
    oled.drawRFrame(81,1,46,12,2); oled.drawStr(84,10,lvl);
  }

  oled.drawDisc(64,11,1);
  if (WiFi.status() == WL_CONNECTED) {
    oled.drawPixel(61,9);oled.drawPixel(62,8);oled.drawPixel(64,7);oled.drawPixel(66,8);oled.drawPixel(67,9);
    oled.drawPixel(58,8);oled.drawPixel(59,6);oled.drawPixel(61,5);oled.drawPixel(64,4);
    oled.drawPixel(67,5);oled.drawPixel(69,6);oled.drawPixel(70,8);
  } else {
    oled.drawPixel(62,8);oled.drawPixel(66,8);
    oled.drawLine(62,4,66,8);oled.drawLine(66,4,62,8);
  }
  if (silentMode) oled.drawStr(73, 11, "S");
  oled.drawLine(0,14,SCREEN_W-1,14);

  if      (currentFace == FACE_MASK) drawMaskFace(frame);
  else if (currentFace == FACE_HOT)  drawHotFace(frame);
  else                                drawSmileFace(frame);

  oled.setFont(u8g2_font_5x8_tr);
  if (dht_ok) { char dl[18]; sprintf(dl,"%.1fC %.0f%%",currentTemp,currentHum); oled.drawStr(0,63,dl); }
  char pl[10]; sprintf(pl,"%.0fpm",currentPPM); oled.drawStr(96,63,pl);
  oled.sendBuffer();
}

void drawDataPage() {
  oled.clearBuffer(); oled.setFont(u8g2_font_5x8_tr);
  int hW = oled.getStrWidth(" SENSOR DATA ");
  int hX = (SCREEN_W - hW) / 2;
  oled.drawRBox(hX-1,1,hW+2,11,2);
  oled.setDrawColor(0); oled.drawStr(hX,10," SENSOR DATA "); oled.setDrawColor(1);
  oled.drawLine(0,14,SCREEN_W-1,14);
  int y = 25;
  if (dht_ok) {
    char l1[22]; sprintf(l1,"Suhu : %.1f C",currentTemp); oled.drawStr(2,y,l1); y+=10;
    char l2[22]; sprintf(l2,"Hum  : %.0f %%",currentHum); oled.drawStr(2,y,l2); y+=10;
  } else { oled.drawStr(2,y,"! DHT11: ERROR"); y+=20; }
  char l3[22]; sprintf(l3,"Gas  : %.0f PPM",currentPPM); oled.drawStr(2,y,l3); y+=10;
  if (bufferFull) { char l4[22]; sprintf(l4,"Pred : %.0f PPM",predictedPPM); oled.drawStr(2,y,l4); }
  else            { oled.drawStr(2,y,"Pred : menghitung.."); }
  int barY=56, barW=SCREEN_W-4;
  float pct = constrain(currentPPM/2000.0f, 0.0f, 1.0f);
  oled.drawRFrame(2,barY,barW,7,2);
  int fillW = (int)(pct*(barW-4));
  if (fillW > 0) oled.drawRBox(3,barY+1,fillW,5,1);
  oled.sendBuffer();
}

void drawAOD() {
  static unsigned long aodPulse = 0; static bool dotOn = true;
  if (millis()-aodPulse > 1000) { dotOn=!dotOn; aodPulse=millis(); }
  oled.clearBuffer(); oled.setFont(u8g2_font_5x8_tr);
  char weatherStr[32]; sprintf(weatherStr,"%s %.1fC",owmWeather.c_str(),owmTemp);
  oled.drawStr(0,8,weatherStr);
  int tW = oled.getStrWidth("ATMOBADGE"); oled.drawStr(SCREEN_W-tW,8,"ATMOBADGE");
  oled.drawLine(0,11,SCREEN_W,11);
  struct tm timeinfo; char th[3]="--"; char tm_str[3]="--";
  if (getLocalTime(&timeinfo)) { sprintf(th,"%02d",timeinfo.tm_hour); sprintf(tm_str,"%02d",timeinfo.tm_min); }
  oled.setFont(u8g2_font_logisoso24_tr);
  int hourW=oled.getStrWidth(th), minW=oled.getStrWidth(tm_str), colonW=oled.getStrWidth(":");
  int startX = (SCREEN_W - hourW - colonW - minW - 2) / 2;
  oled.drawStr(startX,40,th);
  if (dotOn) oled.drawStr(startX+hourW,38,":");
  oled.drawStr(startX+hourW+colonW+2,40,tm_str);
  oled.setFont(u8g2_font_5x8_tr); oled.drawLine(0,50,SCREEN_W,50);
  char info[32];
  if (dht_ok) sprintf(info,"%.1fC | %.0f PPM",currentTemp,currentPPM);
  else        sprintf(info,"Gas: %.0f PPM",currentPPM);
  int iW = oled.getStrWidth(info); oled.drawStr((SCREEN_W-iW)/2,61,info);
  oled.sendBuffer();
}

void drawBootScreen(int bootAngle, float pct) {
  oled.clearBuffer(); oled.setFont(u8g2_font_5x8_tr);
  oled.drawRBox(2,2,124,11,3);
  oled.setDrawColor(0); oled.drawStr(14,10,"  ATMOBADGE  V7.1  "); oled.setDrawColor(1);
  oled.drawLine(0,15,SCREEN_W,15);
  for (int i = 0; i < 9; i++) {
    float phase = (bootAngle + i*36) * M_PI / 180.0f;
    int barH = 4 + (int)(10.0f * fabsf(sinf(phase)));
    int bx = 8 + i*13, by = 36 - barH;
    oled.drawRBox(bx,by,9,barH,2);
  }
  int barX=10, barY=44, barMaxW=SCREEN_W-20;
  oled.drawRFrame(barX,barY,barMaxW,10,4);
  int fillW = (int)(pct*(barMaxW-4)); if (fillW>0) oled.drawRBox(barX+2,barY+2,fillW,6,3);
  char pstr[8]; sprintf(pstr,"%d%%",(int)(pct*100));
  const char* phases[] = {"Booting..","Konek WiFi..","Sync Jam..","Cek Cuaca..","Siap!"};
  int phaseIdx = (int)(pct*4); if (phaseIdx>4) phaseIdx=4;
  oled.drawStr(barX,62,phases[phaseIdx]);
  oled.drawStr(SCREEN_W-oled.getStrWidth(pstr)-barX,62,pstr);
  oled.sendBuffer();
}

// ===========================================================
// HELPER DRAW PIPA
// ===========================================================
void drawPipe(int px, int gapY) {
  if (px > SCREEN_W || px + GAME_PIPE_W + 2 < 0) return;

  int topH = gapY - GAME_GAP_H / 2;
  int botY  = gapY + GAME_GAP_H / 2;
  int botH  = GAME_GROUND_Y - botY;

  if (topH > 0) {
    oled.drawRBox(px, 0, GAME_PIPE_W, topH, 0);
    if (px >= 0 && px + GAME_PIPE_W + 2 <= SCREEN_W)
      oled.drawRBox(px - 2, topH - 5, GAME_PIPE_W + 4, 5, 2);
  }
  if (botH > 0) {
    oled.drawRBox(px, botY, GAME_PIPE_W, botH, 0);
    if (px >= 0 && px + GAME_PIPE_W + 2 <= SCREEN_W)
      oled.drawRBox(px - 2, botY, GAME_PIPE_W + 4, 5, 2);
  }
}

// ===========================================================
// DRAW BURUNG DENGAN ANIMASI SAYAP
// ===========================================================
void drawBird(int bx, int by, int wing) {
  // [FIX] burung lebih kecil: radius 3 (was 4)
  oled.drawDisc(bx + 2, by, 3);
  oled.setDrawColor(0);
  oled.drawPixel(bx + 4, by - 1);   // mata
  oled.setDrawColor(1);
  oled.drawBox(bx + 5, by - 1, 2, 2); // paruh lebih kecil

  if (wing == 1) {
    oled.drawLine(bx - 1, by - 1, bx - 3, by - 4);
    oled.drawLine(bx - 3, by - 4, bx - 1, by - 1);
  } else if (wing == -1) {
    oled.drawLine(bx - 1, by + 1, bx - 3, by + 4);
    oled.drawLine(bx - 3, by + 4, bx - 1, by + 2);
  } else {
    if (birdV < 0) {
      oled.drawLine(bx, by - 1, bx - 2, by - 3);
      oled.drawLine(bx - 2, by - 3, bx - 1, by - 1);
    } else {
      oled.drawLine(bx, by + 1, bx - 2, by + 3);
      oled.drawLine(bx - 2, by + 3, bx - 1, by + 1);
    }
  }
}

// ===========================================================
// DRAW GROUND
// ===========================================================
void drawGround() {
  oled.drawBox(0, GAME_GROUND_Y, SCREEN_W, 2);
  for (int x = (8 - groundOffset) % 8; x < SCREEN_W; x += 8) {
    oled.drawPixel(x,     GAME_GROUND_Y + 4);
    oled.drawPixel(x + 3, GAME_GROUND_Y + 6);
  }
}

// ===========================================================
// DRAW GAME FRAME
// ===========================================================
void drawGameFrame() {
  oled.clearBuffer();

  if (!gameStarted && !gameOver) {
    oled.setFont(u8g2_font_6x10_tr);
    int tw = oled.getStrWidth("FLAPPY BIRD");
    oled.drawStr((SCREEN_W - tw) / 2, 20, "FLAPPY BIRD");

    oled.setFont(u8g2_font_5x8_tr);
    int bx = SCREEN_W / 2 - 5;
    int by = 33 + (int)(3 * sinf(millis() / 300.0f));
    drawBird(bx, by, birdWing);

    if ((millis() / 500) % 2 == 0) {
      int mw = oled.getStrWidth("TAP UNTUK MAIN");
      oled.drawStr((SCREEN_W - mw) / 2, 55, "TAP UNTUK MAIN");
    }
    if (gameHighScore > 0) {
      char hs[20]; sprintf(hs, "BEST: %d", gameHighScore);
      int hw = oled.getStrWidth(hs);
      oled.drawStr((SCREEN_W - hw) / 2, 64, hs);
    }
    oled.sendBuffer();
    btn.tick(); // [FIX] wajib ada agar klik TAP UNTUK MAIN terdeteksi
    return;
  }

  drawGround();
  drawPipe(pipeX, pipeGapY);
  drawPipe(pipe2X, pipe2GapY);

  int bx = 16, by = (int)birdY;
  drawBird(bx, by, birdWing);

  oled.setFont(u8g2_font_6x10_tr);
  char sc[10]; sprintf(sc, "%d", gameScore);
  int scW = oled.getStrWidth(sc);
  oled.drawRBox(SCREEN_W / 2 - scW / 2 - 3, 1, scW + 6, 12, 2);
  oled.setDrawColor(0);
  oled.drawStr(SCREEN_W / 2 - scW / 2, 11, sc);
  oled.setDrawColor(1);

  oled.setFont(u8g2_font_5x8_tr);
  char lvStr[4]; sprintf(lvStr, "x%d", pipeSpeed - PIPE_SPEED_MIN + 1);
  oled.drawStr(SCREEN_W - oled.getStrWidth(lvStr) - 2, 8, lvStr);

  if (gameOver) {
    oled.drawRBox(16, 14, 96, 42, 4);
    oled.setDrawColor(0);

    oled.setFont(u8g2_font_6x10_tr);
    int goW = oled.getStrWidth("GAME OVER");
    oled.drawStr((SCREEN_W - goW) / 2, 27, "GAME OVER");

    oled.setFont(u8g2_font_5x8_tr);
    char ms[18]; sprintf(ms, "Skor: %d", gameScore);
    oled.drawStr((SCREEN_W - oled.getStrWidth(ms)) / 2, 38, ms);

    char hs[20];
    if (gameNewRecord)
      sprintf(hs, "REKOR BARU! :)");
    else
      sprintf(hs, "Best: %d", gameHighScore);
    oled.drawStr((SCREEN_W - oled.getStrWidth(hs)) / 2, 48, hs);

    oled.drawStr((SCREEN_W - oled.getStrWidth("Tap=Ulang")) / 2, 54, "Tap=Ulang");
    oled.setDrawColor(1);
  }

  oled.sendBuffer();
}

// ===========================================================
// RESET GAME STATE
// ===========================================================
void resetGameState() {
  birdY         = 32.0f; // [FIX4] tengah layar, jauh dari ceiling (was 18)
  birdV         = 0.0f;
  birdWing      = 0;
  pipeX         = SCREEN_W;
  pipeGapY      = random(GAME_GAP_H / 2 + 4, GAME_GROUND_Y - GAME_GAP_H / 2 - 4);
  pipe2X        = SCREEN_W + GAME_PIPE_SPACE;
  pipe2GapY     = random(GAME_GAP_H / 2 + 4, GAME_GROUND_Y - GAME_GAP_H / 2 - 4);
  gameScore     = 0;
  pipeSpeed     = PIPE_SPEED_MIN;
  gameOver      = false;
  gameStarted   = false;
  gameNewRecord = false;
  groundOffset       = 0;
  lastGamePhysicsMs  = 0; // [FIX PHYSICS] reset delta time
}

bool checkPipeCollision(int px, int gapY) {
  int bx = 16;
  int bRadius = 3; // [FIX] radius burung dikecilkan (was 4)
  if (bx + bRadius < px || bx - bRadius > px + GAME_PIPE_W) return false;
  int topH = gapY - GAME_GAP_H / 2;
  int botY  = gapY + GAME_GAP_H / 2;
  return ((int)birdY - bRadius < topH) || ((int)birdY + bRadius > botY);
}

// ===========================================================
// TOMBOL
// ===========================================================
void aksiKlik1() {
  lastActivityTime = millis();
  Serial.println("[BTN] KLIK 1x terdeteksi");

  if (currentState == GAME_MODE) {
    if (gameOver) {
      resetGameState();
    } else if (!gameStarted) {
      gameStarted = true;
      birdV = 0.0f; // [FIX4] mulai diam, biarkan gravity tarik dulu, tap berikutnya baru jump
      gameJump = false;
      _tone(NOTE_E5); gameToneEndMs = millis() + 30; // [FIX FPS] non-blocking
    } else {
      gameJump = true;
    }
    return;
  }

  if (isIdleMode) { isIdleMode = false; showDataPage = false; forceRedraw = true; return; } // [FIX] reset showDataPage agar balik ke emoji
  showDataPage = !showDataPage; forceRedraw = true;
}

void aksiKlik2() {
  lastActivityTime = millis();
  Serial.println("[BTN] KLIK 2x (double click) terdeteksi");
  if (currentState == GAME_MODE) return;
  if (isIdleMode) { isIdleMode=false; showDataPage=false; forceRedraw=true; return; } // [FIX]
  oled.clearBuffer(); oled.setFont(u8g2_font_5x8_tr);
  printCentered(32,"Mengirim Laporan..."); oled.sendBuffer();
  int lv = getAQILevel(currentPPM);
  sendTelegramMessage(
    "*Laporan AtmoBadge V7*\n"
    "Gas: `" + String(currentPPM,0) + " PPM` (" + String(aqiLabel[lv]) + ")\n"
    "Suhu: `" + String(currentTemp,1) + " C`\n"
    "Hum: `" + String(currentHum,0) + " %`"
  );
  forceRedraw = true;
}

void aksiMultiKlik() {
  int c = btn.getNumberClicks();
  Serial.printf("[BTN] MULTI KLIK terdeteksi: %d klik | state=%d | idle=%d\n", c, currentState, isIdleMode);
  if (currentState == GAME_MODE) return;
  if (c == 4 && currentState != BOOTING) {
    Serial.println("[BTN] >>> MASUK GAME MODE! <<<");
    lastActivityTime = millis();
    isIdleMode   = false;
    showDataPage = false;
    currentState = GAME_MODE;
    btn.setClickTicks(200);  // [FIX2] responsif untuk game
    resetGameState();
    forceRedraw = true;
  }
  if (c == 5) runAutoCalibration();
}

void aksiTahan() {
  lastActivityTime = millis();
  Serial.println("[BTN] TAHAN LAMA terdeteksi");
  if (currentState == GAME_MODE) {
    // Tahan lama saat game → keluar game
    currentState = IDLE_SENSING; isIdleMode = false; forceRedraw = true;
    btn.setClickTicks(600);  // [FIX2] kembalikan ke normal saat keluar game
    return;
  }
  // [FIX GAME] Tahan lama di luar game → masuk Flappy Bird
  // Jauh lebih mudah dari 4x klik
  Serial.println("[BTN] >>> MASUK GAME MODE via tahan lama! <<<");
  isIdleMode   = false;
  showDataPage = false;
  currentState = GAME_MODE;
  btn.setClickTicks(200);  // [FIX2] responsif untuk game
  resetGameState();
  forceRedraw  = true;
}

// ===== MARIO FREERTOS =====
TaskHandle_t marioTaskHandle = NULL;
volatile bool stopMarioFlag = false;

void marioTask(void *pvParameters) {
  int notes = (int)MARIO_LEN;
  while (!stopMarioFlag) {
    for (int i = 0; i < notes; i++) {
      if (stopMarioFlag) break;
      int note = marioMelody[i*2], divider = marioMelody[i*2+1], dur = 0;
      if      (divider > 0) dur = marioWholenote / divider;
      else if (divider < 0) dur = (int)((marioWholenote / abs(divider)) * 1.5f);
      if (note != 0) {
        _tone(note); vTaskDelay(pdMS_TO_TICKS((uint32_t)(dur*0.85f)));
        _noTone();   vTaskDelay(pdMS_TO_TICKS((uint32_t)(dur*0.15f)));
      } else { _noTone(); vTaskDelay(pdMS_TO_TICKS(dur)); }
    }
    if (stopMarioFlag) break;
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
  _noTone(); marioTaskHandle = NULL; vTaskDelete(NULL);
}

void startMario() {
  stopMarioFlag = false;
  if (marioTaskHandle == NULL)
    xTaskCreate(marioTask, "Mario", 3072, NULL, 1, &marioTaskHandle); // [FIX] priority 1 agar tidak block WiFi/lwIP stack
}

// ===========================================================
// NETWORK TASK - FreeRTOS background task untuk semua operasi
// HTTPS/HTTP agar main loop tidak pernah blocking
// ===========================================================
void networkTask(void* pvParameters) {
  for (;;) {
    // Tunggu sampai ada pekerjaan
    if (!doSendStartup && !doFetchWeather && !doSendTelegram && !doCheckTelegram) {
      vTaskDelay(pdMS_TO_TICKS(50));
      continue;
    }

    if (WiFi.status() != WL_CONNECTED) {
      doSendStartup = doFetchWeather = doSendTelegram = doCheckTelegram = false;
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    }

    netBusy = true;

    // Reconnect WiFi (blocking boleh di sini karena task terpisah)
    if (doReconnectWiFi) {
      doReconnectWiFi = false;
      Serial.println("[WiFi] Task: mencoba reconnect...");
      WiFi.reconnect();
      for (int i = 0; i < 10; i++) {
        vTaskDelay(pdMS_TO_TICKS(500));
        if (WiFi.status() == WL_CONNECTED) break;
      }
      if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[WiFi] Task: full begin() ulang...");
        WiFi.disconnect(true);
        vTaskDelay(pdMS_TO_TICKS(500));
        WiFi.begin(WIFI_SSID, WIFI_PASS);
        for (int i = 0; i < 20; i++) {
          vTaskDelay(pdMS_TO_TICKS(500));
          if (WiFi.status() == WL_CONNECTED) break;
        }
      }
      if (WiFi.status() == WL_CONNECTED) {
        Serial.println("[WiFi] Task: reconnect OK " + WiFi.localIP().toString());
        lastNtpRetryMs = 0;
        lastWeatherMs  = 0;
        doFetchWeather = true;
        forceRedraw    = true;
      } else {
        Serial.println("[WiFi] Task: reconnect gagal.");
        forceRedraw = true;
      }
    }

    // NTP sync (blocking boleh di sini karena task terpisah)
    if (doNtpSync) {
      doNtpSync = false;
      Serial.println("[NTP] Task: sync...");
      configTime(25200, 0, "pool.ntp.org", "time.google.com", "time.cloudflare.com");
      for (int i = 0; i < 6; i++) {
        vTaskDelay(pdMS_TO_TICKS(500));
        struct tm timeinfo;
        if (getLocalTime(&timeinfo)) {
          ntpSynced = true;
          char buf[24];
          sprintf(buf, "%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
          Serial.printf("[NTP] Task: OK! Jam: %s\n", buf);
          break;
        }
      }
      if (!ntpSynced) Serial.println("[NTP] Task: gagal, retry berikutnya.");
    }

    if (doSendStartup) {
      doSendStartup = false;
      bot.sendMessage(CHAT_ID,
        "*AtmoBadge V7.1 Online!*\nIP: `" + WiFi.localIP().toString() + "`\nKetik /help",
        "Markdown");
    }

    if (doFetchWeather) {
      doFetchWeather = false;
      fetchWeather();
    }

    if (doSendTelegram) {
      doSendTelegram = false;
      String msg = netSendMsg;
      bot.sendMessage(CHAT_ID, msg, "Markdown");
    }

    if (doCheckTelegram) {
      doCheckTelegram = false;
      int msgCount = bot.getUpdates(bot.last_message_received + 1);
      int guard = 0;
      while (msgCount && guard++ < 5) {
        for (int i = 0; i < msgCount; i++) {
          String txt  = bot.messages[i].text;
          String from = bot.messages[i].chat_id;
          if (from != String(CHAT_ID)) continue;
          Serial.println("[BOT] " + txt);

          if (txt == "/status") {
            int lv = getAQILevel(currentPPM);
            String msg = "*Status AtmoBadge V7*\n\n";
            msg += "AQI: *" + String(aqiLabel[lv]) + "*\n";
            msg += "Gas: `" + String(currentPPM, 0) + " PPM`\n";
            msg += "Suhu: `" + String(currentTemp, 1) + " C`\n";
            msg += "Hum: `" + String(currentHum, 0) + " %`\n";
            msg += "Prediksi: `" + String(predictedPPM, 0) + " PPM`\n";
            msg += "Cuaca: " + owmWeather + " " + String(owmTemp, 1) + "C\n";
            msg += "Silent: " + String(silentMode ? "ON" : "OFF");
            bot.sendMessage(CHAT_ID, msg, "Markdown");
          }
          else if (txt == "/log") {
            bot.sendMessage(CHAT_ID,
              "*10 Data Terakhir:*\n```\n" + getLogSummary() + "```", "Markdown");
          }
          else if (txt == "/silent") {
            silentMode = !silentMode;
            bot.sendMessage(CHAT_ID,
              silentMode ? "Mode senyap *ON*." : "Mode senyap *OFF*.", "Markdown");
          }
          else if (txt == "/reset") {
            bot.sendMessage(CHAT_ID, "Restarting...", "Markdown");
            vTaskDelay(pdMS_TO_TICKS(1000));
            ESP.restart();
          }
          else if (txt == "/clearlog") {
            SPIFFS.remove(LOG_FILE);
            File f = SPIFFS.open(LOG_FILE, FILE_WRITE);
            if (f) { f.println("timestamp,ppm,temp,hum,aqi"); f.close(); }
            bot.sendMessage(CHAT_ID, "Log dihapus.", "Markdown");
          }
          else if (txt == "/calib") {
            bot.sendMessage(CHAT_ID, "Memulai kalibrasi...", "Markdown");
            runAutoCalibration();
          }
          else if (txt == "/help") {
            String h = "*Perintah AtmoBadge V7*\n\n";
            h += "/status - Data sensor\n";
            h += "/log - 10 data terakhir\n";
            h += "/silent - Toggle buzzer\n";
            h += "/reset - Restart\n";
            h += "/clearlog - Hapus log\n";
            h += "/calib - Kalibrasi MQ135";
            bot.sendMessage(CHAT_ID, h, "Markdown");
          }
          else {
            bot.sendMessage(CHAT_ID, "Tidak dikenal. Ketik /help", "Markdown");
          }
        }
        msgCount = bot.getUpdates(bot.last_message_received + 1);
      }
    }

    netBusy = false;
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

TaskHandle_t networkTaskHandle = NULL;

void startNetworkTask() {
  xTaskCreate(networkTask, "NetTask", 8192, NULL, 1, &networkTaskHandle);
}

// ===========================================================
// WIFI CONNECT
// ===========================================================
void connectWiFi() {
  oled.clearBuffer(); oled.setFont(u8g2_font_5x8_tr);
  printCentered(22, "Konek WiFi..."); printCentered(35, WIFI_SSID); oled.sendBuffer();

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  delay(100);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 40) {
    vTaskDelay(pdMS_TO_TICKS(500)); // [FIX RESET] pakai vTaskDelay agar FreeRTOS watchdog di-feed saat Mario task jalan
    tries++;
    oled.clearBuffer();
    printCentered(22, "Konek WiFi...");
    char buf[20]; sprintf(buf, "Coba %d/40", tries);
    printCentered(35, buf); oled.sendBuffer();
  }

  if (WiFi.status() == WL_CONNECTED) {
    // [FIX] Beri waktu 500ms agar TCP/IP stack benar-benar siap
    // sebelum DNS query NTP maupun HTTP request
    delay(500);

    oled.clearBuffer();
    printCentered(22, "WiFi OK!");
    printCentered(35, WiFi.localIP().toString().c_str()); oled.sendBuffer();
    Serial.println("[WiFi] Connected: " + WiFi.localIP().toString());
    delay(1200);
  } else {
    oled.clearBuffer(); printCentered(30, "WiFi Gagal"); oled.sendBuffer();
    Serial.println("[WiFi] Gagal konek, mode offline.");
    delay(1200);
  }
}

// ===========================================================
// WIFI AUTO-RECONNECT NON-BLOCKING
// Dipanggil dari loop() setiap 10 detik
// Pakai WiFi.reconnect() dulu (cepat), kalau 5 detik masih gagal
// baru WiFi.begin() ulang (full reconnect)
// ===========================================================
// [FIX SMOOTH] checkAndReconnectWiFi hanya set flag — tidak blocking sama sekali
// Proses reconnect yang sesungguhnya dilakukan oleh networkTask di background
void checkAndReconnectWiFi() {
  unsigned long now = millis();
  if (now - lastWifiCheckMs < 10000) return;
  lastWifiCheckMs = now;

  if (WiFi.status() == WL_CONNECTED) return;

  Serial.println("[WiFi] Koneksi putus, trigger reconnect ke networkTask...");
  if (!doReconnectWiFi) doReconnectWiFi = true;
}

// ===========================================================
// NTP NON-BLOCKING RETRY
// Dipanggil dari loop() setiap 15 detik jika jam belum tersync
// ===========================================================
// [FIX SMOOTH] tryNtpSync hanya set flag — tidak blocking sama sekali
// Proses sync yang sesungguhnya dilakukan oleh networkTask di background
void tryNtpSync() {
  if (WiFi.status() != WL_CONNECTED) return;
  if (ntpSynced) return;
  unsigned long now = millis();
  if (now - lastNtpRetryMs < 15000) return;
  lastNtpRetryMs = now;
  Serial.println("[NTP] Trigger sync ke networkTask...");
  if (!doNtpSync) doNtpSync = true;
}

// ===========================================================
// SETUP
// ===========================================================
void setup() {
  Serial.begin(115200); delay(500);

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // [FIX FPS] Init HW I2C dengan clock 800kHz sebelum oled.begin()
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(800000);

  oled.setI2CAddress(0x3C*2); oled.begin(); oled.setContrast(255);
  oled.clearBuffer(); oled.setFont(u8g2_font_5x8_tr);
  printCentered(22,"AtmoBadge V7.1"); printCentered(38,"Starting..."); oled.sendBuffer();

  dht.begin();
  spiffsInit();
  loadCalibration();

  startMario(); // Mario hanya akses buzzer, aman jalan sebelum connectWiFi

  connectWiFi();

  if (WiFi.status() == WL_CONNECTED) {
    secured_client.setCACert(TELEGRAM_CERTIFICATE_ROOT);

    // [FIX TWDT] Semua operasi network (NTP, cuaca, Telegram) TIDAK dilakukan
    // di setup() agar tidak ada blocking panjang yang trigger watchdog reset.
    // configTime dipanggil sekali saja tanpa menunggu hasilnya.
    configTime(25200, 0, "pool.ntp.org", "time.google.com", "time.cloudflare.com");
    Serial.println("[NTP] configTime triggered, semua network via background task.");

    oled.clearBuffer();
    printCentered(22, "WiFi OK!");
    printCentered(38, "Jam & cuaca");
    printCentered(50, "dimuat background...");
    oled.sendBuffer();
    delay(800); // satu-satunya delay di sini, cukup untuk feed watchdog

    // Semua network diserahkan ke networkTask setelah loop() jalan
    doSendStartup  = true;
    doFetchWeather = true;
    doNtpSync      = true;
  }

  btn.attachClick(aksiKlik1);
  btn.attachDoubleClick(aksiKlik2);
  btn.attachMultiClick(aksiMultiKlik);
  btn.attachLongPressStart(aksiTahan);

  // [FIX GAME] Perlebar window deteksi multi-click agar 4x klik lebih mudah terdeteksi
  btn.setDebounceTicks(40);
  btn.setClickTicks(600);   // [FIX] naik ke 600ms — jeda antar klik lebih longgar
  btn.setPressTicks(1200);  // [FIX] naik ke 1200ms — long press tidak kecepetan trigger

  stopMarioFlag = true;

  // [FIX NONBLOCK] Start background task untuk semua operasi network
  startNetworkTask();

  lastActivityTime = millis(); lastSensorMs = millis();
  currentState = IDLE_SENSING;
}

// ===========================================================
// LOOP
// ===========================================================
void loop() {
  btn.tick();

  // [FIX NONBLOCK] Semua network via trigger ke networkTask background
  // Main loop TIDAK pernah blocking karena network

  // Auto-reconnect WiFi jika putus
  checkAndReconnectWiFi();

  // Retry NTP di background
  if (!ntpSynced) tryNtpSync();

  // Trigger fetch cuaca setiap WEATHER_INTERVAL
  if (!netBusy
      && millis() - lastWeatherMs > WEATHER_INTERVAL
      && currentState != GAME_MODE
      && WiFi.status() == WL_CONNECTED) {
    lastWeatherMs = millis();
    doFetchWeather = true;
  }

  // Trigger poll Telegram setiap 3 detik
  if (!netBusy
      && currentState != GAME_MODE
      && millis() - lastTelegramQueueMs > 3000
      && WiFi.status() == WL_CONNECTED) {
    lastTelegramQueueMs = millis();
    doCheckTelegram = true;
  }

  // [FIX FPS] updateHardwareSensor di-rate-limit 2 detik & skip saat game
  {
    static unsigned long lastHwMs = 0;
    if (currentState != GAME_MODE && millis() - lastHwMs > 2000) {
      lastHwMs = millis();
      updateHardwareSensor();
    }
  }

  if (!isIdleMode
      && (millis()-lastActivityTime > AUTO_SLEEP_MS)
      && currentState != ALERT_MODE
      && currentState != GAME_MODE) {
    isIdleMode = true; forceRedraw = true;
  }

  if (isIdleMode) {
    // [FIX GAME] Rate-limit redraw AOD via millis(), btn.tick() tanpa delay
    // agar OneButton bisa deteksi multi-click (4x klik) dengan akurat
    static unsigned long lastAodMs = 0;
    if (millis() - lastAodMs > 1000) {
      lastAodMs = millis();
      drawAOD();
    }
    btn.tick();
    // [FIX GAME] Tidak pakai delay agar multi-click tidak terlewat di idle mode
    return;
  }
  if (showDataPage) {
    static unsigned long lastDataMs = 0;
    if (millis() - lastDataMs > 400) { lastDataMs = millis(); drawDataPage(); }
    btn.tick(); // [FIX] wajib ada agar klik terdeteksi di data page
    return;
  }

  // =========================================================
  // GAME LOOP - FLAPPY BIRD ENHANCED
  // =========================================================
  if (currentState == GAME_MODE) {

    // [FIX INPUT] btn.tick() di awal game loop — bukan hanya saat nunggu frame
    // Ini yang bikin jump tidak terdeteksi sebelumnya
    btn.tick();

    // Animasi sayap burung (200ms per frame)
    if (millis() - lastWingMs > 200) {
      lastWingMs = millis();
      birdWing = (birdWing == 0) ? 1 : (birdWing == 1) ? -1 : 0;
    }

    // [FIX2 INPUT] Direct polling button untuk jump lebih responsif
    // OneButton dengan clickTicks=600ms terlalu lambat untuk game
    if (gameStarted && !gameOver) {
      static unsigned long lastDirectJumpMs = 0;
      if (digitalRead(BUTTON_PIN) == LOW && millis() - lastDirectJumpMs > 150) {
        lastDirectJumpMs = millis();
        gameJump = true;
      }
    }

    if (gameStarted && !gameOver) {

      // [FIX PHYSICS] hitung delta time dalam detik
      unsigned long nowMs = millis();
      float dt = (lastGamePhysicsMs == 0) ? 0.016f
                 : constrain((nowMs - lastGamePhysicsMs) / 1000.0f, 0.001f, 0.05f);
      lastGamePhysicsMs = nowMs;

      if (gameJump) {
        birdV = JUMP_VEL; // pixel/detik
        gameJump = false;
        _tone(NOTE_E5);
        gameToneEndMs = millis() + 25;
      }

      // matikan tone non-blocking
      if (gameToneEndMs > 0 && millis() >= gameToneEndMs) {
        _noTone(); gameToneEndMs = 0;
      }

      // [FIX PHYSICS] physics pakai delta time — konsisten di FPS berapapun
      birdV += GRAVITY_PPS * dt;
      birdY += birdV * dt;

      float pipeVel = PIPE_VEL_MIN + (pipeSpeed - PIPE_SPEED_MIN) * PIPE_VEL_STEP;
      pipeX  -= pipeVel * dt;
      pipe2X -= pipeVel * dt;

      groundOffset = (int)(groundOffset + pipeVel * dt) % 8;

      // Skor & speed up pipa 1
      if (pipeX + GAME_PIPE_W < 16 && pipeX + GAME_PIPE_W + pipeSpeed >= 16) {
        gameScore++;
        _tone(NOTE_C6); gameToneEndMs = millis() + 20;
        if (gameScore % SCORE_PER_LEVEL == 0 && pipeSpeed < PIPE_SPEED_MAX) {
          pipeSpeed++;
          _tone(NOTE_G6); gameToneEndMs = millis() + 50;
        }
      }

      // Skor & speed up pipa 2
      if (pipe2X + GAME_PIPE_W < 16 && pipe2X + GAME_PIPE_W + pipeSpeed >= 16) {
        gameScore++;
        _tone(NOTE_C6); gameToneEndMs = millis() + 20;
        if (gameScore % SCORE_PER_LEVEL == 0 && pipeSpeed < PIPE_SPEED_MAX) {
          pipeSpeed++;
          _tone(NOTE_G6); gameToneEndMs = millis() + 50;
        }
      }

      // Reset pipa 1
      if (pipeX < -(GAME_PIPE_W + 4)) {
        pipeX    = pipe2X + GAME_PIPE_SPACE;
        pipeGapY = random(GAME_GAP_H / 2 + 4, GAME_GROUND_Y - GAME_GAP_H / 2 - 4);
      }

      // Reset pipa 2
      if (pipe2X < -(GAME_PIPE_W + 4)) {
        pipe2X    = pipeX + GAME_PIPE_SPACE;
        pipe2GapY = random(GAME_GAP_H / 2 + 4, GAME_GROUND_Y - GAME_GAP_H / 2 - 4);
      }

      // Deteksi tabrakan
      bool hitCeiling = (birdY < 4);
      bool hitGround  = (birdY > GAME_GROUND_Y - 4);
      bool hitPipe1   = checkPipeCollision(pipeX,  pipeGapY);
      bool hitPipe2   = checkPipeCollision(pipe2X, pipe2GapY);

      if (hitCeiling || hitGround || hitPipe1 || hitPipe2) {
        gameOver = true;

        if (gameScore > gameHighScore) {
          gameHighScore = gameScore;
          gameNewRecord = true;
          saveHighScore();
        }

        // [FIX FPS] game over sound non-blocking
        _tone(NOTE_CS5); gameToneEndMs = millis() + 150;
      }
    }

    drawGameFrame();

    // [FIX FPS] frame limiter ~25fps tanpa delay() agar btn.tick() responsif
    static unsigned long gameLastMs = 0;
    while (millis() - gameLastMs < 33) { btn.tick(); }
    gameLastMs = millis();
    return;
  }

  // Face logic
  if      (currentState == ALERT_MODE)          currentFace = FACE_MASK;
  else if (dht_ok && currentTemp >= 35.0f)      currentFace = FACE_HOT;
  else if (currentPPM >= AQI_SEDANG)            currentFace = FACE_HOT;
  else                                           currentFace = FACE_SMILE;

  if (currentState == ALERT_MODE && !silentMode) {
    static unsigned long lastBuzzMs = 0;
    static bool buzzState = false;
    if (millis() - lastBuzzMs > 200) {
      lastBuzzMs = millis();
      buzzState = !buzzState;
      if (buzzState) _tone(NOTE_C6); else _noTone();
    }
  } else { _noTone(); }

  static unsigned long lastAnimMs = 0;
  static int animFrame = 0;
  if (millis() - lastAnimMs > 500) { lastAnimMs = millis(); animFrame = !animFrame; }
  drawFacePage(animFrame);
  smartDelay(80);
}