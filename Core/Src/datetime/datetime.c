/*
 * datetime.c
 *
 *  Created on: 18 lis 2026
 *      Author: hubert
 */

#include "datetime.h"

void secondsToDatetime(uint64_t seconds, DateTime *dt)
{
    uint32_t days = seconds / 86400UL;
    uint32_t secsInDay = seconds % 86400UL;

    dt->hour = secsInDay / 3600;
    dt->minute = (secsInDay % 3600) / 60;
    dt->second = secsInDay % 60;

    // We are starting from 2000
    uint32_t year = 2000;

    while (1) {
        // Calculate leap year
        uint32_t daysInYear = ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0)) ? 366 : 365;
        if (days >= daysInYear) {
            days -= daysInYear;
            year++;
        } else {
            break;
        }
    }
    dt->year = (uint16_t)year;

    // Calculate leap year
    int leap = ((dt->year % 4 == 0 && dt->year % 100 != 0) || (dt->year % 400 == 0)) ? 1 : 0;
    const uint8_t daysInMonths[12] = {
        31, (leap ? 29 : 28), 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
    };

    uint8_t month = 0;
    while (month < 12 && days >= daysInMonths[month]) {
        days -= daysInMonths[month];
        month++;
    }
    dt->month = month + 1;	// months from 1 to 12
    dt->day = days + 1;    // days from 1 to 31
}
