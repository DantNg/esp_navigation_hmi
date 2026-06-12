#!/usr/bin/env python3
"""
sim_telemetry.py - Stream simulated MAVLink to the Lite Ground Station.

Sends well-formed MAVLink v1 (correct CRC, so it also works when the device
forwards it to a real GCS) over a serial port — the ESP32's USB-CDC port or a
UART adapter. Useful to exercise the HUD, sidebar and the on-demand map without
a real autopilot.

It emits HEARTBEAT, ATTITUDE, VFR_HUD, SYS_STATUS, GPS_RAW_INT,
GLOBAL_POSITION_INT and the occasional STATUSTEXT. The position slowly moves so
the drone eventually reaches the map edge and the device requests a fresh map.

Usage
-----
    pip install pyserial
    python sim_telemetry.py --port COM5
    python sim_telemetry.py --port COM5 --lat 21.0285 --lon 105.8542 --speed 18
    python sim_telemetry.py --port COM5 --circle 400          # orbit instead
    python sim_telemetry.py --port COM5 --static              # don't move

Find the port with `pio device list` (or Device Manager). On the device, flip the
on-screen "Source: USB" switch (or set AppConfig.linkSourceUsb = true) so it reads
MAVLink from USB.
"""

import argparse
import math
import struct
import sys
import time

try:
    import serial  # pyserial
except ImportError:
    print("Error: pyserial required.  pip install pyserial")
    sys.exit(1)

# msgid -> CRC_EXTRA (from MAVLink common.xml)
CRC_EXTRA = {0: 50, 1: 124, 24: 24, 30: 39, 33: 104, 74: 20, 253: 83}

SYSID, COMPID = 1, 1
_seq = 0


def x25_crc(data, crc_extra):
    crc = 0xFFFF
    for b in tuple(data) + (crc_extra,):
        tmp = b ^ (crc & 0xFF)
        tmp = (tmp ^ (tmp << 4)) & 0xFF
        crc = ((crc >> 8) ^ (tmp << 8) ^ (tmp << 3) ^ (tmp >> 4)) & 0xFFFF
    return crc


def frame(msgid, payload):
    """Build a MAVLink v1 frame with a valid checksum."""
    global _seq
    hdr = struct.pack("<BBBB", len(payload), _seq & 0xFF, SYSID, COMPID)
    _seq += 1
    body = hdr + struct.pack("<B", msgid) + payload
    crc = x25_crc(body, CRC_EXTRA[msgid])
    return b"\xFE" + body + struct.pack("<H", crc)


# ---- message encoders -----------------------------------------------------
def heartbeat(custom_mode, armed):
    base_mode = 0x80 if armed else 0x00
    # type=2 (quad), autopilot=3 (ArduPilot), system_status=4 (ACTIVE)
    return frame(0, struct.pack("<IBBBBB", custom_mode, 2, 3, base_mode, 4, 3))


def attitude(t_ms, roll, pitch, yaw):
    return frame(30, struct.pack("<Iffffff", t_ms, roll, pitch, yaw, 0, 0, 0))


def sys_status(voltage_mv, current_ca, remaining):
    return frame(1, struct.pack("<IIIHHhHHHHHHb",
                                0, 0, 0, 0, voltage_mv, current_ca,
                                0, 0, 0, 0, 0, 0, remaining))


def gps_raw(lat, lon, alt_mm, sats):
    return frame(24, struct.pack("<QiiiHHHHBB",
                                 0, lat, lon, alt_mm, 120, 200, 0, 0, 3, sats))


def global_pos(t_ms, lat, lon, alt_mm, rel_mm, hdg_cdeg):
    return frame(33, struct.pack("<IiiiihhhH",
                                 t_ms, lat, lon, alt_mm, rel_mm, 0, 0, 0, hdg_cdeg))


def vfr_hud(airspeed, groundspeed, heading_deg, throttle, alt, climb):
    return frame(74, struct.pack("<ffffhH",
    float(airspeed),
    float(groundspeed),
    float(alt),
    float(climb),
    int(heading_deg),   # cast to int
    int(throttle)))     # cast to int



def statustext(severity, text):
    return frame(253, struct.pack("<B50s", severity, text.encode()[:50]))


# ---- main loop ------------------------------------------------------------
COPTER_MODES = {0: "STABILIZE", 5: "LOITER", 3: "AUTO", 6: "RTL"}


def main():
    ap = argparse.ArgumentParser(description="Simulated MAVLink streamer")
    ap.add_argument("--port", required=True, help="serial port, e.g. COM5 or /dev/ttyACM0")
    ap.add_argument("--baud", type=int, default=115200, help="ignored for USB-CDC")
    ap.add_argument("--lat", type=float, default=21.028511)
    ap.add_argument("--lon", type=float, default=105.804817)
    ap.add_argument("--speed", type=float, default=15.0, help="ground speed m/s")
    ap.add_argument("--heading", type=float, default=45.0, help="track deg (straight mode)")
    ap.add_argument("--circle", type=float, default=0.0, help="orbit radius m (0=straight)")
    ap.add_argument("--static", action="store_true", help="do not move")
    args = ap.parse_args()

    ser = serial.Serial(args.port, args.baud, timeout=0)
    print(f"Streaming MAVLink to {args.port} (Ctrl+C to stop)")

    lat, lon = args.lat, args.lon
    clat, clon = args.lat, args.lon
    t0 = time.time()
    last = {k: 0.0 for k in ("hb", "att", "vfr", "sys", "gps", "pos", "txt")}
    txt_i = 0

    try:
        while True:
            now = time.time()
            t = now - t0
            t_ms = int(t * 1000)

            # --- move ---
            if not args.static:
                if args.circle > 0:
                    w = args.speed / args.circle
                    ang = w * t
                    dN = args.circle * math.cos(ang)
                    dE = args.circle * math.sin(ang)
                    lat = clat + dN / 111320.0
                    lon = clon + dE / (111320.0 * math.cos(math.radians(clat)))
                    track = math.degrees(ang) + 90.0
                else:
                    dN = args.speed * math.cos(math.radians(args.heading)) * 0.05
                    dE = args.speed * math.sin(math.radians(args.heading)) * 0.05
                    lat += dN / 111320.0
                    lon += dE / (111320.0 * math.cos(math.radians(lat)))
                    track = args.heading
            else:
                track = args.heading

            roll = 0.45 * math.sin(t * 0.6)
            pitch = 0.20 * math.sin(t * 0.4)
            yaw = math.radians(track)
            alt = 60.0 + 15.0 * math.sin(t * 0.2)
            climb = 3.0 * math.cos(t * 0.2)
            airspeed = args.speed + 1.5 * math.sin(t)

            if now - last["att"] >= 0.05:           # 20 Hz
                ser.write(attitude(t_ms, roll, pitch, yaw)); last["att"] = now
            if now - last["vfr"] >= 0.10:           # 10 Hz
                ser.write(vfr_hud(airspeed, args.speed, track, 55, alt, climb)); last["vfr"] = now
            if now - last["pos"] >= 0.20:           # 5 Hz
                ser.write(global_pos(t_ms, int(lat * 1e7), int(lon * 1e7),
                                     int(alt * 1000), int(alt * 1000),
                                     int(track * 100) % 36000)); last["pos"] = now
            if now - last["gps"] >= 0.50:           # 2 Hz
                ser.write(gps_raw(int(lat * 1e7), int(lon * 1e7), int(alt * 1000), 14)); last["gps"] = now
            if now - last["sys"] >= 1.0:            # 1 Hz
                v = max(10500, 12600 - int(t * 8))
                rem = max(0, min(100, int((v - 10500) / (12600 - 10500) * 100)))
                ser.write(sys_status(v, 1050, rem)); last["sys"] = now
            if now - last["hb"] >= 1.0:             # 1 Hz
                ser.write(heartbeat(5, armed=True)); last["hb"] = now
                print(f"t={t:6.1f}s  lat={lat:.6f} lon={lon:.6f} "
                      f"alt={alt:4.0f} as={airspeed:4.1f} hdg={track:5.1f}")
            if now - last["txt"] >= 7.0:            # occasional message
                msgs = [(6, "AHRS: using GPS"), (4, "PreArm: check failsafe"),
                        (6, "Mission started"), (5, "GPS Glitch cleared")]
                sev, m = msgs[txt_i % len(msgs)]
                ser.write(statustext(sev, m)); last["txt"] = now; txt_i += 1

            time.sleep(0.01)
    except KeyboardInterrupt:
        print("\nstopped")
    finally:
        ser.close()


if __name__ == "__main__":
    main()
