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
#include <bluetooth.h>

#include "driverlib/sysctl.h"
#include "driverlib/adc.h"
#include <ti/sysbios/knl/Task.h>
#include <xdc/runtime/System.h>
#include <ti/drivers/GPIO.h>
#include <Board.h>

static Bool isArmed = false;
static uint16_t throttle = 1000;
extern uint8_t bluetooth_ready;

void joystick_fnx(UArg arg0 );


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
void setArm(unsigned int index)
{
    // JoyStick Select Button
    //

        if(isArmed == false)
        {
            isArmed = true;
        }
        else
        {
            isArmed = false;
        }
}

void throttleUp(unsigned int index)
{
    throttle = throttle + 10;
    if(throttle > 2000)
    {
        throttle = 2000;
    }
}

void throttleDown(unsigned int index)
{

    throttle = throttle - 10;
    if(throttle < 1000)
    {
        throttle = 1000;
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
void setup_ADC_edumkII(void)
{

    GPIO_setCallback(JS_ARM, setArm);
    GPIO_enableInt(JS_ARM);

    GPIO_setCallback(JS_UP, throttleUp);
    GPIO_enableInt(JS_UP);

    GPIO_setCallback(JS_DOWN, throttleDown);
    GPIO_enableInt(JS_DOWN);

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
 *             for processing joystick and button data. Scale and limit ADC values to range of 1000-2000.
 *             send roll,pitch,throttle,isArmed to
 *
 *@param       arg0   xdc argument for the RTOS task.
 *
 * PNB:
 *
 *@result      Nothing
 * */
void joystick_fnx(UArg arg0 )
{
    uint32_t adcSamples[2];
    static int16_t offsetRoll = 0;
    static int16_t offsetPitch = 0;

    throttle = 1000;
    static uint16_t roll = 1500;
    static uint16_t pitch = 1500;

    ADCIntClear(ADC0_BASE, 1);
    ADCProcessorTrigger(ADC0_BASE, 1);
    while (!ADCIntStatus(ADC0_BASE, 1, false))
    {
    }
    ADCSequenceDataGet(ADC0_BASE, 1, adcSamples);

    //System_printf("Joystick X: %u Joystick Y: %u\n", adcSamples[0],adcSamples[1]);
    //System_flush();

    offsetRoll = 2000 - adcSamples[1];
    offsetPitch = 2000 - adcSamples[0];

    //System_printf("offset Joystick X: %i offset Joystick Y: %i\n", offsetRoll,offsetPitch);
    //System_flush();

    while (1)
    {
        Task_sleep(50);
        if(bluetooth_ready == 0)
        {
            continue;
        }
        ADCIntClear(ADC0_BASE, 1);
        ADCProcessorTrigger(ADC0_BASE, 1);
        while (!ADCIntStatus(ADC0_BASE, 1, false))
        {
        }
        ADCSequenceDataGet(ADC0_BASE, 1, adcSamples);

        roll = (adcSamples[1] + offsetRoll) / 4  + 1000;

        if(roll < 1000)
        {
            roll = 1000;
        }
        if(roll > 2000)
        {
            roll = 2000;
        }

        pitch = (adcSamples[0] + offsetPitch) / 4  + 1000;

        if(pitch < 1000)
        {
            pitch = 1000;
        }
        if(pitch > 2000)
        {
            pitch = 2000;
        }

        //System_printf("Joystick X-Axis: %u Y-Axis: %u\n",roll,pitch);
        //System_flush();

        send_controls(roll,pitch,throttle,isArmed);
    }
}
