/*
 * bluetooth.h
 *
 *  Created on: 15.01.2020
 *      Author: Martin
 */

#ifndef BLUETOOTH_H_
#define BLUETOOTH_H_

void send_controls(uint16_t roll, uint16_t pitch, uint16_t throttle, bool armed);

int setup_UART();

#endif /* BLUETOOTH_H_ */
