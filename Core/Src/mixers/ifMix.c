/*
 * ifMix.c
 *
 *  Created on: Sep 8, 2025
 *      Author: hubert
 */
#include "ifMix.h"

#include "sin_lut.h"
#include "cos_lut.h"

uint32_t sampleRate;
static size_t downsampleRatioPow2;
static size_t downsampleRatio;
static size_t phaseAcc;
static size_t phaseAccStep;

static uint16_t *dcOffset;

void ifMix_init(uint32_t _sampleRate, uint32_t _downsampleRatioPow2, uint16_t *_dcOffset) {
	phaseAcc = 0;
	phaseAccStep = 0;

	sampleRate = _sampleRate;
	downsampleRatioPow2 = _downsampleRatioPow2;
	downsampleRatio = 1 << downsampleRatioPow2;
	dcOffset = _dcOffset;
}

void ifMix_setFreq(uint32_t freq)
{
	// step = (f/sample_ratio)*2^32
	phaseAccStep = (uint64_t)(freq*0x100000000)/sampleRate;
}

void ifMix_Mix(const uint16_t *inputBegin, const uint16_t *inputEnd, struct IQ *outputBegin)
{
	uint16_t *sample = (uint16_t*)inputBegin;
	uint16_t *chunkLastSample = sample;
	struct IQ *outSample = outputBegin;

	while (1) {

		chunkLastSample += downsampleRatio;
		if (chunkLastSample >= inputEnd) break;

		int32_t Isum = 0;
		int32_t Qsum = 0;

		while(sample < chunkLastSample) {

			/* LUT pos is 8 most significant bits of 32-bit phase accumulator */
			uint8_t lutPos = (uint8_t)(phaseAcc >> 24);

			// remove DC offset
			int16_t sampleAC = (int32_t)(*sample) - (int32_t)(*dcOffset);
			Isum += sin_lut[lutPos] * sampleAC;
			Qsum += cos_lut[lutPos] * sampleAC;
 			sample++;
			phaseAcc += phaseAccStep;
		}

		// calculate average
		outSample->i = Isum >> downsampleRatioPow2;
		outSample->q = Qsum >> downsampleRatioPow2;
		outSample++;

	}

}
