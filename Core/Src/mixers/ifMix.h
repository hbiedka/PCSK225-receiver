/*
 * ifMix.h
 *
 *  Created on: Sep 8, 2025
 *      Author: hubert
 */

#ifndef SRC_MIXERS_IFMIX_H_
#define SRC_MIXERS_IFMIX_H_

#include "iq.h"

void ifMix_init(struct IQ *ifSamples, size_t ifSamplesLen, uint32_t ratio, uint32_t period);
size_t ifMix_getOutputBufferPos();
void ifMix_Mix(uint16_t *inputBegin, uint16_t *inputEnd);

#endif /* SRC_MIXERS_IFMIX_H_ */
