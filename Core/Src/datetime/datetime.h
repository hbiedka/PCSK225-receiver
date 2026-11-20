/*
 * datetime.h
 *
 *  Created on: 18 lis 2026
 *      Author: hubert
 */

#ifndef SRC_DATETIME_DATETIME_H_
#define SRC_DATETIME_DATETIME_H_

#include <stdint.h>

typedef struct {
    uint16_t year;
    uint8_t  month;
    uint8_t  day;
    uint8_t  hour;
    uint8_t  minute;
    uint8_t  second;
    uint8_t  cents;
} DateTime;

void secondsToDatetime(uint64_t seconds, DateTime *dt);

#endif /* SRC_DATETIME_DATETIME_H_ */
