# 🧭 magScanner2D

<div style="display: inline_block">
<img align="center" src="/image/magScanner2D.gif" alt="Demonstração"  width="40%">
<img align="center" src="/image/magScanner2D.jpg" alt="Foto"  width="30%">
</div>

**magScanner2D** is a 2D polar scanner built on an ESP32 microcontroller. It uses a magnetometer for angular orientation and a time-of-flight distance sensor to map surrounding objects in real time. The system displays the scan on an OLED screen and logs data via Serial in CSV format.

## 📦 Hardware Components

| Component            | Description                                      |
|---------------------|--------------------------------------------------|
| ESP32               | Main microcontroller                             |
| VL53L0X             | Time-of-flight distance sensor                   |
| QMC5883L Compass    | Magnetometer for heading detection               |
| SSD1306 OLED (128x64)| Display for visualizing scan results            |
| I2C Bus             | Communication protocol between components        |

## 🧠 How It Works

1. **Initialization**
   - Scans the I2C bus for connected devices
   - Initializes the OLED display, VL53L0X sensor, and magnetometer

2. **Main Loop**
   - Reads magnetometer data to determine azimuth (heading)
   - Applies magnetic declination correction and converts to radians
   - Reads distance from VL53L0X sensor
   - Converts polar coordinates (angle, distance) to Cartesian (x, y)
   - Plots the point on the OLED display
   - Displays HUD with angle and distance
   - Logs data to Serial in CSV format

## 📈 Serial Output Format
timestamp(ms),angle(deg),distance(mm),x(mm),y(mm) 123456,45.00,320,226.27,226.27 123556,46.00,315,219.00,226.90 ...

## ⚙️ Requirements

- [PlatformIO](https://platformio.org/) installed in Visual Studio Code
- Required libraries:
  - `Adafruit_GFX`
  - `Adafruit_SSD1306`
  - `Adafruit_VL53L0X`
  - `QMC5883LCompass`

## 🚀 Getting Started

1. Clone the repository:
   ```bash
   git clone https://github.com/your-username/magScanner2D.git
   cd magScanner2D
   ```
- Open the project in VS Code with PlatformIO
- Connect your ESP32 via USB
- Build and upload the firmware
- Open the Serial Monitor at 115200 baud to view live logs
🖥️ OLED Display
The OLED screen shows:
- A central crosshair representing the origin
- A moving dot representing the scanned point
- Real-time HUD with angle and distance
🧪 Calibration & Tuning
- Magnetometer calibration can be adjusted using:
```cpp
compass.setCalibration(mx_min, mx_max, my_min, my_max);
```
- Adjust `scale_mm_per_px` to match your environment's scale
- Set `magnetic_declination` according to your geographic location

## 📊 Future Improvements
- Add support for automatic magnetometer calibration
- Implement data smoothing or filtering
- Integrate with cloud logging or SD card storage
- Add CI/CD pipeline for firmware validation
- Visualize scan data in 3D or heatmap format
🛡️ .gitignore Suggestions
To avoid committing build artifacts and local settings:

## 🤝 Contributing
Pull requests are welcome. For major changes, please open an issue first to discuss what you would like to change.
