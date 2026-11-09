// 2.4GHz ESB link (controller PTX <-> dongle PRX)
// SPDX-License-Identifier: MIT

#pragma once

#include <stdbool.h>
#include <stdint.h>

// Packet layout (6 bytes):
//   [0] magic 0xA5
//   [1] platform: 1=Stadia 2=Switch 3=PC
//   [2..4] button bits 0..17 (see RF_BUTTON_BITS in main.c)
//   [5] sequence number
#define RF_MAGIC 0xA5
#define RF_PAYLOAD_LEN 6

// Milliseconds without a successful exchange before the link is considered lost.
#define RF_STALE_MS 500

// Configure and start the radio. PRX if CONFIG_PGF_ESB_DONGLE, otherwise PTX.
void rf_link_init(void);

// PTX: queue one packet for transmission. Returns 0 on success.
int rf_link_tx(const uint8_t* data);

// PRX: poll for a freshly received packet. Returns true and copies the
// payload into data if a new valid packet arrived.
bool rf_link_poll_rx(uint8_t* data);

// True if packets/ACKs have been exchanged recently (both modes).
bool rf_link_linked(void);
