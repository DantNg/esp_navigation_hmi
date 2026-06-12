/**
 * @file UsbTelemetrySource.h
 * @brief MAVLink byte source backed by the native USB-CDC port.
 *
 * With ARDUINO_USB_CDC_ON_BOOT=1 the global `Serial` is the USB-CDC stream.
 * This source reads MAVLink from it (e.g. a USB tether to the flight controller
 * or a companion). NOTE: when USB is the active data link, avoid printing debug
 * logs to `Serial` as they would interleave with the binary stream.
 */
#ifndef USB_TELEMETRY_SOURCE_H
#define USB_TELEMETRY_SOURCE_H

#include <Arduino.h>

#include "telemetry/ITelemetrySource.h"

namespace telemetry {

class UsbTelemetrySource : public ITelemetrySource {
public:
    explicit UsbTelemetrySource(Stream& stream = Serial) : stream_(stream) {}

    bool begin() override { return true; }  /* Serial already up from boot */
    int  read(uint8_t* buf, size_t maxLen) override;
    const char* name() const override { return "USB"; }

private:
    Stream& stream_;
};

}  // namespace telemetry

#endif /* USB_TELEMETRY_SOURCE_H */
