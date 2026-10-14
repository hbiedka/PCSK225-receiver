/*
 * pll.h
 *
 *  Created on: 14 paź 2026
 *      Author: hubert
 */

#ifndef SRC_DETECTORS_PLL_H_
#define SRC_DETECTORS_PLL_H_

#include "main.h"

void pll_init(uint32_t _sampleRate, uint32_t _initFreq);
int32_t pll_detect(uint32_t angle);

int32_t getAngleError(uint32_t angle);

#endif /* SRC_DETECTORS_PLL_H_ */
