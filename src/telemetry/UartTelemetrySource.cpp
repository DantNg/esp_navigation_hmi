#include "telemetry/UartTelemetrySource.h"

namespace telemetry {

UartTelemetrySource::UartTelemetrySource(int uartNum, int rxPin, int txPin, uint32_t baud)
    : serial_(uartNum), rxPin_(rxPin), txPin_(txPin), baud_(baud) {}

bool UartTelemetrySource::begin() {
    if (!started_) {
        serial_.begin(baud_, SERIAL_8N1, rxPin_, txPin_);
        started_ = true;
    }
    return true;
}

int UartTelemetrySource::read(uint8_t* buf, size_t maxLen) {
    int n = 0;
    while ((size_t)n < maxLen && serial_.available() > 0) {
        buf[n++] = (uint8_t)serial_.read();
    }
    return n;
}

}  // namespace telemetry
