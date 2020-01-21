/*
 * bluetooth.c
 *
 *  Created on: 15.01.2020
 *      Author: Martin
 */

//#define TARGET_IS_TM4C129_RA2 /* Tells rom.h the version of the silicon */

#include <stdint.h> /* C99 header for uint*_t types */
#include <stdbool.h> /* Driverlib headers require stdbool.h to be included fiGPIO_PIN_4 */
#include <driverlib/gpio.h> /* Supplies GPIO* functions and GPIO_PIN_x */
#include <driverlib/pin_map.h>
#include <driverlib/rom.h> /* Supplies ROM_* variations of functions */
#include <driverlib/sysctl.h> /* Supplies SysCtl* functions and SYSCTL_* macros */
#include <driverlib/uart.h>
#include <inc/hw_memmap.h> /* Supplies GPIO_PORTx_BASE */

#include <xdc/runtime/System.h>

/*Board Header files */
#include <Board.h>
#include <EK_TM4C1294XL.h>

/* TI-RTOS Header files */
#include <driverlib/sysctl.h>
#include <ti/drivers/UART.h>

/* BIOS Header files */
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Task.h>

#include <string.h>

#include <xdc/runtime/Error.h>

#include <bluetooth.h>


/*! uart   Global UART handler for UART reading/writing */
UART_Handle uart;
/*! ready_for_data   Global 8 bit variable for signalling that the copter is ready for receiving data */
uint8_t bluetooth_ready = 0;

void send_data(char *data, size_t size)
{
    GPIOPinWrite(GPIO_PORTD_BASE, GPIO_PIN_4, GPIO_PIN_4);
    while(GPIOPinRead(GPIO_PORTP_BASE, GPIO_PIN_5) != 0x00);
    UART_write(uart, data, size);
    while(UARTBusy(UART6_BASE));
    GPIOPinWrite(GPIO_PORTD_BASE, GPIO_PIN_4, 0);
}

void send_controls(uint16_t roll, uint16_t pitch, uint16_t throttle, bool armed)
{
    uint16_t azimuth = 1500; //needed?
    uint8_t payload_size = 16;

    char payload[payload_size];
    payload[0] = 0x24; // $
    payload[1] = 0x4D; // M
    payload[2] = 0x3C; // >
    payload[3] = 0x0A; // size of data
    payload[4] = 0xC8; // cmd (200 for setting RC)

    payload[5] = pitch;
    payload[6] = (pitch>>8);
    payload[7] = roll;
    payload[8] = (roll>>8);
    payload[9] = throttle;
    payload[10] = (throttle>>8);
    payload[11] = azimuth;
    payload[12] = (azimuth>>8);
    payload[13] = armed ? 0xd0 : 0xe8;
    payload[14] = armed ? 0x07 : 0x03;

    char checksum = 0;
    int i;
    for (i = 3; i < 15; i++)
    {
        checksum ^= payload[i];
    }
    payload[15] = checksum;

    send_data(payload, sizeof(payload));
}

void send_command(char *cmd, uint8_t cmdSize, uint8_t returnSize, char *returnVal)
{
    char inp[16] = { '\0' };

    //TODO: check return value
    GPIOPinWrite(GPIO_PORTD_BASE, GPIO_PIN_4, GPIO_PIN_4);

    while(GPIOPinRead(GPIO_PORTP_BASE, GPIO_PIN_5) != 0x00)
    {}

    //TODO: check return value
    UART_write(uart, cmd, cmdSize);

    while(UARTBusy(UART6_BASE))
    {}

    GPIOPinWrite(GPIO_PORTD_BASE, GPIO_PIN_4, 0);

    //TODO: check return value
    UART_read(uart, &inp, returnSize);
    strcpy(returnVal, inp);

    System_printf("sent command: %s\n", cmd);
    System_printf("got return: %s\n", returnVal);

    System_flush();
    Task_sleep(5);
}

//TODO: docu
int connect_to_copter()
{
    char returnVal[30];

    //first enter command mode of bluetooth module
    char cmdMode[3] = "$$$"; //command to enter command mode
    send_command(cmdMode, sizeof(cmdMode), 4, returnVal);

    if(strstr(returnVal, "CMD") == NULL)
    {
        System_printf("Failed to enter command mode\n");
        System_flush();
        return NULL;
    }

    // connect to copter
    char connectCmd[15] = "C,0006668CB2AC\r"; // connect to copter with its MAC-address
    send_command(connectCmd, sizeof(connectCmd), 16, returnVal);

    //TODO: check returnVal correctly
    //    if(strstr(returnVal, "CONNECT") == NULL)
    //    {
    //        System_printf("Failed to connect to copter: %s\n", returnVal);
    //        System_flush();
    //        return NULL;
    //    }

    // wait for connected status
    while((GPIOPinRead(GPIO_PORTQ_BASE, GPIO_PIN_0) != 0x00) || (GPIOPinRead(GPIO_PORTQ_BASE, GPIO_PIN_3) == 0x00));
    System_printf("Connected\n");
    System_flush();

    // leave command mode
    char leaveCmdMode[5] = "---\r\n";
    send_command(leaveCmdMode, sizeof(leaveCmdMode), 1, returnVal);

    // flush UART In-Buffer
    while(UARTCharsAvail(UART6_BASE))
    {
        //TODO: check return value
        UART_read(uart, &returnVal, 1);
    }

    return 1;
}

//TODO: docu
void UART_Task(UArg arg0, UArg arg1)
{
    UART_Params uartParams;

    /* Create a UART with data processing off.*/
    UART_Params_init(&uartParams);
    uartParams.writeDataMode = UART_DATA_BINARY;
    uartParams.readDataMode = UART_DATA_BINARY;
    uartParams.readReturnMode = UART_RETURN_FULL;
    uartParams.readEcho = UART_ECHO_OFF;
    uartParams.baudRate = 115200;
    uartParams.readMode = UART_MODE_BLOCKING;

    uart = UART_open(Board_UART6, &uartParams);

    if (uart == NULL)
    {
        System_abort("Error opening the UART");
    }

    System_printf("UART initialized\n");
    System_flush();
    Task_sleep(10);

    if(connect_to_copter() == NULL)
    {
        System_abort("Connection to copter failed\n");
    }

    Task_sleep(100);

    System_printf("Begin data\n");
    System_flush();
    bluetooth_ready = 1;
}


//TODO: docu
void set_Pins()
{
    //TODO: explanation what PINS are for
    // Status Pins
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOQ);
    GPIOPinTypeGPIOInput(GPIO_PORTQ_BASE, GPIO_PIN_0);
    GPIOPinTypeGPIOInput(GPIO_PORTQ_BASE, GPIO_PIN_3);

    // CTS/RTS
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOP);
    GPIOPinTypeGPIOInput(GPIO_PORTP_BASE, GPIO_PIN_5);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOD);
    GPIOPinTypeGPIOOutput(GPIO_PORTD_BASE, GPIO_PIN_4);
    GPIOPadConfigSet(GPIO_PORTD_BASE, GPIO_PIN_4, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPU);
    GPIOPinWrite(GPIO_PORTD_BASE, GPIO_PIN_4, 0);

    // SW_BTN
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOD);
    GPIOPinTypeGPIOOutput(GPIO_PORTD_BASE, GPIO_PIN_2);
    GPIOPadConfigSet(GPIO_PORTD_BASE, GPIO_PIN_2, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPU);
    GPIOPinWrite(GPIO_PORTD_BASE, GPIO_PIN_2, 0);

    // RST_N
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOP);
    GPIOPinTypeGPIOOutput(GPIO_PORTP_BASE, GPIO_PIN_4);
    GPIOPadConfigSet(GPIO_PORTP_BASE, GPIO_PIN_4, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPU);
    GPIOPinWrite(GPIO_PORTP_BASE, GPIO_PIN_4, GPIO_PIN_4);

    // WAKE_UP
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOM);
    GPIOPinTypeGPIOOutput(GPIO_PORTM_BASE, GPIO_PIN_7);
    GPIOPadConfigSet(GPIO_PORTM_BASE, GPIO_PIN_7, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPU);
    GPIOPinWrite(GPIO_PORTM_BASE, GPIO_PIN_7, GPIO_PIN_7);
}

//TODO: docu
void init_bt_module()
{
    // init sequence
     SysCtlDelay((120000000 / 1000) * 500);
     GPIOPinWrite(GPIO_PORTP_BASE, GPIO_PIN_4, 0);
     SysCtlDelay((120000000 / 1000) * 100);
     GPIOPinWrite(GPIO_PORTP_BASE, GPIO_PIN_4, GPIO_PIN_4);
     SysCtlDelay((120000000 / 1000) * 100);
     GPIOPinWrite(GPIO_PORTD_BASE, GPIO_PIN_2, GPIO_PIN_2);
     SysCtlDelay((120000000 / 1000) * 500);

     // wait for the correct status of the RN4678
     while((GPIOPinRead(GPIO_PORTQ_BASE, GPIO_PIN_0) != 0x00) && (GPIOPinRead(GPIO_PORTQ_BASE, GPIO_PIN_3) != 0x00))
     {}

     SysCtlDelay((120000000 / 1000) * 500);
}

int setup_UART()
{
    /*configure uart6 as interface to bluetooth module*/
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOP);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART6);
    GPIOPinConfigure(GPIO_PP0_U6RX);
    GPIOPinConfigure(GPIO_PP1_U6TX);
    GPIOPinTypeUART(GPIO_PORTP_BASE, GPIO_PIN_1 | GPIO_PIN_0);

    UART_init();

    //set necessary pins for initializing bluetooth module
    set_Pins();

    init_bt_module();

    //Create the task
    Task_Params UART_Task_Params;
    Task_Handle UART_Task_Handle;
    Error_Block eb;

    Error_init(&eb);
    Task_Params_init(&UART_Task_Params);
    UART_Task_Params.stackSize = 1024; /* stack in bytes */
    UART_Task_Params.priority = 15; /* 0-15 (15 is highest priority on default -> see RTOS Task configuration) */
    UART_Task_Handle = Task_create((Task_FuncPtr)UART_Task, &UART_Task_Params, &eb);
    if (UART_Task_Handle == NULL)
    {
        System_printf("Failed to create UART task");
        System_flush();
        return NULL;
    }
    return 1;
}
