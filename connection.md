# AeroSense Hardware Connection Guide
## Interfacing GP2Y1014AU0F Optical Dust Sensor & MQ-135 Gas Sensor with ESP32

This guide provides the complete wiring diagrams, schematic, pinout details, and step-by-step instructions for connecting both the **Sharp GP2Y1014AU0F Optical Dust (PM2.5) Sensor** and the **MQ-135 Air Quality Sensor** to an **ESP32**.

---

## 1. GP2Y1014AU0F Optical Dust Sensor Overview

The **GP2Y1014AU0F** (upgraded version of GP2Y1010AU0F) uses an infrared emitting diode (IRED) and a phototransistor diagonally arranged inside an optical chamber. It detects light reflected by dust, smoke particles, and PM2.5 particulate matter in the air.

```
       +---------------------------------------------+
       |             SHARP GP2Y1014AU0F              |
       |               [AIR AIR HOLE]                |
       |                     (O)                     |
       |                                             |
       |   [ 6-Pin Connector: 1 2 3 4 5 6 ]          |
       +---------------------------------------------+
```

### 6-Pin Connector Pinout

| Pin # | Pin Name | Standard Wire Color | Purpose | Connects To |
|:---:|:---:|:---:|:---|:---|
| **1** | **V-LED** | **White / Red** | Power for Internal IR LED | **ESP32 VIN (5V)** via **150Ω resistor** & **220µF capacitor** |
| **2** | **LED-GND** | **Blue / Black** | Ground for IR LED | **ESP32 GND** (connected to capacitor negative lead) |
| **3** | **LED** | **Green / Yellow** | LED Pulse Drive Control | **ESP32 GPIO 26** (Digital Output) |
| **4** | **S-GND** | **Yellow / Black** | Signal Ground for Phototransistor | **ESP32 GND** |
| **5** | **Vo (VOUT)** | **Black / Blue** | Analog Dust Signal Output (0V - 3.6V) | **ESP32 GPIO 35** (ADC1 Channel 7) |
| **6** | **Vcc** | **Red / Orange** | Sensor Power Supply (5V) | **ESP32 VIN (5V)** |

> [!WARNING]
> **Check your wire ribbon colors!** Different cable manufacturers may swap wire colors. Always match by **Pin Number (1 to 6)** starting from Pin 1 (V-LED) at the edge of the sensor connector.

---

## 2. Required External Components for Dust Sensor

The GP2Y1014AU0F requires two external passive components to supply stable peak current for the internal infrared LED without creating noise on the 5V line:

1. **$150\,\Omega$ Resistor** ($1/4\text{W}$): Limits current to the internal IR diode.
2. **$220\,\mu\text{F}$ Electrolytic Capacitor** ($10\text{V}$, $16\text{V}$, or $25\text{V}$): Filters voltage dips during the $320\,\mu\text{s}$ LED pulse.

### Schematic Diagram (Dust Sensor + ESP32)

```text
       ESP32 DEVKIT                                     SHARP GP2Y1014AU0F
    +-----------------+                                +-------------------+
    |                 |                                |                   |
    |         VIN(5V) |-----+--------------------------| Pin 6 (Vcc)       |
    |                 |     |                          |                   |
    |                 |     +---[ 150Ω Resistor ]--+---| Pin 1 (V-LED)     |
    |                 |                            |   |                   |
    |                 |                     +---[+]--+ |                   |
    |                 |                     |  220µF | |                   |
    |                 |                     |  Cap.  | |                   |
    |                 |                     +---[-]--+ |                   |
    |                 |                            |   |                   |
    |             GND |-----+----------------------O---| Pin 2 (LED-GND)   |
    |                 |     |                          |                   |
    |                 |     +--------------------------| Pin 4 (S-GND)     |
    |                 |                                |                   |
    |         GPIO 26 |--------------------------------| Pin 3 (LED Pulse) |
    |                 |                                |                   |
    |         GPIO 35 |--------------------------------| Pin 5 (Vo / VOUT) |
    |   (ADC1 Input)  |                                |                   |
    +-----------------+                                +-------------------+
```

### Capacitor Polarity Tip:
* **Positive Lead (+):** Longer leg &rarr; Connects to **Pin 1 (V-LED)** and one side of the $150\,\Omega$ resistor.
* **Negative Lead (-):** Shorter leg with a white stripe/minus sign on the can &rarr; Connects to **GND** (Pin 2).

---

## 3. Complete Master Wiring Table (Both Sensors)

Here is the single unified wiring table for your complete AeroSense dual-sensor setup:

| Sensor | Sensor Pin | ESP32 Pin | Wire Function | Notes |
|:---|:---|:---|:---|:---|
| **MQ-135** | **VCC** | **VIN (5V)** | Heating Element & Logic Power | Must be 5V for coil to heat |
| **MQ-135** | **GND** | **GND** | Ground | Common Ground |
| **MQ-135** | **AO (AOUT)** | **GPIO 34** | Analog Gas Voltage | ADC1 Channel 6 |
| **MQ-135** | **DO (DOUT)** | **GPIO 25** | Digital Alert Threshold | *(Optional)* Uses internal pullup |
| **GP2Y1014AU0F** | **Pin 1 (V-LED)** | **VIN (5V)** | IR LED Power | In series with **150Ω resistor** + **220µF cap to GND** |
| **GP2Y1014AU0F** | **Pin 2 (LED-GND)** | **GND** | IR LED Ground | Connects to capacitor negative lead |
| **GP2Y1014AU0F** | **Pin 3 (LED)** | **GPIO 26** | IR LED Pulse Trigger | Microcontroller digital output |
| **GP2Y1014AU0F** | **Pin 4 (S-GND)** | **GND** | Detector Ground | Common Ground |
| **GP2Y1014AU0F** | **Pin 5 (Vo)** | **GPIO 35** | Analog Dust Signal | ADC1 Channel 7 |
| **GP2Y1014AU0F** | **Pin 6 (Vcc)** | **VIN (5V)** | Detector Logic Power | Direct 5V |
| **Status LED** | **Built-in** | **GPIO 2** | Bluetooth Status LED | Pulses when advertising, solid when connected |

> [!NOTE]
> **Why GPIO 34 and GPIO 35?**
> Both **GPIO 34** and **GPIO 35** are part of **ADC1** on the ESP32. ADC1 pins work reliably while Bluetooth (BLE) is transmitting. (Never use ADC2 pins GPIO 0, 2, 4, 12-15, 25-27 for analog reading while Bluetooth is enabled).

---

## 4. How the Code Operates the Dust Sensor

The GP2Y1014AU0F measurement takes place inside `readDustSensor()` using microsecond pulse timing defined in the Sharp datasheet:

```text
LED Drive (GPIO 26): HIGH --------------\________ 320 µs ________/-----------------
                                         |              |
Analog Read (GPIO 35):                   Sample at 280 µs
```

1. **LED ON:** Pull `GPIO 26` **LOW** (the internal transistor activates the IRED).
2. **Delay:** Wait **$280\,\mu\text{s}$** for the optical pulse and detector to stabilize.
3. **Sample:** Read `analogRead(GPIO 35)` to capture the reflected light voltage.
4. **Delay:** Wait another **$40\,\mu\text{s}$** (total pulse = $320\,\mu\text{s}$).
5. **LED OFF:** Pull `GPIO 26` **HIGH**.
6. **Calculation:**
   $$\text{Voltage} = \text{ADC} \times \left(\frac{3.3}{4095}\right)$$
   $$\text{Dust Density } (\text{mg/m}^3) = 0.17 \times \text{Voltage} - 0.1$$
   $$\text{PM2.5 } (\mu\text{g/m}^3) = \text{Dust Density} \times 1000.0$$

---

## 5. Software Status in `aerosense.ino`

The Sharp GP2Y1014AU0F dust sensor is **ACTIVATED and LIVE** in [aerosense.ino](file:///m:/MJ/projects%20coding/Other/aerosense/aerosense.ino):

```cpp
#define ENABLE_DUST_SENSOR true  // Active & Live!
```

### Firmware Processing Pipeline:
* **8-Pulse Trimmed Median Filter:** The ESP32 triggers 8 microsecond pulses ($320\,\mu\text{s}$ duration each, spaced by $10\,\text{ms}$) per measurement cycle.
* **Precise $280\,\mu\text{s}$ Sampling:** Analog voltage is read *while the internal IR LED is energized*, preventing electrical noise or ADC droop.
* **Outlier Rejection:** The lowest 2 and highest 2 pulse readings are discarded to eliminate random dust motes and optical bounce, averaging the middle 4 pulses.
* **Dual Index:** Air quality is evaluated both individually (Gas AQI & PM2.5 AQI) and as an integrated **Composite Air Quality Index**.

---

## 6. PM2.5 ($\mu\text{g/m}^3$) Air Quality Levels Reference

| Dust Density ($\mu\text{g/m}^3$) | Air Quality Status | Typical Environment | Recommended Action |
|:---:|:---:|:---|:---|
| **0 – 35** | **Clean / Excellent** | Normal clean indoor room with air filter | Ideal indoor breathing conditions |
| **35 – 75** | **Moderate** | Standard urban ambient air, slight dust | Acceptable for most individuals |
| **75 – 150** | **Unhealthy / Poor** | Noticeable dust, cooking fumes, hazy room | Turn on air purifier, close windows |
| **150 – 250** | **Very Unhealthy** | Heavy smoke, construction dust | Wear N95 mask indoors, activate filtration |
| **> 250** | **Hazardous** | Direct incense, cigarette smoke, fire | Immediate evacuation or emergency ventilation |

---

## 7. Live Testing & Verification Checklist

Now that your sensor is connected:

1. **IR Pulse Glow Verification:**
   * Point a smartphone camera into the central optical air hole.
   * Because phone camera sensors can detect near-infrared light (850nm), you will observe a faint purple/pink glow pulsing in rhythm with the sensor reads.
2. **Clean Air Baseline Check:**
   * In a normal clean room, the sensor's Vo output pin should report between **$0.55\,\text{V}$ and $0.85\,\text{V}$** (displayed in the Web Dashboard).
   * This corresponds to **$0 - 35\,\mu\text{g/m}^3$** (Clean).
3. **Smoke / Dust Response Test:**
   * Extinguish an incense stick or match and hold it a few inches near the sensor air hole (or blow gently across the aperture).
   * Watch the Web Dashboard: the PM2.5 gauge and the 3D particle cloud will instantly respond, the arc will turn amber/red, and density will climb.
4. **Vertical Orientation:**
   * Ensure the sensor is mounted vertically so that air and particulate matter can naturally flow through the hole without accumulating inside the optical lens.

---

## 8. Battery Power Connection Guide

Powering the dual-sensor AeroSense system portably requires understanding the power demands of your setup:

### Total Power Consumption:
* **ESP32 (with active Bluetooth BLE):** ~120 mA – 180 mA
* **MQ-135 Gas Sensor Heating Element:** ~150 mA – 180 mA (Requires **5.0V** continuously to keep the internal tin-dioxide bead heated to operating temperature)
* **Sharp GP2Y1014AU0F Dust Sensor:** ~15 mA average (spikes up to 100 mA during the 320 µs LED pulses)
* **Total Current Draw:** **~300 mA to 450 mA at 5V**

> [!IMPORTANT]
> **Why 5V is Mandatory:**
> Do NOT connect a bare 3.7V Li-ion battery directly to the ESP32. The **MQ-135 heating coil must have 5V** to produce accurate gas measurements, and the **Sharp dust sensor** requires 5V for optical calibration.

---

### Option 1: Standard USB Power Bank (Recommended & Easiest)

A standard portable phone power bank is the cleanest, safest, and most reliable method:

```text
+-----------------------+               +-----------------------+
|    USB POWER BANK     |  USB Cable    |     ESP32 DEVKIT      |
|  (5V Output, 1A-2A)   |=============> |  Micro-USB / Type-C   |
+-----------------------+               +-----------------------+
                                                    |
                                      VIN Pin passes 5V to:
                                      - MQ-135 VCC
                                      - GP2Y Pin 1 & Pin 6
```

* **How to connect:** Plug any standard USB cable from your power bank directly into the ESP32's USB port!
* **Why it works:** The ESP32 board's onboard Schottky diode feeds the 5V directly to the **VIN (5V) pin**, which simultaneously powers your MQ-135 heater and the Sharp dust sensor with zero extra wiring.
* **Battery Life:** A compact $5,000\,\text{mAh}$ power bank will power AeroSense continuously for **10 to 14 hours**.

---

### Option 2: 18650 / Li-ion Battery (3.7V) with 5V Boost Converter

If building an enclosed DIY project using a raw rechargeable 3.7V lithium cell:

```text
 3.7V 18650 Li-ion          5V Step-Up (Boost) Module           ESP32 DevKit
+------------------+         +---------------------+         +-----------------+
|  [+] (Positive)  |-------->| IN+            OUT+ |-------->| VIN Pin (5V)    |
|                  |         |     (Steps up       |         |                 |
|                  |         |      to 5.0V)       |         |                 |
|  [-] (Negative)  |-------->| IN-            OUT- |-------->| GND Pin         |
+------------------+         +---------------------+         +-----------------+
```

* **Recommended Modules:**
  * **TP4056 + Booster Combo** (e.g., 5V 1A Boost + Charger module with Micro-USB charging port).
  * **1S Power Bank Module (IP5306)**.
* **Wiring Steps:**
  1. Connect **Battery (+)** to **IN+** (or B+) on the module.
  2. Connect **Battery (-)** to **IN-** (or B-) on the module.
  3. Connect **OUT+ (5V)** to the **ESP32 VIN pin**.
  4. Connect **OUT- (GND)** to any **ESP32 GND pin**.

---

### Option 3: 2S Li-ion Battery (7.4V) or 9V Battery with 5V Buck (Step-Down) Converter

If using a 9V alkaline battery or 7.4V 2S LiPo pack:

```text
  7.4V - 9V Battery           5V Step-Down (Buck) Module        ESP32 DevKit
+------------------+         +---------------------+         +-----------------+
|  [+] (Positive)  |-------->| IN+            OUT+ |-------->| VIN Pin (5V)    |
|                  |         |    (Regulates to    |         |                 |
|                  |         |     stable 5.0V)    |         |                 |
|  [-] (Negative)  |-------->| IN-            OUT- |-------->| GND Pin         |
+------------------+         +---------------------+         +-----------------+
```

* **Recommended Module:** **LM2596** or **Mini360** Step-down buck regulator.
* **Important:** Adjust the trimmer screw to output **exactly 5.0V** *before* connecting it to the ESP32!

---

### Critical Safety Rules:

1. ⚠️ **NEVER connect a raw 9V or 12V battery directly to the VIN pin without a 5V buck converter!** Because the MQ-135 and Sharp GP2Y sensors are connected to the VIN pin for their 5V power, feeding 9V into VIN will immediately fry both sensors!
2. ⚠️ **NEVER connect power to the 3.3V pin.** The 3.3V pin bypasses all onboard voltage regulation. Overvoltage here will destroy the ESP32 microcontroller chip.
3. ⚠️ **Always share a Common Ground.** The battery ground, sensor grounds, and ESP32 GND must all be tied together.


