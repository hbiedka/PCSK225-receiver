/*
 * phaseMod.c
 *
 *  Created on: 11 lis 2026
 *      Author: hubert
 */

#include "phaseMod.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define BIT_NUM 96
#define BUFFER_SIZE 200

const int32_t triangleWidth = 100;

//TODO initialize and malloc circBuf base on sample rate
int32_t circBuf[BUFFER_SIZE];
size_t circBufPtr = 0;

uint32_t baudratePeriod = 176; 	//8789.0625/50 (samples per second/bits per second -> samples/bit)

size_t sampleCnt = 0;
int8_t bits[BIT_NUM];
size_t bitCnt = 0;

int32_t lastOne = 0;
int32_t lastZero = 0;

enum {
	WAIT,
	RECV,
	FRAME_END
} state = WAIT;

void phaseMod_init(uint32_t sampleRate, uint32_t baudRate)
{
	baudratePeriod = sampleRate/baudRate;

	//division correction
	if ((sampleRate % baudRate) > (baudRate/2))baudratePeriod++;
}

phaseModReturn_t phaseMod_push(int32_t sample)
{
	phaseModReturn_t ret = NOTHING;

	circBuf[circBufPtr] = sample;
	circBufPtr++;
	if (circBufPtr >= BUFFER_SIZE) circBufPtr = 0;
	int32_t posBegin, posMid;

	int32_t thr = 0;

	switch(state) {
		case WAIT:

			posMid = circBufPtr - triangleWidth;
			if (posMid<0) posMid += BUFFER_SIZE;
			posBegin = posMid - triangleWidth;
			if (posBegin<0) posBegin += BUFFER_SIZE;

			int32_t sampleBegin = circBuf[posBegin];
			int32_t sampleMid = circBuf[posMid];

			if (abs(sampleBegin-sample) < 0x800000 && sampleMid-sample > 0xB000000) {
				ret = FOUND_SYNC_ONE;

				bits[0] = 1;
				bitCnt = 1;

				lastZero = 0;
				lastOne = sampleMid;

				sampleCnt = triangleWidth;
				state = RECV;
			}
			break;

		case RECV:
			sampleCnt++;

			if (sampleCnt >= baudratePeriod) {
				sampleCnt = 0;

				// calculate threshold
				thr = (lastOne+lastZero)/2;

				//detect zero or one
				if (sample > thr) {
					bits[bitCnt] = 1;
					lastOne = sample;
					ret = DETECT_ONE;
				} else {
					bits[bitCnt] = 0;
					lastZero = sample;
					ret = DETECT_ZERO;
				}

				// sanity check: first bits should be syncronising preamble (1010101)
				// if not, it was false detector trigger
				if (bitCnt > 0 && bitCnt < 6) {
					if (bits[bitCnt-1] == bits[bitCnt]) {
						ret = FAIL_INVALID_PREAMBLE;
						state = WAIT;
						break;
					}
				}

				bitCnt++;
				if (bitCnt >= BIT_NUM)
					//success
					state = FRAME_END;
			}

			break;
		case FRAME_END:
			// only to confirm that frame was fully received
			ret = FULL_FRAME_RECVD;
			state = WAIT;
			break;
		default:
			state = WAIT;
	}

	return ret;
}


