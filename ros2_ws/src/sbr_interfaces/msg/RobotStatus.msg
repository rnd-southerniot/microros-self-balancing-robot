# RobotStatus.msg
# Published by sbr_controller/telemetry_node at 1 Hz.
# Aggregated health and status for monitoring / alerting.

std_msgs/Header header

uint32  uptime_s           # seconds since boot
float32 battery_mv         # battery voltage in millivolts
float32 cpu_temp_c         # STM32 internal temperature sensor (°C)
float32 wifi_rssi_dbm      # ESP32 WiFi signal strength
bool    is_balancing        # true when balance loop active
bool    is_estopped         # true when safety node has fired e-stop
uint8   fault_flags         # mirrors BalanceState.fault_flags
string  firmware_version   # STM32 firmware version string
