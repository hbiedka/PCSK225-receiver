/*
 * IF -> AF mixer
 */


#ifndef SRC_MIXERS_AFMIX_H_
#define SRC_MIXERS_AFMIX_H_

#include "iq.h"

void afMix_init(int32_t *lut, size_t *period);
void afMix_mix(struct IQ *inputBegin, struct IQ *inputEnd, struct IQ *outputBegin);

#endif
