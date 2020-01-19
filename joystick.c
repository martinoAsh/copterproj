/*
 * joystick.c
 *
 *  Created on: 15.01.2020
 *      Author: Johannes Fritzsche
 */

/*\include
 * -------------------------------------------------------------- includes --
 */
#include <joystick.h>

#include "driverlib/debug.h"
#include "driverlib/sysctl.h"

#include "inc/hw_types.h"
#include "driverlib/sysctl.h"
#include "driverlib/adc.h"

#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Event.h>
#include <ti/sysbios/knl/Task.h>

#include <xdc/cfg/global.h>
#include <xdc/runtime/System.h>
#include <xdc/runtime/Error.h>
#include <xdc/runtime/Memory.h>
#include "inc/hw_memmap.h"
#include "driverlib/gpio.h"

#include <ti/sysbios/knl/Event.h>
#include <ti/drivers/GPIO.h>
#include <driverlib/sysctl.h>
#include <Board.h>

static Bool qpArmToggle = false;
raw_rc_frame frame;
//extern uint8_t ready_for_data;


void joystick_fnx(UArg arg0 );
//void set_flight_controls();
void arm();
void disarm();


/*!
 * @brief      This is Joystick middle button.
 *             Using simple Interrupt for arm and disarm function
 *
 *@param       index    the button ID
 *
 * PNB:
 *
 *@result      Nothing
 * */
void gpioSeLFxn0(unsigned int index)
{
    // JoyStick Select Button
    //
        System_printf("JoyStick Select Pressed......\n");
        System_flush();

        if(qpArmToggle == false)
        {
            qpArmToggle = true;
            disarm();
        }
        else
        {
            qpArmToggle = false;
            arm();
        }
}

/*!
 * @brief      Set up the GPIO port and pins for the ADC driver
 *             which is used to read the ADC value for the x, y-axis, and accelerometer values
 *             of the EDUMIKI Joystick controller
 *
 *@param       void    nothing
 *@result
 * */
void EdM_ADC_Init(void)
{
    /*
     * Center of the Joystick is a normal Button
     * Set it up
     *
     * This will be used later for locking the Qcopter Position or the Joystick
     * */
    GPIO_setCallback(JS_ARM, gpioSeLFxn0);
    GPIO_enableInt(JS_ARM);

    SysCtlPeripheralEnable(SYSCTL_PERIPH_ADC0);

    GPIOPinTypeADC(JS_GPIO_BASE, JS_X | JS_Y);

    ADCClockConfigSet(JS_ADC_BASE , ADC_CLOCK_SRC_PIOSC | ADC_CLOCK_RATE_EIGHTH, 1);
    ADCHardwareOversampleConfigure(JS_ADC_BASE , 64); // each sample in the ADC FIFO will be the result of 64 measurements being averaged together

    ADCSequenceDisable(JS_ADC_BASE, 1);

    ADCSequenceConfigure(JS_ADC_BASE, 1, ADC_TRIGGER_PROCESSOR, 0);

    ADCSequenceStepConfigure(JS_ADC_BASE, 1, 0, JS_CH_X);
    ADCSequenceStepConfigure(JS_ADC_BASE, 1, 1, JS_CH_Y | ADC_CTL_IE | ADC_CTL_END);

    ADCSequenceEnable(JS_ADC_BASE, 1);

}

/*!
 * @brief      Set up task for the Joystick controller with the hight
 *             task priority for fast pulling of the ADC values
 *
 *@param       void    nothing
 *@result
 * */
void setUpJoyStick_Task(void)
{

    Task_Params old_taskParams;
    Task_Handle old_taskHandle;
    Error_Block eb;
    Error_init(&eb);

    Task_Params_init(&old_taskParams);
    old_taskParams.stackSize = 2024; //Stacksize in bytes
    old_taskParams.priority = 15; // 0-15, 15 being highest priority
    old_taskParams.arg0 = (UArg) 1;
    old_taskHandle = Task_create((Task_FuncPtr) joystick_fnx, &old_taskParams, &eb);

    if (old_taskHandle == NULL)
    {
        System_abort("Create Joystick_task_setup failed");
    }
}


/*!
 * @brief      This is the joystick RTOS task, also used
 *              for processing Joystick and accelerometer data. The ADC values needs to be
 *              processed to limit input signal to the upper and lower saturation values.
 *              set_flight_controls is then called to get data to the quadcopter by
 *              sending payload to the quadcopter via the send_pac function
 *
 *@param       arg0   xdc argument to the RTOS task.
 *
 * PNB:
 *
 *@result      Nothing
 * */
void joystick_fnx(UArg arg0 )
{
    uint32_t adcSamples[6];
    static uint32_t ui32JoyX = 0;
    static uint32_t ui32JoyY = 0;

    frame.throttle = 1000;
    frame.roll = 1500;
    frame.pitch = 1500;

    while (1)
    {
        //if(ready_for_data != 0)
        //{
            ADCIntClear(ADC0_BASE, 1);
            ADCProcessorTrigger(ADC0_BASE, 1);
            while (!ADCIntStatus(ADC0_BASE, 1, false))
            {
            }
            ADCSequenceDataGet(ADC0_BASE, 1, adcSamples);

            //Delay a bit and read the ADC1
            SysCtlDelay(200);

            ui32JoyX = adcSamples[0];
            ui32JoyY = adcSamples[1];

            System_printf("Joystick X-Axis: %u Y-Axis: %u\n",ui32JoyX,ui32JoyY);
            System_flush();

        Task_sleep(1000);
    }
}


void arm()
{
    System_printf("Arming\n");
    System_flush();

    // the sequence value of arm to be armed
    frame.arm = 1;

}

void disarm()
{
    System_printf("DisArming\n");
    System_flush();
    frame.arm = 0; //DEFAULT

}
