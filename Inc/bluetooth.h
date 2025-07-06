#ifndef bluetooth_h
#define bluetooth_h

#include <stdbool.h>
#include "nimble/ble.h"
#include "modlog/modlog.h"
#include "esp_peripheral.h"`
#ifdef __cplusplus
extern "C" {
#endif

/* 16 Bit SPP Service UUID */
#define BLE_SVC_SPP_UUID16                                  0xABF0

/* 16 Bit SPP Service Characteristic UUID */
#define BLE_SVC_SPP_CHR_UUID16                              0xABF1

struct ble_hs_cfg;
struct ble_gatt_register_ctxt;
void init_ble();

#ifdef __cplusplus
}
#endif

#endif