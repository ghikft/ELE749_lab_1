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
#include "system.h"

/* pilote pour 4 afficheurs 7 segments controles par un PIO 32bit
 sortie seulement. */
#define PIO_DATA_REG_OFT 0 // offset du registre Data
#define pio_write(base, data) IOWR(base, PIO_DATA_REG_OFT, data)

#define LOAD_KEY_MSK 0x02;
#define PAUSE_KEY_MSK 0x01;
alt_u8 sseg_conv_hex(int hex)

{
	/* patrons hexadecimaux pour afficheur 7seg active-low (0-9, a-f)
 le msb est ignore */
	static const alt_u8 SSEG_HEX_TABLE[16] = {
			0x04, 0x79, 0x24, 0x30, 0x19, 0x92, 0x02, 0x78, 0x00, 0x10, // 0-9
			0x88, 0x03, 0x46, 0x21, 0x06, 0x0E};		 	 	 	 	 // a-f
	alt_u8 pattern;
	if (hex < 16) {
		pattern = SSEG_HEX_TABLE[hex];
	} else {
		pattern = 0xff; // tous eteint
	}
	return (pattern);
}

/* pilote pour 4 afficheurs 7 segments controles par un PIO 32bit
 sortie seulement. (suite) */
void sseg_disp_4_digit(alt_u32 baseLow, alt_u32 baseHigh, alt_u8 *digit)
{
	/* digit est l'adresse d'un tableau de 4 alt_u8 */
	alt_u32 sseg_data=0;
	int i;
	/* assemblage de 4 donnees par OR et decalage pour former un 32 bit */
	for (i = 0; i < 3; i++) {
		sseg_data = (sseg_data << 8) | *digit;
		digit++;
	}
	pio_write(baseHigh, sseg_data);
	sseg_data=0;

	for (i = 3; i < 6; i++) {
			sseg_data = (sseg_data << 8) | *digit;
			digit++;
	}
	pio_write(baseLow, sseg_data);
}

/* pilote pour 6 afficheurs 7 segments controles par deux PIO 24bit
 sortie seulement. (suite) */
void sseg_disp_6_digit(alt_u32 baseLow, alt_u32 baseHigh, alt_u8 *digit)
{
	/* digit est l'adresse d'un tableau de 6 alt_u8 */
	alt_u32 sseg_data=0;
	int i;
	/* assemblage de 3 donnees par OR et decalage pour former un 24 bit */
	for (i = 0; i < 3; i++) {
		sseg_data = (sseg_data << 8) | *digit;
		digit++;
	}
	pio_write(baseHigh, sseg_data);//affiche sur les trois afficheur de droite HEX2,HEX1,HEX0

	sseg_data=0;
	/* assemblage de 3 donnees par OR et decalage pour former un 24 bit */
	for (i = 3; i < 6; i++) {
			sseg_data = (sseg_data << 8) | *digit;
			digit++;
	}
	pio_write(baseLow, sseg_data);//affiche sur les trois afficheur de gauche HEX5,HEX4,HEX3
}
void get_switches(alt_u32 sw_base, int *period)
{
	/**************************************************************************
	 * Lecture de la periode, selon position des switches
	 **************************************************************************
	 * Parametres
	 * sw_base : adresse de base des switches
	 * period : pointeur vers variables qui contiendra la periode
	 *
	 * Valeur de retour
	 * aucune
	 *
	 * Effets de bords
	 * period : variable pointee par period est modifiee
	 *
	 * Notes
	 * Puisque le registre switches n�a que 10 bits, on applique un masque
	 * sur la valeur lue pour s�assurer que les bits 10 a 31 sont nuls.
	 *************************************************************************/

	*period = IORD(sw_base, 0) & 0x000003FF;
}

void get_keys(alt_u32 key_base, int *keyValue)
{
	/**************************************************************************
	 * Lecture de des bouton, selon position des switches
	 **************************************************************************
	 * Parametres
	 * key_base : adresse de base des bouton
	 * keyValue : pointeur vers variables qui contiendra l'�tat des bouton
	 *
	 * Valeur de retour
	 * aucune
	 *
	 * Effets de bords
	 * keyValue : variable pointee par keyValue est modifiee
	 *
	 * Notes
	 * Puisque le registre keys n�a que 4 bits, on applique un masque
	 * sur la valeur lue pour s�assurer que les bits 4 a 31 sont nuls.
	 *************************************************************************/

	*keyValue = IORD(key_base, 0) & 0x0000000F;
}

void led_flash(alt_u32 led_base, int period)
{
	/**************************************************************************
	 * Inversion de deux DEL, selon periode donnee
	 **************************************************************************
	 * Parametres
	 * led_base: adresse de base des leds
	 * period : periode en milisecondes (approx)
	 *
	 * Valeur de retour
	 * aucune
	 *
	 * Effets de bords
	 * aucun
	 *
	 * Notes
	 * Le delais est genere par l�execution d�une boucle for inutile.
	 * On estime a 500ns le temps d�execution de la boucle, il faut
	 * donc 2000 tours de boucle par miliseconde.
	 * Estimation basee sur :
	 * - 5 instructions par iteration de boucle
	 * - 5 cycles par instruction sur le Nios II/s
	 * - 20 ns par cycle d�horloge (horloge de 50MHz)
	 *************************************************************************/

	static alt_u8 led_pattern = 0x01; // patron initial

	alt_u32 i, itr;

	led_pattern ^= 0x03; // inverse 2 LSB
	IOWR(led_base, 0, led_pattern); // Ecriture du patron dans registres de leds
	itr = period * 2000;
	for (i = 0; i < itr; ++i) { // boucle inutile, 1 comparaison
		; // et 1 incrementation par iteration
	}
}

void number_to_character(alt_u16 number, alt_u8 *charOut){
	int tempAff=number;

	charOut[0] = sseg_conv_hex(tempAff%10);
	tempAff = tempAff-tempAff%10;
	charOut[1] = sseg_conv_hex(tempAff%100/10);
	tempAff = tempAff-tempAff%100;
	charOut[2] = sseg_conv_hex(tempAff%1000/100);
	tempAff = tempAff-tempAff%1000;
	charOut[3] = sseg_conv_hex(tempAff/1000);
	charOut[4] = sseg_conv_hex(0);
	charOut[5] = sseg_conv_hex(0);
}
int main(void)
{
	/**************************************************************************
	 * Programme principal
	 *************************************************************************/
	int period;
	int displayVal;
	int keyVal;
	char PauseFlag=0;
	//start timer with default period value
	while (1) {
		alt_u8 message[6];
		//alt_u8 pause_msg[6]={'';//setup pause value in the array/// HARD code Letter with define
		message[0]=sseg_conv_hex(10);
		message[1]=sseg_conv_hex(11);
		message[2]=sseg_conv_hex(12);
		message[3]=sseg_conv_hex(1);
		message[4]=sseg_conv_hex(2);
		message[5]=sseg_conv_hex(3);

		get_switches(SWITCHES_BASE, &period);

		/*tempAff=period;
		message[0] = sseg_conv_hex(tempAff%10);
		tempAff = tempAff-tempAff%10;
		message[1] = sseg_conv_hex(tempAff%100/10);
		tempAff = tempAff-tempAff%100;
		message[2] = sseg_conv_hex(tempAff%1000/100);
		tempAff = tempAff-tempAff%1000;
		message[3] = sseg_conv_hex(tempAff/1000);*/
		sseg_disp_4_digit(DISP_0_TO_2_BASE,DISP_3_TO_5_BASE,message);


		led_flash(LEDS_BASE, period);

		/*//Application Final
		get_switches(SWITCHES_BASE, &displayVal);
		get_keys(PUSHBT_BASE, *keyVal)
		//load pressed?
		if(!(keyVal & LOAD_KEY_MSK)){
			period = displayVal;
			//load timer
			//send JTAG msg
		}

		//Pause pressed? flip the pause flagState
		if(!(keyVal & PAUSE_KEY_MSK)){
			pauseFlag ~= pauseFlag

			if(pauseFlag != 0){
			//stop timer
			}
			else{
				//startTimer
		 	}
		}

		//number_to_character(period, message);//split the period into individual number
		if(pauseFlag==0){
			sseg_disp_6_digit(DISP_0_TO_2_BASE,DISP_3_TO_5_BASE,message);
		}
		else{
			sseg_disp_6_digit(DISP_0_TO_2_BASE,DISP_3_TO_5_BASE,pause_msg);
		}
		 * led_flash(LEDS_BASE, period);*/

	}
}








