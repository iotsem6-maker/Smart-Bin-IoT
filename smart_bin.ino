#define BLYNK_TEMPLATE_ID "ID-Blynk_Anda"
#define BLYNK_TEMPLATE_NAME "Nama_Template"
#define BLYNK_AUTH_TOKEN "Token_anda_Blynk"
#define BLYNK_PRINT Serial

#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <Servo.h>

char ssid[] = "Nama Wifi";
char pass[] = "Password";

// ===== PIN =====
#define TRIG      D1
#define ECHO      D2
#define BUZZER    D5
#define SERVO_PIN D6

// ===== PENGATURAN =====
const float JARAK_TANGAN = 15.0;
const float JARAK_MIN    = 2.0;
const float SUHU_BAHAYA  = 20.0;
const int   SERVO_BUKA   = 115;
const int   SERVO_TUTUP  = 0;
const unsigned long TAHAN_BUKA = 1000;

const int BUZZER_ON  = HIGH;
const int BUZZER_OFF = LOW;

Servo tutup;
BlynkTimer timer;

bool terbuka = false;
bool perintahBuka = false;     // dari tombol Blynk
bool buzzerManual = false;
bool sudahNotifSuhu = false;
int  hitungTangan = 0;
int  jumlahBuka = 0;
float suhuTerakhir = 0;
unsigned long waktuTangan = 0;
unsigned long waktuLaporan = 0;

// ===== BLYNK =====
BLYNK_WRITE(V2) {                       // buka tutup dari dashboard
  if (param.asInt() == 1) perintahBuka = true;
}

BLYNK_WRITE(V3) {                       // buzzer manual
  buzzerManual = param.asInt();
  digitalWrite(BUZZER, buzzerManual ? BUZZER_ON : BUZZER_OFF);
}

BLYNK_CONNECTED() {
  Blynk.virtualWrite(V2, 0);
  Blynk.virtualWrite(V3, 0);
  Blynk.virtualWrite(V0, jumlahBuka);
}

void kirimData() {
  Blynk.virtualWrite(V0, jumlahBuka);
  Blynk.virtualWrite(V1, suhuTerakhir);
}

// ===== SENSOR =====
float bacaJarakSekali() {
  digitalWrite(TRIG, LOW);  delayMicroseconds(2);
  digitalWrite(TRIG, HIGH); delayMicroseconds(10);
  digitalWrite(TRIG, LOW);
  long durasi = pulseIn(ECHO, HIGH, 25000);
  if (durasi == 0) return 999;
  return durasi * 0.0343 / 2.0;
}

float bacaJarak() {
  float d[5];
  for (int i = 0; i < 5; i++) { d[i] = bacaJarakSekali(); delay(10); }
  for (int i = 0; i < 4; i++)
    for (int j = i + 1; j < 5; j++)
      if (d[j] < d[i]) { float t = d[i]; d[i] = d[j]; d[j] = t; }
  return d[2];
}

float bacaSuhu() {
  long total = 0;
  for (int i = 0; i < 10; i++) { total += analogRead(A0); delay(5); }
  return (total / 10.0) * (3.3 / 1023.0) * 100.0;
}

// ===== BUZZER =====
void bip(int kali, int lamaMs) {
  for (int i = 0; i < kali; i++) {
    digitalWrite(BUZZER, BUZZER_ON);  delay(lamaMs);
    digitalWrite(BUZZER, BUZZER_OFF); delay(lamaMs);
  }
  digitalWrite(BUZZER, buzzerManual ? BUZZER_ON : BUZZER_OFF);
}

// ===== SETUP =====
void setup() {
  Serial.begin(115200);
  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);
  pinMode(BUZZER, OUTPUT);
  digitalWrite(BUZZER, BUZZER_OFF);

  tutup.attach(SERVO_PIN);
  tutup.write(SERVO_TUTUP);

  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  timer.setInterval(2000L, kirimData);

  bip(1, 150);
  Serial.println("Smart Bin siap");
}

// ===== LOOP =====
void loop() {
  Blynk.run();
  timer.run();

  unsigned long now = millis();
  float suhu  = bacaSuhu();
  float jarak = bacaJarak();
  suhuTerakhir = suhu;

  if (jarak > JARAK_MIN && jarak < JARAK_TANGAN) hitungTangan++;
  else hitungTangan = 0;

  if (hitungTangan >= 2 || perintahBuka) {
    if (!terbuka) {
      tutup.write(SERVO_BUKA);
      terbuka = true;
      jumlahBuka++;
    }
    waktuTangan = now;
    if (perintahBuka) {
      perintahBuka = false;
      Blynk.virtualWrite(V2, 0);        // switch kembali OFF
    }
  }

  if (terbuka && (now - waktuTangan > TAHAN_BUKA)) {
    tutup.write(SERVO_TUTUP);
    terbuka = false;
  }

  // Peringatan suhu tinggi
  if (suhu >= SUHU_BAHAYA) {
    bip(3, 80);
    if (!sudahNotifSuhu) {
      Blynk.logEvent("suhu_tinggi", "Suhu tong tinggi!");
      sudahNotifSuhu = true;
    }
  } else if (suhu < SUHU_BAHAYA - 5) {
    sudahNotifSuhu = false;
  }

  if (now - waktuLaporan >= 1000) {
    waktuLaporan = now;
    Serial.printf("Jarak: %.0f cm | Suhu: %.1f C | Tutup: %s | Buka: %d\n",
                  jarak, suhu, terbuka ? "BUKA" : "TUTUP", jumlahBuka);
  }

  delay(100);
}