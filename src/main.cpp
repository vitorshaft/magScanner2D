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
#define QUEUE_SIZE 10  // Queue size for last readings

Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RESET);
Adafruit_VL53L0X lox = Adafruit_VL53L0X();
QMC5883LCompass compass;

const float DEG2RAD = 0.017453292519943295f;
const float RAD2DEG = 57.29577951308232f;

// Simple calibration (hard-iron) - adjust after printing min/max
float mx_min = -1000.0f, mx_max = 1000.0f;
float my_min = -1000.0f, my_max = 1000.0f;

float scale_mm_per_px = 20.0f; // 20 mm per pixel (adjust)
int cx = OLED_WIDTH / 2;
int cy = OLED_HEIGHT / 2;

float magnetic_declination = -21.0f; // Adjust for your location

// Structure to store coordinates
struct Coordinate {
  float x;
  float y;
  unsigned long timestamp;
};

// Circular queue for last coordinates
Coordinate coordinateQueue[QUEUE_SIZE];
int front = 0;
int rear = 0;
int count = 0;

// Scan I2C devices
void scanI2C() {
  Serial.println("Scanning I2C bus...");
  byte error, address;
  int nDevices = 0;
  
  for(address = 1; address < 127; address++ ) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();
    
    if (error == 0) {
      Serial.print("I2C device found at address 0x");
      if (address < 16) Serial.print("0");
      Serial.print(address, HEX);
      Serial.println(" !");
      nDevices++;
    } else if (error == 4) {
      Serial.print("Unknown error at address 0x");
      if (address < 16) Serial.print("0");
      Serial.println(address, HEX);
    }
  }
  
  if (nDevices == 0) {
    Serial.println("No I2C devices found\n");
  } else {
    Serial.println("Scan completed\n");
  }
}

// Function to add coordinate to queue
void addCoordinate(float x, float y) {
  coordinateQueue[rear].x = x;
  coordinateQueue[rear].y = y;
  coordinateQueue[rear].timestamp = millis();
  
  rear = (rear + 1) % QUEUE_SIZE;
  if (count < QUEUE_SIZE) {
    count++;
  } else {
    front = (front + 1) % QUEUE_SIZE;
  }
}

// Function to draw all coordinates from queue on display
void drawCoordinates() {
  for (int i = 0; i < count; i++) {
    int index = (front + i) % QUEUE_SIZE;
    int px = cx + (int)roundf(coordinateQueue[index].x / scale_mm_per_px);
    int py = cy - (int)roundf(coordinateQueue[index].y / scale_mm_per_px);
    
    // Check display boundaries
    if (px >= 0 && px < OLED_WIDTH && py >= 0 && py < OLED_HEIGHT) {
      // Newer coordinates are larger and brighter
      int size = 1 + (i * 2 / count); // Size proportional to age
      int brightness = SSD1306_WHITE - (i * 10); // Older coordinates darker
      
      display.fillCircle(px, py, size, brightness);
      
      // Connect points with lines (optional)
      if (i > 0) {
        int prev_index = (front + i - 1) % QUEUE_SIZE;
        int prev_px = cx + (int)roundf(coordinateQueue[prev_index].x / scale_mm_per_px);
        int prev_py = cy - (int)roundf(coordinateQueue[prev_index].y / scale_mm_per_px);
        display.drawLine(prev_px, prev_py, px, py, SSD1306_WHITE);
      }
    }
  }
}

// Function to display queue information on HUD
void displayQueueInfo() {
  display.setCursor(0, OLED_HEIGHT - 8);
  display.printf("Points: %d/%d", count, QUEUE_SIZE);
}

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);

  scanI2C();

  // OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("Failed to initialize display"));
    //while (1);
  }
  display.clearDisplay(); display.display();

  // VL53L0X
  if (!lox.begin()) {
    Serial.println("VL53L0X not found!");
    //while (1);
  }

  // Initialize Magnetometer
  compass.init();
  compass.setSmoothing(10, true);  // Reading smoothing

  Serial.println("Magnetometer initialized!");

  // Initialize queue
  for (int i = 0; i < QUEUE_SIZE; i++) {
    coordinateQueue[i] = {NAN, NAN, 0};
  }

  // Initial screen
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("2D Polar Scanner");
  display.println("ESP32+QMC5883+VL53L0X");
  display.display();

  delay(1000);
}

void loop() {
  // --- 1) Read magnetometer ---
  compass.read();
  int mx_raw = compass.getX();
  int my_raw = compass.getY();

  // Angle calibration
  int azimuth = compass.getAzimuth();

  float theta_deg = azimuth;
  float theta = theta_deg * DEG2RAD;    // radians

  // Adjust for magnetic declination
  theta_deg += magnetic_declination;
  
  // Normalize between 0 and 360 degrees
  if(theta_deg < 0) theta_deg += 360;
  if(theta_deg >= 360) theta_deg -= 360;
  theta = theta_deg * DEG2RAD;

  // --- 2) Read VL53 distance ---
  VL53L0X_RangingMeasurementData_t measure;
  lox.rangingTest(&measure, false);
  int dist_mm = (measure.RangeStatus == 4) ? -1 : measure.RangeMilliMeter; // -1 invalid

  // --- 3) Polar to cartesian (mm) ---
  float x_mm = (dist_mm > 0) ? dist_mm * cosf(theta) : NAN;
  float y_mm = (dist_mm > 0) ? dist_mm * sinf(theta) : NAN;

  // --- 4) Add to queue if valid reading ---
  if (dist_mm > 0 && dist_mm < 4000) { // Filter for valid readings
    addCoordinate(x_mm, y_mm);
  }

  // --- 5) Plot on OLED ---
  display.clearDisplay();
  
  // axis/crosshair
  display.drawLine(cx-5, cy, cx+5, cy, SSD1306_WHITE);
  display.drawLine(cx, cy-5, cx, cy+5, SSD1306_WHITE);
  
  // Draw reference grid (optional)
  /*
  for (int i = -2; i <= 2; i++) {
    if (i != 0) {
      int posx = cx + i * (50 / scale_mm_per_px);
      int posy = cy + i * (50 / scale_mm_per_px);
      display.drawLine(posx, cy-2, posx, cy+2, SSD1306_WHITE);
      display.drawLine(cx-2, posy, cx+2, posy, SSD1306_WHITE);
    }
  }
  */

  // Draw all coordinates from queue
  drawCoordinates();

  // HUD
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.printf("ang: %3.0f deg  d: %4d mm\n", theta_deg, dist_mm);
  
  // Display queue information
  //displayQueueInfo();
  
  display.display();

  // --- 6) CSV log to Serial ---
  unsigned long t = millis();
  if (dist_mm > 0) {
    Serial.printf("%lu,%.2f,%d,%.1f,%.1f\n", t, theta_deg, dist_mm, x_mm, y_mm);
  } else {
    Serial.printf("%lu,%.2f,-1,NaN,NaN\n", t, theta_deg);
  }

  delay(100); // ~10 Hz
}

// Function for queue debug (optional)
void debugQueue() {
  Serial.println("=== QUEUE CONTENT ===");
  for (int i = 0; i < count; i++) {
    int index = (front + i) % QUEUE_SIZE;
    Serial.printf("%d: X=%.1fmm, Y=%.1fmm, Age=%lums\n", 
                 i, coordinateQueue[index].x, coordinateQueue[index].y, 
                 millis() - coordinateQueue[index].timestamp);
  }
  Serial.println("=====================");
}