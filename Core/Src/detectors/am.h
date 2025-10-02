/*
 * am.h
 *
 *  Created on: Oct 2, 2025
 *      Author: hubert
 */

#ifndef SRC_DETECTORS_AM_H_
#define SRC_DETECTORS_AM_H_

#include "iq.h"

void amDetector_init(void);
void amDetector_detect(struct IQ *inputBegin, struct IQ *inputEnd, int32_t *outputBegin);

#endif /* SRC_DETECTORS_AM_H_ */
