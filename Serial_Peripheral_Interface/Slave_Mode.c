/* Number Guessing Game
 *
 * CONTROLLER B
 *
 *
 *
 SPI_bounce_master
 At regular WDT intervals, this sends data out the UCB SPI interface.
 The data received is logged by the RX interrupt handler.
 This example can be used to loop back (ie, connecting MOSI to MISO)
 A global counter keeps track of the numbers of TX and RX operations.

 Timing and clock.
 MCLK and SMCLK = 8 Mhz
 UCB0BRx interface divisor is a parameterized below.
 WDT divides SMCL by 512 (==> fastest rate gives 1 TX every 64 microseconds)
 Parameter ACTION_INTERVAL controls actual frequency of WDT interrupts that TX
 16 bit Parameter BIT_RATE_DIVISOR controls the SPI bitrate clock
*/

#include "msp430g2553.h"

 /* declarations of functions defined later */
 void init_spi(void);
 void init_wdt(void);

// Global variables and parameters (all volatilel to maintain for debugger)
volatile unsigned int start = 0;			// flag for the start of a round
volatile unsigned int compare_rec = 0;		// 0 - no comparisons made yet
volatile unsigned int game_over = 0;		// flags the end of the round
volatile unsigned int guess = 0x800;		// start with the middle number of 65535
volatile unsigned int i;

// Try for a fast send.  One transmission every 64 microseconds
// bitrate = 1 bit every 4 microseconds
#define ACTION_INTERVAL 1
#define BIT_RATE_DIVISOR 16

// ===== Watchdog Timer Interrupt Handler ====

volatile unsigned int action_counter=ACTION_INTERVAL;

interrupt void WDT_interval_handler(){
	//right now - setup to send one guess
	if(start == 1 && game_over==0){
		if(compare_rec == 0){
			guess = 0x8000;					//first guess - 32768
			UCB0TXBUF = guess>>8;			//transmit high order bits to master
			//for(i = 10; i > 0; i--){		//to delay between transmissions
			//}
			UCB0TXBUF = guess;				//transmit low order bits to master
		}
	}
}
ISR_VECTOR(WDT_interval_handler, ".int10")

void init_wdt(){
	// setup the watchdog timer as an interval timer
	// INTERRUPT NOT YET ENABLED!
  	WDTCTL =(WDTPW +		// (bits 15-8) password
     	                   	// bit 7=0 => watchdog timer on
       	                 	// bit 6=0 => NMI on rising edge (not used here)
                        	// bit 5=0 => RST/NMI pin does a reset (not used here)
           	 WDTTMSEL +     // (bit 4) select interval timer mode
  		     WDTCNTCL  		// (bit 3) clear watchdog timer counter
  		                	// bit 2=0 => SMCLK is the source
  		     +2         	// bits 1-0 = 10=> source/512
 			 );
  	IE1 |= WDTIE; // enable WDT interrupt
 }


// ======== Receive interrupt Handler for UCB0 ==========

void interrupt spi_rx_handler(){
	if(start==0){
		start = UCB0RXBUF;	//
		//for(i = 10; i > 0; i--){		//to delay between receiving bit streams
		//}
		//secret_number = secret_number + UCB0RXBUF;  // was used in debugging
	}
	IFG2 &= ~UCB0RXIFG;		 // clear UCB0 RX flag
}
ISR_VECTOR(spi_rx_handler, ".int07")


//Bit positions in P1 for SPI
#define SPI_CLK 0x20
#define SPI_SOMI 0x40
#define SPI_SIMO 0x80

// calculate the lo and hi bytes of the bit rate divisor
#define BRLO (BIT_RATE_DIVISOR &  0xFF)
#define BRHI (BIT_RATE_DIVISOR / 0x100)

void init_spi(){
	UCB0CTL1 = UCSSEL_2+UCSWRST;  		// Reset state machine; SMCLK source;
	UCB0CTL0 = UCCKPH					// Data capture on rising edge
			   							// read data while clock high
										// lsb first, 8 bit mode,
										// SLAVE
			   +UCMODE_0				// 3-pin SPI mode
			   +UCSYNC;					// sync mode (needed for SPI or I2C)
	UCB0BR0=BRLO;						// set divisor for bit rate
	UCB0BR1=BRHI;
	UCB0CTL1 &= ~UCSWRST;				// enable UCB0 (must do this before setting
										//              interrupt enable and flags)
	IFG2 &= ~UCB0RXIFG;					// clear UCB0 RX flag
	IE2 |= UCB0RXIE;					// enable UCB0 RX interrupt
	// Connect I/O pins to UCB0 SPI
	P1SEL |=SPI_CLK+SPI_SOMI+SPI_SIMO;
	P1SEL2|=SPI_CLK+SPI_SOMI+SPI_SIMO;
}


/*
 * The main program just initializes everything and leaves the action to
 * the interrupt handlers!
 */


void main(){

	WDTCTL = WDTPW + WDTHOLD;       // Stop watchdog timer
	BCSCTL1 = CALBC1_8MHZ;			// 8Mhz calibration for clock
  	DCOCTL  = CALDCO_8MHZ;

  	init_spi();
  	init_wdt();

  	//UCB0TXBUF=0x00; 	//set transmit buffer to 0

 	_bis_SR_register(GIE+LPM0_bits);
}