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
	FOUND_SYNC_ONE,
	DETECT_ZERO,
	DETECT_ONE,
	FULL_FRAME_RECVD,
	FAIL_INVALID_PREAMBLE
} phaseModReturn_t;

void phaseMod_init(uint32_t sampleRate, uint32_t baudRate);
phaseModReturn_t phaseMod_push(int32_t sample);

uint8_t* phaseMod_getBitstream(size_t* size);

#endif /* SRC_DETECTORS_PHASEMOD_H_ */
