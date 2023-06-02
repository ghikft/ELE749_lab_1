/*
 * main.c
 *
 *  Created on: 2023-05-29
 *      Author: Robin
 */

/****** flashing_leds.c *****************************************************
 * Description
 * Allume et eteint 2 DEL selon un intervalle determine par les switches
 ******************************************************************************
 * Author : Simon Pichette
 * Creation date : 2014-05-01
 * Last Modified : 2014-05-01f
 ******************************************************************************
 * Notes
 * Base sur le listing 9.5 de [Pong Chu SOPC]

 *****************************************************************************/
#include "io.h"
#include "alt_types.h"
#include "sys/alt_irq.h"
#include "system.h"
# include "sys/alt_stdio.h"

#define PIO_DATA_REG_OFT 0 // offset of data register
#define pio_write(base, data) IOWR(base, PIO_DATA_REG_OFT, data);

#define LOAD_KEY_MSK 0x02
#define PAUSE_KEY_MSK 0x01

#define HEX_P 0x0C
#define HEX_A 0x08
#define HEX_U 0x41
#define HEX_S 0x12
#define HEX_E 0x06
#define HEX_BLANK 0xFF

#define TIMER_CONVERSION 50000

//JTAGs related defines
#define JUART_DATA_REG_OFT		0		 	 // offset of data register
#define JUART_CTRL_REG_OFT		1		 	 // offset of control register
#define JUART_WSPACE_MASK	 	0xffff0000
#define JUART_WSPACE_BIT_OFT 	16		 	 // offset bit of field wspace
#define JUART_CHAR_MASK	 	 	0x000000ff

/* check remaining space in write FIFO */
#define juart_read_wspace(base) \
		((IORD(base, JUART_CTRL_REG_OFT) & JUART_WSPACE_MASK) >> JUART_WSPACE_BIT_OFT)
/* write an 8 bit char */
#define juart_write_char(base, data) \
		IOWR(base, JUART_DATA_REG_OFT, data & JUART_CHAR_MASK)

#define TIMER_STAT_REG_OFT		0 	// status register
#define TIMER_CTRL_REG_OFT		1 	// control register
#define TIMER_PERIODL_REG_OFT 	2 	// period register, bits 15:0
#define TIMER_PERIODH_REG_OFT 	3 	// period register, bits 31:16

alt_u8 sseg_conv_hex(int hex)
{
	/**************************************************************************
	 * convert a hex into the correct hex to display on a 7-seg display
	 **************************************************************************
	 * Parameters
	 * hex : number to be converted
	 *
	 * Return
	 * character to display
	 *
	 * Side effects
	 * period : none
	 *
	 *************************************************************************/

	//Hex templates for a active low 7 segments display (0-9, a-f). Ignore msb
	static const alt_u8 SSEG_HEX_TABLE[16] = {
			0x40, 0x79, 0x24, 0x30, 0x19, 0x92, 0x02, 0x78, 0x00, 0x10, // 0-9
			0x88, 0x03, 0x46, 0x21, 0x06, 0x0E};		 	 	 	 	 // a-f
	alt_u8 pattern;
	if (hex < 16) {
		pattern = SSEG_HEX_TABLE[hex];
	} else {
		pattern = 0xff; // all off
	}
	return (pattern);
}

void sseg_disp_6_digit(alt_u32 baseLow, alt_u32 baseHigh, alt_u8 *digit)
{
	/**************************************************************************
	 * Driver to display 6 characters on the the built in 7 segments displays
	 **************************************************************************
	 * Parameters
	 * baseLow : base adress the 3 right most 7-seg displays
	 * baseHigh : base adress the 3 left most 7-seg displays
	 * *digit : Pointer to the array to be displayed
	 *
	 * Return
	 * none
	 *
	 * Side effects
	 * period : none
	 *
	 *************************************************************************/

	/* ssegData is the address of an array of six 7 segment displays */
	alt_u32 ssegData=0;
	int i;
	// Assembly of 3 data with an OR and bit shifting to form a 24 bit
	for (i = 0; i < 3; i++) {
		ssegData = (ssegData << 8) | *digit;
		digit++;
	}
	pio_write(baseHigh, ssegData);//display on the 3 right displays  HEX2,HEX1,HEX0

	ssegData=0;
	// Assembly of 3 data with an OR and bit shifting to form a 24 bit
	for (i = 3; i < 6; i++) {
			ssegData = (ssegData << 8) | *digit;
			digit++;
	}
	pio_write(baseLow, ssegData);//display on the 3 left displays HEX5,HEX4,HEX3
}
void get_switches(alt_u32 sw_base, int *period)
{
	/**************************************************************************
	 * Read the state of the switches for the timer period
	 **************************************************************************
	 * Parameters
	 * sw_base : base adress of the switches
	 * period : pointer to the variable that contains the period
	 *
	 * Return
	 * none
	 *
	 * Side effects
	 * period : The pointed variable is modified
	 *
	 * Notes
	 * Puisque le registre switches n�a que 10 bits, on applique un masque
	 * sur la valeur lue pour s�assurer que les bits 10 a 31 sont nuls.
	 *************************************************************************/

	*period = IORD(sw_base, 0) & 0x000003FF;
}

void get_keys(alt_u32 keyBase, int *keyValue)
{
	/**************************************************************************
	 * Reads the state of the buttons
	 **************************************************************************
	 * Parameters
	 * key_base : base address of the switches
	 * keyValue : pointer for state of the switches
	 *
	 * Return
	 * none
	 *
	 * Side Effects
	 * keyValue : Pointed variable is modified
	 *
	 * Notes
	 * Since the register of keys only has 4 bits, we appy a mask
	 * on the value read in order to make sure that the bits 4 to 31 are NULL
	 *************************************************************************/

	*keyValue = IORD(keyBase, 0) & 0x0000000F;
}

void delay_ms(int delay)
{
	/**************************************************************************
	* function that waits a certain number of ms (approximative)
	**************************************************************************
	* Parametres
	* delay: time to wait in ms
	*
	* Return
	* nothing
	*
	* Side effects
	* None
	*
	* Notes
	* The delay is based on the execution of a useless loop.
	* One turn of the loop takes approximately 500ns (2000 loops per ms)
	* Estimation is based on :
	* - 5 instructions per loops
	* - 5 cycles per instruction for the Nios II/s
	* - 20 ns per clock cycle @ 50MHz
	*************************************************************************/
	alt_u32 i, itr;
	itr = delay * 2000;
	for (i = 0; i < itr; ++i) { // uselless loop
		;
	}
}

void number_to_character(alt_u16 number, alt_u8 *charOut)
{
	/**************************************************************************
	* Splits a the 4 digits of arg number into seperates chars
	*		1234 ----> [1][2][3][4]
	**************************************************************************
	* Parametres
	* numberIn: period of timer
	* *charOut: target string for the characters
	*
	* Return
	* nothing
	*
	* Side effects
	* None
	*
	*************************************************************************/
	int tempAff=number;

	charOut[5] = sseg_conv_hex(tempAff%10);
	tempAff = tempAff-tempAff%10;
	charOut[4] = sseg_conv_hex(tempAff%100/10);
	tempAff = tempAff-tempAff%100;
	charOut[3] = sseg_conv_hex(tempAff%1000/100);
	tempAff = tempAff-tempAff%1000;
	charOut[2] = sseg_conv_hex(tempAff/1000);
	charOut[1] = sseg_conv_hex(0);
	charOut[0] = sseg_conv_hex(0);
}
alt_u8 number_to_ascii(alt_u8 numberIn)
{
	/**************************************************************************
	 * takes a unsigned 8 bit integer and returns the corresponding char
	 **************************************************************************
	 * Parametres
	 * numberIn: period of timer
	 *
	 * Return
	 * ASCII character
	 *
	 * Side effects
	 * None
	 *
	 *************************************************************************/
	return numberIn+0x30;
}
void periode_to_message(alt_u16 number, alt_u8 *messageOut)
{
	/**************************************************************************
	 * Transforms a timer period into a string of chars to be sent over
	 * UART
	 **************************************************************************
	 * Parametres
	 * number: period of timer
	 * *messageOut: output string of message
	 *
	 * Return
	 * None
	 *
	 * Side effects
	 * None
	 *
	 *************************************************************************/
	int tempAff=number;

	messageOut[16] = number_to_ascii(tempAff%10);
	tempAff = tempAff-tempAff%10;
	messageOut[15] = number_to_ascii(tempAff%100/10);
	tempAff = tempAff-tempAff%100;
	messageOut[14] = number_to_ascii(tempAff%1000/100);
	tempAff = tempAff-tempAff%1000;
	messageOut[13] = number_to_ascii(tempAff/1000);
	messageOut[12] = number_to_ascii(0);
	messageOut[11] = number_to_ascii(0);
	messageOut[17]=' ';
	messageOut[18]='m';
	messageOut[19]='s';
	messageOut[20]='\n';
	messageOut[21]='\r';
	messageOut[22]='\0';
}

void juart_write_string(alt_u32 jtagBase, alt_u8 *message)
{
	/**************************************************************************
	 * Sends a string of char on the JTAG UART
	 **************************************************************************
	 * Parametres
	 * jtagBase: Base address of jtag
	 * *message: string to send over JTAG
	 *
	 * Return
	 * None
	 *
	 * Side effects
	 * None
	 *
	 *************************************************************************/
	// message is a cstr (string finishing in a NULL)
	alt_u32 data32;
	while(*message != '\0') {
		data32 = (alt_u32) *message;
		if (juart_read_wspace(jtagBase) != 0) {
			juart_write_char(jtagBase, data32);
			message++;
		}
	}
}

void timer_write_period(alt_u32 timerBase, alt_u32 period)
{
	/**************************************************************************
	 * start the timer in continuous mode with interrupts
	 **************************************************************************
	 * Parametres
	 * timerBase: Base address of timer
	 * period: desired period in ms
	 *
	 * Return
	 * None
	 *
	 * Side effects
	 * None
	 *
	 *************************************************************************/
	period = period*TIMER_CONVERSION;
	alt_u16 high, low;
	/* split 32 bits period into two 16 bits */
	high = (alt_u16) (period >> 16);
	low = (alt_u16) (period & 0x0000FFFF);
	/* write period */
	IOWR(timerBase, TIMER_PERIODH_REG_OFT, high);
	IOWR(timerBase, TIMER_PERIODL_REG_OFT, low);
	/* setup timer for start in continuous mode w/ interrupt */
	IOWR(timerBase, TIMER_CTRL_REG_OFT, 0x0007); // bits 0, 1, 2 actives
}

void timer_0_ISR(void *context)
{
	/**************************************************************************
	 * Interrupt handler for timer 0, flashes the LEDs at the timer period
	 **************************************************************************
	 * Parametres
	 * *context: conext of ISR
	 *
	 * Return
	 * None
	 *
	 * Side effects
	 * None
	 *
	 *************************************************************************/

	// clear irq status in order to prevent retriggering
	IOWR(INTERVALTIMER_BASE, TIMER_STAT_REG_OFT, 0b10);

	static alt_u8 ledPattern = 0x01; // patron initial

	ledPattern ^= 0x03; // inverse 2 LSB
	IOWR(LEDS_BASE, 0, ledPattern); // Ecriture du patron dans registres de leds
}

void start_timer(alt_u32 timerBase)
{
	/**************************************************************************
	 * start the timer in continuous mode with interrupts
	 **************************************************************************
	 * Parametres
	 * timerBase: Base address of timer
	 *
	 * Return
	 * None
	 *
	 * Side effects
	 * None
	 *
	 *************************************************************************/

	IOWR(timerBase, TIMER_CTRL_REG_OFT, 0b0111);
}

void stop_timer(alt_u32 timerBase)
{
	/**************************************************************************
	 * Stop the timer
	 **************************************************************************
	 * Parametres
	 * timerBase: Base address of timer
	 *
	 * Return
	 * None
	 *
	 * Side effects
	 * turns off the interrupt,
	 * turns off continuous
	 *
	 *************************************************************************/

	IOWR(timerBase, TIMER_CTRL_REG_OFT, 0b1000);
}

int main(void)
{
	/**************************************************************************
	 * Main program :)
	 *************************************************************************/
	alt_u32 period = 100;
	int displayVal;
	int keyVal;
	char pauseFlag=0;
	
	//Stop timer and setup the interrupt, then start with 100ms period (default)
	stop_timer(INTERVALTIMER_BASE);
	timer_write_period(INTERVALTIMER_BASE,period);
	alt_ic_isr_register(INTERVALTIMER_IRQ_INTERRUPT_CONTROLLER_ID, INTERVALTIMER_IRQ, timer_0_ISR,0x0,0x0);	

	while (1) {
		alt_u8 message[6]; //Empty message to fill with the period for the 7 segments 
		alt_u8 pause_msg[6]={HEX_BLANK, HEX_P, HEX_A, HEX_U, HEX_S, HEX_E}; //setup pause value in the array to siplay pause on the displays
		alt_u8 jtag_message[50]={'N','e','w',' ','P','e','r','i','o','d',':'};

		//Application Final
		get_switches(SWITCHES_BASE, &displayVal);
		get_keys(PUSHBT_BASE, &keyVal);
		//load pressed?
		if(!(keyVal & LOAD_KEY_MSK)){
			delay_ms(50);
			period = displayVal;
			periode_to_message(period,jtag_message);
			
			//load timer if period is not 0
			if(period != 0){
				stop_timer(INTERVALTIMER_BASE);
				//send JTAG msg
				juart_write_string(JTAG_UART_0_BASE,jtag_message);
				//write timer period
				timer_write_period(INTERVALTIMER_BASE,period);
			}
			while(!(keyVal & LOAD_KEY_MSK)){
				get_keys(PUSHBT_BASE, &keyVal);
			}
			delay_ms(100);
		}

		//Pause pressed? flip the pause flagState
		if(!(keyVal & PAUSE_KEY_MSK)){	
			delay_ms(50);		
			pauseFlag = ~pauseFlag;
			sseg_disp_6_digit(DISP_0_TO_2_BASE,DISP_3_TO_5_BASE,pause_msg);
			if(pauseFlag != 0){
				stop_timer(INTERVALTIMER_BASE);
			}
			else{
				start_timer(INTERVALTIMER_BASE);
		 	}
			while(!(keyVal & PAUSE_KEY_MSK)){
				get_keys(PUSHBT_BASE, &keyVal);
			}
			delay_ms(100);
		}

		number_to_character(displayVal, message);//split the period into individual number
		//Show switch state if not paused
		if(pauseFlag==0){
			sseg_disp_6_digit(DISP_0_TO_2_BASE,DISP_3_TO_5_BASE,message);
		}
	}
}
