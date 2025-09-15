/*
 * ifMix.h
 *
 *  Created on: Sep 8, 2025
 *      Author: hubert
 */

#ifndef SRC_MIXERS_IFMIX_H_
#define SRC_MIXERS_IFMIX_H_

#include "iq.h"

void ifMix_init(int32_t *i_lut,int32_t *q_lut, uint32_t *period, uint32_t ratio);

//TODO use const pointers
void ifMix_Mix(uint16_t *inputBegin, uint16_t *inputEnd, struct IQ *outputBegin);

#endif /* SRC_MIXERS_IFMIX_H_ */
