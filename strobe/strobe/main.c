/*
 * strobe.c
 *
 * Created: 22.12.2016 19:00:13
 * Author : Patrick Steinforth (dev@steinforth.eu)
 */ 

#include <avr/io.h>
#include <avr/interrupt.h>
#include <inttypes.h> //int8_t and similar datatypes
#include <util/delay.h>

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
#if F_TIMER_INT > 256
#error "F_TIMER_INT must be in the range between 1 and 256 because we use a 8 bit variable."
#endif
#if F_TIMER_INT < 1
#error "F_TIMER_INT must be in the range between 1 and 256 because we use a 8 bit variable."
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

typedef enum {WAIT_FOR_KEYPRESS=0, BLINKING, BUZZER_LOCKED} state_t;
	
typedef struct {//0 = false/no, 1 = true/yes, -1 = don't care
	//Outputs
	int8_t buzzer_enabled; //(LED)
	int8_t relais_enabled;
	//Inputs / requirements for state transition
	int8_t buzzer_pressed;
	int8_t timer_counting;
	//Next state
	state_t next_state;
	} strobe_state_t;

int8_t volatile counter_seconds = 0;
int8_t volatile counter_minutes = 0;
int8_t volatile buzzer_was_pressed = 0;

//Interrupt Service Routine for INT0
ISR(EXT_INT0_vect)
{
	buzzer_was_pressed = 1;
	GIMSK &= ~(1 << INT0); //turn interrupt off, so we do not have to debounce the push-button
_delay_ms(500);
	if (GIFR & (1 << INTF0))
	{
		GIFR = (1 << INTF0);	
	}	
}

//Interrupt Service Routine for timer 1 compare matches
ISR(TIM1_COMPA_vect)
{
	    static uint8_t prescaler=(uint8_t)F_TIMER_INT;

	    if( --prescaler == 0 ) {
		    prescaler = (uint8_t)F_TIMER_INT;                             
		    if( --counter_seconds <= 0 )                       
			{
				if (counter_minutes > 0)
				{
					counter_minutes--;
					counter_seconds = 60;
				}
				else
				{//we are done
					TCCR1B &= ~(1 << CS12 | 1<< CS11 | 1 << CS10); //disable timer/counter
					//TCNT1 = 0x0000; //clear counter (we use the clear timer on compare match mode)
				}
			}
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
	DDRB = (1 << DDB0 | 1 << DDB1);
	//_NOP(); //nop for synchronisation
	
	//Configure Timer1
	TCCR1B = (1 << WGM12); //clear timer on compare match
	OCR1A = OC_MATCH_VAL; //Set value for interrupt
	TIMSK1 = (1 << OCIE1A); //enable interrupt on compare match
	
	//Enable interrupts globally
	sei();
}

int main(void)
{
	init();
	
	state_t fsm_state = WAIT_FOR_KEYPRESS;
	
	strobe_state_t state_table[3] = {
	   // buzzer_enabled
	   // |   relais_enabled
	   // |   |   buzzer_pressed
	   // |   |   |   timer_counting
	   // |   |   |   |  next_state
		{ 1,  0,  1, -1, BLINKING}, //S_0 (WAIT_FOR_KEYPRESS)
		{ 0,  1, -1,  0, BUZZER_LOCKED}, //S_1 (BLINKING)
		{ 0,  0, -1,  0, WAIT_FOR_KEYPRESS}, //S_2 (BUZZER_LOCKED)
	};
	
    while (1) 
    {
		if (state_table[fsm_state].buzzer_enabled)
		{
			PORTB |= (1 << PORTB0); 
		} 
		else
		{
			PORTB &= ~(1 << PORTB0); 
		}
		if (state_table[fsm_state].relais_enabled)
		{
			PORTB |= (1 << PORTB1);
		} 
		else
		{
			PORTB &= ~(1 << PORTB1);
		}
		
		if (
			!(
				(state_table[fsm_state].buzzer_pressed == -1 ) ||
				(state_table[fsm_state].buzzer_pressed == 1 && buzzer_was_pressed) ||
				(state_table[fsm_state].buzzer_pressed == 0 && !buzzer_was_pressed)
			)
		)
		{
			continue;
		}
		int8_t timer_is_running = TCCR1B & (1 << CS12 | 1 << CS11 | 1 << CS10);  //timer is active if at least one bit of CS12, CS11 or CS10 is set
		if (
			!(
				(state_table[fsm_state].timer_counting == -1 ) ||
				(state_table[fsm_state].timer_counting == 1 && timer_is_running) ||
				(state_table[fsm_state].timer_counting == 0 && !timer_is_running)
				)
			)
		{
			continue;
		}
						
		fsm_state = state_table[fsm_state].next_state;
		if (fsm_state == WAIT_FOR_KEYPRESS)
		{
			GIMSK = (1 << INT0); //enable external interrupt
		}
		if (fsm_state == BLINKING)
		{
			buzzer_was_pressed = 0;
			counter_minutes = 1;
			counter_seconds = 10;
			TCCR1B |= (1 << CS10); //set prescaler to 1 and enable timer/counter
		}
		if (fsm_state == BUZZER_LOCKED)
		{
			counter_minutes = 0;
			counter_seconds = 7;
			TCCR1B |= (1 << CS10); //set prescaler to 1 and enable timer/counter
		}
		
    }
}

