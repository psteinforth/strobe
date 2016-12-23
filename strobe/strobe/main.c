/*
 * strobe.c
 *
 * Created: 22.12.2016 19:00:13
 * Author : Patrick Steinforth (dev@steinforth.eu)
 */ 

#include <avr/io.h>
#include <avr/interrupt.h>
#include <inttypes.h> //int8_t and similar datatypes

#ifndef F_CPU
/* Define CPU frequency F_CPU if F_CPU was not already defined 
   (e.g. by command line parameter for compiler within 
   a Makefile). Warn if no frequency was passed by parameter. */
#warning "F_CPU was not defined and will be set to 1MHz"
#define F_CPU 1000000UL     /* Internal oscillator with 1.0000 MHz (prescaled by 8) */
#endif

#ifndef F_TIMER_INT
/* Define how often Timer1 Interrupt service routine is called 
   (e.g. by command line parameter for compiler within 
   a Makefile). Warn if no frequency was passed by parameter. */
#warning "F_TIMER_INT was not defined and will be set to 256"
#define F_TIMER_INT 256UL     /* Interrupt called every 1000/F_TIMER_INT= x milliseconds */
#endif

#ifndef TIMER_PRESCALER
/* Set hardware prescaler of timer 
   (e.g. by command line parameter for compiler within 
   a Makefile). Warn if no prescaler was passed by parameter. 
   Value has to match the timer/counter configuration. */
#warning "TIMER_PRESCALER was not defined and will be set to 1"
#define TIMER_PRESCALER 1UL     /* Timer counts every TIMER_PRESCALER clock cycle */
#endif

#define OC_MATCH_VAL ((F_CPU / (TIMER_PRESCALER * F_TIMER_INT)) -1) 
/* We count F_TIMER_INT times to the above calculated value. After that, one second is gone. Ok, this
is not completely correct, we have to account for a divisions remainder in the last step. This will
calculated next. We subtract 1 because timer period equals output compare register plus one.*/
#define REMAINDER (F_CPU % (TIMER_PRESCALER * F_TIMER_INT))

#if (OC_MATCH_VAL+REMAINDER) > 65535
#error "Configuration would result in overflow of timer/counter register. Increase F_TIMER_INT or TIMER_PRESCALER."
#endif

//#define LED_BUZZER_ACTIVE PORTB0
//#define BUZZER PORTB2

uint8_t volatile fsm_next_state = 0; // 0=wait for key press, 1=buzzer locked, 2=blinking
uint8_t volatile counter_seconds = 0;
uint8_t volatile counter_minutes = 0;

//Interrupt Service Routine for INT0
ISR(EXT_INT0_vect)
{
	PINB = (1 << PINB0); //Toggle LED
}

//Interrupt Service Routine for timer 1 compare matches
ISR(TIM1_COMPA_vect)
{
	    static uint8_t prescaler=(uint8_t)F_TIMER_INT;

	    if( --prescaler == 0 ) {
		    prescaler = (uint8_t)F_TIMER_INT;
		    counter_seconds++;                               // one second is gone
		    if( counter_seconds == 60 ) counter_seconds = 0;          // one minute is gone
#if REST==0												//if there is a remainder
	    }                                           
#else
	    OCR1A = OC_MATCH_VAL + REMAINDER;             // account for remainder in last call of ISR for every second
	    } else {
	    OCR1A = OC_MATCH_VAL;                    // use default value in any other cases
    }
#endif
}


void init(void)
{
	//TODO: = instead of |= is enough, if init() is only called after a microcontroller reset because in that case I/O register include default values (0 in most cases)
	
	//Setup external interrupt INT0
	MCUCR = (1 << ISC01); //look for falling edges
	GIMSK = (1 << INT0); //enable INT0 interrupt
	
	//Configure pins
	//PORTxn is input if DDxn = 0, output if DDxn = 1; 
	//if PORTxn = 1 and input => pull-up resistor activated
	//if pin configured as output then PORTxn determines high (1) or low (0) state
	//writing 1 to PINxn toggles value of PORTxn
	PORTB = (1 << PORTB2);
	DDRB = (1 << DDB0);
	//_NOP(); //nop for synchronisation
	
	//Configure Timer1
	TCCR1B = (1 << WGM12); //clear timer on compare match
	//TCCR1B |= (1 << CS10); //set prescaler to 1
	OCR1A = OC_MATCH_VAL; //Set value for interrupt
	//TIMSK1 = (1 << OCIE1A); //enable interrupt on compare match
	
	//Enable interrupts globally
	sei();
}

int main(void)
{
	init();
	
	
	
    /* Replace with your application code */
    while (1) 
    {
    }
}

