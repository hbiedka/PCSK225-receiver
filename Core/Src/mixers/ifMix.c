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
size_t downsampleRatio;
size_t phaseAcc;
size_t phaseAccStep;

void ifMix_init(uint32_t _sampleRate, uint32_t _downsampleRatio) {
	phaseAcc = 0;
	phaseAccStep = 0;

	sampleRate = _sampleRate;
	downsampleRatio = _downsampleRatio;
}

void ifMix_setFreq(uint32_t freq)
{
	// step = (f/sample_ratio)*2^32
	phaseAccStep = (uint64_t)(freq*4294967296)/sampleRate;
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
			size_t lutPos = phaseAcc >> 24;

			Isum += sin_lut[lutPos] * *sample;
			Qsum += cos_lut[lutPos] * *sample;

 			sample++;
			phaseAcc += phaseAccStep;
		}

		// I and Q are now ~(2^18) -> ~2048x128 (ADC midpoint * num of samples
		//convert it to ~ 2^9
		outSample->i = Isum >> 9;
		outSample->q = Qsum >> 9;
		outSample++;

	}

}
