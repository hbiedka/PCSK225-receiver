/*
 * afMix.c
 *
 *  Created on: Sep 29, 2025
 *      Author: hubert
 */


#include "ifMix.h"

static size_t lutPos;
static int32_t *afLUT;
static size_t *afLUTperiod;

void afMix_init(int32_t *lut, size_t *period)
{
	afLUT = lut;
	afLUTperiod = period;
	lutPos = 0;
}

void afMix_mix(struct IQ *inputBegin, struct IQ *inputEnd, struct IQ *outputBegin)
{
	  struct IQ *out = outputBegin;

	  //IF->AF mix loop
	  for(struct IQ *in = inputBegin; in < inputEnd; in++) {

		// RF to IF mixing
		int32_t I = in->i * afLUT[lutPos];
		int32_t Q = in->q * afLUT[lutPos];

		I >>= 8;
		Q >>= 8;

		out->i = I;
		out->q = Q;
		out++;

		lutPos++;
		if (lutPos >= *afLUTperiod) lutPos = 0;
	  }
}
