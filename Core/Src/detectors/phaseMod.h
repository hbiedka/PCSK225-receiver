/*
 * phaseMod.h
 *
 *  Created on: 11 lis 2026
 *      Author: hubert
 */

#ifndef SRC_DETECTORS_PHASEMOD_H_
#define SRC_DETECTORS_PHASEMOD_H_

#include "main.h"

typedef enum {
	NOTHING = 0,
	FOUND_SYNC_ZERO,
	DETECT_ZERO,
	DETECT_ONE,
	FULL_FRAME_RECVD,
	FAIL_INVALID_PREAMBLE
} phaseModReturn_t;

uint32_t phaseMod_init(uint32_t sampleRate, uint32_t baudRate, uint32_t syncBitWidth);
phaseModReturn_t phaseMod_push(int32_t sample);

uint8_t* phaseMod_getBitstream(size_t* size);
void phaseMod_printBitstream(UART_HandleTypeDef *tty);

#endif /* SRC_DETECTORS_PHASEMOD_H_ */
