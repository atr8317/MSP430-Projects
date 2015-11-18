/* Number Guessing Game
 *
 * CONTROLLER A
 *
*/

#include "msp430g2553.h"

 /* declarations of functions defined later */
 void init_spi(void);
 void init_wdt(void);

// Global variables and parameters (all volatilel to maintain for debugger)
volatile unsigned int secret_number = 0; 	// number chosen for B to guess
volatile unsigned int start = 0;		// flag for the start of a round
volatile int compare_send = 0;			// indicates the comparison secret and guessed numbers
volatile unsigned int game_over = 0;		// flags the end of the round
volatile unsigned int guess_rec = 0;		// number guessed received by B
unsigned int temp;				// temp variable
volatile unsigned int last_guess = 0;		// to verify a new guess was sent
volatile unsigned int last_button = 1;		// button press will be used to choose a number
volatile unsigned int b;			// for the current button state
volatile unsigned int i = 0;			// for loop counter

//--------------------------------------------------------------------------------

// Try for a fast send.  One transmission every 64 microseconds
// bitrate = 1 bit every 4 microseconds
#define ACTION_INTERVAL 1
#define BIT_RATE_DIVISOR 16
//Bit mask for button
#define BUTTON 0x08

// ===== Watchdog Timer Interrupt Handler ====
interrupt void WDT_interval_handler(){

	//Chooses the number
	if(start==0){ 			// only read button at start of game
		b = (P1IN & BUTTON); 	// read button bit
		if(b==0)		//button is pressed and game not started 
					//--- choosing secret number
			secret_number++;//increment secret number as long as button is held down

			//secret_number = 25000;//hardwired value for debugging purposes
						//to check what is transmitted to B in a 2-transmission cycle
						//i.e. send high order bits, then low order bits
						//***current transmissions of the full 16 bits are missing bits after 
							// receiving and shifting***
		else if(last_button==0 && b!=0)	//when button is released- set start flag
			start = 1;
		last_button = b;
	}

	//Transmit start flag to B
	if(start==1 && game_over==0) //keep transmitting start flag until a guess is received
	{
		UCB0TXBUF = start;		// send start flag - receive high order bits of guess from B
		//for(i = 10; i > 0; i--){	// to delay between transmissions
		//}
		UCB0TXBUF = start; 		//dummy tx to receive low order bits of guess from B
						// ***second transmission being received correctly by B***
						// ***maybe because only LSB is high and no shifting is required***
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
  		     WDTCNTCL  	// (bit 3) clear watchdog timer counter
  		                // bit 2=0 => SMCLK is the source
  		     +2		// bits 1-0 = 10=> source/512
 			 );
  	IE1 |= WDTIE; // enable WDT interrupt
 }


// ======== Receive interrupt Handler for UCB0 ==========
void interrupt spi_rx_handler(){
	//Receiving guess from B
	if(start==1 && game_over==0){
		temp = UCB0RXBUF; 		// copy data to global variable
		guess_rec = temp<<8;		// shift high order bits into upper 8-bits of variable
		//for(i = 10; i > 0; i--){	//to delay between receiving
		//}
		temp = UCB0RXBUF;
		guess_rec = guess_rec + temp;	// add lower bits into variable
				// *** rx from B is currently receiving more high bits than it should***
				// *** first guess should be 1000 0000 0000 0000
				// *** for different codding attempts, the closest rx has been:  1000 0000 1000 0000
				// *** "seems" to be sending high order bits twice
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
	UCB0CTL0 = UCCKPH			// Data capture on rising edge
			   			// read data while clock high
						// lsb first, 8 bit mode,
			   +UCMST		// master
			   +UCMODE_0		// 3-pin SPI mode
			   +UCSYNC;		// sync mode (needed for SPI or I2C)
	UCB0BR0=BRLO;				// set divisor for bit rate
	UCB0BR1=BRHI;
	UCB0CTL1 &= ~UCSWRST;			// enable UCB0 (must do this before setting
						//              interrupt enable and flags)
	IFG2 &= ~UCB0RXIFG;			// clear UCB0 RX flag
	IE2 |= UCB0RXIE;			// enable UCB0 RX interrupt
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
	BCSCTL1 = CALBC1_8MHZ;		// 8Mhz calibration for clock
  	DCOCTL  = CALDCO_8MHZ;

  	init_spi();
  	init_wdt();

   	P1DIR &= ~BUTTON;		//set direction
  	P1OUT |= BUTTON;		//set out
  	P1REN |= BUTTON;		//enable pullup resistor - active low

  	//UCB0TXBUF=0x00; 		//set transmit buffer to 0

 	_bis_SR_register(GIE+LPM0_bits);
}
