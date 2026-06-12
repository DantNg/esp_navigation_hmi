/**
 * @file TelemetryStore.h
 * @brief Thread-safe holder of the latest TelemetrySnapshot.
 *
 * Single responsibility: guard the shared snapshot with a FreeRTOS mutex so the
 * LinkTask (writer) and the UI / Map tasks (readers) never tear a read. Writers
 * mutate under lock via with(); readers copy out via get().
 */
#ifndef TELEMETRY_STORE_H
#define TELEMETRY_STORE_H

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "telemetry/TelemetryTypes.h"

namespace telemetry {

class TelemetryStore {
public:
    TelemetryStore();
    ~TelemetryStore();

    TelemetryStore(const TelemetryStore&) = delete;
    TelemetryStore& operator=(const TelemetryStore&) = delete;

    /** Copy the whole snapshot out under lock. */
    TelemetrySnapshot get() const;

    /**
     * Mutate the snapshot under lock.
     * @tparam Fn  callable taking `TelemetrySnapshot&`.
     */
    template <typename Fn>
    void with(Fn&& fn) {
        lock();
        fn(snapshot_);
        unlock();
    }

private:
    void lock() const;
    void unlock() const;

    mutable SemaphoreHandle_t mutex_;
    TelemetrySnapshot         snapshot_;
};

}  // namespace telemetry

#endif /* TELEMETRY_STORE_H */
