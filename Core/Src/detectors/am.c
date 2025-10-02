/*
 * am.c
 *
 *  Created on: Oct 2, 2025
 *      Author: hubert
 */

#include "am.h"

// for square root approximation
const float sqrt_approx_a = -8.3553519533e-12f;
const float sqrt_approx_b = 3.3562705851e-04f;
const float sqrt_approx_c = 6.9878899460e+02f;

static float filterBuffer;

void amDetector_init(void)
{
	filterBuffer = 0;
}

void amDetector_detect(struct IQ *inputBegin, struct IQ *inputEnd, int32_t *outputBegin)
{
	  int32_t *out = outputBegin;
	  for (struct IQ *sample = inputBegin; sample < inputEnd; sample++) {

		int32_t I = sample->i;
		int32_t Q = sample->q;
		I *= I;
		Q *= Q;

		uint32_t IQsquareSum = I+Q;

		//sqrt approximation by quadratic function to make it faster
		float absOut = sqrt_approx_c;
		absOut += IQsquareSum*sqrt_approx_b;
		absOut += (IQsquareSum*IQsquareSum)*sqrt_approx_a;

		//simple IIR LPF filter
		//TODO HPF to remove DC offset?

		filterBuffer *=0.95;
		filterBuffer += (absOut*0.05);

		*out = filterBuffer;
		out++;
	  }
}
