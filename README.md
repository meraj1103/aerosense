# AeroSense - Dual Sensor (MQ-135 + GP2Y1014AU0F) Air Quality System & 3D Dashboard

A complete IoT air quality monitoring system using an **ESP32**, **MQ-135 (Gas & VOCs)**, and **Sharp GP2Y1014AU0F (PM2.5 Optical Dust)** sensor with wireless **Bluetooth (BLE)** and **USB Web Serial** streaming directly into an interactive 3D Web Dashboard.

---

## Hardware Connections

See **[connection.md](file:///m:/MJ/projects%20coding/Other/aerosense/connection.md)** for complete schematics, resistor-capacitor wiring, and pinout details for both sensors.

### Master Pin Mapping

| Sensor | Sensor Pin | ESP32 Pin | Description |
|---|---|---|---|
| **MQ-135** | **VCC** | **VIN / 5V** | MQ-135 heating coil requires **5V** to operate properly. |
| **MQ-135** | **GND** | **GND** | Common ground. |
| **MQ-135** | **AOUT** | **GPIO 34** | ADC1 pin (safe with Bluetooth). |
| **MQ-135** | **DOUT** | **GPIO 25** *(Optional)* | Digital threshold trigger. |
| **GP2Y1014AU0F** | **Pin 1 (V-LED)** | **VIN / 5V** | Powered via 150Ω resistor + 220µF capacitor to GND. |
| **GP2Y1014AU0F** | **Pin 2 (LED-GND)**| **GND** | IR LED ground (negative lead of 220µF capacitor). |
| **GP2Y1014AU0F** | **Pin 3 (LED)** | **GPIO 26** | Digital pulse control pin (IRED drive). |
| **GP2Y1014AU0F** | **Pin 4 (S-GND)** | **GND** | Phototransistor signal ground. |
| **GP2Y1014AU0F** | **Pin 5 (Vo)** | **GPIO 35** | ADC1 pin for dust concentration analog voltage (0V to 3.6V). |
| **GP2Y1014AU0F** | **Pin 6 (Vcc)** | **VIN / 5V** | Sensor logic power (5V). |
| **Status LED** | **Built-in** | **GPIO 2** | Blue LED pulses when advertising, solid when connected. |

---

## Flash Instructions via Arduino IDE

Because the Bluetooth stack requires extra flash space:

1. Open [aerosense.ino](file:///m:/MJ/projects%20coding/Other/aerosense/aerosense.ino) in Arduino IDE.
2. Verify `#define ENABLE_DUST_SENSOR true` is set at the top.
3. In Arduino IDE menu, go to:
   * **Tools > Partition Scheme** → Select **`Huge APP (3MB No OTA / 1MB SPIFFS)`**
   *(A `partitions.csv` is also included in this folder for automatic detection)*.
4. Select your ESP32 board and COM port.
5. Click **Upload** (`→`).

---

## Mobile Phone App & Installation (PWA)

AeroSense is now an installable **Progressive Web App (PWA)** with persistent **IndexedDB storage** on your phone!

### How to Open on Your Phone:
1. Ensure your phone and your PC are connected to the same Wi-Fi network.
2. Run [start_dashboard.bat](file:///m:/MJ/projects%20coding/Other/aerosense/start_dashboard.bat).
3. The command window will display your computer's local Wi-Fi address, for example:
   ```text
   http://192.168.1.50:8000/dashboard.html
   ```
4. Type that address into your phone's browser:
   * **Android (Chrome):** Tap the 3 dots menu (`⋮`) &rarr; **"Install App"** (or **"Add to Home screen"**).
   * **iPhone (Safari / Bluefy):** Tap the Share button &rarr; **"Add to Home Screen"**. *(Note: On iOS, install the free **Bluefy** browser to connect via Web Bluetooth directly from iPhone!)*
5. The AeroSense icon appears on your phone's home screen. Tap it to launch full-screen like a native mobile app!

---

## Persistent History & Analytics (IndexedDB)

All sensor readings are automatically saved to your phone or computer's internal **IndexedDB** database:
* **Survives Reboots:** Data remains stored even if you close the browser, turn off your phone, or reboot.
* **History Horizon Filters:** View trends across **1 Hour**, **6 Hours**, **24 Hours**, **7 Days**, or **All Time**.
* **Dual-Axis Historical Graph:** View both Gas ADC contamination and PM2.5 particle density over real clock timestamps.
* **Telemetry Analytics:** Computes total readings recorded, peak PM2.5, average PM2.5, peak gas, average gas, and clean air exposure ratio.
* **📥 Export to CSV:** One-tap button on mobile or desktop to export your complete historical dataset as an Excel/Sheets-compatible `.csv` file.
* **🗑️ Clear History:** Easily clear historical records whenever you want to start a fresh logging session.

---

* **🤖 AeroBot AI Companion & Real-Time Motivation Engine:**
  * **Live Motivational Coaching:** Automatically boosts morale, focus, and energy when air is pristine, delivering actionable tips for workouts and productivity.
  * **Proactive Pollution Alerts:** Instantly triggers audio alarms, speech synthesis voice alerts, and push notifications when PM2.5 or VOCs reach unhealthy thresholds.
  * **Interactive Glassmorphic AI Chat:** Ask AeroBot about outdoor running safety, mask precautions, indoor air-purifying plants (NASA study), sensor physics, or breathing exercises.
  * **Zero-Setup Offline Intelligence + Gemini Support:** Built-in expert medical/air-quality reasoning engine runs 100% locally offline, with optional Google Gemini 1.5/2.0 Flash API support.
  * **🧘 Guided 4-7-8 Mindful Breathing Tool:** Animated visual circle with gentle audio tones to expand lung capacity and lower stress.
  * **🔊 Speech-to-Text & Text-to-Speech:** Talk to AeroBot using your microphone and hear spoken alerts while measuring pollution hands-free.
* **Dual Hero Gauges:** Dedicated real-time radial gauges for both **MQ-135 Gas & VOCs** (ADC & Volts) and **Sharp GP2Y PM2.5 Dust** ($\mu\text{g/m}^3$ & $V_o$).
* **Interactive 3D Particle Cloud (Three.js):** Ambient atmospheric simulation behind the glassmorphism UI. Particle density, turbulence, speed, and glow color react dynamically to live air quality in real time!
* **Official CPCB / MPCB National AQI Scale:** Real-time 0–500 spectrum bar with sliding needle and 6 breakpoint category cards (Good, Satisfactory, Moderate, Poor, Very Poor, Severe).
* **Multi-Stream Real-Time Chart:** Toggle between **Dual Mode (Gas + PM2.5)**, **Gas ADC**, or **PM2.5 Dust ($\mu\text{g/m}^3$)**.
* **Composite Environmental Index:** Aggregates both gas contamination and particulate matter to deliver immediate health recommendations.
* **🧪 Simulation Demo Mode:** Test all dual-gauge physics, 3D particle reactions, AeroBot alerts, and graphs instantly without hardware.
* **Dual Transport:** Zero-install Web Bluetooth (BLE) with fallback to USB Web Serial (115200 baud).
