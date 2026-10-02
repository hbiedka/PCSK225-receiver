/*
 * dcOffset.c
 *
 *  Created on: 1 paź 2026
 *      Author: hubert
 */


#include <stddef.h>	//for size_t
#include <stdlib.h> //for malloc()

#include "dcOffset.h"

static uint16_t outputOffset = 0;
static uint32_t acc = 0;

static uint16_t *bufferBegin = 0;
static uint16_t *bufferEnd = 0;
static uint16_t *bufferCurrentSample = 0;

uint8_t windowLenOrder = 0;

void dcOffset_init(uint8_t _windowLenOrder)
{
	windowLenOrder = _windowLenOrder;
	size_t windowLen = 1 << windowLenOrder;

	//initialize circular buffer
	bufferBegin = malloc(windowLen);
	bufferEnd = bufferBegin + windowLen;

	  //initialize circullar buffer
	bufferCurrentSample = bufferBegin;
	  while (bufferCurrentSample < bufferEnd) {
		  *bufferCurrentSample = 0;
		  bufferCurrentSample++;
	  }
	  bufferCurrentSample = bufferBegin;
}

uint16_t dcOffset_filter(uint16_t input)
{
	  // update DC offset
	  *bufferCurrentSample = input;
	  acc += *bufferCurrentSample;	//newest sample
	  bufferCurrentSample++;

	  //rollover
	  if (bufferCurrentSample >= bufferEnd)
		  bufferCurrentSample = bufferBegin;

	  acc -= *bufferCurrentSample;	//oldest sample
	  outputOffset = acc >> windowLenOrder;	//calculate average

	  return outputOffset;
}




