#include "telemetry/UsbTelemetrySource.h"

namespace telemetry {

int UsbTelemetrySource::read(uint8_t* buf, size_t maxLen) {
    int n = 0;
    while ((size_t)n < maxLen && stream_.available() > 0) {
        buf[n++] = (uint8_t)stream_.read();
    }
    return n;
}

}  // namespace telemetry
