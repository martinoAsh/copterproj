/*
 *  ======== StartBIOS.c ========
 */
#include <stdbool.h>
#include <stdint.h>

/* XDCtools Header files */
#include <xdc/std.h>
#include <xdc/cfg/global.h>
#include <xdc/runtime/System.h>
#include <xdc/runtime/Error.h>
#include <xdc/runtime/Memory.h>

/* BIOS Header files */
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Event.h>
#include <ti/sysbios/knl/Semaphore.h>

/* Board Header files */
#include <Board.h>
#include <EK_TM4C1294XL.h>

#include <stdbool.h>
#include <stdint.h>
#include <inc/hw_memmap.h>

/* XDCtools Header files */
#include <xdc/std.h>
#include <xdc/cfg/global.h>
#include <xdc/runtime/System.h>
#include <xdc/runtime/Error.h>
#include <xdc/runtime/Memory.h>

/* BIOS Header files */
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Task.h>

/* Driverlib headers */
#include <driverlib/gpio.h>

/* Board Header files */
#include <Board.h>
#include <EK_TM4C1294XL.h>

/* Clocks and Events */
#include <ti/sysbios/knl/Clock.h>
#include <ti/sysbios/knl/Event.h>

//driverlib
#include <driverlib/sysctl.h>
#include <driverlib/rom.h>

#include <bluetooth.h>
#include <joystick.h>

int main(void)
{
    //Clock auf 120MHZ setzen
    SysCtlClockFreqSet(SYSCTL_XTAL_25MHZ | SYSCTL_OSC_MAIN | SYSCTL_USE_PLL | SYSCTL_CFG_VCO_480, 120000000);

    //Aktivieren Port C
     SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOC);
     //Aktivieren Pin 7 (Pin für LED Ansteuerung)
     GPIOPinTypeGPIOOutput(GPIO_PORTC_BASE,GPIO_PIN_7);

    //Sysclock
    uint32_t ui32SysClock;

    //Call board init functions.
    ui32SysClock = Board_initGeneral(120*1000*1000);
    (void)ui32SysClock;
    Board_initI2C();

    setup_UART();

    EdM_ADC_Init();
    System_printf("Setting up ADC for Joystick Done\n");
    System_flush();

    setUpJoyStick_Task();
    System_printf("Set up Joystick Task\n");
    System_flush();



    //SysMin will only print to the console upon calling flush or exit8*
    //Start BIOS
    BIOS_start();
    System_printf("Start BIOS\n");
    System_flush();
}
