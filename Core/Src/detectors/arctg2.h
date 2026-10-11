/*
 * arctg2.h
 *
 *  Created on: 11 paź 2026
 *      Author: hubert
 */

#ifndef SRC_DETECTORS_ARCTG2_H_
#define SRC_DETECTORS_ARCTG2_H_

#include <stdlib.h>
#include "iq.h"

#define ATAN_PI 0x80000000	//(2^32)/2 -> 180 deg.
#define ATAN_HALF_PI 0x40000000	//(2^32)/4 -> 90 deg.

uint32_t arctg2(int32_t y, int32_t x);
uint32_t IQarctg2(struct IQ *iq);



#endif /* SRC_DETECTORS_ARCTG2_H_ */
