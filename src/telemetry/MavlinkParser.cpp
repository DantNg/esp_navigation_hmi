#include "telemetry/MavlinkParser.h"

#include <cstring>

namespace telemetry {

namespace {
constexpr uint8_t kStxV1 = 0xFE;
constexpr uint8_t kStxV2 = 0xFD;
constexpr uint8_t kIncompatSigned = 0x01;
}  // namespace

bool MavlinkParser::parse(uint8_t c, MavlinkFrame& out, uint32_t* errors) {
    switch (state_) {
        case State::Sync:
            if (c == kStxV2 || c == kStxV1) {
                v2_       = (c == kStxV2);
                headerLen_ = v2_ ? 10 : 6;
                idx_      = 0;
                buf_[idx_++] = c;
                state_    = State::Len;
            }
            break;

        case State::Len:
            buf_[idx_++] = c;
            payloadLen_  = c;
            state_       = State::Header;
            break;

        case State::Header:
            buf_[idx_++] = c;
            if (idx_ == headerLen_) {
                if (v2_) incompat_ = buf_[2];
                else     incompat_ = 0;
                /* payload + 2-byte CRC + optional 13-byte signature */
                remaining_ = payloadLen_ + 2 +
                             ((v2_ && (incompat_ & kIncompatSigned)) ? 13 : 0);
                state_ = State::Rest;
            }
            break;

        case State::Rest:
            buf_[idx_++] = c;
            if (--remaining_ == 0) {
                out.len           = idx_;
                out.v2            = v2_;
                out.payloadLen    = payloadLen_;
                out.payloadOffset = headerLen_;
                if (v2_) {
                    out.sysid  = buf_[5];
                    out.compid = buf_[6];
                    out.msgid  = (uint32_t)buf_[7] |
                                 ((uint32_t)buf_[8] << 8) |
                                 ((uint32_t)buf_[9] << 16);
                } else {
                    out.sysid  = buf_[3];
                    out.compid = buf_[4];
                    out.msgid  = buf_[5];
                }
                memcpy(out.raw, buf_, idx_);
                state_ = State::Sync;
                return true;
            }
            break;
    }

    /* Overflow guard: corrupt length / lost sync -> drop and resync. */
    if (idx_ >= kMavMaxFrame) {
        if (errors) (*errors)++;
        state_ = State::Sync;
        idx_   = 0;
    }
    return false;
}

}  // namespace telemetry
