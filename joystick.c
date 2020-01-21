/*
 * joystick.c
 *
 *  Created on: 15.01.2020
 *      Author: Johannes Fritzsche
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

void joystick_fnx(UArg arg0);

/*
 *  Interrupt for arming and disarming
 */
void setArm(unsigned int index)
{
        if(isArmed == false)
        {
            isArmed = true;
        }
        else
        {
            isArmed = false;
        }
}

/*
 *  Interrupt for adjusting the throttle up (+25 steps). Limit at 2000.
 */
void throttleUp(unsigned int index)
{
    throttle = throttle + 25;
    if(throttle > 2000)
    {
        throttle = 2000;
    }
}

/*
 *  Interrupt for adjusting the throttle down (-25 steps). Limit at 1000.
 */
void throttleDown(unsigned int index)
{

    throttle = throttle - 25;
    if(throttle < 1000)
    {
        throttle = 1000;
    }
}

/*
 *  Set up the GPIO port and pins for the ADC driver to read the ADC values for the x and y axis.
 *  Set up the arm,up,and down buttons.
 */
void setup_ADC_edumkII(void)
{

    GPIO_setCallback(JS_ARM, setArm); //Associate a callback function with a particular GPIO pin interrupt.
    GPIO_enableInt(JS_ARM); //Enables GPIO interrupts for the selected index to occur.

    GPIO_setCallback(JS_UP, throttleUp);
    GPIO_enableInt(JS_UP);

    GPIO_setCallback(JS_DOWN, throttleDown);
    GPIO_enableInt(JS_DOWN);

    SysCtlPeripheralEnable(SYSCTL_PERIPH_ADC0); //This function enables ADC0 peripheral.
    while(SysCtlPeripheralReady(SYSCTL_PERIPH_ADC0) != true) //wait for ADC0 peripheral to be accessible
    {
    }

    GPIOPinTypeADC(JS_GPIO_BASE, JS_PITCH | JS_ROLL); //Configures the EDUMKII joystick pins for use as analog-to-digital converter inputs.

    ADCClockConfigSet(JS_ADC_BASE , ADC_CLOCK_SRC_PIOSC | ADC_CLOCK_RATE_EIGHTH, 1); //Configure the ADC to use PIOSC divided by one (16 MHz) and sample at an eighth the rate.
    ADCHardwareOversampleConfigure(JS_ADC_BASE , 64); // each sample in the ADC FIFO will be the result of 64 measurements being averaged together

    ADCSequenceDisable(JS_ADC_BASE, 1); //disable sample sequence before configuring it
    ADCSequenceConfigure(JS_ADC_BASE, 1, ADC_TRIGGER_PROCESSOR, 0); //sample sequence 1 (up to 4 samples) with processor trigger

    //two sample sequence steps for pitch and roll potentiometer
    ADCSequenceStepConfigure(JS_ADC_BASE, 1, 0, JS_CH_PITCH);
    ADCSequenceStepConfigure(JS_ADC_BASE, 1, 1, JS_CH_ROLL | ADC_CTL_IE | ADC_CTL_END);

    ADCSequenceEnable(JS_ADC_BASE, 1); //allows sample capture when triggered
}

/*
 *  Set up task for the Joystick controller with the highest task priority for fast reading of the ADC values
 */
void setUpJoyStick_Task(void)
{

    Task_Params old_taskParams;
    Task_Handle old_taskHandle;
    Error_Block eb;
    Error_init(&eb);

    Task_Params_init(&old_taskParams);
    old_taskParams.stackSize = 2024; //Stacksize in bytes
    old_taskParams.priority = 15; //0-15, 15 being highest priority
    old_taskParams.arg0 = (UArg) 1;
    old_taskHandle = Task_create((Task_FuncPtr) joystick_fnx, &old_taskParams, &eb);

    if (old_taskHandle == NULL)
    {
        System_abort("Create Joystick_task_setup failed");
    }
}

/*
 *  This is the joystick RTOS task, also used
 *  for processing joystick and button data. Scale and limit ADC values to range of 1000-2000.
 *  Send copter control data (roll,pitch,throttle,isArmed) to bluetooth.c to be packaged and sent to the copter.
 */
void joystick_fnx(UArg arg0)
{
    throttle = 1000;
    uint32_t adcSamples[2];
    static int16_t offsetRoll = 0;
    static int16_t offsetPitch = 0;
    static uint16_t roll = 1500;
    static uint16_t pitch = 1500;

    ADCIntClear(ADC0_BASE, 1);
    ADCProcessorTrigger(ADC0_BASE, 1);
    while (!ADCIntStatus(ADC0_BASE, 1, false))
    {
    }
    ADCSequenceDataGet(ADC0_BASE, 1, adcSamples);

    //calculate offset, while not touching joystick at the start
    offsetRoll = 2000 - adcSamples[0];
    offsetPitch = 2000 - adcSamples[1];

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

        roll = (adcSamples[1] + offsetRoll) / 4  + 1000; //scale and limit adc roll value
        if(roll < 1000)
        {
            roll = 1000;
        }
        if(roll > 2000)
        {
            roll = 2000;
        }

        pitch = (adcSamples[0] + offsetPitch) / 4  + 1000; //scale and limit adc pitch value
        if(pitch < 1000)
        {
            pitch = 1000;
        }
        if(pitch > 2000)
        {
            pitch = 2000;
        }

        send_controls(roll,pitch,throttle,isArmed); //send adc and button data to bluetooth.c
    }
}
