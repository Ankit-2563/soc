#include <Wire.h>
#include <Adafruit_INA219.h>

Adafruit_INA219 ina219;

// ─── I2C Pin Definitions ───────────────────────────────────────────────
// ESP32 default I2C pins
#define SDA_PIN 21
#define SCL_PIN 22

// ESP8266 (NodeMCU) → uncomment below and comment out ESP32 pins above
// #define SDA_PIN 4   // D2
// #define SCL_PIN 5   // D1
// ───────────────────────────────────────────────────────────────────────

// ─── Battery Specs (18650 3S Li-ion) ──────────────────────────────────
#define BATTERY_MAX_V 12.6
#define BATTERY_NOM_V 11.1
#define BATTERY_CUTOFF 9.0
// ───────────────────────────────────────────────────────────────────────

float getBatteryPercent(float voltage)
{
    if (voltage >= 12.6)
        return 100.0;
    if (voltage >= 12.2)
        return 90.0;
    if (voltage >= 11.8)
        return 75.0;
    if (voltage >= 11.5)
        return 60.0;
    if (voltage >= 11.2)
        return 45.0;
    if (voltage >= 11.0)
        return 30.0;
    if (voltage >= 10.5)
        return 20.0;
    if (voltage >= 10.0)
        return 10.0;
    if (voltage >= 9.0)
        return 5.0;
    return 0.0; // below cutoff — battery critically low
}

String getBatteryStatus(float voltage)
{
    if (voltage >= 12.4)
        return "FULL";
    if (voltage >= 11.5)
        return "GOOD";
    if (voltage >= 10.5)
        return "LOW";
    if (voltage >= 9.0)
        return "CRITICAL";
    return "DEAD — Stop using!";
}

void setup(void)
{
    Serial.begin(115200);
    while (!Serial)
    {
        delay(1);
    }

    Serial.println("=============================");
    Serial.println("  INA219 Battery Monitor");
    Serial.println("  18650 3S Li-ion Pack");
    Serial.println("=============================");
    Serial.println("");
    Serial.println("Wiring:");
    Serial.println("  Battery (+) → INA219 VIN+");
    Serial.println("  INA219 VIN− → Load (+)");
    Serial.println("  Battery (−) → Load (−) → ESP GND  [common ground]");
    Serial.println("  INA219 VCC  → ESP 3.3V");
    Serial.println("  INA219 GND  → ESP GND");
    Serial.printf("  INA219 SDA  → ESP GPIO %d\n", SDA_PIN);
    Serial.printf("  INA219 SCL  → ESP GPIO %d\n", SCL_PIN);
    Serial.println("");

    // Initialize I2C with defined pins
    Wire.begin(SDA_PIN, SCL_PIN);

    if (!ina219.begin())
    {
        Serial.println("Failed to find INA219 chip!");
        Serial.println("   Check wiring and I2C address (default: 0x40)");
        while (1)
        {
            delay(10);
        }
    }

    // Default calibration: 32V, 2A — suitable for this 11.1V battery
    // Uncomment below for better precision if load current stays under 1A:
    // ina219.setCalibration_32V_1A();

    Serial.println("INA219 found! Starting measurements...");
    Serial.println("");
    delay(1000);
}

void loop(void)
{
    float shuntvoltage = 0;
    float busvoltage = 0;
    float current_mA = 0;
    float loadvoltage = 0;
    float power_mW = 0;

    shuntvoltage = ina219.getShuntVoltage_mV();
    busvoltage = ina219.getBusVoltage_V();
    current_mA = ina219.getCurrent_mA();
    power_mW = ina219.getPower_mW();
    loadvoltage = busvoltage + (shuntvoltage / 1000);

    float batteryPercent = getBatteryPercent(loadvoltage);
    String batteryStatus = getBatteryStatus(loadvoltage);

    Serial.println("-----------------------------");
    Serial.print("Bus Voltage:   ");
    Serial.print(busvoltage);
    Serial.println(" V");
    Serial.print("Shunt Voltage: ");
    Serial.print(shuntvoltage);
    Serial.println(" mV");
    Serial.print("Load Voltage:  ");
    Serial.print(loadvoltage);
    Serial.println(" V");
    Serial.print("Current:       ");
    Serial.print(current_mA);
    Serial.println(" mA");
    Serial.print("Power:         ");
    Serial.print(power_mW);
    Serial.println(" mW");
    Serial.print("Battery:       ");
    Serial.print(batteryPercent);
    Serial.println(" %");
    Serial.print("Status:        ");
    Serial.println(batteryStatus);

    // Warning if battery is critically low
    if (loadvoltage <= BATTERY_CUTOFF)
    {
        Serial.println("WARNING: Battery below cutoff voltage! Disconnect load!");
    }

    Serial.println("");
    delay(2000);
}