/*
 * bluetooth.c
 *
 *  Created on: 15.01.2020
 *      Author: Martin
 */

//#define TARGET_IS_TM4C129_RA2 /* Tells rom.h the version of the silicon */

#include <stdint.h> /* C99 header for uint*_t types */
#include <stdbool.h> /* Driverlib headers require stdbool.h to be included first */
#include <driverlib/gpio.h> /* Supplies GPIO* functions and GPIO_PIN_x */
#include <driverlib/pin_map.h>
#include <driverlib/rom.h> /* Supplies ROM_* variations of functions */
#include <driverlib/sysctl.h> /* Supplies SysCtl* functions and SYSCTL_* macros */
#include <driverlib/uart.h>
#include <inc/hw_memmap.h> /* Supplies GPIO_PORTx_BASE */

/*Board Header files */
#include <Board.h>
#include <EK_TM4C1294XL.h>

/* TI-RTOS Header files */
#include <driverlib/sysctl.h>
#include <ti/drivers/UART.h>

/* BIOS Header files */
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Task.h>

#include <xdc/runtime/Error.h>

#include <bluetooth.h>


int setup_UART()
{
    Task_Params taskUARTParams;
    Task_Handle taskUART;
    Error_Block eb;

    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART6);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOP);

    //set UART pins
    GPIOPinConfigure(GPIO_PP0_U6RX);
    GPIOPinConfigure(GPIO_PP1_U6TX);
    GPIOPinTypeUART(GPIO_PORTP_BASE, GPIO_PIN_0 | GPIO_PIN_0);
    UART_init(); //--linker error, missing include???

    return 0;
}
