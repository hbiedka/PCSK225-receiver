/*
 * ifMix.c
 *
 *  Created on: Sep 8, 2025
 *      Author: hubert
 */
#include "ifMix.h"

size_t lutPos;
size_t sampleRatio;

int32_t *iLUT;
int32_t *qLUT;
uint32_t *lutPeriod;

void ifMix_init(int32_t *i_lut,int32_t *q_lut, uint32_t *period, uint32_t ratio) {
	lutPos = 0;
	lutPeriod = period;
	sampleRatio = ratio;

	iLUT = i_lut;
	qLUT = q_lut;

}

void ifMix_Mix(uint16_t *inputBegin, uint16_t *inputEnd, struct IQ *outputBegin)
{
	uint16_t *sample = inputBegin;
	uint16_t *chunkLastSample = sample;
	struct IQ *outSample = outputBegin;

	while (sample < inputEnd) {

		if (lutPos >= sampleRatio) {
			//move back only for product of period to avoid phase shifts
			lutPos = lutPos % *lutPeriod;
		}

		int32_t Isum = 0;
		int32_t Qsum = 0;

		chunkLastSample += sampleRatio;

		while(sample < chunkLastSample) {

			Isum += iLUT[lutPos] * *sample;
			Qsum += qLUT[lutPos] * *sample;

 			sample++;
			lutPos++;
		}

		// I and Q are now ~(2^17) -> ~2048x64 (ADC midpoint * num of samples
		//convert it to ~ 2^9
		outSample->i = Isum >> 8;
		outSample->q = Qsum >> 8;
		outSample++;

	}

}
