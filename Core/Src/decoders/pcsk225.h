/*
 * pcsk225.h
 *
 *  Created on: 12 lis 2026
 *      Author: hubert
 */

#ifndef SRC_DETECTORS_PCSK225_H_
#define SRC_DETECTORS_PCSK225_H_

#include "main.h"

typedef struct {
	size_t preambleLen;		// to be used to calculate offset from header start
	uint8_t header;
	uint8_t payload[9];
} pcsk225Frame;

int pcsk225_decode(uint8_t *data, size_t len, pcsk225Frame *output);
void pcsk225_print(UART_HandleTypeDef *tty, pcsk225Frame *frame);


#endif /* SRC_DETECTORS_PCSK225_H_ */
