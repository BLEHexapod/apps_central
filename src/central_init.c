/*
 * Copyright 2016 Bart Monhemius.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "central_config.h"
#include "central_init.h"
#include "boards.h"


uint32_t central_stackInit(softdevice_evt_schedule_func_t rtosHandler,
        ble_evt_handler_t bleEventHandler)
{
    uint32_t errCode;
    uint32_t err_code;

    nrf_clock_lf_cfg_t clock_lf_cfg = NRF_CLOCK_LFCLKSRC;

    // Initialize the SoftDevice handler module.
    SOFTDEVICE_HANDLER_INIT(&clock_lf_cfg, rtosHandler);

    ble_enable_params_t bleEnableParams;
    errCode = softdevice_enable_get_default_config(CENTRAL_LINK_COUNT,
            PERIPHERAL_LINK_COUNT,
            &bleEnableParams);
    APP_ERROR_CHECK(errCode);

    //Check the ram settings against the used number of links
    CHECK_RAM_START_ADDR(CENTRAL_LINK_COUNT,PERIPHERAL_LINK_COUNT);

    // Enable BLE stack.
    errCode = softdevice_enable(&bleEnableParams);
    APP_ERROR_CHECK(errCode);

    // Register with the SoftDevice handler module for BLE events.
    errCode = softdevice_ble_evt_handler_set(bleEventHandler);
    APP_ERROR_CHECK(errCode);
    return errCode;
}

uint32_t central_dbDiscInit(ble_db_discovery_evt_handler_t dbEventHandler)
{
    uint32_t errCode = ble_db_discovery_init(dbEventHandler);
    APP_ERROR_CHECK(errCode);
    return errCode;
}


uint32_t central_scanStart(ble_gap_scan_params_t *scanParams)
{
    uint32_t errCode = sd_ble_gap_scan_start(scanParams);
    APP_ERROR_CHECK(errCode);
    return errCode;
}

bool central_isUuidPresent(ble_uuid_t *uuid, ble_gap_evt_adv_report_t *advReport)
{
    uint32_t errCode;
    uint32_t index = 0;
    uint8_t *data = (uint8_t *)advReport->data;
    ble_uuid_t advUuid;

    while (index < advReport->dlen) {
        uint8_t field_length = data[index];
        uint8_t field_type   = data[index+1];

        if ((field_type == BLE_GAP_AD_TYPE_16BIT_SERVICE_UUID_MORE_AVAILABLE)
           || (field_type == BLE_GAP_AD_TYPE_16BIT_SERVICE_UUID_COMPLETE)) {
            for (uint32_t i = 0; i < (field_length/UUID16_SIZE); i++) {
                errCode = sd_ble_uuid_decode(UUID16_SIZE,
                        &data[i * UUID16_SIZE + index + 2],
                        &advUuid);
                if (errCode == NRF_SUCCESS) {
                    if ((advUuid.uuid == uuid->uuid)
                        && (advUuid.type == uuid->type))
                        return true;
                }
            }
        } else if ( (field_type == BLE_GAP_AD_TYPE_32BIT_SERVICE_UUID_MORE_AVAILABLE)
                || (field_type == BLE_GAP_AD_TYPE_32BIT_SERVICE_UUID_COMPLETE)) {
            for (uint32_t i = 0; i < (field_length/UUID32_SIZE); i++) {
                errCode = sd_ble_uuid_decode(UUID16_SIZE,
                &data[i * UUID32_SIZE + index + 2],
                &advUuid);
                if (errCode == NRF_SUCCESS) {
                    if ((advUuid.uuid == uuid->uuid)
                        && (advUuid.type == uuid->type))
                        return true;
                }
            }
        } else if ( (field_type == BLE_GAP_AD_TYPE_128BIT_SERVICE_UUID_MORE_AVAILABLE)
                || (field_type == BLE_GAP_AD_TYPE_128BIT_SERVICE_UUID_COMPLETE)) {
            errCode = sd_ble_uuid_decode(UUID128_SIZE,
                    &data[index + 2], &advUuid);
            if (errCode == NRF_SUCCESS) {
                if ((advUuid.uuid == uuid->uuid)
                    && (advUuid.type == uuid->type))
                    return true;
            }
        }
        index += field_length + 1;
    }
    return false;
}