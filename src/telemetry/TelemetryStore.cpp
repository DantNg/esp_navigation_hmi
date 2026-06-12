#include "telemetry/TelemetryStore.h"

namespace telemetry {

TelemetryStore::TelemetryStore() {
    mutex_ = xSemaphoreCreateMutex();
}

TelemetryStore::~TelemetryStore() {
    if (mutex_) vSemaphoreDelete(mutex_);
}

void TelemetryStore::lock() const {
    xSemaphoreTake(mutex_, portMAX_DELAY);
}

void TelemetryStore::unlock() const {
    xSemaphoreGive(mutex_);
}

TelemetrySnapshot TelemetryStore::get() const {
    lock();
    TelemetrySnapshot copy = snapshot_;
    unlock();
    return copy;
}

}  // namespace telemetry
