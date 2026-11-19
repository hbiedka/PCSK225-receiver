/*
 * nmea0103.h
 *
 *  Created on: 19 lis 2026
 *      Author: hubert
 */

#ifndef SRC_DATETIME_NMEA_H_
#define SRC_DATETIME_NMEA_H_

#include "main.h"
#include "datetime.h"

void nmea_sendGPRMCFromTimestamp(UART_HandleTypeDef *tty, uint32_t timestamp);
void nmea_sendGPRMCFromDT(UART_HandleTypeDef *tty, DateTime *dt);


#endif /* SRC_DATETIME_NMEA_H_ */
