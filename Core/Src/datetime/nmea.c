/*
 * nmea0103.c
 *
 *  Created on: 19 lis 2026
 *      Author: hubert
 */

#include "nmea.h"

#include <stdio.h>
#include <string.h>

static char buffer[100];


void nmea_sendGPRMCFromTimestamp(UART_HandleTypeDef *tty, uint32_t timestamp) {
	DateTime dt;
	secondsToDatetime(timestamp,&dt);
	nmea_sendGPRMCFromDT(tty,&dt);

}

void nmea_sendGPRMCFromDT(UART_HandleTypeDef *tty, DateTime *dt) {
    char body[80];
    uint8_t checksum = 0;

    int body_len = snprintf(body, sizeof(body),
        "GPRMC,%02d%02d%02d.00,V,,,,,,%02d%02d%02d,,,N",
        dt->hour,
        dt->minute,
        dt->second,
        dt->day,
        dt->month,
        (dt->year % 100) // format YY
    );

    // checksum (XOR over each char between '$' and '*')
    for (int i = 0; i < body_len; i++) {
        checksum ^= body[i];
    }

    int total_len = snprintf(buffer, sizeof(buffer), "$%s*%02X\r\n", body, checksum);

	while (tty->gState != HAL_UART_STATE_READY);
    HAL_UART_Transmit_DMA(tty, (uint8_t*)buffer, total_len);
}
