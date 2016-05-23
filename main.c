#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "nordic_common.h"
#include "app_error.h"
#include "app_uart.h"
#include "app_trace.h"
#include "ble_db_discovery.h"
#include "app_timer.h"
#include "app_trace.h"
#include "app_util.h"
#include "app_error.h"
#include "boards.h"
#include "ble.h"
#include "ble_gap.h"
#include "ble_hci.h"
#include "softdevice_handler.h"
#include "ble_advdata.h"
#include "ble_nus_c.h"

#include "central_config.h"
#include "central_init.h"
#include "os_thread.h"

static os_threadHandle_t centralThreadHandle = NULL;
static ble_nus_c_t m_ble_nus_c;                             /**< Instance of NUS service. Must be passed to all NUS_C API calls. */
static ble_db_discovery_t m_ble_db_discovery;               /**< Instance of database discovery module. Must be passed to all db_discovert API calls */

/**
 * @brief Connection parameters requested for connection.
 */
static ble_gap_conn_params_t connParams =
{
    (uint16_t)MIN_CONNECTION_INTERVAL,  // Minimum connection
    (uint16_t)MAX_CONNECTION_INTERVAL,  // Maximum connection
    (uint16_t)SLAVE_LATENCY,            // Slave latency
    (uint16_t)SUPERVISION_TIMEOUT       // Supervision time-out
};

/**
 * @brief Parameters used when scanning.
 */
static ble_gap_scan_params_t scanParams =
{
    .active      = SCAN_ACTIVE,
    .selective   = SCAN_SELECTIVE,
    .p_whitelist = NULL,
    .interval    = SCAN_INTERVAL,
    .window      = SCAN_WINDOW,
    .timeout     = SCAN_TIMEOUT
};

/**
 * @brief NUS uuid
 */
static ble_uuid_t nusUuid =
{
    .uuid = BLE_UUID_NUS_SERVICE,
    .type = NUS_SERVICE_UUID_TYPE
};

/**
 * @brief Function for asserts in the SoftDevice.
 */
void assert_nrf_callback(uint16_t line_num, const uint8_t * p_file_name)
{
    app_error_handler(0xDEADBEEF, line_num, p_file_name);
}

/**@brief Function for handling database discovery events.
 *
 * @details This function is callback function to handle events from the database discovery module.
 *          Depending on the UUIDs that are discovered, this function should forward the events
 *          to their respective services.
 *
 * @param[in] p_event  Pointer to the database discovery event.
 */
static void dbEventHandler(ble_db_discovery_evt_t * p_evt)
{
    ble_nus_c_on_db_disc_evt(&m_ble_nus_c, p_evt);
}


/**@brief   Function for handling app_uart events.
 *
 * @details This function will receive a single character from the app_uart module and append it to
 *          a string. The string will be be sent over BLE when the last character received was a
 *          'new line' i.e '\n' (hex 0x0D) or if the string has reached a length of
 *          @ref NUS_MAX_DATA_LENGTH.
 */
void uart_event_handle(app_uart_evt_t * p_event)
{
    static uint8_t data_array[BLE_NUS_MAX_DATA_LEN];
    static uint8_t index = 0;

    switch (p_event->evt_type)
    {
        /**@snippet [Handling data from UART] */
        case APP_UART_DATA_READY:
            UNUSED_VARIABLE(app_uart_get(&data_array[index]));
            index++;

            if ((data_array[index - 1] == '\n') || (index >= (BLE_NUS_MAX_DATA_LEN)))
            {
                while (ble_nus_c_string_send(&m_ble_nus_c, data_array, index) != NRF_SUCCESS)
                {
                    // repeat until sent.
                }
                index = 0;
            }
            break;
        /**@snippet [Handling data from UART] */
        case APP_UART_COMMUNICATION_ERROR:
            APP_ERROR_HANDLER(p_event->data.error_communication);
            break;

        case APP_UART_FIFO_ERROR:
            APP_ERROR_HANDLER(p_event->data.error_code);
            break;

        default:
            break;
    }
}


/**@brief Callback handling NUS Client events.
 *
 * @details This function is called to notify the application of NUS client events.
 *
 * @param[in]   p_ble_nus_c   NUS Client Handle. This identifies the NUS client
 * @param[in]   p_ble_nus_evt Pointer to the NUS Client event.
 */

/**@snippet [Handling events from the ble_nus_c module] */
static void nus_eventHandler(ble_nus_c_t * p_ble_nus_c,
        const ble_nus_c_evt_t * p_ble_nus_evt)
{
    uint32_t errCode;
    switch (p_ble_nus_evt->evt_type)
    {
        case BLE_NUS_C_EVT_DISCOVERY_COMPLETE:
            errCode = ble_nus_c_handles_assign(p_ble_nus_c, p_ble_nus_evt->conn_handle, &p_ble_nus_evt->handles);
            APP_ERROR_CHECK(errCode);

            errCode = ble_nus_c_rx_notif_enable(p_ble_nus_c);
            APP_ERROR_CHECK(errCode);
            break;

        case BLE_NUS_C_EVT_NUS_RX_EVT:
            for (uint32_t i = 0; i < p_ble_nus_evt->data_len; i++)
            {
                while(app_uart_put( p_ble_nus_evt->p_data[i]) != NRF_SUCCESS);
            }
            break;

        case BLE_NUS_C_EVT_DISCONNECTED:
            central_scanStart(&scanParams);
            break;
    }
}

/**@brief Function for handling the Application's BLE Stack events.
 *
 * @param[in] bleEvt  Bluetooth stack event.
 */
static void on_ble_evt(ble_evt_t * bleEvt)
{
    uint32_t errCode;
    ble_gap_evt_t *gapEvt = &bleEvt->evt.gap_evt;

    switch (bleEvt->header.evt_id)
    {
        case BLE_GAP_EVT_ADV_REPORT:
        {
            ble_gap_evt_adv_report_t * advReport = &gapEvt->params.adv_report;

            if (central_isUuidPresent(&nusUuid, advReport))
                sd_ble_gap_connect(&advReport->peer_addr,
                        &scanParams, &connParams);
            break;
        }
        case BLE_GAP_EVT_CONNECTED:
            // start discovery of services. The NUS Client waits for a discovery result
            errCode = ble_db_discovery_start(&m_ble_db_discovery,
                bleEvt->evt.gap_evt.conn_handle);
            APP_ERROR_CHECK(errCode);
            break;
        case BLE_GAP_EVT_TIMEOUT:
            if (gapEvt->params.timeout.src == BLE_GAP_TIMEOUT_SRC_SCAN) {
                central_scanStart(&scanParams);
            } else if (gapEvt->params.timeout.src == BLE_GAP_TIMEOUT_SRC_CONN) {
                // TODO?
            }
            break;
        case BLE_GAP_EVT_SEC_PARAMS_REQUEST:
            // Pairing not supported
            errCode = sd_ble_gap_sec_params_reply(bleEvt->evt.gap_evt.conn_handle,
                    BLE_GAP_SEC_STATUS_PAIRING_NOT_SUPP, NULL, NULL);
            APP_ERROR_CHECK(errCode);
            break;
        case BLE_GAP_EVT_CONN_PARAM_UPDATE_REQUEST:
            // Accepting parameters requested by peer.
            errCode = sd_ble_gap_conn_param_update(gapEvt->conn_handle,
                    &gapEvt->params.conn_param_update_request.conn_params);
            APP_ERROR_CHECK(errCode);
            break;
        default:
            break;
    }
}

/**@brief Function for dispatching a BLE stack event to all modules with a BLE stack event handler.
 *
 * @details This function is called from the scheduler in the main loop after a BLE stack event has
 *          been received.
 *
 * @param[in] bleEvt  Bluetooth stack event.
 */
static void bleEventHandler(ble_evt_t * bleEvt)
{
    on_ble_evt(bleEvt);
    ble_db_discovery_on_ble_evt(&m_ble_db_discovery, bleEvt);
    ble_nus_c_on_ble_evt(&m_ble_nus_c,bleEvt);
}

/**@brief Function for initializing the UART.
 */
static void uart_init(void)
{
    uint32_t errCode;

    const app_uart_comm_params_t comm_params =
      {
        .rx_pin_no    = RX_PIN_NUMBER,
        .tx_pin_no    = TX_PIN_NUMBER,
        .rts_pin_no   = RTS_PIN_NUMBER,
        .cts_pin_no   = CTS_PIN_NUMBER,
        .flow_control = APP_UART_FLOW_CONTROL_ENABLED,
        .use_parity   = false,
        .baud_rate    = UART_BAUDRATE_BAUDRATE_Baud115200
      };

    APP_UART_FIFO_INIT(&comm_params,
                        UART_RX_BUF_SIZE,
                        UART_TX_BUF_SIZE,
                        uart_event_handle,
                        APP_IRQ_PRIORITY_LOW,
                        errCode);

    APP_ERROR_CHECK(errCode);
}

/**@brief Function for initializing the NUS Client.
 */
static void nus_c_init(void)
{
    uint32_t         errCode;
    ble_nus_c_init_t nus_c_init_t;

    nus_c_init_t.evt_handler = nus_eventHandler;

    errCode = ble_nus_c_init(&m_ble_nus_c, &nus_c_init_t);
    APP_ERROR_CHECK(errCode);
}

static uint32_t centralEventHandler(void)
{
    os_threadNotify(centralThreadHandle);
    return NRF_SUCCESS;
}

static void ble_centralThread(void *args)
{
    central_stackInit(centralEventHandler, bleEventHandler);
    central_dbDiscInit(dbEventHandler);
    nus_c_init();

    // Start scanning for peripherals and initiate connection
    // with devices that advertise NUS UUID.
    central_scanStart(&scanParams);

    while(1) {
        os_threadWait();
        intern_softdevice_events_execute();
    }

}

int main(void)
{
    uart_init();
    os_threadConfig_t centralThreadConf = {
        .name = "CT",
        .threadCallback = ble_centralThread,
        .threadArgs = NULL,
        .stackSize = STACK_SIZE_BIG,
        .priority = THREAD_PRIO_NORM
    };
    os_threadNew(&centralThreadConf);
    os_startScheduler();

    while(1);
}
