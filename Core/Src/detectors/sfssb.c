/*
 * sfssb.c
 *
 *  Created on: Oct 2, 2025
 *      Author: hubert
 */

#include "sfssb.h"

static uint32_t sampleShift;
static struct IQ *sourceBufferBegin;
size_t sourceBufferLength;

void sfSsbDetector_init(struct IQ *bufferBegin, struct IQ *bufferEnd, uint32_t sampleRatio, uint32_t targetFreq)
{
	sourceBufferBegin  = bufferBegin;
	sourceBufferLength = bufferEnd-bufferBegin;

	//calculate shift
	uint32_t period = sampleRatio / targetFreq;		//period in samples
	sampleShift = period / 4;						// 1/4 (90 deg) shift
}

void sfSsbDetector_detect(struct IQ *inputBegin, struct IQ *inputEnd, int32_t *outputBegin)
{
  int32_t *out = outputBegin;
  for (struct IQ *sample = inputBegin; sample < inputEnd; sample++) {

	struct IQ *sampleShifted = sample - sampleShift;

	//deal with buffer overflow
	if (sampleShifted < sourceBufferBegin) {
		sampleShifted += sourceBufferLength;
	}

	*out = sample->i - sampleShifted->q;
	out++;
  }
}
