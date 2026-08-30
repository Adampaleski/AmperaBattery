#pragma once

// What this firmware build actually does on the CAN bus.
// Read at boot so bench vs future master-BMS behavior is obvious.

#define BMS_CAP_TELEMETRY_ONLY   TELEMETRY_ONLY
#define BMS_CAP_READ_CELL_VOLTS  1
#define BMS_CAP_READ_TEMPS       0   // temps logged in sniffer counts only
#define BMS_CAP_KEEPALIVE_TX     1
#define BMS_CAP_BALANCE_TX       0   // 1 = TX 0x300/0x310; default stays monitor
#define BMS_CAP_CHARGE_TX        0   // no charger commands
#define BMS_CAP_CONTACTOR_DRV    0   // GPIO exists in teensy41_bms only; still manual 'E'
