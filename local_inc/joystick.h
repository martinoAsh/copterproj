/*
 * joystick.h
 *
 *  Created on: 15.01.2020
 *      Author: Johannes Fritzsche
 */

#ifndef LOCAL_INC_JOYSTICK_H_
#define LOCAL_INC_JOYSTICK_H_

/*\include
 * -------------------------------------------------------------- includes --
 */
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include "inc/hw_memmap.h"
#include "driverlib/gpio.h"

#include <ti/sysbios/knl/Mailbox.h>
#include <xdc/runtime/Error.h>
#include <ti/sysbios/knl/Event.h>

/*!
 * --------------------------------------------------------------- defines --
 */
#define JS_GPIO_BASE GPIO_PORTE_BASE
#define JS_ADC_BASE ADC0_BASE
#define JS_TIMER_BASE TIMER0_BASE

#define JS_SAMPLE_RATE 4

#define JS_X        GPIO_PIN_4
#define JS_Y        GPIO_PIN_3
#define JS_CH_X     ADC_CTL_CH9
#define JS_CH_Y     ADC_CTL_CH0

#define JS_UP       EDUMKII_BUTTON1
#define JS_DOWN     EDUMKII_BUTTON2
#define JS_ARM      EDUMKII_SELECT

/*
 * Gt All the stuffs for the ADC ready for the TI EDUMIKI Joystick Controller
 * */
/**\fn
 * ------------------------------------------------------------- functions --
 * */
extern void EdM_ADC_Init(void);
extern void setUpJoyStick_Task();

#endif /* LOCAL_INC_JOYSTICK_H_ */

