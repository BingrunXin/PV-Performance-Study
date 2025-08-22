#include <Wire.h>
#include "TCA9548.h"
#include <RTClib.h>
#include <Adafruit_BMP280.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Adafruit_SSD1306.h>
#include <SPI.h>
#include <SD.h>
#include <WiFi.h>
#include <time.h>
#include "INA226.h"
#include <Max44009.h>

// ---------------- WiFi / NTP ----------------
const char* ssid = "your wifi";
const char* password = "your password";
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 28800;
const int daylightOffset_sec = 0;

// ---------------- TCA9548 & I2C ----------------
#define TCA_ADDR 0x70
TCA9548 tca(TCA_ADDR, &Wire);

// ---------------- RTC / Sensors ----------------
#define RTC_CHANNEL 0
RTC_DS3231 rtc;

#define BMP_ADDRESS 0x76
#define BMP_CHANNEL 1
Adafruit_BMP280 bmp;

#define MAX44009_ADDRESS 0x4B
#define MAX44009_CHANNEL 2
Max44009 max44009(MAX44009_ADDRESS);

// ---------- INA226 Configuration ----------
#define INA226_ADDRESS 0x44
#define INA226_CHANNEL 3
const float SHUNT_RESISTOR_OHM = 0.1;
const float MAX_EXPECTED_AMPS = 0.8;
INA226 ina226(INA226_ADDRESS, &Wire);

// ---------------- DS18B20 x2 ----------------
#define ONE_WIRE_BUS_1 4
#define ONE_WIRE_BUS_2 32
OneWire oneWire1(ONE_WIRE_BUS_1);
OneWire oneWire2(ONE_WIRE_BUS_2);
DallasTemperature sensors1(&oneWire1);
DallasTemperature sensors2(&oneWire2);

// ---------------- OLED ----------------
#define OLED_MOSI 23
#define OLED_CLK  18
#define OLED_DC   26
#define OLED_CS   25
#define OLED_RESET 27
Adafruit_SSD1306 display(128, 64, &SPI, OLED_DC, OLED_RESET, OLED_CS);

// ---------------- SD Card ----------------
#define SD_CS   13
#define SD_MOSI 15
#define SD_SCK  14
#define SD_MISO 2
SPIClass sdSPI(VSPI);
const char* data_filename = "/data.csv";

// ---------------- Status Flags ----------------
bool rtc_ok = false, bmp_ok = false, max44009_ok = false, ina226_ok = false;
bool ds18b20_1_ok = false, ds18b20_2_ok = false, sd_ok = false, wifi_ok = false;

DeviceAddress ds18b20_1_addr, ds18b20_2_addr;

// ---------------- Recovery Variables ----------------
const int SENSOR_FAIL_THRESHOLD = 3;
int max44009_fail_count = 0;
int bmp280_fail_count = 0;
int ina226_fail_count = 0;

// NTP Sync
unsigned long last_sync_time = 0;
const unsigned long sync_interval = 600000; // 10 minutes

// I2C Scanner
void scanI2CChannel(uint8_t channel) {
    tca.selectChannel(channel);
    Serial.printf("Scanning I2C channel %d...\n", channel);
    byte error, address;
    int nDevices = 0;
    for(address = 1; address < 127; address++ ) {
        Wire.beginTransmission(address);
        error = Wire.endTransmission();
        if (error == 0) {
            Serial.printf("I2C device found at address 0x%02X\n", address);
            nDevices++;
        }
    }
    if (nDevices == 0) {
        Serial.println("No I2C devices found on this channel.");
    }
}

// Initialization with retry
void ensureInit(const char* deviceName, bool (*initFunc)()) {
    Serial.printf("Initializing %s...\n", deviceName);
    while (!initFunc()) {
        Serial.printf("%s initialization failed. Retrying in 3 seconds...\n", deviceName);
        display.clearDisplay();
        display.setCursor(0, 0);
        display.setTextSize(1);
        display.setTextColor(SSD1306_WHITE);
        display.printf("Init Error:\n%s\n\nRetrying...", deviceName);
        display.display();
        delay(3000);
    }
    Serial.printf("%s initialized successfully!\n", deviceName);
}

// NTP Sync
void syncTimeFromNTP() {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Solar Panel Test");
  display.println("WiFi...");
  display.display();
  
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  
  int wifiTimeout = 0;
  while (WiFi.status() != WL_CONNECTED && wifiTimeout < 30) {
    delay(500);
    Serial.print(".");
    wifiTimeout++;
  }
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\nWiFi connection failed!");
    wifi_ok = false;
    delay(2000);
    return;
  }
  
  Serial.println("\nWiFi connected");
  wifi_ok = true;
  
  Serial.print("Waiting for NTP time sync");
  time_t now = 0;
  int ntpTimeout = 0;
  while (now < 24 * 3600 && ntpTimeout < 50) {
    delay(100);
    Serial.print(".");
    now = time(nullptr);
    ntpTimeout++;
  }
  
  if (now >= 24 * 3600) {
    Serial.println("\nTime synchronized");
    struct tm timeinfo;
    getLocalTime(&timeinfo);
    rtc.adjust(DateTime(timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                      timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec));
  } else {
    Serial.println("\nNTP sync failed, using RTC time");
  }
  delay(2000);
}

// OLED Init Progress
void displayInitStep(const char* module, int step, int total) {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.printf("Init %s...\n", module);
  display.printf("Step %d/%d\n", step, total);
  display.printf("%d%%\n", (step * 100) / total);
  display.display();
}

// Sensor Recovery
void recoverI2CDevice(const char* deviceName, uint8_t channel, bool &status_flag, bool (*init_func)()) {
    Serial.printf("WARNING: Persistent failure on %s. Attempting recovery...\n", deviceName);
    tca.selectChannel(channel);
    delay(100);
    if (init_func()) {
        Serial.printf("SUCCESS: %s recovered!\n", deviceName);
        status_flag = true;
    } else {
        Serial.printf("FAILURE: %s recovery failed. Marking as offline.\n", deviceName);
        status_flag = false;
    }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("System Startup...");

  Wire.begin(21, 22);
  Wire.setClock(100000);
  
  if (!display.begin(SSD1306_SWITCHCAPVCC)) {
    Serial.println(F("CRITICAL: SSD1306 allocation failed"));
    while (1);
  }
  display.clearDisplay();
  display.println("System Init...");
  display.display();

  int totalSteps = 7;

  displayInitStep("TCA9548", 1, totalSteps);
  ensureInit("TCA9548", []() { return tca.begin(); });

  displayInitStep("RTC", 2, totalSteps);
  ensureInit("RTC", []() { tca.selectChannel(RTC_CHANNEL); return rtc.begin(); });
  rtc_ok = true;

  syncTimeFromNTP();

  displayInitStep("BMP280", 3, totalSteps);
  ensureInit("BMP280", []() { tca.selectChannel(BMP_CHANNEL); return bmp.begin(BMP_ADDRESS); });
  bmp_ok = true;

  displayInitStep("MAX44009", 4, totalSteps);
  ensureInit("MAX44009", []() { tca.selectChannel(MAX44009_CHANNEL); return max44009.isConnected(); });
  max44009_ok = true;

  displayInitStep("INA226", 5, totalSteps);
  ensureInit("INA226", []() {
      tca.selectChannel(INA226_CHANNEL);
      if (!ina226.begin()) return false;
      ina226.reset();
      if (ina226.setMaxCurrentShunt(MAX_EXPECTED_AMPS, SHUNT_RESISTOR_OHM) != INA226_ERR_NONE) return false;
      ina226.setModeShuntBusContinuous();
      return true;
  });
  ina226_ok = true;

  displayInitStep("DS18B20 #1", 6, totalSteps);
  sensors1.begin();
  ensureInit("DS18B20 #1", []() { sensors1.requestTemperatures(); return sensors1.getDeviceCount() > 0 && sensors1.getAddress(ds18b20_1_addr, 0); });
  ds18b20_1_ok = true;

  displayInitStep("DS18B20 #2", 7, totalSteps);
  sensors2.begin();
  ensureInit("DS18B20 #2", []() { sensors2.requestTemperatures(); return sensors2.getDeviceCount() > 0 && sensors2.getAddress(ds18b20_2_addr, 0); });
  ds18b20_2_ok = true;

  displayInitStep("SD Card", 8, 8);
  sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  ensureInit("SD Card", [](){ return SD.begin(SD_CS, sdSPI); });
  sd_ok = true;
  
  if (!SD.exists(data_filename)) {
    File dataFile = SD.open(data_filename, FILE_WRITE);
    if (dataFile) {
      dataFile.println("Timestamp,Panel_Temp1_C,Panel_Temp2_C,Env_Temp_BMP_C,Pressure_hPa,Light_lux,Voltage_V,Current_mA,Power_mW");
      dataFile.close();
    } else {
      sd_ok = false;
    }
  }

  display.clearDisplay();
  display.println("All Systems GO!");
  display.display();
  Serial.println("Initialization Complete. Starting main loop...");
  delay(2000);
}

void loop() {
  if (millis() - last_sync_time > sync_interval) {
    syncTimeFromNTP();
    last_sync_time = millis();
  }

  sensors1.requestTemperatures();
  float temp1 = sensors1.getTempC(ds18b20_1_addr);
  sensors2.requestTemperatures();
  float temp2 = sensors2.getTempC(ds18b20_2_addr);
  
  float h = 0, t_dht = 0;

  tca.selectChannel(RTC_CHANNEL); 
  DateTime now = rtc.now();

  float pressure = 0, t_bmp = 0;
  if (bmp_ok) {
    tca.selectChannel(BMP_CHANNEL);
    pressure = bmp.readPressure() / 100.0F;
    t_bmp = bmp.readTemperature();
    if (isnan(pressure) || isnan(t_bmp)) {
        bmp280_fail_count++;
        if(bmp280_fail_count >= SENSOR_FAIL_THRESHOLD) {
            recoverI2CDevice("BMP280", BMP_CHANNEL, bmp_ok, []() { return bmp.begin(BMP_ADDRESS); });
            bmp280_fail_count = 0;
        }
    } else {
        bmp280_fail_count = 0;
    }
  }

  float lux = 0;
  if (max44009_ok) {
    tca.selectChannel(MAX44009_CHANNEL);
    lux = max44009.getLux();
    if (lux < 0) {
        max44009_fail_count++;
        if(max44009_fail_count >= SENSOR_FAIL_THRESHOLD) {
            recoverI2CDevice("MAX44009", MAX44009_CHANNEL, max44009_ok, []() { return max44009.isConnected(); });
            max44009_fail_count = 0;
        }
        lux = 0; 
    } else {
        max44009_fail_count = 0;
    }
  }

  float busvoltage = 0, current_mA = 0, power_mW = 0;
  if (ina226_ok) {
    tca.selectChannel(INA226_CHANNEL);
    busvoltage = ina226.getBusVoltage();
    current_mA = ina226.getCurrent() * 1000.0;
    power_mW = ina226.getPower() * 1000.0;
    if (isnan(busvoltage) || isnan(current_mA)) {
        ina226_fail_count++;
        if(ina226_fail_count >= SENSOR_FAIL_THRESHOLD) {
            recoverI2CDevice("INA226", INA226_CHANNEL, ina226_ok, []() { return ina226.begin() && (ina226.setMaxCurrentShunt(MAX_EXPECTED_AMPS, SHUNT_RESISTOR_OHM) == INA226_ERR_NONE); });
            ina226_fail_count = 0;
        }
    } else {
        ina226_fail_count = 0;
    }
  }

  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.printf("%02d-%02d %02d:%02d:%02d\n", now.month(), now.day(), now.hour(), now.minute(), now.second());
  display.printf("V:%.2f I:%.1f\n", busvoltage, current_mA);
  display.printf("P:%.1fmW L:%.0f\n", power_mW, lux);
  display.printf("Pnl T: %.1f, %.1f\n", temp1, temp2);
  display.printf("Env T: %.1f(BMP)\n", t_bmp);
  display.printf("Prs:%.0fhPa", pressure);
  display.display();

  if (sd_ok) {
    File dataFile = SD.open(data_filename, FILE_APPEND);
    if (dataFile) {
      char buffer[200];
      sprintf(buffer, "%04d-%02d-%02d %02d:%02d:%02d,%.2f,%.2f,%.2f,%.1f,%.1f,%.3f,%.2f,%.2f",
        now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second(),
        temp1, temp2, t_bmp, pressure, lux, busvoltage, current_mA, power_mW);
      dataFile.println(buffer);
      dataFile.close();
      Serial.println("SD write OK!");
    } else {
      Serial.println("SD write error!");
    }
  }

  Serial.println("╔══════════════════════════════════════════════════════════════╗");
  Serial.printf("║ Time: %04d-%02d-%02d %02d:%02d:%02d                             ║\n", 
    now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second());
  Serial.println("╠══════════════════════════════════════════════════════════════╣");
  Serial.printf("║ Voltage: %6.2f V | Current: %6.1f mA | Power: %6.1f mW ║\n", busvoltage, current_mA, power_mW);
  Serial.printf("║ Light: %9.1f lx                                      ║\n", lux);
  Serial.println("╠══════════════════════════════════════════════════════════════╣");
  Serial.printf("║ Panel T1: %5.1f C | Panel T2: %5.1f C | Env T_BMP: %5.1f C   ║\n", temp1, temp2, t_bmp);
  Serial.printf("║ Pressure: %7.1f hPa | DHT22 Status: FAILED             ║\n", pressure);
  Serial.println("╚══════════════════════════════════════════════════════════════╝\n");
  
  delay(1000);  //adjust suitably for the time period of the experiment
}
