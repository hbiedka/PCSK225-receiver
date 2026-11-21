/*
 * phaseMod.c
 *
 *  Created on: 11 lis 2026
 *      Author: hubert
 */

#include "phaseMod.h"
#include <stdlib.h>
#include <string.h>

#define BIT_NUM 96
#define BUFFER_SIZE 200

static uint32_t triangleWidth;

int32_t circBuf[BUFFER_SIZE];
size_t circBufPtr = 0;

static uint32_t baudratePeriod = 176; 	//8789.0625/50 (samples per second/bits per second -> samples/bit)

size_t sampleCnt = 0;
uint8_t bits[BIT_NUM];
size_t bitCnt = 0;

int32_t lastOne = 0;
int32_t lastZero = 0;

// must be static to be consistend while sending data via uart async-ly
static char buffer[BIT_NUM+4];

typedef enum {
	WAIT,
	RECV_UNSTABLE,
	RECV_STABLE
} state_t;

static state_t state = WAIT;
static state_t prevState = WAIT;

static void driveRxLED() {
	if (prevState != state) {
		//state changed - deal with RX led
		if (state == RECV_STABLE) HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_SET);
		else HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_RESET);
		prevState = state;
	}
}

uint32_t phaseMod_init(uint32_t sampleRate, uint32_t baudRate, uint32_t syncBitWidth)
{
	baudratePeriod = sampleRate/baudRate;
	//division correction
	if ((sampleRate % baudRate) > (baudRate/2))baudratePeriod++;

	triangleWidth = syncBitWidth;

	return baudratePeriod;
}

uint8_t* phaseMod_getBitstream(size_t *size) {
	if (size) *size = BIT_NUM;
	return bits;
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

			// find phase shift up then down - it means zero from sync preamble
			// phase shift means zero and no phase shift means one

			posMid = circBufPtr - triangleWidth;
			if (posMid<0) posMid += BUFFER_SIZE;
			posBegin = posMid - triangleWidth;
			if (posBegin<0) posBegin += BUFFER_SIZE;

			int32_t sampleBegin = circBuf[posBegin];
			int32_t sampleMid = circBuf[posMid];

			if (abs(sampleBegin-sample) < 0x800000 && sampleMid-sample > 0xB000000) {
				ret = FOUND_SYNC_ZERO;

				bits[0] = 0;
				bitCnt = 1;

				lastZero = sampleMid;
				lastOne = 0;

				sampleCnt = triangleWidth;
				state = RECV_UNSTABLE;
			}
			break;

		case RECV_UNSTABLE:
		case RECV_STABLE:
			sampleCnt++;

			if (sampleCnt >= baudratePeriod) {
				sampleCnt = 0;

				// calculate threshold
				thr = (lastOne+lastZero)/2;

				//detect zero or one
				if (sample > thr) {
					bits[bitCnt] = 0;
					lastZero = sample;
					ret = DETECT_ZERO;
				} else {
					bits[bitCnt] = 1;
					lastOne = sample;
					ret = DETECT_ONE;
				}

				// sanity check: first bits should be syncronising preamble (01010101)
				// if not, it was false detector trigger
				if (bitCnt > 0 && bitCnt < 6) {
					if (bits[bitCnt-1] == bits[bitCnt]) {
						ret = FAIL_INVALID_PREAMBLE;
						state = WAIT;
						break;
					}
				} else {
					state = RECV_STABLE;
				}

				bitCnt++;
				if (bitCnt >= BIT_NUM) {
					//success
					ret = FULL_FRAME_RECVD;
					state = WAIT;
				}
			}

			break;
		default:
			state = WAIT;
	}

	//update Rx LED
	driveRxLED();
	return ret;
}

void phaseMod_cancel()
{
	state = WAIT;
	driveRxLED();
}


void phaseMod_printBitstream(UART_HandleTypeDef *tty)
{
	char *bufPos = buffer;

	*bufPos++ = '>';
	for (size_t i = 0; i < BIT_NUM; i++) {
		*bufPos++ = '0' + bits[i];
	}

	*bufPos++ = '\r';
	*bufPos++ = '\n';
	*bufPos++ = '\0';

	while (tty->gState != HAL_UART_STATE_READY);
	HAL_UART_Transmit_DMA(tty,(uint8_t*)buffer,strnlen(buffer,sizeof(buffer)));
}

