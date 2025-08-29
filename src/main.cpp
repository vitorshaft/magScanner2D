#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_VL53L0X.h>
#include <QMC5883LCompass.h>
#include <math.h>

#define OLED_WIDTH 128
#define OLED_HEIGHT 64
#define OLED_RESET -1

Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RESET);
Adafruit_VL53L0X lox = Adafruit_VL53L0X();
QMC5883LCompass compass;

const float DEG2RAD = 0.017453292519943295f;
const float RAD2DEG = 57.29577951308232f;

// Calibração simples (hard-iron) — ajuste após imprimir min/max
float mx_min = -1000.0f, mx_max = 1000.0f;
float my_min = -1000.0f, my_max = 1000.0f;

float scale_mm_per_px = 20.0f; // 20 mm por pixel (ajuste)
int cx = OLED_WIDTH / 2;
int cy = OLED_HEIGHT / 2;

float magnetic_declination = -21.0f;//-0.366f;

// Scan de dispositivos I2C
void scanI2C() {
  Serial.println("Scan no barramento i2c...");
  byte error, address;
  int nDevices = 0;
  
  for(address = 1; address < 127; address++ ) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();
    
    if (error == 0) {
      Serial.print("dispositivo i2c encontrado em 0x");
      if (address < 16) Serial.print("0");
      Serial.print(address, HEX);
      Serial.println(" !");
      nDevices++;
    } else if (error == 4) {
      Serial.print("Erro desconhecido no endereco 0x");
      if (address < 16) Serial.print("0");
      Serial.println(address, HEX);
    }
  }
  
  if (nDevices == 0) {
    Serial.println("Nenhum dispositivo i2c encontrado\n");
  } else {
    Serial.println("Scan concluido\n");
  }
}

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);

  scanI2C();

  // OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { while (1); }
  display.clearDisplay(); display.display();

  // VL53L0X
  if (!lox.begin()) {
    Serial.println("VL53L0X nao encontrado!");
    while (1);
  }

  //Inicia Magnetometro
  compass.init();
  compass.setSmoothing(10,true);  //Suavizacao de leitura

  //Configurar calibracao
  //compass.setCalibration(mx_min, mx_max, my_min, my_max);
  Serial.println("Magnetometro iniciado!");

  // Tela inicial
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.println("Scanner 2D Polar");
  display.println("ESP32+HMC5883L+VL53L0X");
  display.display();

  delay(1000);
}

void loop() {
  // --- 1) Ler magnetometro ---
  compass.read();
  int mx_raw = compass.getX();
  int my_raw = compass.getY();

  // Calibracao do angulo
  int azimute = compass.getAzimuth();
  //compass.getAzimuth(&azimute);

  float theta_deg = azimute;
  float theta = theta_deg * DEG2RAD;    // radianos

  // Ajustar com a declinacao magnetica
  theta_deg += magnetic_declination;
  
  // Normalizar entre 0 e 360 graus
  if(theta_deg < 0) theta_deg += 360;
  if(theta_deg >= 360) theta_deg -= 360;
  theta = theta_deg * DEG2RAD;

  // --- 2) Ler distancia VL53 ---
  VL53L0X_RangingMeasurementData_t measure;
  lox.rangingTest(&measure, false);
  int dist_mm = (measure.RangeStatus == 4) ? -1 : measure.RangeMilliMeter; // -1 invalido

  // --- 3) Polar -> cartesiano (mm) ---
  float x_mm = (dist_mm > 0) ? dist_mm * cosf(theta) : NAN;
  float y_mm = (dist_mm > 0) ? dist_mm * sinf(theta) : NAN;

  // --- 4) Plot no OLED ---
  display.clearDisplay();
  // eixo/crosshair
  display.drawLine(0, cy, OLED_WIDTH-1, cy, SSD1306_WHITE);
  display.drawLine(cx, 0, cx, OLED_HEIGHT-1, SSD1306_WHITE);

  // ponto
  if (dist_mm > 0) {
    int px = cx + (int)roundf(x_mm / scale_mm_per_px);
    int py = cy - (int)roundf(y_mm / scale_mm_per_px);
    // Verifica limites no display
    if (px >= 0 && px < OLED_WIDTH && py >= 0 && py < OLED_HEIGHT) {
      display.fillCircle(px, py, 1, SSD1306_WHITE);
    }
  }

  // HUD
  display.setCursor(0,0);
  display.setTextSize(1);
  display.printf("ang: %3.0f deg  d: %4d mm\n", theta_deg, dist_mm);
  display.display();

  // --- 5) Log CSV na Serial ---
  unsigned long t = millis();
  if (dist_mm > 0) {
    Serial.printf("%lu,%.2f,%d,%.1f,%.1f\n", t, theta_deg, dist_mm, x_mm, y_mm);
  } else {
    Serial.printf("%lu,%.2f,-1,NaN,NaN\n", t, theta_deg);
  }

  delay(100); // ~16 Hz
}
