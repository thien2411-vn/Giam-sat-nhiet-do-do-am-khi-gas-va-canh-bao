#define BLYNK_TEMPLATE_ID         "TMPL6OSaUo7-o"
#define BLYNK_TEMPLATE_NAME       "Cảm biến khí gas và nhiệt độ độ ẩm"
#define BLYNK_AUTH_TOKEN          "4U5Dq2xRSU13mxx40x5EcZV_kFQxKrRQ"

#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <DHT.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <math.h>

// WiFi
char ssid[] = "Thiên";
char pass[] = "12345678999";

// LCD 16x2 I2C
LiquidCrystal_I2C lcd(0x27, 16, 2);

// DHT (nếu đổi sang DHT22 thì chỉ sửa DHTTYPE)
#define DHTPIN    4
#define DHTTYPE   DHT11
DHT dht(DHTPIN, DHTTYPE);

// MQ2
#define MQ2_PIN   34
#define RL        5.0    // điện trở tải (kΩ)
float Ro = 10.0;         // giá trị Ro đã hiệu chuẩn (kΩ)

// Buzzer
#define BUZZER_PIN 25

// LED trạng thái kết nối (V0)
bool ledState = false;
unsigned long lastBlink = 0;

// Số mẫu để đọc trung bình DHT
#define NUM_SAMPLES 5
// Offset bù trừ (nếu cần hiệu chuẩn thực tế)
float temp_offset = 0.8;    // cộng thêm 0.8°C
float hum_offset  = -3.0;   // trừ 3%RH

// --- HÀM ĐỌC TRUNG BÌNH NHIỆT ĐỘ ---
float readTemperatureAverage() {
  float sum = 0;
  int count = 0;
  for (int i = 0; i < NUM_SAMPLES; i++) {
    float t = dht.readTemperature();
    if (!isnan(t)) {
      sum += t;
      count++;
    }
    delay(200);
  }
  if (count == 0) {
    return NAN;
  }
  return (sum / count) + temp_offset;
}

// --- HÀM ĐỌC TRUNG BÌNH ĐỘ ẨM ---
float readHumidityAverage() {
  float sum = 0;
  int count = 0;
  for (int i = 0; i < NUM_SAMPLES; i++) {
    float h = dht.readHumidity();
    if (!isnan(h)) {
      sum += h;
      count++;
    }
    delay(200);
  }
  if (count == 0) {
    return NAN;
  }
  return (sum / count) + hum_offset;
}

// --- HÀM TÍNH TOÁN NỒNG ĐỘ KHÍ GAS (MQ2) ---
float calculateGasPPM() {
  int adc = analogRead(MQ2_PIN);
  // ESP32 ADC chuẩn 0–4095, tham chiếu 0–5V (nếu bạn cấp MUX2 bằng 5V)
  float voltage = adc * 3.3 / 4095.0;
  float Rs = ((3.3 - voltage) / voltage) * RL;     // tính R_sensor (kΩ)d:\DoAn\DoAn1\DoAn1C\DoAn1C.ino
  float ratio = Rs / Ro;
  // Công thức chuyển ratio sang PPM tham khảo datasheet MQ2
  float ppm = 370.0 * pow(ratio, -1.38);
  return ppm;
}

// --- HÀM GỘP ĐỌC TẤT CẢ CẢM BIẾN ---
void readAllSensors(float &temperature, float &humidity, float &gasPPM) {
  // Đọc nhiệt độ và độ ẩm trung bình
  temperature = readTemperatureAverage();
  humidity    = readHumidityAverage();
  // Đọc khí gas
  gasPPM      = calculateGasPPM();
}

void setup() {
  Serial.begin(115200);

  lcd.init();
  lcd.backlight();

  dht.begin();
  pinMode(BUZZER_PIN, OUTPUT);

  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  lcd.setCursor(0, 0);
  lcd.print("Dang ket noi...");
  delay(2000);
  lcd.clear();
}

void loop() {
  Blynk.run();

  // === GỌI HÀM ĐỌC CHUNG CHO CẢ 3 CẢM BIẾN ===
  float t, h, ppm;
  readAllSensors(t, h, ppm);

  // === GỬI DỮ LIỆU LÊN BLYNK ===
  if (!isnan(t)) {
    Blynk.virtualWrite(V1, t);
  }
  if (!isnan(h)) {
    Blynk.virtualWrite(V2, h);
  }
  Blynk.virtualWrite(V3, ppm);

  // === HIỂN THỊ VÀ CẢNH BÁO TRÊN LCD VÀ BÚZZER ===
  // Nếu gas vượt ngưỡng
  if (ppm > 350.0) {
    lcd.setCursor(0, 0);
    if (!isnan(t) && !isnan(h)) {
      lcd.print("T:"); lcd.print(t, 1); lcd.write(223);
      lcd.print("C H:"); lcd.print(h, 0); lcd.print("%");
    }
    lcd.setCursor(0, 1);
    lcd.print("Gas vuot nguong ");
    digitalWrite(BUZZER_PIN, HIGH);
  }
  // Nếu chỉ nhiệt > 35°C
  else if (!isnan(t) && t > 35.0) {
    lcd.setCursor(0, 0);
    lcd.print("Qua Nhiet > 35  ");
    digitalWrite(BUZZER_PIN, HIGH);
    lcd.setCursor(0, 1);
    lcd.print("Gas:");
    lcd.print((int)ppm);
    lcd.print(" ppm     ");
  }
  // Trạng thái bình thường
  else {
    lcd.setCursor(0, 0);
    if (!isnan(t) && !isnan(h)) {
      lcd.print("T:"); lcd.print(t, 1); lcd.write(223);
      lcd.print("C H:"); lcd.print(h, 0); lcd.print("%");
    } else {
      lcd.print("Sensor loi     ");
    }
    lcd.setCursor(0, 1);
    lcd.print("Gas:");
    lcd.print((int)ppm);
    lcd.print(" ppm     ");
    digitalWrite(BUZZER_PIN, LOW);
  }

  // === LED TRẠNG THÁI KẾT NỐI (V0) ===
  // Nhấp nháy mỗi 500ms
  if (millis() - lastBlink >= 500) {
    ledState = !ledState;
    Blynk.virtualWrite(V0, ledState ? 255 : 0); // Gửi 0 hoặc 255 để LED V0 chớp
    lastBlink = millis();
  }

  // Không cần delay thêm vì đã có delay trong hàm đọc trung bình DHT
}
