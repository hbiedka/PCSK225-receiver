/*
 * pll.c
 *
 *  Created on: 14 paź 2026
 *      Author: hubert
 */

#include "pll.h"
#include <stdlib.h>


static uint32_t localOsc = 0;
static int32_t localOscSpeed = 0;	//speed in cycles*2^32 perSample (0x80000000 means 0,5 cycles per sample)
static int32_t phaseErrorFiltered = 0;
static int32_t phaseLockCounter = 0;
static uint32_t phaseLocked = 0;

void pll_init(uint32_t _sampleRate, uint32_t _initFreq)
{
	// cycles/sample =  cycles/sec / samples/sec
	localOscSpeed = (uint64_t)(_initFreq*0x100000000)/_sampleRate;
}

uint32_t pll_detect(uint32_t angle, uint32_t *output)
{
	// calculate error between input and local osc
	int32_t phaseError = angle - localOsc;

	// rotate local oscillator
	localOsc += localOscSpeed;

	// count tune-in/out
	if (abs(phaseErrorFiltered) < 0xB000000) {
		// in-phase
		if (phaseLockCounter > 30000) {
			phaseLocked = 1;
			// Carrier led ON
			HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);
		} else {
			phaseLockCounter++;
		}
	} else {
		// out-of-phase
		if (phaseLockCounter < 100) {
			phaseLocked = 0;
			// Carrier led OFF
			HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET);
		} else {
			phaseLockCounter--;
		}
	}

	if (phaseLocked) {
		phaseErrorFiltered += (phaseError - phaseErrorFiltered) >> 6;

		// perform asymetric regulation -> positive error has smaller power than negative
		// it prevents phase modulation to tune-off PLL too mucj
		if (phaseErrorFiltered > 0x10000) {
			localOsc += phaseErrorFiltered >> 20;
			localOscSpeed += phaseErrorFiltered >> 26;
		} else {
			localOsc += phaseErrorFiltered >> 10;
			localOscSpeed += phaseErrorFiltered >> 24;
		}
	} else {
		// more aggresive regulation
		phaseErrorFiltered += (phaseError - phaseErrorFiltered) >> 4;
		localOsc += phaseErrorFiltered >> 10;
		localOscSpeed += phaseErrorFiltered >> 24;

	}
	if (output)
		*output = localOsc;

	return phaseLocked;
}
