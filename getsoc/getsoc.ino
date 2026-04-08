#include <Wire.h>
#include <Adafruit_INA219.h>
#include <Preferences.h>

Adafruit_INA219 ina219;
Preferences preferences;

// ─────────────────────────────────────────
// Battery Config
// ─────────────────────────────────────────
const float BATTERY_CAPACITY_MAH = 8000.0;
const float COULOMB_EFFICIENCY = 0.98;

// Rest detection
const float REST_CURRENT_MA = 20.0;
const float REST_SECONDS_FOR_CORR = 45.0;

// Filter
float filteredCurrent = 0;
const float alpha = 0.2;

// ─────────────────────────────────────────
// OCV TABLE (PER CELL)
// ─────────────────────────────────────────
const int OCV_POINTS = 18;

const float OCV_VOLTAGE[OCV_POINTS] = {
    3.00, 3.10, 3.20, 3.30,
    3.40, 3.50, 3.60, 3.70,
    3.75, 3.80, 3.85, 3.90,
    3.95, 4.00, 4.05, 4.10,
    4.15, 4.20};

const float OCV_SOC[OCV_POINTS] = {
    0, 5, 10, 15,
    20, 25, 30, 40,
    50, 60, 70, 80,
    85, 90, 92, 95,
    98, 100};

// ─────────────────────────────────────────
// State Variables
// ─────────────────────────────────────────
float soc = 50.0;
float consumed_mah = 0;
float restSeconds = 0;
bool initialized = false;

unsigned long lastTime = 0;
float lastVoltage = 0;

// ─────────────────────────────────────────
// Voltage → SOC
// ─────────────────────────────────────────
float voltageToSOC(float v)
{
    if (v <= OCV_VOLTAGE[0])
        return OCV_SOC[0];
    if (v >= OCV_VOLTAGE[OCV_POINTS - 1])
        return OCV_SOC[OCV_POINTS - 1];

    for (int i = 0; i < OCV_POINTS - 1; i++)
    {
        if (v >= OCV_VOLTAGE[i] && v <= OCV_VOLTAGE[i + 1])
        {
            float t = (v - OCV_VOLTAGE[i]) / (OCV_VOLTAGE[i + 1] - OCV_VOLTAGE[i]);
            return OCV_SOC[i] + t * (OCV_SOC[i + 1] - OCV_SOC[i]);
        }
    }
    return 50.0;
}

// ─────────────────────────────────────────
// Setup
// ─────────────────────────────────────────
void setup()
{
    Serial.begin(115200);
    delay(1000);

    if (!ina219.begin())
    {
        Serial.println("INA219 not found!");
        while (1)
            ;
    }

    // Load saved SOC
    preferences.begin("battery", false);
    soc = preferences.getFloat("soc", 50.0);
    preferences.end();

    Serial.print("Loaded SOC: ");
    Serial.println(soc);

    lastTime = millis();
}

// ─────────────────────────────────────────
// Loop
// ─────────────────────────────────────────
void loop()
{

    float shuntVoltage = ina219.getShuntVoltage_mV();
    float busVoltage = ina219.getBusVoltage_V();
    float current_mA = ina219.getCurrent_mA();

    float loadVoltage = busVoltage + (shuntVoltage / 1000.0);
    float cellVoltage = loadVoltage / 3.0;

    // Noise filtering
    filteredCurrent = alpha * current_mA + (1 - alpha) * filteredCurrent;

    if (fabs(filteredCurrent) < 5.0)
        filteredCurrent = 0;

    // Time delta
    unsigned long now = millis();
    float dt_hours = (now - lastTime) / 3600000.0;
    float dt_seconds = (now - lastTime) / 1000.0;
    lastTime = now;

    // Voltage stability check
    bool voltageStable = fabs(loadVoltage - lastVoltage) < 0.01;
    lastVoltage = loadVoltage;

    bool atRest = (fabs(filteredCurrent) < REST_CURRENT_MA) && voltageStable;

    // ───── INITIALIZATION ─────
    if (!initialized)
    {
        if (atRest)
        {
            soc = voltageToSOC(cellVoltage);
            consumed_mah = BATTERY_CAPACITY_MAH * (1 - soc / 100.0);
            initialized = true;

            Serial.print("[INIT SOC] ");
            Serial.println(soc);
        }
        else
        {
            Serial.println("Waiting for rest...");
            delay(1000);
            return;
        }
    }

    // ───── COULOMB COUNTING ─────
    float delta_mah = filteredCurrent * dt_hours;

    if (delta_mah > 0)
        consumed_mah += delta_mah / COULOMB_EFFICIENCY;
    else
        consumed_mah += delta_mah * COULOMB_EFFICIENCY;

    consumed_mah = constrain(consumed_mah, 0, BATTERY_CAPACITY_MAH);

    float soc_coulomb = 100.0 * (1 - consumed_mah / BATTERY_CAPACITY_MAH);

    // ───── REST CORRECTION ─────
    if (atRest)
    {
        restSeconds += dt_seconds;

        if (restSeconds > REST_SECONDS_FOR_CORR)
        {
            float soc_ocv = voltageToSOC(cellVoltage);

            float weight = 0.7;
            soc = weight * soc_ocv + (1 - weight) * soc_coulomb;

            consumed_mah = BATTERY_CAPACITY_MAH * (1 - soc / 100.0);
        }
        else
        {
            soc = soc_coulomb;
        }
    }
    else
    {
        restSeconds = 0;
        soc = soc_coulomb;
    }

    soc = constrain(soc, 0, 100);

    // ───── FULL / EMPTY FIX ─────
    if (loadVoltage >= 12.5 && fabs(filteredCurrent) < 50)
    {
        soc = 100;
        consumed_mah = 0;
    }

    if (loadVoltage <= 9.0)
    {
        soc = 0;
    }

    // ───── SAVE SOC ─────
    static int saveCounter = 0;
    saveCounter++;

    if (saveCounter >= 20)
    {
        preferences.begin("battery", false);
        preferences.putFloat("soc", soc);
        preferences.end();
        saveCounter = 0;
    }

    // ───── OUTPUT ─────
    Serial.println("-----------------------------");
    Serial.print("Voltage: ");
    Serial.println(loadVoltage);
    Serial.print("Current: ");
    Serial.println(filteredCurrent);
    Serial.print("SOC: ");
    Serial.println(soc);
    Serial.print("Consumed mAh: ");
    Serial.println(consumed_mah);

    delay(500);
}