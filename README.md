# Lite Ground Station — CrowPanel ESP32‑S3 5.0"

A FreeRTOS, LVGL ground‑station for a 800×480 CrowPanel ESP32‑S3 display. It:

1. **Receives & decodes MAVLink** from a drone over a serial link (switchable
   **UART ⇄ USB‑CDC** at runtime) and shows a **Mission‑Planner‑style HUD**
   (artificial horizon + pitch ladder + roll arc + airspeed/altitude/heading),
   flight mode, arming, battery, GPS, warnings (STATUSTEXT) and the drone
   position on a map.
2. **Forwards** the raw MAVLink stream over **WiFi (UDP)** to a real ground
   station (Mission Planner / QGroundControl) — toggle on/off from the screen.
3. Shows a **satellite map**: it keeps one map image in PSRAM and plots the
   drone marker; only when the drone nears the **edge** does it request a fresh
   image (centered on the drone) from a **PC tool over WiFi**. Normal movement
   just moves the marker.
4. Is organised around **SOLID** principles with the work split across FreeRTOS
   tasks.

## Architecture

```
[Drone] --UART/USB--> LinkTask ──> MavlinkParser ──┬─> TelemetryDecoder ─> TelemetryStore ─┐
                                  (core 0)         └─> UdpMavlinkForwarder ─ForwardTask─UDP─> [GCS]
                                                                                            │ (mutex)
GPS (from Store) ─> MapController (MapTask, core 0) ─edge?─HTTP─> [PC map_server.py] ─RGB565┘
                                                                                            │
                              UiTask (LVGL, loop / core 1) reads Store + MapExchange ───────┘ -> screen
```

| Task         | Core | Responsibility |
|--------------|------|----------------|
| LinkTask     | 0    | poll active source → parse → decode into Store + enqueue raw frame to forwarder |
| ForwardTask  | 0    | drain queue → UDP‑send to the GCS (only while forwarding is enabled) |
| MapTask      | 0    | watch drone vs map bbox; near edge → HTTP fetch a new map into a PSRAM back‑buffer |
| UI (loop)    | 1    | the **only** LVGL caller — refresh sidebar, swap/draw map, handle touch |

LVGL is single‑threaded: only the UI task touches it. Cross‑task data flows
through `TelemetryStore` (mutex) and `MapExchange` (double‑buffer handshake).

### Source layout (`src/`)
- `board/` — `Display` (LVGL + LovyanGFX panel), `TouchInput` (GT911), `Lgfx.h`, `BoardPins.h`
- `telemetry/` — `MavlinkParser`, `MavlinkIds`, `TelemetryDecoder`, `TelemetryStore`,
  `ITelemetrySource` + `Uart`/`Usb` sources, `LinkManager`, `FlightMode`
- `forward/` — `IFrameForwarder`, `UdpMavlinkForwarder`
- `net/` — `WifiManager`
- `map/` — `MapProjection`, `MapExchange`, `IMapImageProvider`, `WifiMapImageProvider`, `MapController`
- `ui/` — `DashboardView` (split layout), `HudView` (Mission-Planner artificial-horizon HUD), `MapView`, `UiTask`
- `app/` — `AppConfig` (NVS), `GroundStationApp` (composition root)
- `main.cpp` — delegates to `GroundStationApp`

## Configure

Edit defaults in [src/app/AppConfig.h](src/app/AppConfig.h) (WiFi SSID/pass, PC
tool host/port, GCS UDP target, map size/zoom, edge margin, baud). Values are
persisted to NVS via `AppConfig::save()` and reloaded on boot.

Telemetry UART pins default to **RX=44 / TX=43** in
[src/board/BoardPins.h](src/board/BoardPins.h) — confirm against your board's
exposed header. `USB` source is the native USB‑CDC port (`Serial`).

## Build / flash

```
pio run -e esp32-s3-devkitc-1-myboard            # build
pio run -e esp32-s3-devkitc-1-myboard -t upload  # flash
pio device monitor -b 115200                     # logs
```

> The platform is pinned to classic `espressif32@6.9.0` (Arduino core 2.0.x).
> The machine's default unversioned `espressif32` resolves to the pioarduino
> fork, whose builder is incompatible with the pinned 2.0.3 core.

## PC map tool (offline satellite imagery)

```
pip install pillow numpy
python tools/map_server.py --imagery ./tiles --port 8080   # serve real tiles
python tools/map_server.py                                 # synthetic grid (no imagery)
```

- Imagery store = standard XYZ tiles on disk: `tiles/<z>/<x>/<y>.png` (or `.jpg`).
  Pre‑download the area you'll fly over (any offline‑tiles downloader pointed at
  a satellite source). Missing tiles render gray.
- Point `AppConfig.pcHost/pcPort` at the machine running this server.
- Protocol: `GET /map?lat&lon&w&h&zoom` → 40‑byte binary header (magic `MAP1`,
  `u16 w`, `u16 h`, `f64 latTop/lonLeft/latBottom/lonRight`) + `w*h*2` RGB565 LE
  bytes. See [tools/map_server.py](tools/map_server.py).

## WiFi forward → real GCS

Enable the **WiFi forward** switch on screen. The device UDP‑sends every raw
MAVLink frame to `AppConfig.gcsIp:gcsPort` (default `…255:14550`, i.e. /24
broadcast). In Mission Planner/QGroundControl, listen on UDP `14550`.

## Test tools (`tools/`)

- **`sim_telemetry.py`** — streams simulated MAVLink (HEARTBEAT, ATTITUDE,
  VFR_HUD, SYS_STATUS, GPS_RAW_INT, GLOBAL_POSITION_INT, STATUSTEXT) to a serial
  port with valid CRC (works for the device *and* GCS forwarding). The position
  drifts so the drone reaches the map edge. `pip install pyserial`.
  ```
  python tools/sim_telemetry.py --port COM5            # straight track, drifts to edge
  python tools/sim_telemetry.py --port COM5 --circle 400   # orbit
  python tools/sim_telemetry.py --port COM5 --static       # fixed position
  ```
  On the device flip the on-screen **Source: USB** switch so it reads from USB.
- **`map_client_test.py`** — calls `map_server.py` like the device does, decodes
  the RGB565 response, prints the bounds and saves a PNG. `pip install pillow numpy`.
  ```
  python tools/map_client_test.py --host 127.0.0.1 --port 8080 \
      --lat 21.0285 --lon 105.8048 --w 1024 --h 1024 --zoom 17 --out map.png
  ```

## Verify

- **HUD / telemetry**: `python tools/sim_telemetry.py --port <COM>` → the HUD
  horizon should roll/pitch, the airspeed/altitude/heading boxes update, and the
  sidebar shows mode/arm/battery/GPS + the last STATUSTEXT. Toggle **Source** to
  switch UART⇄USB.
- **Forward**: enable **WiFi forward**, point a GCS at the device's UDP target
  (default `…255:14550`), and confirm it receives the stream; disable to stop.
- **Map (no device)**: run `map_server.py`, then `map_client_test.py` and open
  the PNG — verifies the server independently.
- **Map (with device)**: run `map_server.py` + `sim_telemetry.py` with WiFi up;
  as the simulated drone nears the edge a new satellite image loads with the
  marker placed correctly. Small moves only move the marker; drag to pan.
```
