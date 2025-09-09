// === LIBRARY ===
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <U8g2lib.h>
#include <RTClib.h>

// === WIFI DAN TELEGRAM ===
const char* ssid = "vivoz1pro";
const char* password = "11111111";
const char* botToken = "7704293281:AAG6P2mDWJjlOJv4rWqv2vMVqhNyN10zFc0";
const int telegramUserID = 1452143232;
WiFiClientSecure client;
UniversalTelegramBot bot(botToken, client);

// === PIN KONFIGURASI ===
#define ONE_WIRE_BUS 4  // DS18B20
#define SENSOR_PH 34    // pH
#define RELAY_PH_DOWN 26
#define RELAY_PH_UP 25
#define RELAY_NUTRISI 33
#define RELAY_KIPAS 32
#define RELAY_LAMPU 18
#define RELAY_HEATER 5
#define BUZZER 23
#define SENSOR_LEVEL 19
#define SENSOR_LEVEL_PH_UP 35
#define SENSOR_LEVEL_PH_DOWN 27
#define LED_MERAH 13
#define I2C_SDA 21
#define I2C_SCL 22

// === OLED ===
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE);
unsigned long lastOLED = 0;
bool layarOLED = false;

// === SENSOR SUHU ===
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// === SENSOR PH ===
unsigned long lastPHReadTime = 0;
unsigned long intervalPH = 2000;
float nilaiPH = 0;
bool koreksiPHAktif = false;
unsigned long waktuKoreksiPH = 0;

//  Level air pH
bool lastLevelPHUp = false;
bool lastLevelPHDown = false;
unsigned long lastBuzzPHUp = 0;
unsigned long lastBuzzPHDown = 0;
bool isBuzzingPHUp = false;
bool isBuzzingPHDown = false;
float calibration_value = 21.20;  // nilai yang kamu peroleh dari kalibrasi real




enum KoreksiPH { TIDAK,
                 UP,
                 DOWN };
KoreksiPH statusKoreksiPH = TIDAK;

// Waktu terakhir pompa pH menyala
unsigned long waktuPompaPH = 0;
bool pompaPHAktif = false;
const unsigned long durasiPompaPH = 6000;  // 6 detik

// === RTC ===
RTC_DS3231 rtc;
String statusLampu = "OFF";
String statusNutrisi = "OFF";
bool lampuManual = false;

// === Penambahan Nutrisi ===
bool flagNutrisi = false;
unsigned long lastCheck = 0;
unsigned long lastNutrisi = 0;
unsigned long nutrisiStartTime = 0;
bool nutrisiSedangAktif = false;
bool nutrisiDilakukanMingguIni = false;
int mingguTerakhirNutrisi = -1;

// === Level Air ===
bool lastLevelLow = false;

// === Alarm Buzzer===
unsigned long lastBuzzMillis = 0;
unsigned long buzzerStartMillis = 0;
bool isBuzzing = false;

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  client.setInsecure();
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("WiFi Connected");

  // === PINMODE ===
  pinMode(RELAY_KIPAS, OUTPUT);
  pinMode(RELAY_HEATER, OUTPUT);
  pinMode(RELAY_PH_UP, OUTPUT);
  pinMode(RELAY_PH_DOWN, OUTPUT);
  pinMode(RELAY_LAMPU, OUTPUT);
  pinMode(RELAY_NUTRISI, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  pinMode(SENSOR_LEVEL, INPUT_PULLUP);
  pinMode(SENSOR_LEVEL_PH_UP, INPUT_PULLUP);
  pinMode(SENSOR_LEVEL_PH_DOWN, INPUT_PULLUP);
  pinMode(LED_MERAH, OUTPUT);

  digitalWrite(RELAY_KIPAS, HIGH);
  digitalWrite(RELAY_HEATER, HIGH);
  digitalWrite(RELAY_PH_UP, HIGH);
  digitalWrite(RELAY_PH_DOWN, HIGH);
  digitalWrite(RELAY_LAMPU, HIGH);
  digitalWrite(RELAY_NUTRISI, HIGH);
  digitalWrite(LED_MERAH, LOW);

  sensors.begin();
  rtc.begin();

  u8g2.begin();
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_fub11_tr);

  u8g2.setCursor((128 - u8g2.getStrWidth("Hallo")) / 2, 20);
  u8g2.print("Hallo");

  u8g2.setCursor((128 - u8g2.getStrWidth("Sahabat")) / 2, 35);
  u8g2.print("Sahabat");

  u8g2.setCursor((128 - u8g2.getStrWidth("Aquascaper")) / 2, 50);
  u8g2.print("Aquascaper");

  u8g2.sendBuffer();
  delay(3000);
}

void loop() {
  sensors.requestTemperatures();
  float suhu_raw = sensors.getTempCByIndex(0);
  float offset = 0.28;  // kalibrasi offset
  // float offset = 0.31;
  float suhu = suhu_raw - offset;

  // === LOGIKA SENSOR SUHU ===
  if (suhu < 22) {
    digitalWrite(RELAY_HEATER, LOW);
    digitalWrite(RELAY_KIPAS, HIGH);
  } else if (suhu > 28) {
    digitalWrite(RELAY_KIPAS, LOW);
    digitalWrite(RELAY_HEATER, HIGH);
  } else if (suhu >= 22.5 && suhu <= 27.5) {
    digitalWrite(RELAY_KIPAS, HIGH);
    digitalWrite(RELAY_HEATER, HIGH);
  }

  // === LOGIKA SENSOR PH ===
  if (!koreksiPHAktif && (millis() - lastPHReadTime >= intervalPH)) {
    lastPHReadTime = millis();

    int sensorValue = analogRead(SENSOR_PH);
    float voltage = (sensorValue / 4095.0) * 3.3;

    float pH = 7.47;  // Nilai pH awal sistem (realistis, mendekati batas atas)

    // === SIMULASI pH NAIK SETELAH 1 MENIT ===
    if (millis() > 90000 && millis() < 360000) {  // antara 1–6 menit
      pH = 7.61;                                  // pH naik → sistem koreksi aktif
    }

    // === SIMULASI pH STABIL SETELAH 6 MENIT ===
    if (millis() >= 360000) {
      pH = 7.25;  // pH kembali stabil di rentang sehat
    }

    nilaiPH = pH;  // Simpan untuk OLED dan Telegram

    // === Logika Koreksi pH ===
    if (pH < 6.5) {
      digitalWrite(RELAY_PH_UP, LOW);
      waktuKoreksiPH = millis();
      koreksiPHAktif = true;
      statusKoreksiPH = UP;
      intervalPH = 300000;
    } else if (pH > 7.5) {
      digitalWrite(RELAY_PH_DOWN, LOW);
      waktuKoreksiPH = millis();
      koreksiPHAktif = true;
      statusKoreksiPH = DOWN;
      intervalPH = 300000;
    } else {
      intervalPH = 2000;
    }
  }


  // Matikan pompa setelah 6 detik
  if (koreksiPHAktif && (millis() - waktuKoreksiPH >= durasiPompaPH)) {
    if (statusKoreksiPH == UP) {
      digitalWrite(RELAY_PH_UP, HIGH);  // matikan (aktif LOW)
    } else if (statusKoreksiPH == DOWN) {
      digitalWrite(RELAY_PH_DOWN, HIGH);
    }
    koreksiPHAktif = false;
    statusKoreksiPH = TIDAK;
  }

  // === PENJADWALAN RTC ===
  DateTime now = rtc.now();

  // ---------- Logika Lampu Otomatis + Manual ----------
  bool dalamJadwal = now.hour() >= 14 && now.hour() < 21;
  if (dalamJadwal) {
    lampuManual = false;             // Nonaktifkan override manual
    digitalWrite(RELAY_LAMPU, LOW);  // ON otomatis
  } else {
    // Di luar jadwal: pakai kontrol manual
    if (lampuManual) {
      digitalWrite(RELAY_LAMPU, LOW);  // Manual ON
    } else {
      digitalWrite(RELAY_LAMPU, HIGH);  // Manual OFF (default)
    }
  }

  // ---------- Pompa Nutrisi: ON setiap Senin jam 07:00:00 selama 5 detik ----------
  if (now.dayOfTheWeek() == 1 && now.hour() == 7 && now.minute() == 0 && now.second() < 5) {
    digitalWrite(RELAY_NUTRISI, LOW);  // ON (aktif LOW)

    if (!nutrisiDilakukanMingguIni) {
      nutrisiDilakukanMingguIni = true;
      mingguTerakhirNutrisi = now.day();
    }

  } else {
    digitalWrite(RELAY_NUTRISI, HIGH);  // OFF
  }

  // Reset flag minggu baru setelah lewat jam 08:00
  if (now.dayOfTheWeek() == 1 && now.hour() >= 8 && mingguTerakhirNutrisi != now.day()) {
    nutrisiDilakukanMingguIni = false;
  }


  // === NOTIFIKASI LEVEL AIR NUTRISI===
  bool levelLow = digitalRead(SENSOR_LEVEL) == LOW;
  if (levelLow) {
    digitalWrite(LED_MERAH, HIGH);
    if (lastBuzzMillis == 0 || millis() - lastBuzzMillis >= 600000) {
      digitalWrite(BUZZER, HIGH);
      buzzerStartMillis = millis();
      isBuzzing = true;
      bot.sendMessage(String(telegramUserID), "🚨 Nutrisi cair hampir habis! Segera isi ulang.", "");
      lastBuzzMillis = millis();
    }
    if (isBuzzing && millis() - buzzerStartMillis >= 5000) {
      digitalWrite(BUZZER, LOW);
      isBuzzing = false;
    }
  } else {
    digitalWrite(LED_MERAH, LOW);
    digitalWrite(BUZZER, LOW);
    isBuzzing = false;
    lastBuzzMillis = 0;
  }

  if (lastLevelLow && !levelLow) {
    bot.sendMessage(String(telegramUserID), "✅ Botol nutrisi berhasil diisi kembali!", "");
  }
  lastLevelLow = levelLow;

  // // === NOTIFIKASI LEVEL AIR PH UP ===
  // bool levelPHUpLow = digitalRead(SENSOR_LEVEL_PH_UP) == LOW;
  // if (levelPHUpLow) {
  //   digitalWrite(LED_MERAH, HIGH);  // bisa diganti LED khusus jika tersedia
  //   if (lastBuzzPHUp == 0 || millis() - lastBuzzPHUp >= 600000) {
  //     digitalWrite(BUZZER, HIGH);
  //     buzzerStartMillis = millis();
  //     isBuzzingPHUp = true;
  //     bot.sendMessage(String(telegramUserID), "🚨 Cairan pH Up hampir habis! Segera isi ulang.", "");
  //     lastBuzzPHUp = millis();
  //   }
  //   if (isBuzzingPHUp && millis() - buzzerStartMillis >= 5000) {
  //     digitalWrite(BUZZER, LOW);
  //     isBuzzingPHUp = false;
  //   }
  // } else {
  //   if (lastLevelPHUp && !levelPHUpLow) {
  //     bot.sendMessage(String(telegramUserID), "✅ Botol pH Up berhasil diisi kembali!", "");
  //   }
  //   isBuzzingPHUp = false;
  //   lastBuzzPHUp = 0;
  // }
  // lastLevelPHUp = levelPHUpLow;

  // === NOTIFIKASI LEVEL AIR PH DOWN ===
  bool levelPHDownLow = digitalRead(SENSOR_LEVEL_PH_DOWN) == LOW;
  if (levelPHDownLow) {
    digitalWrite(LED_MERAH, HIGH);  // sama: ganti jika punya LED sendiri
    if (lastBuzzPHDown == 0 || millis() - lastBuzzPHDown >= 600000) {
      digitalWrite(BUZZER, HIGH);
      buzzerStartMillis = millis();
      isBuzzingPHDown = true;
      bot.sendMessage(String(telegramUserID), "🚨 Cairan pH Down hampir habis! Segera isi ulang.", "");
      lastBuzzPHDown = millis();
    }
    if (isBuzzingPHDown && millis() - buzzerStartMillis >= 5000) {
      digitalWrite(BUZZER, LOW);
      isBuzzingPHDown = false;
    }
  } else {
    if (lastLevelPHDown && !levelPHDownLow) {
      bot.sendMessage(String(telegramUserID), "✅ Botol pH Down berhasil diisi kembali!", "");
    }
    isBuzzingPHDown = false;
    lastBuzzPHDown = 0;
  }
  lastLevelPHDown = levelPHDownLow;

  // === OLED DISPLAY ===
  unsigned long currentMillis = millis();
  if (currentMillis - lastOLED >= 2000) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x13_tr);
    if (!layarOLED) {
      String s1 = "SUHU : " + String(suhu, 1) + " C";
      String s2 = "STATUS : " + String(suhu < 22 ? "HEATER ON" : suhu > 28 ? "KIPAS ON"
                                                                           : "NORMAL");
      // float ph = 3.5 * analogRead(SENSOR_PH) * 3.3 / 4095.0;
      // float ph = 6.5;
      String s3 = "PH : " + String(nilaiPH, 2);
      String s4 = "STATUS : " + String(intervalPH > 10000 ? (nilaiPH < 6.5 ? "UP PH" : "DOWN PH") : "NORMAL");

      u8g2.drawStr(tengahX(s1), 12, s1.c_str());
      u8g2.drawStr(tengahX(s2), 27, s2.c_str());
      u8g2.drawStr(tengahX(s3), 42, s3.c_str());
      u8g2.drawStr(tengahX(s4), 57, s4.c_str());
    } else {
      String s1 = "LAMPU : " + String(digitalRead(RELAY_LAMPU) == LOW ? "ON" : "OFF");
      String s2 = "14:00 - 21:00";
      String s3 = "NUTRISI : " + String(nutrisiDilakukanMingguIni ? "Senin 07:00" : "Belum");
      String s4 = hariJamString(now);
      u8g2.drawStr(tengahX(s1), 12, s1.c_str());
      u8g2.drawStr(tengahX(s2), 27, s2.c_str());
      u8g2.drawStr(tengahX(s3), 42, s3.c_str());
      u8g2.drawStr(tengahX(s4), 57, s4.c_str());
    }
    u8g2.sendBuffer();
    layarOLED = !layarOLED;
    lastOLED = currentMillis;
  }

  // === SERIAL MONITOR ===
  static unsigned long lastSerialMillis = 0;
  if (millis() - lastSerialMillis > 1000) {
    lastSerialMillis = millis();
    Serial.println("======= Status SUHU & PH =======");
    Serial.printf("🌡️ Suhu   : %.2f C\n", suhu);
    Serial.printf("❄️ Kipas  : %s\n", digitalRead(RELAY_KIPAS) == LOW ? "ON" : "OFF");
    Serial.printf("🔥 Heater : %s\n", digitalRead(RELAY_HEATER) == LOW ? "ON" : "OFF");
    Serial.printf("💧 pH     : %.2f\n", nilaiPH);  // Tampilkan nilai pH
    if (nilaiPH < 6.5) {
      Serial.println("⚠️ pH rendah! Menambah pH Up...");
    } else if (nilaiPH > 7.5) {
      Serial.println("⚠️ pH tinggi! Menambah pH Down...");
    } else {
      Serial.println("✅ pH stabil.");
    }
    Serial.println("========= PENJADWALAN =========");
    Serial.printf("💡 Lampu  : %s\n", digitalRead(RELAY_LAMPU) == LOW ? "ON" : "OFF");
    Serial.printf("🧪 Nutrisi: %s\n", nutrisiDilakukanMingguIni ? "Senin 07:00" : "Belum");

    Serial.print("🕒 Waktu: ");
    Serial.println(hariJamString(now));
  }

  // === CEK PERINTAH TELEGRAM ===
  if (millis() - lastCheck > 2000) {
    lastCheck = millis();
    // === Telegram Perintah "cek" atau "Cek" ===
    int n = bot.getUpdates(bot.last_message_received + 1);
    for (int i = 0; i < n; i++) {
      // Perintah "cek" atau "Cek"
      if (bot.messages[i].text == "cek" || bot.messages[i].text == "Cek") {
        String msg = "📊 STATUS TERKINI:\n";
        msg += "🌡️ *Suhu*: " + String(suhu, 1) + " C\n";
        msg += "❄️ *Kipas*: " + String(digitalRead(RELAY_KIPAS) == LOW ? "ON" : "OFF") + "\n";    // Kipas
        msg += "♨️ *Heater*: " + String(digitalRead(RELAY_HEATER) == LOW ? "ON" : "OFF") + "\n";  // Heater
        // msg += "💧 *pH*: " + String(3.5 * analogRead(SENSOR_PH) * 3.3 / 4095.0, 2) + "\n";             // GPIO34 untuk sensor pH
        // msg += "💧 *pH*: 6.50\n";
        msg += "💧 *pH*: " + String(nilaiPH, 2) + "\n";
        msg += "💧+*pH Up*: " + String(digitalRead(RELAY_PH_UP) == LOW ? "ON" : "OFF") + "\n";      // PH_UP_PIN adalah pin untuk pompa pH Up
        msg += "💧-*pH Down*: " + String(digitalRead(RELAY_PH_DOWN) == LOW ? "ON" : "OFF") + "\n";  // PH_DOWN_PIN adalah pin untuk pompa pH Down
        msg += "💡 *Lampu*: " + String(digitalRead(RELAY_LAMPU) == LOW ? "ON" : "OFF") + "\n";
        msg += "🧪 *Nutrisi*: " + String(nutrisiDilakukanMingguIni ? "Sudah (Senin 07:00)" : "Belum minggu ini") + "\n";


        bot.sendMessage(bot.messages[i].chat_id, msg, "Markdown");
      }

      // Perintah "nyalakan lampu"
      else if (bot.messages[i].text == "lampu on" || bot.messages[i].text == "Lampu on") {
        DateTime now = rtc.now();
        if (now.hour() < 14 || now.hour() >= 21) {
          lampuManual = true;
          digitalWrite(RELAY_LAMPU, LOW);
          bot.sendMessage(bot.messages[i].chat_id, "*💡 Lampu dinyalakan secara manual!*", "Markdown");
        } else {
          bot.sendMessage(bot.messages[i].chat_id, "⚠️ Tidak bisa menyalakan manual saat lampu dalam jadwal otomatis (14:00–21:00).", "");
        }
      }

      // Perintah "Matikan lampu"
      else if (bot.messages[i].text == "lampu off" || bot.messages[i].text == "Lampu off") {
        lampuManual = false;
        digitalWrite(RELAY_LAMPU, HIGH);
        bot.sendMessage(bot.messages[i].chat_id, "*💡 Lampu dimatikan secara manual!*", "Markdown");
      }

      // Menanggapi perintah yang tidak dikenali
      else {
        String helpMessage = "❌ *Perintah tidak dikenali.*\n\n";
        helpMessage += "Gunakan salah satu perintah berikut:\n";
        helpMessage += "1️⃣ *Cek* — Cek keadaan aquascape\n";
        helpMessage += "2️⃣ *Lampu on* — Nyalakan lampu\n";
        helpMessage += "3️⃣ *Lampu off* — Matikan lampu\n";
        bot.sendMessage(bot.messages[i].chat_id, helpMessage, "Markdown");
      }
    }
  }
}

int tengahX(String text) {
  return (128 - u8g2.getStrWidth(text.c_str())) / 2;
}

String hariJamString(DateTime now) {
  String hari[] = { "Minggu", "Senin", "Selasa", "Rabu", "Kamis", "Jumat", "Sabtu" };
  String waktu = String(now.hour()) + ":" + (now.minute() < 10 ? "0" : "") + String(now.minute());
  return hari[now.dayOfTheWeek()] + ", " + waktu;
}