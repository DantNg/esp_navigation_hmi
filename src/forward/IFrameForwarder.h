/**
 * @file IFrameForwarder.h
 * @brief Abstraction over "send this raw MAVLink frame somewhere".
 *
 * Keeps the transport (UDP today, maybe TCP/serial later) behind an interface so
 * the ForwardTask drains a queue and calls send() without knowing the medium.
 */
#ifndef I_FRAME_FORWARDER_H
#define I_FRAME_FORWARDER_H

#include <cstddef>
#include <cstdint>

namespace forward {

class IFrameForwarder {
public:
    virtual ~IFrameForwarder() = default;

    /** Send one complete MAVLink frame. Returns false if it could not be sent. */
    virtual bool send(const uint8_t* data, size_t len) = 0;
};

}  // namespace forward

#endif /* I_FRAME_FORWARDER_H */
