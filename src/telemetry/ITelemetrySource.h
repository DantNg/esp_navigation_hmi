/**
 * @file ITelemetrySource.h
 * @brief Abstraction over the byte source carrying MAVLink (UART or USB-CDC).
 *
 * LinkManager depends on this interface, not on a concrete Serial. Switching the
 * physical link at runtime is just swapping the implementation (LSP), and adding
 * a new transport later means adding a class, not editing LinkManager (OCP/DIP).
 */
#ifndef I_TELEMETRY_SOURCE_H
#define I_TELEMETRY_SOURCE_H

#include <cstdint>
#include <cstddef>

namespace telemetry {

class ITelemetrySource {
public:
    virtual ~ITelemetrySource() = default;

    /** Open the underlying transport. Safe to call again after a source switch. */
    virtual bool begin() = 0;

    /** Read up to maxLen bytes; returns the count (0 when nothing available). */
    virtual int read(uint8_t* buf, size_t maxLen) = 0;

    /** Short human label shown on the UI / in link stats (<= 11 chars). */
    virtual const char* name() const = 0;
};

}  // namespace telemetry

#endif /* I_TELEMETRY_SOURCE_H */
