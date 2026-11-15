/*
 * agc.h
 *
 *  Created on: 15 lis 2026
 *      Author: hubert
 */

#ifndef SRC_FILTERS_AGC_H_
#define SRC_FILTERS_AGC_H_

#include <stdint.h>

void agc_init(uint8_t _windowLenOrder);
uint32_t agc_update(int16_t input);


#endif /* SRC_FILTERS_AGC_H_ */

