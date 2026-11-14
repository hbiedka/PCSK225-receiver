/*
 * eczas.c
 *
 *  Created on: 14 lis 2026
 *      Author: hubert
 */

#include "eczas.h"
#include <stdio.h>
#include <string.h>

#define CRC8_POLY 0x07

static char buffer[256];

// scrambling magic (ASCII "\NGUM+") used to scramble
uint8_t scrambleMagic[5] = {0x0A,0x47,0x55,0x4D,0x2B};


// helper to move from e-Czas counter (3-sec periods from 2000-01-01)
// to unix epoch (secs from 1970-01-01)
static uint64_t cntToUnixEpoch(uint32_t cnt)
{
	return (cnt*3) + 946684800;
}

static uint8_t crc8(const uint8_t *payload, size_t len, uint8_t rxCrc)
{
    uint8_t crc = 0x00;

    for (int i = 0; i < len; i++) {
        uint8_t byte = payload[i];
        for (int bit = 7; bit >= 0; bit--) {
            uint8_t msb = crc & 0x80;
            crc = (crc << 1) | ((byte >> bit) & 0x01);
            if (msb) {
                crc ^= CRC8_POLY;
            }
        }
    }

    for (int bit = 7; bit >= 0; bit--) {
        uint8_t msb = crc & 0x80;
        crc = (crc << 1) | ((rxCrc >> bit) & 0x01);
        if (msb) {
            crc ^= CRC8_POLY;
        }
    }

    return crc;
}

void eCzas_decode(pcsk225Frame *pcsk, eCzasFrame *frame)
{
	uint32_t cnt = 0;
	int8_t tz = 0;
	uint8_t txMaintenance = 0;

	// descramble
	uint8_t payload[5];
	for (size_t i = 0; i < 5; i++)
		payload[i] = pcsk->payload[i] ^ scrambleMagic[i];

	frame->timeDiff = pcsk->payload[0] >> 5;

	cnt = payload[0] & 0x1F;
	cnt <<= 8;
	cnt += payload[1];
	cnt <<= 8;
	cnt += payload[2];
	cnt <<= 8;
	cnt += payload[3];
	cnt <<= 1;
	cnt += payload[4] >> 7;

	frame->counter = cnt;

	// timezone is MSB first
	if (payload[4] & 0x40) tz += 1;
	if (payload[4] & 0x20) tz += 2;
	frame->timeZone = tz;

	// leap second
	if (payload[4] & 0x10) {
		frame->leapSecond = (payload[4] & 0x08) ? -1 : 1;
	} else {
		frame->leapSecond = 0;
	}

	// time zone change announced
	frame->timeZoneChange = (payload[4] & 0x04) ? 1 : 0;

	// transmitter maintenance (MSB first
	if (payload[4] & 0x02) txMaintenance += 1;
	if (payload[4] & 0x01) txMaintenance += 2;
	frame->txMaintenance = txMaintenance;

	//CRC8
	frame->crc8 = crc8(pcsk->payload,5,pcsk->payload[8]);
}
void eCzas_print(UART_HandleTypeDef *tty, eCzasFrame *frame)
{
	snprintf(buffer,sizeof(buffer),"tdiff=%u,cnt=%lu(unix=%llu), tz=%d, ls=%d, txMt=%u, CRC=0x%02X\r\n",
			frame->timeDiff,
			frame->counter,
			cntToUnixEpoch(frame->counter),
			frame->timeZone,
			frame->leapSecond,
			frame->txMaintenance,
			frame->crc8
	);

	while (tty->gState != HAL_UART_STATE_READY);
	HAL_UART_Transmit_DMA(tty,(uint8_t*)buffer,strnlen(buffer,sizeof(buffer)));

}
