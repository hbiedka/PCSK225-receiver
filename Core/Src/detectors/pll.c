/*
 * pll.c
 *
 *  Created on: 14 paź 2026
 *      Author: hubert
 */

#include "pll.h"
#include <stdlib.h>


static uint32_t localOsc = 0;
static uint32_t localOscSpeed = 0;	//speed in cycles*2^32 perSample (0x80000000 means 0,5 cycles per sample)
static int32_t phaseErrorFiltered = 0;
static int32_t phaseLockCounter = 0;
static uint32_t phaseLocked = 0;

void pll_init(uint32_t _sampleRate, uint32_t _initFreq)
{
	// cycles/sample =  cycles/sec / samples/sec
	localOscSpeed = (uint64_t)(_initFreq*0x100000000)/_sampleRate;
}

int32_t pll_detect(uint32_t angle)
{
	// advance local oscillator
	localOsc += localOscSpeed;

	// calculate error between input and local osc
	int32_t phaseError = angle - localOsc;

	// apply simple low-pass filter
	phaseErrorFiltered += (phaseError - phaseErrorFiltered) >> 4;

	int32_t phaseErrorFilteredAmpilfied = phaseErrorFiltered * 2;

	//more aggressive gain if we are not in sync
//	if (!phaseLocked)
//		phaseErrorFilteredAmpilfied *= 10;

	//apply speed correction
	// positive phaseError means angle > localOsc -> increase speed
	localOscSpeed += phaseErrorFilteredAmpilfied;

	// detect how long we are in sync
	if (abs(phaseErrorFiltered) < 100) {
		if (phaseLockCounter < 10000) phaseLockCounter++;
	} else {
		if (phaseLockCounter > 10) phaseLockCounter -=3;
	}

	// toggle phaseLocked if we are far in sync / out of sync
	if (phaseLockCounter > 8000) {
		phaseLocked = 1;
		HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_SET);
	}
	if (phaseLockCounter < 100) {
		phaseLocked = 0;
		HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);
	}

	return phaseError;
}
