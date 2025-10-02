/*
 * sfssb.h
 *
 *  Created on: Oct 2, 2025
 *      Author: hubert
 */

#ifndef SRC_DETECTORS_SFSSB_H_
#define SRC_DETECTORS_SFSSB_H_

#include "iq.h"

void sfSsbDetector_init(struct IQ *bufferBegin, struct IQ *bufferEnd, uint32_t sampleRatio, uint32_t targetFreq);
void sfSsbDetector_detect(struct IQ *inputBegin, struct IQ *inputEnd, int32_t *outputBegin);

#endif /* SRC_DETECTORS_SFSSB_H_ */
