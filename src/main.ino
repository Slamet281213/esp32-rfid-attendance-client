#include <Arduino_JSON.h>
#include <MFRC522.h>
#include <LiquidCrystal_I2C.h>
#include <SPI.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "SoftwareSerial.h"
#include "DFRobotDFPlayerMini.h"
#include <NTPClient.h>
#include <WiFiUdp.h>

#define SS_PIN 5
#define RST_PIN 17

MFRC522 rfid(SS_PIN, RST_PIN);
LiquidCrystal_I2C lcd(0x27, 16, 2); // I2C address 0x27, 16 column and 2 rows

static const uint8_t PIN_MP3_TX = 26; // Connects to module's RX 
static const uint8_t PIN_MP3_RX = 27; // Connects to module's TX 
SoftwareSerial softwareSerial(PIN_MP3_RX, PIN_MP3_TX);

DFRobotDFPlayerMini player;

const long utcOffsetInSeconds = 7 * 3600; // 7 jam dalam detik

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);

const char* ssid = "Rifli";
const char* password = "bayargopa";

const String host = "https://vincentiusyudha23.my.id";
const String URL = host + "/api/send-card-id";
const String token = "1|O2VvDhAnvkwWo9siymZQDibIjYthneSWhquOi0J8ac1ea7d2";

String uidString = "";

unsigned long lastReadTime = 0;
const unsigned long interval = 500;  // Interval waktu minimum antara pembacaan kartu (dalam milidetik)

int messageIndex = 0;

bool lastConnectionStatus = false;

String messages[] = {
  "ROCKERTECH", //0
  "PRESENSI",
  "Proses Sedang",
  "BERHASIL", //3
  "REGISTRASI",
  "Berlangsung",
  "Proses Gagal", //6
  "Silahkan Coba",
  "Kembali",
  "ERROR", //9
  "No Internet",
  "Sedang Mencari", //11
  "WIFI"
};

void setup() {
  Serial.begin(115200);
  softwareSerial.begin(9600);
  lcd.init();
  lcd.backlight();
  SPI.begin();
  rfid.PCD_Init();
  connectWiFi();
  if (player.begin(softwareSerial)) {
    Serial.println("DFDPLAYER FOUND");
  } else {
    Serial.println("DFDPLAYER NOT FOUND");
  }

  timeClient.begin();
  timeClient.setTimeOffset(utcOffsetInSeconds);
}

void loop() {
    bool currentConnectionStatus = (WiFi.status() == WL_CONNECTED);

    if (currentConnectionStatus != lastConnectionStatus) {
        Serial.println("tidak terhubung ke wifi");
        lcd.clear();  // Bersihkan layar hanya jika status berubah
    }

    if (currentConnectionStatus) {
        lcd.setCursor(3, 0);
        lcd.print(messages[0]);
        lcd.setCursor(4, 1);
        lcd.print(messages[1]);
    } else {
        lcd.setCursor(0, 0);
        lcd.print(messages[9]);
        lcd.setCursor(0, 1);
        lcd.print(messages[10]);
    }

    lastConnectionStatus = currentConnectionStatus;

  timeClient.update();

  int hour = timeClient.getHours();
  int minute = timeClient.getMinutes();

  Serial.print("Waktu saat ini: ");
  Serial.print(hour);   
  Serial.print(":");
  if (minute < 10) {
    Serial.print("0");
  }
  Serial.println(minute);

  unsigned long currentMillis = millis();

  if (rfid.PICC_IsNewCardPresent()){
    Serial.println("Kartu terdeteksi");
    if (rfid.PICC_ReadCardSerial()) {
      Serial.println("Kartu terbaca");
      Serial.println(messageIndex);
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print(messages[2]);
      lcd.setCursor(0, 1);
      lcd.print(messages[5]);
      delay(300);
      lastReadTime = currentMillis;
      handleRFIDTag();

    } else {
      Serial.println("Gagal membaca kartu");
    }
    delay(100);
  }
}

void handleRFIDTag() {
  MFRC522::PICC_Type piccType = rfid.PICC_GetType(rfid.uid.sak);

  uidString = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    uidString += (rfid.uid.uidByte[i] < 0x10 ? "0" : "");
    uidString += String(rfid.uid.uidByte[i], HEX);
  }

  uidString.toUpperCase();
  Serial.print("UID String: ");
  Serial.println(uidString);

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();

  if (WiFi.status() == WL_CONNECTED) {
    sendPostRequest();
  } else {
    connectWiFi();
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(messages[6]);
    delay(500);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(messages[7]);
    lcd.setCursor(0, 1);
    lcd.print(messages[8]);
    delay(500);
  }
}

void sendPostRequest() {
  HTTPClient http;
  http.begin(URL);

  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  http.addHeader("Authorization", "Bearer " + token);

  String data = "card_id=" + uidString;

  int httpResponseCode = http.POST(data);

  Serial.print("HTTP Response code: ");
  Serial.println(httpResponseCode);
  
  if (httpResponseCode > 0) {
    String payload = http.getString();
    Serial.print("JSON Response: ");
    Serial.println(payload);

    JSONVar jsonResponse = JSON.parse(payload);

    if (JSON.typeof(jsonResponse) == "undefined") {
      Serial.println("Parsing JSON gagal!");
      return;
    }

    String message = jsonResponse["message"];
    Serial.print("Message from server: ");
    Serial.println(message);

    String sound = jsonResponse["sound"];
    Serial.print("Sound from server: ");
    Serial.println(sound);

    if (sound == ""){
      Serial.println("HAMAX");
    } 

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(message);
    delay(2000);
    lcd.clear();
    
  } else {
    Serial.print("Error saat mengirimkan request: ");
    Serial.println(httpResponseCode);
    Serial.println(http.errorToString(httpResponseCode));
  }

  http.end();
  Serial.println("Selesai");
}

void connectWiFi() {
  WiFi.disconnect(true);
  delay(200);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  lcd.clear();
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    Serial.print(".");
    lcd.setCursor(0, 0);
    lcd.print(messages[11]);
    lcd.setCursor(0, 1);
    lcd.print(messages[12]);
  }
  
  Serial.println("");
  Serial.println("Koneksi WiFi berhasil");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  lcd.clear();
}
