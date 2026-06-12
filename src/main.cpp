/**
 * @file main.cpp
 * @brief Firmware entry point.
 *
 * All wiring lives in app::GroundStationApp (the composition root). Arduino's
 * setup()/loop() just delegate to it — main.cpp stays trivial on purpose.
 */
#include "app/GroundStationApp.h"

namespace {
app::GroundStationApp g_app;
}

void setup() {
    g_app.begin();
}

void loop() {
    g_app.loop();
}
