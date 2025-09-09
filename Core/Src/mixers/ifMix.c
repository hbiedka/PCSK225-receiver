/*
 * ifMix.c
 *
 *  Created on: Sep 8, 2025
 *      Author: hubert
 */
#include "ifMix.h"
#include "sin_lut.h"
#include "cos_lut.h"

size_t lutPos;
size_t lutPeriod;
size_t sampleRatio;

static struct IQ *if_IQ;
size_t ifLen;
volatile size_t ifIndex;

void ifMix_init(struct IQ *ifSamples, size_t ifSamplesLen, uint32_t ratio, uint32_t period) {
	lutPos = 0;
	lutPeriod = period;
	sampleRatio = ratio;

	if_IQ = ifSamples;
	ifLen = ifSamplesLen;
	ifIndex = 0;
}

size_t ifMix_getOutputBufferPos() {
	return ifIndex;
}

void ifMix_Mix(uint16_t *inputBegin, uint16_t *inputEnd)
{
	uint16_t *sample = inputBegin;
	uint16_t *chunkLastSample = sample;

	while (sample < inputEnd) {

		if (lutPos >= sampleRatio) {
			//move back only for product of period to avoid phase shifts
			lutPos = lutPos % lutPeriod;
		}

		int32_t Isum = 0;
		int32_t Qsum = 0;

		chunkLastSample += sampleRatio;

		while(sample < chunkLastSample) {

			Isum += sin_lut[lutPos] * *sample;
			Qsum += cos_lut[lutPos] * *sample;

 			sample++;
			lutPos++;
		}

		// I and Q are now ~(2^17) -> ~2048x64 (ADC midpoint * num of samples
		//convert it to ~ 2^9
		if_IQ[ifIndex].i = Isum >> 8;
		if_IQ[ifIndex].q = Qsum >> 8;

		ifIndex++;
		if(ifIndex >= ifLen) ifIndex = 0;

	}

}
