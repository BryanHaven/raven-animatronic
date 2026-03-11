#pragma once
#include <Arduino.h>
#include <ESPmDNS.h>
#include "raven_device.h"

void mdnsBegin() {
    if (MDNS.begin(device.mdns_hostname.c_str())) {
        MDNS.addService("http", "tcp", 80);
        Serial.printf("[mdns] http://%s.local\n", device.mdns_hostname.c_str());
    } else {
        Serial.println("[mdns] Failed to start");
    }
}
