/*
 * eczas.h
 *
 *  Created on: 14 lis 2026
 *      Author: hubert
 */

#ifndef SRC_DECODERS_ECZAS_H_
#define SRC_DECODERS_ECZAS_H_

#include "main.h"
#include "pcsk225.h"

typedef struct{
	uint8_t timeDiff;	// 3 bits always 5 according to docs
	uint32_t counter;	// 3-second ticks since 1.01.2000
	int8_t timeZone;	// time zone shift in hours
	int8_t leapSecond;	// leapSecond (-1,0,1)
	uint8_t timeZoneChange;	// time zone change announced
	uint8_t txMaintenance;	//announcement of transmitter maintenance break
	uint8_t crc8;		// CRC8 output (0 means success)
} eCzasFrame;

void eCzas_decode(pcsk225Frame *frame, eCzasFrame *output);
void eCzas_print(UART_HandleTypeDef *tty, eCzasFrame *frame);
void eCzas_printDate(UART_HandleTypeDef *tty, eCzasFrame *frame);

#endif /* SRC_DECODERS_ECZAS_H_ */
