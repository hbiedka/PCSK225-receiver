/*
 * pcsk225.c
 *
 *  Created on: 12 lis 2026
 *      Author: hubert
 */

#include "pcsk225.h"
#include <stdio.h>
#include <string.h>

static char buffer[128];

// convert array of (len*8) bits to (len) bytes
static void bitstreamToData(uint8_t *bs, size_t numBytes, uint8_t *output)
{
    for (size_t i = 0; i < numBytes; i++) {
        uint8_t byteVal = 0;
        for (int j = 0; j < 8; j++) {
        	byteVal <<= 1;
            if (bs[i * 8 + j]) {
                byteVal |= 1;
            }
        }
        output[i] = byteVal;
    }
}

int pcsk225_decode(uint8_t *data, size_t len, pcsk225Frame *output)
{
	if (!output)
		return -1;


	uint8_t header = 0;
	uint32_t headerFound = 0;

	// look for header
	// we don't know for sure where it starts - sync bits may be detected during frame started
	// bits 0..5 are checked by phaseMod, so in worst case scenario it will be 6th bit
	// but max is 16th bit  (sync at very first sync bit
	size_t headerStart = 6;

	while (headerStart <= 16) {
		bitstreamToData(&data[headerStart],1,&header);
		// We assume that first 3 bits of header are not "010xxxxx"
		// For now we know two headers: 0x60 (e-Czas) and 0x1F (Enea)
		// but may be more in future, so for now it is minimal to recognise header
		if ((header & 0xE0) != 0x40) {
			headerFound = 1;
			break;
		}
		headerStart+=2;
	}

	if (!headerFound)
		return -1;

	output->preambleLen = headerStart;
	output->header = header;
	bitstreamToData(&data[headerStart+8],9,output->payload);

	return 0;
}

void pcsk225_print(UART_HandleTypeDef *tty, pcsk225Frame *frame)
{
	size_t cursor = 0;

	snprintf(buffer,sizeof(buffer),":%02X:",frame->header);
	cursor = strnlen(buffer,sizeof(buffer));

	for (size_t i = 0; i < sizeof(frame->payload); i++) {
		snprintf(buffer+cursor,sizeof(buffer)-cursor,"%02X",frame->payload[i]);
		cursor = strnlen(buffer,sizeof(buffer));
	}
	snprintf(buffer+cursor,sizeof(buffer)-cursor,"\r\n");

	while (tty->gState != HAL_UART_STATE_READY);
	HAL_UART_Transmit_DMA(tty,(uint8_t*)buffer,strnlen(buffer,sizeof(buffer)));

}



