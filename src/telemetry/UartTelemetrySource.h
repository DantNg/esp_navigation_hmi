/**
 * @file UartTelemetrySource.h
 * @brief MAVLink byte source backed by a hardware UART (drone telemetry radio).
 */
#ifndef UART_TELEMETRY_SOURCE_H
#define UART_TELEMETRY_SOURCE_H

#include <HardwareSerial.h>

#include "telemetry/ITelemetrySource.h"

namespace telemetry {

class UartTelemetrySource : public ITelemetrySource {
public:
    UartTelemetrySource(int uartNum, int rxPin, int txPin, uint32_t baud);

    /** Override the baud rate before begin() / before the source is selected. */
    void setBaud(uint32_t baud) { baud_ = baud; }

    bool begin() override;
    int  read(uint8_t* buf, size_t maxLen) override;
    const char* name() const override { return "UART"; }

private:
    HardwareSerial serial_;
    int       rxPin_;
    int       txPin_;
    uint32_t  baud_;
    bool      started_ = false;
};

}  // namespace telemetry

#endif /* UART_TELEMETRY_SOURCE_H */
