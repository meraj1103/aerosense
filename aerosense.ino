/*
 * AeroSense - Dual Sensor Air Quality Monitor (ESP32)
 * 1. MQ-135 Gas / VOC / Smoke Sensor
 * 2. Sharp GP2Y1014AU0F Optical Dust / PM2.5 Sensor (See connection.md)
 * 
 * Hardware Connections:
 *   MQ-135 VCC          --> ESP32 VIN (5V)
 *   MQ-135 GND          --> ESP32 GND
 *   MQ-135 AOUT         --> ESP32 GPIO 34 (ADC1 pin)
 *   MQ-135 DOUT         --> ESP32 GPIO 25 (Optional)
 * 
 *   GP2Y1014AU0F Pin 1  --> ESP32 VIN (5V) via 150Ω resistor + 220µF cap to GND
 *   GP2Y1014AU0F Pin 2  --> ESP32 GND
 *   GP2Y1014AU0F Pin 3  --> ESP32 GPIO 26 (LED pulse control)
 *   GP2Y1014AU0F Pin 4  --> ESP32 GND
 *   GP2Y1014AU0F Pin 5  --> ESP32 GPIO 35 (Vo analog read, ADC1 pin)
 *   GP2Y1014AU0F Pin 6  --> ESP32 VIN (5V)
 * 
 *   Status LED          --> ESP32 GPIO 2 (Built-in Blue LED)
 */

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// ==========================================
// Hardware Pin Definitions
// ==========================================
// Sensor 1: MQ-135 Gas Sensor
#define MQ135_ANALOG_PIN   34  // ADC1 Channel 6
#define MQ135_DIGITAL_PIN  25  // Optional digital threshold pin

// Sensor 2: GP2Y1014AU0F Optical Dust Sensor
#define DUST_ANALOG_PIN    35  // ADC1 Channel 7 (Vo Pin 5)
#define DUST_LED_PIN       26  // Digital Output (LED Pulse Pin 3)

// Status LED
#define STATUS_LED_PIN     2   // Onboard Blue LED

// ==========================================
// Feature Flags
// ==========================================
// GP2Y1014AU0F is physically connected & active!
#define ENABLE_DUST_SENSOR true

// ==========================================
// BLE Service & Characteristic (Nordic UART Service)
// ==========================================
#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

BLEServer *pServer = NULL;
BLECharacteristic *pTxCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;

// ==========================================
// Global Variables & State
// ==========================================
// MQ-135 Gas Variables
int rawValue = 0;
float voltage = 0.0;
int digitalAlert = HIGH;
String airQuality = "Warming Up...";
String airColor = "#64748b";
int qualityCode = 0;

// GP2Y1014AU0F Dust Variables
int dustRawValue = 0;
float dustDensityMgM3 = 0.0;
float dustDensityUgM3 = 0.0;
float dustVoltage = 0.0;
String dustQuality = "Clean";
int dustQualityCode = 0;

// Composite Air Quality
String combinedQuality = "Clean Air";
String combinedColor = "#10b981";

unsigned long lastSensorRead = 0;
const unsigned long SENSOR_INTERVAL = 1500; // Read every 1.5 seconds
const unsigned long PREHEAT_DURATION_MS = 180000; // 3 minutes preheat
unsigned long startTime = 0;

// BLE Server Callbacks
class MyServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
    digitalWrite(STATUS_LED_PIN, HIGH);
    Serial.println("\n[Bluetooth Connected!]");
  }

  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
    digitalWrite(STATUS_LED_PIN, LOW);
    Serial.println("\n[Bluetooth Disconnected]");
    // Restart advertising immediately so devices can reconnect
    BLEDevice::startAdvertising();
  }
};

// ==========================================
// Safe Chunked BLE Notification
// ==========================================
void sendBleString(const String &data) {
  if (!deviceConnected || pTxCharacteristic == NULL) return;

  const int CHUNK_SIZE = 20; // 20-byte BLE MTU standard chunk
  int len = data.length();
  for (int i = 0; i < len; i += CHUNK_SIZE) {
    int chunkLen = min(CHUNK_SIZE, len - i);
    pTxCharacteristic->setValue((uint8_t*)(data.c_str() + i), chunkLen);
    pTxCharacteristic->notify();
    delay(10);
  }
}

// ==========================================
// GP2Y1014AU0F Dust Sensor Reading Function
// ==========================================
void readDustSensor() {
  if (!ENABLE_DUST_SENSOR) {
    dustRawValue = 0;
    dustDensityMgM3 = 0.0;
    dustDensityUgM3 = 0.0;
    dustVoltage = 0.0;
    dustQuality = "Standby (Unplugged)";
    dustQualityCode = 0;
    return;
  }

  // To eliminate optical flicker, dust motes, and ADC electrical ripple,
  // take 8 fast oversampled optical pulses (spaced by ~10ms datasheet spec)
  // and compute a trimmed median average.
  const int NUM_PULSES = 8;
  float pulseVoltages[NUM_PULSES];
  int pulseRaws[NUM_PULSES];

  for (int i = 0; i < NUM_PULSES; i++) {
    // 1. Energize internal IR LED (Active LOW drive via transistor)
    digitalWrite(DUST_LED_PIN, LOW);
    delayMicroseconds(280);

    // 2. CRITICAL: Sample analog voltage precisely at 280 µs WHILE LED is ON!
    pulseRaws[i] = analogRead(DUST_ANALOG_PIN);
    #if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
      pulseVoltages[i] = analogReadMilliVolts(DUST_ANALOG_PIN) / 1000.0;
    #else
      pulseVoltages[i] = (pulseRaws[i] / 4095.0) * 3.3;
    #endif

    // 3. Complete remaining pulse time (total ON period = 320 µs)
    delayMicroseconds(40);
    digitalWrite(DUST_LED_PIN, HIGH); // De-energize IR LED

    // 4. Inter-pulse recovery period (~9.64 ms, keeping total cycle ~10 ms)
    delayMicroseconds(9640);
  }

  // Bubble sort to discard lowest and highest pulse spikes
  for (int i = 0; i < NUM_PULSES - 1; i++) {
    for (int j = i + 1; j < NUM_PULSES; j++) {
      if (pulseVoltages[i] > pulseVoltages[j]) {
        float tempV = pulseVoltages[i];
        pulseVoltages[i] = pulseVoltages[j];
        pulseVoltages[j] = tempV;

        int tempR = pulseRaws[i];
        pulseRaws[i] = pulseRaws[j];
        pulseRaws[j] = tempR;
      }
    }
  }

  // Discard 2 lowest and 2 highest outliers, average middle 4 pulses
  float sumVolt = 0.0;
  long sumRaw = 0;
  for (int i = 2; i < NUM_PULSES - 2; i++) {
    sumVolt += pulseVoltages[i];
    sumRaw += pulseRaws[i];
  }
  dustVoltage = sumVolt / (NUM_PULSES - 4);
  dustRawValue = sumRaw / (NUM_PULSES - 4);

  // Sharp GP2Y characteristic conversion:
  // Typical clean-air baseline V0 is ~0.58V to 0.85V.
  // Sensitivity K = 0.5V / 0.1 mg/m3 = 5.0 V / (mg/m3) => 0.17 mg/m3 per volt.
  // Standard equation: Dust (mg/m3) = 0.17 * Voltage - 0.1
  dustDensityMgM3 = (0.17 * dustVoltage) - 0.1;
  if (dustDensityMgM3 < 0.0) dustDensityMgM3 = 0.0;

  // Convert mg/m3 to ug/m3 (standard international PM2.5 metric)
  dustDensityUgM3 = dustDensityMgM3 * 1000.0;

  // Dust quality index based on US EPA / WHO Air Quality Standards for PM2.5
  if (dustDensityUgM3 < 35.0) {
    dustQuality = "Clean";
    dustQualityCode = 0;
  } else if (dustDensityUgM3 < 75.0) {
    dustQuality = "Moderate";
    dustQualityCode = 1;
  } else if (dustDensityUgM3 < 150.0) {
    dustQuality = "Poor";
    dustQualityCode = 2;
  } else {
    dustQuality = "Hazardous";
    dustQualityCode = 3;
  }
}

// ==========================================
// Main Sensor Reading Function
// ==========================================
void readSensor() {
  // 1. Read MQ-135 Gas Sensor
  rawValue = analogRead(MQ135_ANALOG_PIN);
  
  #if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
    voltage = analogReadMilliVolts(MQ135_ANALOG_PIN) / 1000.0;
  #else
    voltage = (rawValue / 4095.0) * 3.3;
  #endif

  digitalAlert = digitalRead(MQ135_DIGITAL_PIN);

  // Air quality threshold classification (ESP32 12-bit ADC: 0 - 4095, ~0 to 3.3V)
  // MQ-135 typical baseline in clean room air sits between 1500 - 2100 (1.3V - 1.8V)
  if (rawValue < 2100) {
    airQuality = "Clean Air";
    airColor = "#10b981";
    qualityCode = 0;
  } else if (rawValue < 2700) {
    airQuality = "Moderate";
    airColor = "#f59e0b";
    qualityCode = 1;
  } else if (rawValue < 3300) {
    airQuality = "Poor Air";
    airColor = "#f97316";
    qualityCode = 2;
  } else {
    airQuality = "Hazardous";
    airColor = "#ef4444";
    qualityCode = 3;
  }

  // 2. Read GP2Y1014AU0F Optical Dust Sensor
  readDustSensor();

  // 3. Compute Composite Air Quality (Worst of Gas and Dust)
  int maxSeverity = qualityCode;
  if (dustQualityCode > maxSeverity) maxSeverity = dustQualityCode;

  if (maxSeverity == 0) {
    combinedQuality = "Clean Air";
    combinedColor = "#10b981";
  } else if (maxSeverity == 1) {
    combinedQuality = "Moderate";
    combinedColor = "#f59e0b";
  } else if (maxSeverity == 2) {
    combinedQuality = "Poor Air";
    combinedColor = "#f97316";
  } else {
    combinedQuality = "Hazardous";
    combinedColor = "#ef4444";
  }

  unsigned long now = millis();
  unsigned long uptimeSec = (now - startTime) / 1000;
  long preheatRemaining = 0;
  if (now - startTime < PREHEAT_DURATION_MS) {
    preheatRemaining = (PREHEAT_DURATION_MS - (now - startTime)) / 1000;
  }

  // Construct JSON packet
  String json = "{";
  json += "\"raw\":" + String(rawValue) + ",";
  json += "\"voltage\":" + String(voltage, 2) + ",";
  json += "\"status\":\"" + String(digitalAlert == LOW ? "ALERT" : "NORMAL") + "\",";
  json += "\"quality\":\"" + airQuality + "\",";
  json += "\"color\":\"" + airColor + "\",";
  json += "\"qualityCode\":" + String(qualityCode) + ",";
  json += "\"dustRaw\":" + String(dustRawValue) + ",";
  json += "\"dustUgM3\":" + String(dustDensityUgM3, 1) + ",";
  json += "\"dustMgM3\":" + String(dustDensityMgM3, 3) + ",";
  json += "\"dustVoltage\":" + String(dustVoltage, 2) + ",";
  json += "\"dustQuality\":\"" + dustQuality + "\",";
  json += "\"dustQualityCode\":" + String(dustQualityCode) + ",";
  json += "\"dustEnabled\":" + String(ENABLE_DUST_SENSOR ? "true" : "false") + ",";
  json += "\"combinedQuality\":\"" + combinedQuality + "\",";
  json += "\"combinedColor\":\"" + combinedColor + "\",";
  json += "\"uptimeSec\":" + String(uptimeSec) + ",";
  json += "\"preheatSecRemaining\":" + String(preheatRemaining);
  json += "}\n";

  // Send via BLE
  if (deviceConnected) {
    sendBleString(json);
  }

  // Print JSON to USB Serial for Web Dashboard
  Serial.print(json);

  // Human-readable debug text prefixed with [LOG]
  Serial.print("[LOG] Gas ADC: ");
  Serial.print(rawValue);
  Serial.print(" (");
  Serial.print(airQuality);
  Serial.print(") | PM2.5 Dust: ");
  if (ENABLE_DUST_SENSOR) {
    Serial.print(dustDensityUgM3, 1);
    Serial.print(" ug/m3 (");
    Serial.print(dustQuality);
    Serial.print(", ");
    Serial.print(dustVoltage, 2);
    Serial.print("V)");
  } else {
    Serial.print("Standby (Unplugged)");
  }
  Serial.print(" | Combined: ");
  Serial.print(combinedQuality);
  Serial.print(" | BT: ");
  Serial.println(deviceConnected ? "Connected" : "Advertising");
}

// ==========================================
// Setup
// ==========================================
void setup() {
  // 115200 baud is standard for ESP32 and matches boot output
  Serial.begin(115200);
  delay(1000);

  pinMode(STATUS_LED_PIN, OUTPUT);
  pinMode(MQ135_ANALOG_PIN, INPUT);
  
  // Use INPUT_PULLUP so if DOUT (optional) is not connected, it defaults to HIGH (NORMAL)
  pinMode(MQ135_DIGITAL_PIN, INPUT_PULLUP);

  // Initialize GP2Y1014AU0F Dust Sensor Pins
  pinMode(DUST_LED_PIN, OUTPUT);
  digitalWrite(DUST_LED_PIN, HIGH); // Default OFF (active LOW)
  pinMode(DUST_ANALOG_PIN, INPUT);

  // Initialize ADC1 Channels (Safe with Bluetooth)
  analogRead(MQ135_ANALOG_PIN);
  analogRead(DUST_ANALOG_PIN);
  analogReadResolution(12);
  analogSetPinAttenuation(MQ135_ANALOG_PIN, ADC_11db);
  analogSetPinAttenuation(DUST_ANALOG_PIN, ADC_11db);

  startTime = millis();

  Serial.println("\n==============================================");
  Serial.println("   AeroSense: ESP32 Bluetooth (BLE) Server    ");
  Serial.println("   Dual Sensors: MQ-135 Gas + GP2Y1014AU0F    ");
  Serial.println("==============================================");

  // Initialize BLE Device
  BLEDevice::init("AeroSense-BLE");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create Nordic UART Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create TX Characteristic (Notify)
  pTxCharacteristic = pService->createCharacteristic(
                        CHARACTERISTIC_UUID_TX,
                        BLECharacteristic::PROPERTY_NOTIFY
                      );
  pTxCharacteristic->addDescriptor(new BLE2902());

  pService->start();

  // Configure BLE Advertising
  // Important: Broadcast name in primary packet so Windows & Chrome Web Bluetooth detect it instantly!
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  
  BLEAdvertisementData oAdvData;
  oAdvData.setName("AeroSense-BLE");
  oAdvData.setFlags(0x06); // General discoverable mode + BR/EDR not supported
  pAdvertising->setAdvertisementData(oAdvData);

  // Put 128-bit Service UUID into Scan Response to stay well within 31-byte advertising packet limit
  BLEAdvertisementData oScanResponseData;
  oScanResponseData.setCompleteServices(BLEUUID(SERVICE_UUID));
  pAdvertising->setScanResponseData(oScanResponseData);

  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();

  Serial.println("Bluetooth Advertising Started as: 'AeroSense-BLE'");
  Serial.println("Waiting for Web Dashboard or Phone to connect...\n");
}

// ==========================================
// Loop
// ==========================================
void loop() {
  unsigned long currentMillis = millis();

  if (currentMillis - lastSensorRead >= SENSOR_INTERVAL) {
    lastSensorRead = currentMillis;

    // Pulse LED if not connected to indicate advertising
    if (!deviceConnected) {
      digitalWrite(STATUS_LED_PIN, HIGH);
      delay(30);
      digitalWrite(STATUS_LED_PIN, LOW);
    }

    readSensor();
  }

  // Handle reconnecting advertising if disconnected
  if (!deviceConnected && oldDeviceConnected) {
    delay(500);
    BLEDevice::startAdvertising();
    Serial.println("Bluetooth Restarted Advertising...");
    oldDeviceConnected = deviceConnected;
  }
  if (deviceConnected && !oldDeviceConnected) {
    oldDeviceConnected = deviceConnected;
  }
}
