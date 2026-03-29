#pragma once
#include <Wire.h>
#include <Adafruit_INA219.h>
#include "raven_config.h"

static Adafruit_INA219 ina219(INA219_ADDR);
static bool   pwrAvail   = false;
static float  pwrBusV    = 0.0f;
static float  pwrCurrMA  = 0.0f;
static float  pwrPowerMW = 0.0f;

void powerBegin() {
    Wire.begin(INA219_SDA, INA219_SCL);
    pwrAvail = ina219.begin(&Wire);
    Serial.printf("[power] INA219 @ 0x%02X — %s\n", INA219_ADDR,
                  pwrAvail ? "OK" : "not found");
}

// Read all three values from the INA219 into the cache.
void powerSample() {
    if (!pwrAvail) return;
    pwrBusV    = ina219.getBusVoltage_V();
    pwrCurrMA  = ina219.getCurrent_mA();
    pwrPowerMW = ina219.getPower_mW();
}

String powerToJson() {
    return "{\"available\":"  + String(pwrAvail ? "true" : "false") +
           ",\"bus_v\":"      + String(pwrBusV,    3) +
           ",\"current_mA\":" + String(pwrCurrMA,  2) +
           ",\"power_mW\":"   + String(pwrPowerMW, 2) + "}";
}
