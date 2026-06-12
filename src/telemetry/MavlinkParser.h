/**
 * @file MavlinkParser.h
 * @brief Minimal incremental MAVLink v1/v2 framer.
 *
 * Single responsibility: turn a byte stream into complete frames. It does NOT
 * interpret payloads (that is TelemetryDecoder's job) and keeps the full raw
 * frame so it can be forwarded byte-for-byte. CRC is not validated (lite); the
 * length-driven state machine resyncs on the next STX after any glitch.
 */
#ifndef MAVLINK_PARSER_H
#define MAVLINK_PARSER_H

#include <cstdint>
#include <cstddef>

namespace telemetry {

/* v2 worst case: 10 (hdr) + 255 (payload) + 2 (crc) + 13 (sig) = 280. */
constexpr size_t kMavMaxFrame = 280;

struct MavlinkFrame {
    uint8_t        raw[kMavMaxFrame];  /* full frame, STX .. CRC[/sig] */
    uint16_t       len      = 0;       /* bytes used in raw            */
    uint32_t       msgid    = 0;
    uint8_t        sysid    = 0;
    uint8_t        compid   = 0;
    uint8_t        payloadLen = 0;
    uint16_t       payloadOffset = 0;  /* index of payload[0] in raw   */
    bool           v2       = false;

    const uint8_t* payload() const { return raw + payloadOffset; }
};

class MavlinkParser {
public:
    /**
     * Feed one byte. Returns true exactly when `out` now holds a complete frame.
     * @param errors  optional counter incremented on buffer overflow / resync.
     */
    bool parse(uint8_t byte, MavlinkFrame& out, uint32_t* errors = nullptr);

    void reset() { state_ = State::Sync; idx_ = 0; }

private:
    enum class State : uint8_t { Sync, Len, Header, Rest };

    State    state_      = State::Sync;
    uint8_t  buf_[kMavMaxFrame] = {0};
    uint16_t idx_        = 0;
    bool     v2_         = false;
    uint8_t  headerLen_  = 0;
    uint8_t  payloadLen_ = 0;
    uint8_t  incompat_   = 0;
    uint16_t remaining_  = 0;
};

}  // namespace telemetry

#endif /* MAVLINK_PARSER_H */
