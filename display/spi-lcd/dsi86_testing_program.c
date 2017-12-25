//*****************************************************************************
//
// SN65DSI86 testing program
//
// This is part of revision 1.0 of the EK-TM4C123GXL Firmware Package.
//
//*****************************************************************************

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include "inc/hw_ints.h"
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "driverlib/debug.h"
#include "driverlib/fpu.h"
#include "driverlib/gpio.h"
#include "driverlib/interrupt.h"
#include "driverlib/pin_map.h"
#include "driverlib/rom.h"
#include "driverlib/timer.h"
#include "driverlib/sysctl.h"
#include "driverlib/uart.h"
#include "utils/uartstdio.h"
#include "utils/softi2c.h"

//*****************************************************************************
//
//! UART0, connected to the Virtual Serial Port and running at
//! 115,200, 8-N-1, is used to display messages from this application.
//
//*****************************************************************************
#define SLAVE_ADDR 0x2C

//*****************************************************************************
//
// The states in the interrupt handler state machine.
//
//*****************************************************************************
#define STATE_IDLE              0
#define STATE_WRITE_PRE_NEXT	1
#define STATE_WRITE_PRE_FINAL   2 
#define STATE_WRITE_NEXT        3
#define STATE_WRITE_FINAL       4
#define STATE_WAIT_ACK          5
#define STATE_SEND_ACK          6
#define STATE_OFFSET_ONE		7
#define STATE_OFFSET_FIRST		8
#define STATE_READ_ONE          9
#define STATE_READ_FIRST		10
#define STATE_READ_NEXT         11
#define STATE_READ_FINAL        12
#define STATE_READ_WAIT			13


//
//
//
#define EDP_CONFIGURATION_CAP		(0x000D)
# define ASSR_SUPPORT				(1<<0)
# define ENHANCE_FRAMING			(1<<1)
# define DPCD_DISPLAY_CONTORL_CAP   (1<<3)


#define EDP_CONFIGURATION_SET		(0x010A)
# define EN_ASSR					(1<<0)
# define EN_ENHANCE_FRAMING			(1<<1)
# define EN_SELF_TEST				(1<<7)

static tSoftI2C g_sI2C;

static uint8_t  g_ui8SlaveAddr  = SLAVE_ADDR;
static uint8_t *g_pui8Data = 0;
static uint32_t g_ui32Count = 0;
static uint32_t g_ui32Offset = 0;

static volatile uint32_t g_ui32State = STATE_IDLE;

static uint8_t g_ui8EDID[128];

//*****************************************************************************
//
// The error routine that is called if the driver library encounters an error.
//
//*****************************************************************************
#ifdef DEBUG
void
__error__(char *pcFilename, uint32_t ui32Line)
{
}
#endif

//*****************************************************************************
//
// The callback function for the SoftI2C module.
//
//*****************************************************************************
void
SoftI2CCallback(void)
{
    //
    // Clear the SoftI2C interrupt.
    //
    SoftI2CIntClear(&g_sI2C);

    //
    // Determine what to do based on the current state.
    //
    switch(g_ui32State)
    {
        //
        // The idle state.
        //
        case STATE_IDLE:
        {
            //
            // There is nothing to be done.
            //
            break;
        }

		case STATE_WRITE_PRE_NEXT:
		{

			SoftI2CDataPut(&g_sI2C, (g_ui32Offset & 0xFF));
			SoftI2CControl(&g_sI2C, SOFTI2C_CMD_BURST_SEND_CONT);
			g_ui32State = STATE_WRITE_NEXT;
			break;
		}

		case STATE_WRITE_PRE_FINAL:
		{
			SoftI2CDataPut(&g_sI2C, (g_ui32Offset & 0xFF));
			SoftI2CControl(&g_sI2C, SOFTI2C_CMD_BURST_SEND_CONT);
			g_ui32State = STATE_WRITE_FINAL;
			break;
		}

        //
        // The state for the middle of a burst write.
        //
        case STATE_WRITE_NEXT:
        {
            //
            // Write the next data byte.
            //
            SoftI2CDataPut(&g_sI2C, *g_pui8Data++);
            g_ui32Count--;

            //
            // Continue the burst write.
            //
            SoftI2CControl(&g_sI2C, SOFTI2C_CMD_BURST_SEND_CONT);

            //
            // If there is one byte left, set the next state to the final write
            // state.
            //
            if(g_ui32Count == 1)
            {
                g_ui32State = STATE_WRITE_FINAL;
            }

            //
            // This state is done.
            //
            break;
        }

        //
        // The state for the final write of a burst sequence.
        //
        case STATE_WRITE_FINAL:
        {
            //
            // Write the final data byte.
            //
            SoftI2CDataPut(&g_sI2C, *g_pui8Data++);
            g_ui32Count--;

            //
            // Finish the burst write.
            //
            SoftI2CControl(&g_sI2C, SOFTI2C_CMD_BURST_SEND_FINISH);

            //
            // The next state is to wait for the burst write to complete.
            //
            g_ui32State = STATE_SEND_ACK;

            //
            // This state is done.
            //
            break;
        }

        //
        // Wait for an ACK on the read after a write.
        //
        case STATE_WAIT_ACK:
        {
            //
            // See if there was an error on the previously issued read.
            //
            if(SoftI2CErr(&g_sI2C) == SOFTI2C_ERR_NONE)
            {
                //
                // Read the byte received.
                //
                SoftI2CDataGet(&g_sI2C);

                //
                // There was no error, so the state machine is now idle.
                //
                g_ui32State = STATE_IDLE;

                //
                // This state is done.
                //
                break;
            }

            //
            // Fall through to STATE_SEND_ACK.
            //
        }

        //
        // Send a read request, looking for the ACK to indicate that the write
        // is done.
        //
        case STATE_SEND_ACK:
        {
            //
            // Put the I2C master into receive mode.
            //
            SoftI2CSlaveAddrSet(&g_sI2C, g_ui8SlaveAddr, true);

            //
            // Perform a single byte read.
            //
            SoftI2CControl(&g_sI2C, SOFTI2C_CMD_SINGLE_RECEIVE);

            //
            // The next state is the wait for the ack.
            //
            g_ui32State = STATE_WAIT_ACK;

            //
            // This state is done.
            //
            break;
        }

		// The state for a single byte read.
		case STATE_OFFSET_ONE:
		{
			SoftI2CDataPut(&g_sI2C, (g_ui32Offset & 0xFF));
			SoftI2CControl(&g_sI2C, SOFTI2C_CMD_BURST_SEND_CONT);
			g_ui32State = STATE_READ_ONE;
			break;
		}

		case STATE_OFFSET_FIRST:
		{
			SoftI2CDataPut(&g_sI2C, (g_ui32Offset & 0xFF));
			SoftI2CControl(&g_sI2C, SOFTI2C_CMD_BURST_SEND_CONT);
			g_ui32State = STATE_READ_FIRST;
			break;
		}

        //
        // The state for a single byte read.
        //
        case STATE_READ_ONE:
        {
            //
            // Put the SoftI2C module into receive mode.
            //
            SoftI2CSlaveAddrSet(&g_sI2C, g_ui8SlaveAddr, true);

            //
            // Perform a single byte read.
            //
            SoftI2CControl(&g_sI2C, SOFTI2C_CMD_SINGLE_RECEIVE);

            //
            // The next state is the wait for final read state.
            //
            g_ui32State = STATE_READ_WAIT;

            //
            // This state is done.
            //
            break;
        }

        //
        // The state for the start of a burst read.
        //
        case STATE_READ_FIRST:
        {
            //
            // Put the SoftI2C module into receive mode.
            //
            SoftI2CSlaveAddrSet(&g_sI2C, g_ui8SlaveAddr, true);

            //
            // Start the burst receive.
            //
            SoftI2CControl(&g_sI2C, SOFTI2C_CMD_BURST_RECEIVE_START);

            //
            // The next state is the middle of the burst read.
            //
            g_ui32State = STATE_READ_NEXT;

            //
            // This state is done.
            //
            break;
        }

        //
        // The state for the middle of a burst read.
        //
        case STATE_READ_NEXT:
        {
            //
            // Read the received character.
            //
            *g_pui8Data++ = SoftI2CDataGet(&g_sI2C);
            g_ui32Count--;

            //
            // Continue the burst read.
            //
            SoftI2CControl(&g_sI2C, SOFTI2C_CMD_BURST_RECEIVE_CONT);

            //
            // If there are two characters left to be read, make the next
            // state be the end of burst read state.
            //
            if(g_ui32Count == 2)
            {
                g_ui32State = STATE_READ_FINAL;
            }

            //
            // This state is done.
            //
            break;
        }

        //
        // The state for the end of a burst read.
        //
        case STATE_READ_FINAL:
        {
            //
            // Read the received character.
            //
            *g_pui8Data++ = SoftI2CDataGet(&g_sI2C);
            g_ui32Count--;

            //
            // Finish the burst read.
            //
            SoftI2CControl(&g_sI2C, SOFTI2C_CMD_BURST_RECEIVE_FINISH);

            //
            // The next state is the wait for final read state.
            //
            g_ui32State = STATE_READ_WAIT;

            //
            // This state is done.
            //
            break;
        }

        //
        // This state is for the final read of a single or burst read.
        //
        case STATE_READ_WAIT:
        {
            //
            // Read the received character.
            //
            *g_pui8Data++ = SoftI2CDataGet(&g_sI2C);
            g_ui32Count--;

            //
            // The state machine is now idle.
            //
            g_ui32State = STATE_IDLE;

            //
            // This state is done.
            //
            break;
        }
    }
}

void
I2C_Write8(uint8_t *pui8Data, uint32_t ui32Offset, uint32_t ui32Count)
{
    //
    // Save the data buffer to be written.
    //
    g_pui8Data = pui8Data;
    g_ui32Count = ui32Count;
	g_ui32Offset = ui32Offset;
    //
    // Set the next state of the callback state machine based on the number of
    // bytes to write.
    //
    if(ui32Count != 1)
    {
        g_ui32State = STATE_WRITE_NEXT;
    }
    else
    {
        g_ui32State = STATE_WRITE_FINAL;
    }

    //
    // Set the slave address and setup for a transmit operation.
    //
    SoftI2CSlaveAddrSet(&g_sI2C, g_ui8SlaveAddr, false);

    //
    // Write the address to be written as the first data byte.
    //
    SoftI2CDataPut(&g_sI2C, (ui32Offset & 0xFF));

    //
    // Start the burst cycle, writing the address as the first byte.
    //
    SoftI2CControl(&g_sI2C, SOFTI2C_CMD_BURST_SEND_START);

    //
    // Wait until the SoftI2C callback state machine is idle.
    //
    while(g_ui32State != STATE_IDLE)
    {
    }
}


//*****************************************************************************
//
// Write to the Atmel device.
//
//*****************************************************************************
void
AtmelWrite(uint8_t *pui8Data, uint32_t ui32Offset, uint32_t ui32Count)
{
    //
    // Save the data buffer to be written.
    //
    g_pui8Data = pui8Data;
    g_ui32Count = ui32Count;
	g_ui32Offset = ui32Offset;
    //
    // Set the next state of the callback state machine based on the number of
    // bytes to write.
    //
    if(ui32Count != 1)
    {
        g_ui32State = STATE_WRITE_PRE_NEXT;
    }
    else
    {
        g_ui32State = STATE_WRITE_PRE_FINAL;
    }

    //
    // Set the slave address and setup for a transmit operation.
    //
    SoftI2CSlaveAddrSet(&g_sI2C, g_ui8SlaveAddr, false);

    //
    // Write the address to be written as the first data byte.
    //
    SoftI2CDataPut(&g_sI2C, (ui32Offset>>8));

    //
    // Start the burst cycle, writing the address as the first byte.
    //
    SoftI2CControl(&g_sI2C, SOFTI2C_CMD_BURST_SEND_START);

    //
    // Wait until the SoftI2C callback state machine is idle.
    //
    while(g_ui32State != STATE_IDLE)
    {
    }
}

void 
I2C_Read8(uint8_t *pui8Data, uint32_t ui32Offset, uint32_t ui32Count)
{
	//
    // Save the data buffer to be read.
    //
    g_pui8Data = pui8Data;
    g_ui32Count = ui32Count;
	g_ui32Offset = ui32Offset;

    //
    // Set the next state of the callback state machine based on the number of
    // bytes to read.
    //
    if(ui32Count == 1)
    {
        g_ui32State = STATE_READ_ONE;
    }
    else
    {
        g_ui32State = STATE_READ_FIRST;
    }

    //
    // Start with a dummy write to get the address set in the EEPROM.
    //
    SoftI2CSlaveAddrSet(&g_sI2C, g_ui8SlaveAddr, false);

    //
    // Write the address to be written as the first data byte.
    //
    SoftI2CDataPut(&g_sI2C, (ui32Offset & 0xFF));

    //
    // Perform a single send, writing the address as the only byte.
    //
    SoftI2CControl(&g_sI2C, SOFTI2C_CMD_SINGLE_SEND);
	//SoftI2CControl(&g_sI2C, SOFTI2C_CMD_BURST_SEND_START);

    //
    // Wait until the SoftI2C callback state machine is idle.
    //
    while(g_ui32State != STATE_IDLE)
    {
	} 
}


//*****************************************************************************
//
// Read from the Atmel device.
//
//*****************************************************************************
void
AtmelRead(uint8_t *pui8Data, uint32_t ui32Offset, uint32_t ui32Count)
{
    //
    // Save the data buffer to be read.
    //
    g_pui8Data = pui8Data;
    g_ui32Count = ui32Count;
	g_ui32Offset = ui32Offset;

    //
    // Set the next state of the callback state machine based on the number of
    // bytes to read.
    //
    if(ui32Count == 1)
    {
        g_ui32State = STATE_OFFSET_ONE;
    }
    else
    {
        g_ui32State = STATE_OFFSET_FIRST;
    }

    //
    // Start with a dummy write to get the address set in the EEPROM.
    //
    SoftI2CSlaveAddrSet(&g_sI2C, g_ui8SlaveAddr, false);

    //
    // Write the address to be written as the first data byte.
    //
    SoftI2CDataPut(&g_sI2C, (ui32Offset>>8));

    //
    // Perform a single send, writing the address as the only byte.
    //
//    SoftI2CControl(&g_sI2C, SOFTI2C_CMD_SINGLE_SEND);
	SoftI2CControl(&g_sI2C, SOFTI2C_CMD_BURST_SEND_START);

    //
    // Wait until the SoftI2C callback state machine is idle.
    //
    while(g_ui32State != STATE_IDLE)
    {
    }
}

//*****************************************************************************
//
// This is the interrupt handler for the Timer0A interrupt.
//
//*****************************************************************************
void
Timer0AIntHandler(void)
{
    //
    // Clear the timer interrupt.
    // TODO: change this to whichever timer you are using.
    //
    TimerIntClear(TIMER0_BASE, TIMER_TIMA_TIMEOUT);

    //
    // Call the SoftI2C tick function.
    //
    SoftI2CTimerTick(&g_sI2C);
}


//*****************************************************************************
//
// Configure the UART and its pins.  This must be called before UARTprintf().
//
//*****************************************************************************
void
ConfigureUART(void)
{
    //
    // Enable the GPIO Peripheral used by the UART.
    //
    ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);

    //
    // Enable UART0
    //
    ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);

    //
    // Configure GPIO Pins for UART mode.
    //
    ROM_GPIOPinConfigure(GPIO_PA0_U0RX);
    ROM_GPIOPinConfigure(GPIO_PA1_U0TX);
    ROM_GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);

    //
    // Use the internal 16MHz oscillator as the UART clock source.
    //
    UARTClockSourceSet(UART0_BASE, UART_CLOCK_PIOSC);

    //
    // Initialize the UART for console I/O.
    //
    UARTStdioConfig(0, 115200, 16000000);
}

static void READ_DPCD(uint8_t* buff, uint32_t v_addr, uint32_t cnt)
{
	uint8_t tmp[4];
	// AUX 20bit address
	tmp[0] = (v_addr>>16) & 0x0F;
	tmp[1] = (v_addr>>8) & 0xFF;
	tmp[2] = (v_addr & 0xFF);
	// length
	tmp[3] = (cnt & 0x1F);
	I2C_Write8(tmp, 0x74, 4); // write DPCD register address

	// AUX_NATIVE_CMD (0x9)
	tmp[0] = (9<<4) | // AUX_NATIVE_READ
			 (1<<0);  // EN_SEND_AUX
    I2C_Write8(tmp, 0x78, 1);
	ROM_SysCtlDelay(ROM_SysCtlClockGet()/10/3); // waiting for response
	I2C_Read8(buff, 0x79, (cnt&0x1F));
}

static void WRITE_DPCD(uint8_t* buff, uint32_t v_addr, uint32_t cnt)
{
	uint8_t tmp[4];
	// AUX 20bit address
	tmp[0] = (v_addr>>16) & 0x0F;
	tmp[1] = (v_addr>>8) & 0xFF;
	tmp[2] = (v_addr & 0xFF);
	// length
	tmp[3] = (cnt & 0x1F);
	I2C_Write8(tmp, 0x74, 4); // write DPCD register address

	I2C_Write8(buff, 0x64, cnt);

	// AUX_NATIVE_CMD (0x8)
	tmp[0] = (8<<4) | // AUX_NATIVE_WRITE
			 (1<<0);  // EN_SEND_AUX
    I2C_Write8(tmp, 0x78, 1);
	ROM_SysCtlDelay(ROM_SysCtlClockGet()/10/3);
}


static void opAUX(uint32_t v_addr, uint8_t* buff, uint8_t cnt, uint8_t cmd)
{
	uint8_t tmp[4];
	// AUX 20bit address
	tmp[0] = (v_addr>>16) & 0x0F;
	tmp[1] = (v_addr>>8) & 0xFF;
	tmp[2] = (v_addr & 0xFF);
	// length
	tmp[3] = (cnt & 0x1F);
	I2C_Write8(tmp, 0x74, 4); // write address
	
	if ((cnt != 0) && cmd != 5)
		I2C_Write8(buff, 0x64, cnt);
	tmp[0] = (cmd << 4) |
			 ( 1 << 0); // SEND flag
	I2C_Write8(tmp, 0x78, 1);
	ROM_SysCtlDelay(ROM_SysCtlClockGet()/10/3);
}

static void readEDID(void)
{
	uint8_t buf[2];
	uint8_t count;
	
	memset(buf, 0, sizeof(buf));
	opAUX(0x50, buf, 0, 4); // Write with MOT, I2C over AUX, SYNC
	opAUX(0x50, buf, 1, 4); // Write with MOT, Offset 0
	opAUX(0x50, buf, 0, 5); // Read with MOT, READ START
	for (count=0; count < 128; count+= 16) {
		opAUX(0x50, buf, 0x10, 5); // Read with MOT, read 16 byte
		I2C_Read8((g_ui8EDID+count), 0x79, 0x10);
	}
	opAUX(0x50, buf, 0, 1); // read without MOT, read stop
}

static void dumpEDID(void)
{
	uint8_t count;
	uint8_t vo;
	
	count = vo = 0;
	UARTprintf("--------------------------------------------------------\n");
	UARTprintf("Dump Panel EDID \n");
	UARTprintf("--------------------------------------------------------\n");
	UARTprintf("    00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F\n");
	UARTprintf("--------------------------------------------------------");
	for(count = 0; count < sizeof(g_ui8EDID); count++) {
		if ((count % 16) == 0) {
			UARTprintf("\n%02X|", vo);
			vo += 0x10;
		}
		UARTprintf(" %02x", g_ui8EDID[count]);
	}
	
	UARTprintf("\n");
}

static void dumpPanelTiming(void)
{
	uint32_t t;
	uint16_t ha;
	uint16_t hb;
	uint16_t ho;
	uint16_t va;
	uint16_t vb;
	uint16_t vo;
	uint16_t hpw;
	uint16_t vpw;
	uint16_t hsz;
	uint16_t vsz;
	uint16_t hbp;
	uint16_t vbp;
	
	UARTprintf("--------------------------------------------------------\n");
	UARTprintf("Extract Panel timing from EDID DTD1\n");
	UARTprintf("--------------------------------------------------------\n");
	ROM_SysCtlDelay(ROM_SysCtlClockGet()/10/3); // wating for UARTprintf

	#define DTD1_OFFSET  (0x36)
	t = (g_ui8EDID[DTD1_OFFSET+1]<<8) + g_ui8EDID[DTD1_OFFSET+0];
	ha = ((g_ui8EDID[DTD1_OFFSET+4] & 0xF0)<<4) + g_ui8EDID[DTD1_OFFSET+2];
	hb = ((g_ui8EDID[DTD1_OFFSET+4] & 0x0F)<<8) + g_ui8EDID[DTD1_OFFSET+3];
	va = ((g_ui8EDID[DTD1_OFFSET+7] & 0xF0)<<4) + g_ui8EDID[DTD1_OFFSET+5];
	vb = ((g_ui8EDID[DTD1_OFFSET+7] & 0x0F)<<8) + g_ui8EDID[DTD1_OFFSET+6];
	ho = ((g_ui8EDID[DTD1_OFFSET+11] & 0xC0)<<2) + g_ui8EDID[DTD1_OFFSET+8];
	vo = ((g_ui8EDID[DTD1_OFFSET+11] & 0x0C)<<2) + (g_ui8EDID[DTD1_OFFSET+10] >> 4);	
	hpw = ((g_ui8EDID[DTD1_OFFSET+11] & 0x30)<<4) + g_ui8EDID[DTD1_OFFSET+9];
	vpw = ((g_ui8EDID[DTD1_OFFSET+11] & 0x03)<<4) + (g_ui8EDID[DTD1_OFFSET+10]&0x0F);
	hsz = ((g_ui8EDID[DTD1_OFFSET+14] & 0xF0)<<4) + g_ui8EDID[DTD1_OFFSET+12];
	vsz = ((g_ui8EDID[DTD1_OFFSET+14] & 0x0F)<<8) + g_ui8EDID[DTD1_OFFSET+13];
	hbp = g_ui8EDID[DTD1_OFFSET+15];
	vbp = g_ui8EDID[DTD1_OFFSET+16];
	
	UARTprintf("Pixel Clock %dMHz(%dKHz)\n", t/100, t*10);
	UARTprintf("Active zone(Hori.xVert.): %dx%d\n", ha, va);
	ROM_SysCtlDelay(ROM_SysCtlClockGet()/10/3); // wating for UARTprintf	
	UARTprintf("Blinking zone(Hori. & Vert.): %d & %d\n", hb, vb);
	ROM_SysCtlDelay(ROM_SysCtlClockGet()/10/3); // wating for UARTprintf	
	UARTprintf("Horizontal sync Offset & Pulse width: %d & %d\n", ho, hpw);
	ROM_SysCtlDelay(ROM_SysCtlClockGet()/10/3); // wating for UARTprintf	
	UARTprintf("Vertical sync Offset & Pulse width: %d & %d\n", vo, vpw);	
	ROM_SysCtlDelay(ROM_SysCtlClockGet()/10/3); // wating for UARTprintf	
	UARTprintf("Border (Hori. & Vert.): %d & %d\n", hbp, vbp);
	UARTprintf("Display Size(Hori.xVert.): %dx%d mm2\n", hsz, vsz);
	ROM_SysCtlDelay(ROM_SysCtlClockGet()/10/3); // wating for UARTprintf
	if ((g_ui8EDID[DTD1_OFFSET+17] & 0x18) !=  0x18) {
		UARTprintf("can't support the panel, because we need a digital separate type\n");
		return;
	}
	UARTprintf("Horizontal sync. Polarity: ");		
	if (g_ui8EDID[DTD1_OFFSET+17] & (1<<1)) {
		UARTprintf("Positive\n");
	} else {
		UARTprintf("Negative\n");
	}

	UARTprintf("Vertical sync. Polarity: ");	
	if (g_ui8EDID[DTD1_OFFSET+17] & (1<<2)) {
		UARTprintf("Positive\n");
	} else {
		UARTprintf("Negative\n");
	}
}

static void dumpDPCD(void)
{
	uint8_t count;
	uint8_t i;
	uint8_t buf[16];
	uint16_t lists[] = {
		0x0000, 0x0100, 0x0200, 0x0210 };

	UARTprintf("--------------------------------------------------------\n");
	UARTprintf("dump panel DPCD value\n");
	UARTprintf("--------------------------------------------------------\n");

	for (i = 0; i < (sizeof(lists)/sizeof(uint16_t)); i++) {
		READ_DPCD(buf, lists[i], 16);
		UARTprintf("DPCD %04xh:", lists[i]);
		for(count = 0; count < 16; count++) {
			UARTprintf(" %02X", buf[count]);
		}
		UARTprintf("\n");
		ROM_SysCtlDelay(ROM_SysCtlClockGet()/10/3); // wating for UARTprintf
	}
}

static void showDPCDInfo(void)
{
	uint8_t buf[16];
	READ_DPCD(buf, 0, 16);

	UARTprintf("DPCD: REV:%d.%d, MAX_LINK_RATE:", (buf[0] >> 4), (buf[0]&0xF));
	if (buf[1] == 0x06) {
		UARTprintf("1.62Gbps");
	} else if (buf[1] == 0x0A) {
		UARTprintf("2.7Gbps");
	}
	UARTprintf(" MAX_LINK_LANE:%d\n", buf[2]);
	if (buf[0x0D] & ASSR_SUPPORT) {
		UARTprintf(" support ASSR");
	} else {
		UARTprintf(" not support ASSR");
	}
	if (buf[0x0D] & ENHANCE_FRAMING) {
		UARTprintf(" support Enhance framing");
	} else {
		UARTprintf(" not support Enhance framing");
	}
	UARTprintf("\n");
}
//*****************************************************************************
//
//
//*****************************************************************************
int
main(void)
{
    //volatile uint32_t ui32Loop;
	uint32_t count;
	uint8_t BUF[32];
	uint32_t tmp;
	uint32_t vo = 0;

	tmp = 0;
	BUF[0] = BUF[1] = BUF[2] = BUF[3] = tmp;
	tmp = BUF[0];

    //
    // Enable lazy stacking for interrupt handlers.  This allows floating-point
    // instructions to be used within interrupt handlers, but at the expense of
    // extra stack usage.
    //
    ROM_FPULazyStackingEnable();

    //
    // Set the clocking to run directly from the crystal.
    //
    ROM_SysCtlClockSet(SYSCTL_SYSDIV_4 | SYSCTL_USE_PLL | SYSCTL_XTAL_16MHZ |
                       SYSCTL_OSC_MAIN);

    //
    // Enable the GPIO port that is used for the on-board LED.
    //
    ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);

    //
    // Enable the GPIO pins for the LED (PF2 & PF3).
    //
    ROM_GPIOPinTypeGPIOOutput(GPIO_PORTF_BASE, GPIO_PIN_2);

	//
	// Initialize SoftI2C 
	//
	ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER0);
	ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
	GPIOPinTypeI2C(GPIO_PORTA_BASE, GPIO_PIN_6 | GPIO_PIN_7);
	memset(&g_sI2C, 0, sizeof(g_sI2C));
	SoftI2CCallbackSet(&g_sI2C, SoftI2CCallback);
	SoftI2CSCLGPIOSet(&g_sI2C, GPIO_PORTA_BASE, GPIO_PIN_6);
	SoftI2CSDAGPIOSet(&g_sI2C, GPIO_PORTA_BASE, GPIO_PIN_7);
	SoftI2CInit(&g_sI2C);

	SoftI2CIntEnable(&g_sI2C);

    TimerConfigure(TIMER0_BASE, TIMER_CFG_PERIODIC);
    TimerLoadSet(TIMER0_BASE, TIMER_A, SysCtlClockGet() / 40000);
    TimerIntEnable(TIMER0_BASE, TIMER_TIMA_TIMEOUT);
    TimerEnable(TIMER0_BASE, TIMER_A);

    IntEnable(INT_TIMER0A);


    //
    // Initialize the UART.
    //
    ConfigureUART();
	
	UARTprintf("for ColorBar Test \r\n");
	BUF[0] = 1;
	I2C_Write8(BUF, 0x09, 1); // software reset
	ROM_SysCtlDelay(ROM_SysCtlClockGet() / 10 / 3);

	BUF[0] = 0 ;
	I2C_Write8(BUF, 0x5A, 1); // disable VSTREAM
	ROM_SysCtlDelay(ROM_SysCtlClockGet() / 10 / 3);

	BUF[0] = 0x0;
	I2C_Write8(BUF, 0x0D, 1); // disable DP_PLL_EN
	ROM_SysCtlDelay(ROM_SysCtlClockGet() / 10 / 3);
	
	BUF[0] = 1;
	I2C_Write8(BUF, 0x09, 1); // software reset
	ROM_SysCtlDelay(ROM_SysCtlClockGet() / 10 / 3);
	
	BUF[0] = 0x2;
	I2C_Write8(BUF, 0x0A, 1); // Clock Ref = 19.2MHz

	readEDID();
	dumpEDID();
	dumpPanelTiming();
	dumpDPCD();
	showDPCDInfo();
	BUF[0] = (1 << 5) | // SINGL CHANNEL DSI (A)
			(0 << 3) | // Four Lane
			(0);	   // SOT_ERR_TOL_DIS
	I2C_Write8(BUF, 0x10, 1);

	BUF[0] = 0; // disable ASSR
	I2C_Write8(BUF, 0x5A, 1);

	BUF[0] = 1<<1; // 24BPP
	I2C_Write8(BUF, 0x5B, 1);

	BUF[0] = 0; // HPD Enable
	I2C_Write8(BUF, 0x5C, 1);

	BUF[0] = (0 << 6)  | // DP pre emphasis lv 0
			(2 << 4)  | // DP 2 lane
			(2 << 1)  | // Downspread 3750ppm
			(0 << 0);   // disable SSC
	I2C_Write8(BUF, 0x93, 1);

	BUF[0] = (4 << 5) | // 2.70Gbps
			(0 << 2) | // 61ps
			(0 << 0);  // Voltage
	I2C_Write8(BUF, 0x94, 1);
// Panel timing 
#define  HA			(1920)
#define  HSPW		(39)
#define  HFP		(59)
#define  HBP		(62)
#define  HSP		(0) // Hsyn Polarity:0 POS
#define  VA         (1200)
#define  VSPW		(3)
#define  VFP		(6)
#define  VBP		(26)
#define  VSP        (1) // Vsyn Polarity: NEG

	BUF[0] = (HA & 0xFF);
	BUF[1] = ((HA>>8) & 0xFF);
	I2C_Write8(BUF, 0x20, 2); // ActiveLine
	ROM_SysCtlDelay(ROM_SysCtlClockGet() / 10 / 3);

	BUF[0] = (VA & 0xFF);
	BUF[1] = ((VA>>8) & 0xFF);
	I2C_Write8(BUF, 0x24, 2);  // Vertical line
	ROM_SysCtlDelay(ROM_SysCtlClockGet() / 10 / 3);

	BUF[0] = (HSPW & 0xFF);
	BUF[1] = ((HSPW>>8) & 0x7F) |
			(HSP << 7);
	I2C_Write8(BUF, 0x2C, 2); // HSYNC Pulse width
	ROM_SysCtlDelay(ROM_SysCtlClockGet() / 10 / 3);

	BUF[0] = (VSPW & 0xFF);
	BUF[1] = ((VSPW>>8) & 0xFF) |
			(VSP << 7); // neg
	I2C_Write8(BUF, 0x30, 2); // VSYNC Pulse width
	ROM_SysCtlDelay(ROM_SysCtlClockGet() / 10 / 3);

	BUF[0] = HBP;
	I2C_Write8(BUF, 0x34, 1); // H_BACK_PORCH
	ROM_SysCtlDelay(ROM_SysCtlClockGet() / 10 / 3);

	BUF[0] = VBP;
	I2C_Write8(BUF, 0x36, 1); // V_BACK_PORCH
	ROM_SysCtlDelay(ROM_SysCtlClockGet() / 10 / 3);

	BUF[0] = HFP;
	I2C_Write8(BUF, 0x38, 1); // H_FRONT_PORCH
	ROM_SysCtlDelay(ROM_SysCtlClockGet() / 10 / 3);
	
	BUF[0] = VFP;
	I2C_Write8(BUF, 0x3A, 1); // V_FRONT_PORCH
	ROM_SysCtlDelay(ROM_SysCtlClockGet() / 10 / 3);
	
	BUF[0] = 0;
	I2C_Write8(BUF, 0x5B, 1); //24bpp
	ROM_SysCtlDelay(ROM_SysCtlClockGet() / 10 / 3);

	BUF[0] = 0x1;
	I2C_Write8(BUF, 0x0D, 1); // Enable DP_PLL_EN
	ROM_SysCtlDelay(ROM_SysCtlClockGet() / 10 / 3);

	I2C_Read8(BUF, 0x0A, 1);
	if (BUF[0] & 0x80) UARTprintf("DSI86 PLL has Locked now\n");

	BUF[0] = 0xA;	// ML_TX_MODE,Semi Auto Link Training
	I2C_Write8(BUF, 0x96, 1);
	ROM_SysCtlDelay(ROM_SysCtlClockGet() / 10 / 3);

	I2C_Read8(BUF, 0x96, 1);
	UARTprintf("SemiTraning result:%02X\n", BUF[0]);

#if 0
	BUF[0] = EN_SELF_TEST;
	WRITE_DPCD(BUF, EDP_CONFIGURATION_SET, 1);
#endif

	BUF[0] = 0x18; // Color Bar enable
	I2C_Write8(BUF, 0x3C, 1);
	SysCtlDelay(SysCtlClockGet() / 10 / 3);

	I2C_Read8(BUF, 0x20, 4);

	BUF[0] = (1<<3) ; // VStream
			//(0<<2) | // Enhanced Framing
	I2C_Write8(BUF, 0x5A, 1);
	SysCtlDelay(SysCtlClockGet() / 10 / 3);
	
	UARTprintf("--------------------------------------------------------\n");
	UARTprintf("    00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F\n");
	UARTprintf("--------------------------------------------------------");

#if 1
	for (tmp = 0; tmp < 8; tmp++) {
		I2C_Read8(BUF, tmp*sizeof(BUF), sizeof(BUF));
		for(count = 0; count < sizeof(BUF); count++) {
			if ((count % 16) == 0) {
				UARTprintf("\n%02X|", vo);
				vo += 0x10;
			}
			UARTprintf(" %02x", BUF[count]);
		}
	}
#endif
	UARTprintf("\n");
	dumpDPCD();
    while(1)
    {
		
        //
        // Turn on the BLUE LED.
        //
        GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_2, GPIO_PIN_2);

        //
        // Delay for a bit.
        //
		 SysCtlDelay(SysCtlClockGet() / 10 / 3);
	
        //
        // Turn off the BLUE LED.
        //
        GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_2, 0);

        //
        // Delay for a bit.
        //
        SysCtlDelay(SysCtlClockGet() / 10 / 3);
	}
}