/*
 * dcOffset.h
 *
 *  Created on: 1 paź 2026
 *      Author: hubert
 */

#ifndef SRC_FILTERS_DCOFFSET_H_
#define SRC_FILTERS_DCOFFSET_H_

#include <stdint.h>

void dcOffset_init(uint8_t _windowLenOrder);
uint16_t dcOffset_filter(uint16_t input);


#endif /* SRC_FILTERS_DCOFFSET_H_ */
