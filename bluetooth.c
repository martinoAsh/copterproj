/*
 * bluetooth.c
 *
 *  Created on: 15.01.2020
 *      Author: Martin
 */

#include <stdint.h> /* C99 header for uint*_t types */
#include <stdbool.h> /* Driverlib headers require stdbool.h to be included fiGPIO_PIN_4 */
#include <driverlib/gpio.h> /* Supplies GPIO* functions and GPIO_PIN_x */
#include <driverlib/pin_map.h>
#include <driverlib/uart.h>
#include <inc/hw_memmap.h> /* Supplies GPIO_PORTx_BASE */

/*Board Header files */
#include <Board.h>
#include <EK_TM4C1294XL.h>

/* TI-RTOS Header files */
#include <driverlib/sysctl.h>
#include <ti/drivers/UART.h>

/* BIOS Header files */
#include <ti/sysbios/knl/Task.h>

#include <string.h>

/* XDCtools Header files */
#include <xdc/runtime/Error.h>
#include <xdc/runtime/System.h>

#include <bluetooth.h>


/*! uart   Global UART handler for UART reading/writing */
UART_Handle uart;
/*! ready_for_data   Global 8 bit variable for signalling that the copter is ready for receiving data */
uint8_t bluetooth_ready = 0;

void send_data(char *data, size_t size)
{
    //D4 is to activate uart?
    GPIOPinWrite(GPIO_PORTD_BASE, GPIO_PIN_4, GPIO_PIN_4);
    //Read P5 --- P5 is to read UART?
    while(GPIOPinRead(GPIO_PORTP_BASE, GPIO_PIN_5) != 0x00);
    UART_write(uart, data, size);
    while(UARTBusy(UART6_BASE));

    //Set D4 0
    GPIOPinWrite(GPIO_PORTD_BASE, GPIO_PIN_4, 0);
}

void send_controls(uint16_t roll, uint16_t pitch, uint16_t throttle, bool armed)
{
    uint16_t spin = 1500; //currently not possible to control the spin (leave at default: 1500)

    char payload[16];
    payload[0] = 0x24; // $
    payload[1] = 0x4D; // M
    payload[2] = 0x3C; // >
    payload[3] = 0x0A; // size of data = 10
    payload[4] = 0xC8; // cmd (200 for setting RC)

    payload[5] = pitch;
    payload[6] = (pitch>>8);
    payload[7] = roll;
    payload[8] = (roll>>8);
    payload[9] = throttle;
    payload[10] = (throttle>>8);
    payload[11] = spin;
    payload[12] = (spin>>8);
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
    char uartReturn[16] = { '\0' };

    //TODO: check return value
    //Set D4 high -> writing to uart
    GPIOPinWrite(GPIO_PORTD_BASE, GPIO_PIN_4, GPIO_PIN_4);

    //P5 must be 0
    while(GPIOPinRead(GPIO_PORTP_BASE, GPIO_PIN_5) != 0x00)
    {}

    //TODO: check return value
    UART_write(uart, cmd, cmdSize);

    //wait until finished
    while(UARTBusy(UART6_BASE))
    {}

    //set D4 low
    GPIOPinWrite(GPIO_PORTD_BASE, GPIO_PIN_4, 0);

    //TODO: check return value
    UART_read(uart, &uartReturn, returnSize);
    strcpy(returnVal, uartReturn);

//    System_printf("sent command: %s\n", cmd);
//    System_printf("got return: %s\n", returnVal);

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
    //Q0 AND Q3 must be HIGH
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
        //read all chars in uart
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

    //set global uart handler
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
    //Q0 + Q3 = Status Pins of bluetooth module?
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOQ);

    //Q0
    GPIOPinTypeGPIOInput(GPIO_PORTQ_BASE, GPIO_PIN_0);
    //Q3
    GPIOPinTypeGPIOInput(GPIO_PORTQ_BASE, GPIO_PIN_3);

    // CTS/RTS -- for reading and writing uart?
    //P5 to read from uart?
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOP);
    GPIOPinTypeGPIOInput(GPIO_PORTP_BASE, GPIO_PIN_5);

    //D4 to activate writing to uart?
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOD);
    GPIOPinTypeGPIOOutput(GPIO_PORTD_BASE, GPIO_PIN_4);

    //set D4 0
    GPIOPadConfigSet(GPIO_PORTD_BASE, GPIO_PIN_4, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPU);
    GPIOPinWrite(GPIO_PORTD_BASE, GPIO_PIN_4, 0);

    // SW_BTN -- D2 - just for startup of bluetooth module
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOD);
    GPIOPinTypeGPIOOutput(GPIO_PORTD_BASE, GPIO_PIN_2);
    GPIOPadConfigSet(GPIO_PORTD_BASE, GPIO_PIN_2, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPU);

    //Set D2 0
    GPIOPinWrite(GPIO_PORTD_BASE, GPIO_PIN_2, 0);

    // RST_N
    //P4 - just for startup of bluetooth module?
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOP);
    GPIOPinTypeGPIOOutput(GPIO_PORTP_BASE, GPIO_PIN_4);
    GPIOPadConfigSet(GPIO_PORTP_BASE, GPIO_PIN_4, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPU);

    //Set P4 high
    GPIOPinWrite(GPIO_PORTP_BASE, GPIO_PIN_4, GPIO_PIN_4);

    // WAKE_UP
    //M7
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOM);
    GPIOPinTypeGPIOOutput(GPIO_PORTM_BASE, GPIO_PIN_7);
    GPIOPadConfigSet(GPIO_PORTM_BASE, GPIO_PIN_7, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPU);
    //set M7 high = WAKE_UP
    GPIOPinWrite(GPIO_PORTM_BASE, GPIO_PIN_7, GPIO_PIN_7);
}

//TODO: docu
void init_bt_module()
{
    // init sequence
     SysCtlDelay((120000000 / 1000) * 500);
     //P4 low
     GPIOPinWrite(GPIO_PORTP_BASE, GPIO_PIN_4, 0);
     SysCtlDelay((120000000 / 1000) * 100);
     //P4 high
     GPIOPinWrite(GPIO_PORTP_BASE, GPIO_PIN_4, GPIO_PIN_4);
     SysCtlDelay((120000000 / 1000) * 100);
     //D2 high
     GPIOPinWrite(GPIO_PORTD_BASE, GPIO_PIN_2, GPIO_PIN_2);
     SysCtlDelay((120000000 / 1000) * 500);

     // wait for the correct status of the RN4678
     //Q0 must be != 0 AND Q3 must be != 0
     while((GPIOPinRead(GPIO_PORTQ_BASE, GPIO_PIN_0) != 0x00) && (GPIOPinRead(GPIO_PORTQ_BASE, GPIO_PIN_3) != 0x00))
     {}

     //why?
     SysCtlDelay((120000000 / 1000) * 500);
}

int setup_UART()
{
    /*configure uart6 as interface to bluetooth module*/
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOP);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART6);

    //P0 and P1 for uart6
    GPIOPinConfigure(GPIO_PP0_U6RX);
    GPIOPinConfigure(GPIO_PP1_U6TX);
    GPIOPinTypeUART(GPIO_PORTP_BASE, GPIO_PIN_1 | GPIO_PIN_0);

    UART_init();

    //set necessary pins for initializing bluetooth module and writing/reading uart
    set_Pins();

    //initialize bluetooth module
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
