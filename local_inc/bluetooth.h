/*
 * bluetooth.h
 *
 *  Created on: 15.01.2020
 *      Author: Martin
 */

#ifndef BLUETOOTH_H_
#define BLUETOOTH_H_


extern void send_data(char *data, size_t size);

int setup_UART();

#endif /* BLUETOOTH_H_ */
