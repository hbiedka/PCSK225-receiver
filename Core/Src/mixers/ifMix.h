/*
 * ifMix.h
 *
 *  Created on: Sep 8, 2025
 *      Author: hubert
 */

#ifndef SRC_MIXERS_IFMIX_H_
#define SRC_MIXERS_IFMIX_H_

#include "iq.h"

void ifMix_init(uint32_t _sampleRate, uint32_t _downsampleRatio);
void ifMix_setFreq(uint32_t freq);
void ifMix_Mix(const uint16_t *inputBegin, const uint16_t *inputEnd, struct IQ *outputBegin);

#endif /* SRC_MIXERS_IFMIX_H_ */
