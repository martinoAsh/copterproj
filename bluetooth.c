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


//uart global handler for reading/writing to uart
UART_Handle uart;

//global variable that indicates if the copter is ready for controls
uint8_t bluetooth_ready = 0;

//used to send data via uart to the bluetooth module
void send_data(char *data, size_t size)
{
    //Set D4 = Set CTS high
    GPIOPinWrite(GPIO_PORTD_BASE, GPIO_PIN_4, GPIO_PIN_4);
    //Read P5 = make sure there is no RTS
    while(GPIOPinRead(GPIO_PORTP_BASE, GPIO_PIN_5) != 0x00);

    if(UART_write(uart, data, size) == UART_ERROR)
    {
        System_printf("Error on writing uart!\n");
        System_flush();
        return;
    }


    while(UARTBusy(UART6_BASE));

    //Set CTS low
    GPIOPinWrite(GPIO_PORTD_BASE, GPIO_PIN_4, 0);
}


//global function that can be used to send controls to the copter
//must only be used if the global "bluetooth_ready" is != 0!
//also values for roll, pitch and throttle must only be 1000-2000
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

//Used to send commands to the bluetooth module using UART
//returns 1 if command was sent successfully, NULL if an ERROR occured
int send_command(char *cmd, uint8_t cmdSize, uint8_t returnSize, char *returnVal)
{
    char uartReturn[16] = { '\0' };

    //Set D4 = Set CTS high
    GPIOPinWrite(GPIO_PORTD_BASE, GPIO_PIN_4, GPIO_PIN_4);

    //Read P5 = make sure RTS is low
    while(GPIOPinRead(GPIO_PORTP_BASE, GPIO_PIN_5) != 0x00)
    {}

    //write to uart
    if(UART_write(uart, cmd, cmdSize) == UART_ERROR)
    {
        return NULL;
    }


    //wait until finished
    while(UARTBusy(UART6_BASE))
    {}

    //set CTS low again
    GPIOPinWrite(GPIO_PORTD_BASE, GPIO_PIN_4, 0);

    //read response
    if(UART_read(uart, &uartReturn, returnSize) == UART_ERROR)
    {
        return NULL;
    }
    strcpy(returnVal, uartReturn);

    Task_sleep(5);
    return 1;
}

//Tries to establish a connection to the copter, aborts if connection fails
//returns 1 on successful connection, NULL if an error occured
int connect_to_copter()
{
    char returnVal[30];

    //first enter command mode of bluetooth module
    char cmdMode[3] = "$$$";
    if(send_command(cmdMode, sizeof(cmdMode), 4, returnVal) == NULL)
    {
        return NULL;
    }

    if(strstr(returnVal, "CMD") == NULL)
    {
        System_printf("Failed to enter command mode\n");
        System_flush();
        return NULL;
    }

    // connect to copter
    char connectCmd[15] = "C,0006668CB2AC\r"; // connect to copter with its MAC-address
    if(send_command(connectCmd, sizeof(connectCmd), 16, returnVal) == NULL)
    {
        return NULL;
    }

    //if connection was not successful returnVal[10] would be 'E' (error)
    if(returnVal[10] != 'C')
    {
        System_printf("Failed to connect to copter!\n");
        System_flush();
        return NULL;
    }
    System_printf("Connected to copter\n");
    System_flush();

    //Check status pins of bluetooth module: wait for correct status
    while((GPIOPinRead(GPIO_PORTQ_BASE, GPIO_PIN_0) != 0x00) || (GPIOPinRead(GPIO_PORTQ_BASE, GPIO_PIN_3) == 0x00))
    {}

    // leave command mode
    char leaveCmdMode[5] = "---\r\n";
    if(send_command(leaveCmdMode, sizeof(leaveCmdMode), 1, returnVal) == NULL)
    {
        return NULL;
    }

    // flush UART In-Buffer
    while(UARTCharsAvail(UART6_BASE))
    {
        //read all chars in uart
        UART_read(uart, &returnVal, 1);
    }

    return 1;
}

//This task establishes a connection to the copter and creates a global UART handler for sending commands to the copter
void UART_Task(UArg arg0, UArg arg1)
{
    UART_Params uartParams;

    //Create a UART with data processing off
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

    //try to connect to the copter
    if(connect_to_copter() == NULL)
    {
        System_abort("Connection to copter failed\n");
    }

    Task_sleep(100);

    System_printf("Bluetooth is ready!\n");
    System_flush();
    bluetooth_ready = 1;
}

//runs the init sequence for the bluetooth module
//all necessary pins must be activated before calling
void init_bt_module()
{
    //set M7 = WAKE_UP high -> wake up from sleep mode
    GPIOPinWrite(GPIO_PORTM_BASE, GPIO_PIN_7, GPIO_PIN_7);
    SysCtlDelay((120000000 / 1000) * 500);
    //P4 = RST on module low
    GPIOPinWrite(GPIO_PORTP_BASE, GPIO_PIN_4, 0);
    SysCtlDelay((120000000 / 1000) * 100);
    //P4 = RST on module high -> module reset (pulse of at least 63ns)
    GPIOPinWrite(GPIO_PORTP_BASE, GPIO_PIN_4, GPIO_PIN_4);
    SysCtlDelay((120000000 / 1000) * 100);
    //D2 = SW_BTN high -> power ON
    GPIOPinWrite(GPIO_PORTD_BASE, GPIO_PIN_2, GPIO_PIN_2);
    SysCtlDelay((120000000 / 1000) * 500);

    //check status pins of the bluetooth module
    //Q0 must be != 0 AND Q3 must be != 0
    while((GPIOPinRead(GPIO_PORTQ_BASE, GPIO_PIN_0) != 0x00) && (GPIOPinRead(GPIO_PORTQ_BASE, GPIO_PIN_3) != 0x00))
    {}
}

//sets up all necessary pins for using UART6 and for using the bluetooth module on the boosterpack2 slot
//also creates task to connect and controll the copter
int setup_UART()
{
    //configure uart6
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOP);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART6);

    //P0 and P1 for uart6
    GPIOPinConfigure(GPIO_PP0_U6RX); //P0 connects to data output (TX) of bluetooth module
    GPIOPinConfigure(GPIO_PP1_U6TX); //P1 connects to data input (RX) of bluetooth module
    GPIOPinTypeUART(GPIO_PORTP_BASE, GPIO_PIN_1 | GPIO_PIN_0);

    UART_init();

    //see jumper4 -> uart -> D4 = INT on boosterpack -> CTS on bluetooth module (input) = Clear to send
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOD);
    GPIOPinTypeGPIOOutput(GPIO_PORTD_BASE, GPIO_PIN_4);
    //set D4 low
    GPIOPadConfigSet(GPIO_PORTD_BASE, GPIO_PIN_4, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPU);
    GPIOPinWrite(GPIO_PORTD_BASE, GPIO_PIN_4, 0);

    //P5 = CS on boosterpack -> RTS on blueetooth module (output) = Request to send
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOP);
    GPIOPinTypeGPIOInput(GPIO_PORTP_BASE, GPIO_PIN_5);

    //Q0 = SCK on boosterpack -> STATUS2 on bluetooth module
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOQ);
    GPIOPinTypeGPIOInput(GPIO_PORTQ_BASE, GPIO_PIN_0);
    //Q3 = SDI on boosterpack -> STATUS1 on bluetooth module
    GPIOPinTypeGPIOInput(GPIO_PORTQ_BASE, GPIO_PIN_3);

    //D2 = AN on boosterpack -> SW_BTN on bluetooth module = power on/off
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOD);
    GPIOPinTypeGPIOOutput(GPIO_PORTD_BASE, GPIO_PIN_2);
    GPIOPadConfigSet(GPIO_PORTD_BASE, GPIO_PIN_2, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPU);
    //Set D2 low
    GPIOPinWrite(GPIO_PORTD_BASE, GPIO_PIN_2, 0);

    //P4 = RST on boosterpack -> RST on bluetooth module = for startup of bluetooth module
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOP);
    GPIOPinTypeGPIOOutput(GPIO_PORTP_BASE, GPIO_PIN_4);
    GPIOPadConfigSet(GPIO_PORTP_BASE, GPIO_PIN_4, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPU);
    //Set P4 high
    GPIOPinWrite(GPIO_PORTP_BASE, GPIO_PIN_4, GPIO_PIN_4);

    //M7 = PWN on boosterpack -> WAKE_UP on bluetooth module = set to high for wake up
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOM);
    GPIOPinTypeGPIOOutput(GPIO_PORTM_BASE, GPIO_PIN_7);
    GPIOPadConfigSet(GPIO_PORTM_BASE, GPIO_PIN_7, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPU);

    //initialize bluetooth module
    init_bt_module();
    System_printf("Bluetooth module initialized\n");
    System_flush();

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
