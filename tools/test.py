from pymavlink import mavutil
import time
import math

# COM port của bạn
master = mavutil.mavlink_connection(
    "COM3",
    baud=57600,
    source_system=1
)

boot_time = time.time()

lat = 21.028511
lon = 105.804817

alt_m = 15.0
battery_v = 12.4

print("Sending MAVLink telemetry...")

while True:

    now_ms = int((time.time() - boot_time) * 1000)

    # --------------------------------------------------
    # HEARTBEAT
    # --------------------------------------------------
    master.mav.heartbeat_send(
        mavutil.mavlink.MAV_TYPE_QUADROTOR,
        mavutil.mavlink.MAV_AUTOPILOT_ARDUPILOTMEGA,
        0,
        0,
        mavutil.mavlink.MAV_STATE_ACTIVE
    )

    # --------------------------------------------------
    # IMU
    # --------------------------------------------------
    ax = int(100 * math.sin(time.time()))
    ay = int(50 * math.cos(time.time()))
    az = 1000

    gx = 0
    gy = 0
    gz = 0

    mx = 0
    my = 0
    mz = 0

    master.mav.scaled_imu_send(
        now_ms,
        ax,
        ay,
        az,
        gx,
        gy,
        gz,
        mx,
        my,
        mz
    )

    # --------------------------------------------------
    # GPS
    # --------------------------------------------------
    lat += 0.000001
    lon += 0.000001

    master.mav.gps_raw_int_send(
        now_ms,
        3,
        int(lat * 1e7),
        int(lon * 1e7),
        int(alt_m * 1000),
        100,
        100,
        0,
        10,
        0
    )

    # --------------------------------------------------
    # GLOBAL POSITION
    # --------------------------------------------------
    master.mav.global_position_int_send(
        now_ms,
        int(lat * 1e7),
        int(lon * 1e7),
        int(alt_m * 1000),
        int(alt_m * 1000),
        0,
        0,
        0,
        0
    )

    # --------------------------------------------------
    # BATTERY
    # --------------------------------------------------
    battery_v -= 0.0002

    master.mav.sys_status_send(
    0,      # sensors_present
    0,      # sensors_enabled
    0,      # sensors_health

    200,    # load (20%)

    int(battery_v * 1000),  # voltage mV

    -1,     # current_battery (-1 unknown)

    90,     # battery remaining %

    0,      # drop_rate_comm
    0,      # errors_comm
    0,      # errors_count1
    0,      # errors_count2
    0,      # errors_count3
    0       # errors_count4
    )
    roll = math.sin(time.time()) * 0.2
    pitch = math.cos(time.time()) * 0.1
    yaw = (time.time() * 0.2) % (2 * math.pi)

    master.mav.attitude_send(
        now_ms,
        roll,
        pitch,
        yaw,
        0.01,
        0.02,
        0.03
    )
    print(
        f"GPS {lat:.6f},{lon:.6f} "
        f"ALT {alt_m:.1f}m "
        f"BAT {battery_v:.2f}V"
    )

    time.sleep(0.1)