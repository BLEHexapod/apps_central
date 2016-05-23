/*
 * Copyright 2016 Bart Monhemius
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


/** @file
 * @defgroup central_init Central initialization
 * @{
 * @ingroup central
 *
 * @brief Functions to intialize the central modules.
 *
 */


#ifndef CENTRAL_INIT_H
#define CENTRAL_INIT_H

#include <stdint.h>
#include <stdbool.h>

#include "nordic_common.h"
#include "ble_db_discovery.h"
#include "ble.h"
#include "softdevice_handler.h"

#ifdef  __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the BLE stack for the central role.
 * @param rtosHandler Callback for schedular events.
 * @param bleEventHandler Callback for BLE events.
 * @return NRF_SUCCES if succes, error code else.
 */
uint32_t central_stackInit(softdevice_evt_schedule_func_t rtosHandler,
        ble_evt_handler_t bleEventHandler);

/**
 * @brief Start scanning for advertising BLE devices.
 * @param scanParams Parameters to use for scanning.
 * @return NRF_SUCCES if succes, error code else.
 */
uint32_t central_scanStart(ble_gap_scan_params_t *scanParams);

/**
 * @brief Initialize the central discovery database.
 * @param dbEventHandler Callback for database events.
 * @return NRF_SUCCES if succes, error code else.
 */
uint32_t central_dbDiscInit(ble_db_discovery_evt_handler_t dbEventHandler);

/**
 * @brief Helper function to check if a advertising package contains a specific
 * UUID.
 * @details This detects 16-bit, 32-bit and 128-bit UUID's
 * @param uuid UUID to check
 * @param advReport Report of the advertising packages during scanning.
 * @retval  true if the advertising report contains the uuid
 * @retval  false if the adversting report does not contain the uuid.
 */
bool central_isUuidPresent(ble_uuid_t *uuid, ble_gap_evt_adv_report_t *advReport);

#ifdef  __cplusplus
}
#endif

#endif /* CENTRAL_INIT_H */

/**
 *@}
 **/