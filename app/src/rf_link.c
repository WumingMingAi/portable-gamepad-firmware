// 2.4GHz ESB link implementation (Nordic Enhanced ShockBurst).
// SPDX-License-Identifier: MIT

#include "rf_link.h"

#include <errno.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(rf_link, LOG_LEVEL_INF);

#ifdef CONFIG_ESB

#include <esb.h>

// Fixed link address: pipe 0, base address + prefix.
// Personal-device binding: both ends of this project share these constants.
static uint8_t base_addr_0[4] = {0xDE, 0xAD, 0xBE, 0xEF};
static uint8_t addr_prefixes[4] = {0x42, 0x43, 0x44, 0x45};

static struct esb_payload tx_payload = {
    .pipe = 0,
    .length = RF_PAYLOAD_LEN,
    .noack = false,
};

static volatile bool rx_pending;
static uint8_t rx_data[RF_PAYLOAD_LEN];
static int64_t last_exchange_ms;

static void event_handler(const struct esb_evt* evt)
{
    switch (evt->evt_id) {
    case ESB_EVENT_TX_SUCCESS:
        last_exchange_ms = k_uptime_get();
        break;
    case ESB_EVENT_TX_FAILED:
        break;
    case ESB_EVENT_RX_RECEIVED: {
        struct esb_payload p;
        if (esb_read_rx_payload(&p) == 0) {
            if (p.length >= RF_PAYLOAD_LEN && p.data[0] == RF_MAGIC) {
                memcpy(rx_data, p.data, RF_PAYLOAD_LEN);
                rx_pending = true;
            }
        }
        break;
    }
    default:
        break;
    }
}

void rf_link_init(void)
{
    struct esb_config config = ESB_DEFAULT_CONFIG;

    config.protocol = ESB_PROTOCOL_ESB_DPL;
    config.mode = IS_ENABLED(CONFIG_PGF_ESB_DONGLE) ? ESB_MODE_PRX : ESB_MODE_PTX;
    config.event_handler = event_handler;
    config.bitrate = ESB_BITRATE_2MBPS;
    config.crc = ESB_CRC_16BIT;
    config.tx_output_power = 4;   /* +4 dBm */
    config.retransmit_delay = 500;
    config.retransmit_count = 2;
    config.tx_mode = ESB_TXMODE_AUTO;
    config.selective_auto_ack = true;
    /* Keep the standard 130us radio ramp-up: CONFIG_ESB_FAST_SWITCHING is
     * nRF54H20-only, and the official nRF52 samples leave this disabled. */
    config.use_fast_ramp_up = false;

    /* With CONFIG_ESB_MPSL_TIMESLOT, MPSL is initialized automatically at
     * boot by the Zephyr glue (SYS_INIT in subsys/mpsl/init/mpsl_init.c),
     * so nothing to do here. */

    if (esb_init(&config) != 0) {
        LOG_ERR("esb_init failed");
        return;
    }
    esb_set_base_address_0(base_addr_0);
    esb_set_prefixes(addr_prefixes, ARRAY_SIZE(addr_prefixes));
    esb_set_rf_channel(CONFIG_PGF_ESB_RF_CHANNEL);

    if (IS_ENABLED(CONFIG_PGF_ESB_DONGLE)) {
        esb_enable_pipes(0x01);
        esb_start_rx();
    } else {
        esb_flush_tx();
    }

    LOG_INF("rf_link started (%s), channel %d",
            IS_ENABLED(CONFIG_PGF_ESB_DONGLE) ? "PRX" : "PTX",
            CONFIG_PGF_ESB_RF_CHANNEL);
}

int rf_link_tx(const uint8_t* data)
{
    if (esb_tx_full()) {
        return -EBUSY;
    }
    memcpy(tx_payload.data, data, RF_PAYLOAD_LEN);
    return esb_write_payload(&tx_payload);
}

bool rf_link_poll_rx(uint8_t* data)
{
    if (!rx_pending) {
        return false;
    }
    rx_pending = false;
    memcpy(data, rx_data, RF_PAYLOAD_LEN);
    last_exchange_ms = k_uptime_get();
    return true;
}

bool rf_link_linked(void)
{
    return (k_uptime_get() - last_exchange_ms) < RF_STALE_MS;
}

#endif  // CONFIG_ESB
