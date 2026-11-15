/*
 * agc.c
 *
 *  Created on: 15 lis 2026
 *      Author: hubert
 */

#include "agc.h"

#include <stddef.h>	//for size_t
#include <stdlib.h> //for malloc()


static uint16_t amplitude = 0;
static uint32_t acc = 0;

static uint16_t *bufferBegin = 0;
static uint16_t *bufferEnd = 0;
static uint16_t *bufferCurrentSample = 0;

static uint8_t windowLenOrder = 0;

void agc_init(uint8_t _windowLenOrder)
{
	windowLenOrder = _windowLenOrder;
	size_t windowLen = 1 << windowLenOrder;

	//initialize circular buffer
	bufferBegin = malloc(windowLen*sizeof(uint16_t));
	bufferEnd = bufferBegin + windowLen;

	  //initialize circullar buffer
	bufferCurrentSample = bufferBegin;
	  while (bufferCurrentSample < bufferEnd) {
		  *bufferCurrentSample = 0;
		  bufferCurrentSample++;
	  }
	  bufferCurrentSample = bufferBegin;
}

uint32_t agc_update(int16_t input)
{
	  // update circullar buffer
	  *bufferCurrentSample = abs(input) << 6;
	  acc += *bufferCurrentSample;	//newest sample
	  bufferCurrentSample++;

	  //rollover
	  if (bufferCurrentSample >= bufferEnd)
		  bufferCurrentSample = bufferBegin;

	  acc -= *bufferCurrentSample;	//oldest sample
	  uint32_t amplitudeUnfiltered = acc >> windowLenOrder;	//calculate average

	  //filter amplitude
	  amplitude += (amplitudeUnfiltered - amplitude) >> 6;

	  // provide min/max boundaries
	  if (amplitude < 100) amplitude = 100;
	  else if (amplitude > 2000) amplitude = 2000;

	  // normal amplitude is ~900, and it should return ~64 ratio
	  uint32_t agcRatio = 50000 / amplitude;

	  return agcRatio;
}
