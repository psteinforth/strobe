/*
 * main.c
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

#ifndef ADC_AVERAGE_STEPS
/* Define from how much analog digital conversions the average will be calculated. */
#warning "ADC_AVERAGE_STEPS was not defined and will be set to 3"
#define ADC_AVERAGE_STEPS 3
#endif
#if ADC_AVERAGE_STEPS > 31
#error "ADC_AVERAGE_STEPS must be in the range between 1 and 31 so we can use one single 8 bit variable for all ADC[1:7] pins."
#endif
#if ADC_AVERAGE_STEPS < 1
#error "ADC_AVERAGE_STEPS must be in the range between 1 and 31 so we can use one single 8 bit variable for all ADC[1:7] pins."
#endif

#ifndef MAX_BLINKING_TIME
/* Define the maximum blinking time in seconds. This value will be set if potentiometer is in maximum position. */
#warning "MAX_BLINKING_TIME was not defined and will be set to 30"
#define MAX_BLINKING_TIME 30
#endif
#if MAX_BLINKING_TIME > 255
#error "MAX_BLINKING_TIME must be in the range between 1 and 255."
#endif
#if MAX_BLINKING_TIME < 1
#error "MAX_BLINKING_TIME must be in the range between 1 and 255."
#endif

#ifndef MAX_BUZZER_LOCKED_TIME
/* Define the maximum buzzer blocking time in TENs of seconds. This value will be set if potentiometer is in maximum position. 
Example: A value of 37 results in a lock time of 37*10=370 seconds = 5 minutes 10 seconds*/
#warning "MAX_BUZZER_LOCKED_TIME was not defined and will be set to 2"
#define MAX_BUZZER_LOCKED_TIME 2
#endif
#if MAX_BUZZER_LOCKED_TIME > 255
#error "MAX_BUZZER_LOCKED_TIME must be in the range between 1 and 255."
#endif
#if MAX_BUZZER_LOCKED_TIME < 1
#error "MAX_BUZZER_LOCKED_TIME must be in the range between 1 and 255."
#endif

#define MAX_ADC_VALUE 256
//Maximum value of ADC conversion. 256 because of 8 bit resolution.

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
uint8_t volatile trimmer_BLINKING = 128;
uint8_t volatile trimmer_BUZZER_LOCKED = 128;

//Interrupt Service Routine for INT0
ISR(EXT_INT0_vect)
{
	buzzer_was_pressed = 1;
	GIMSK &= ~(1 << INT0); //turn interrupt off, so we do not have to debounce the push-button
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

ISR(ADC_vect)
{
	static uint8_t adc_iteration = 0;
	static uint16_t intermediate_result_BLINKING = 0;
	static uint16_t intermediate_result_BUZZER_LOCKED = 0;
	
	switch(adc_iteration / (ADC_AVERAGE_STEPS + 1)) //get conversion for ADC pin 0..(USED_ADC_PINS-1), plus 1 because the first conversion result will be discarded
	{
		case 0: // PA3 respectively ADC3
			if (adc_iteration % (ADC_AVERAGE_STEPS+1) == 0)
			{
				//First conversion result after ADC multiplexer change should be discarded
				intermediate_result_BLINKING = 0; //init temp variable
			}
			else
			{
				intermediate_result_BLINKING += ADCH;
			}
			if (1 * (ADC_AVERAGE_STEPS+1) -1 == adc_iteration)
			{
				// last iteration for this ADC pin
				trimmer_BLINKING = intermediate_result_BLINKING / ADC_AVERAGE_STEPS;
				ADMUX &= ~(1 << MUX5 | 1 << MUX4 | 1 << MUX3 | 1 << MUX2 | 1 << MUX1 | 1 << MUX0);
				ADMUX |= (1 << MUX1); //make conversion for other input next (ADC2)
			}
			ADCSRA |= (1 << ADSC); //do further conversion
			adc_iteration++;
			break;
		case 1: // PA2 respectively ADC2
			if (adc_iteration % (ADC_AVERAGE_STEPS+1) == 0)
			{
				//First conversion result after ADC multiplexer change should be discarded
				intermediate_result_BUZZER_LOCKED = 0; //init temp variable
			}
			else
			{
				intermediate_result_BUZZER_LOCKED += ADCH;
			}
			if (2 * (ADC_AVERAGE_STEPS+1) -1 == adc_iteration)
			{
				// last iteration for this ADC pin
				trimmer_BUZZER_LOCKED = intermediate_result_BUZZER_LOCKED / ADC_AVERAGE_STEPS;
				ADMUX &= ~(1 << MUX5 | 1 << MUX4 | 1 << MUX3 | 1 << MUX2 | 1 << MUX1 | 1 << MUX0);
				ADMUX |= (1 << MUX1 | 1 << MUX0); //make conversion for other input next (ADC3)
				
				//all ADC-pin values calculated. Re-init some variables, for the case that a new conversion will be started.
				adc_iteration = 0;
			}
			else
			{
				adc_iteration++;
				ADCSRA |= (1 << ADSC); //do further conversion
			}
			break;
		default:
			adc_iteration++;
		//should never be reached
	}
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
	DDRA = (1 << DDA1); //DEBUG
	//_NOP(); //nop for synchronisation
	
	//Configure Timer1
	TCCR1B = (1 << WGM12); //clear timer on compare match
	OCR1A = OC_MATCH_VAL; //Set value for interrupt
	TIMSK1 = (1 << OCIE1A); //enable interrupt on compare match
	
	//Configure Analog Digital Converter
	DIDR0 = (1 << ADC3D | 1 << ADC2D); //Disable digital input to ADC pins (not required but reduces power consumption)
	ADMUX = (1 << MUX1 | 1 << MUX0); //Select ADC3 as input for ADC
	ADCSRB = (1 << ADLAR); //Left adjust conversion result (so it's possible to read the 8 most significant bits in a easy way, 10 bit resolution not required)
	ADCSRA = (1 << ADPS1 | 1 << ADPS0 | 1 << ADIE | 1 << ADEN | 1 << ADSC); //Set ADC prescaler to 8; enable ADC interrupt; enable ADC; start conversion
		
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
    {PORTA |= (1 << DDA1); //Debug
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
			if (GIFR & (1 << INTF0))
			{
				GIFR = (1 << INTF0); //clear External Interrupt Flag
			}
			GIMSK = (1 << INT0); //enable external interrupt
		}
		if (fsm_state == BLINKING)
		{
			buzzer_was_pressed = 0;
			//counter_minutes = 0;
			//counter_seconds = 2;
			counter_seconds = (trimmer_BLINKING / (MAX_ADC_VALUE / MAX_BLINKING_TIME)) % 60;
			counter_minutes = (trimmer_BLINKING / (MAX_ADC_VALUE / MAX_BLINKING_TIME)) / 60;
			TCCR1B |= (1 << CS10); //set prescaler to 1 and enable timer/counter
		}
		if (fsm_state == BUZZER_LOCKED)
		{
			//counter_minutes = 0;
			//counter_seconds = 4;
			counter_seconds = (int8_t)((uint16_t)(trimmer_BUZZER_LOCKED / (MAX_ADC_VALUE / (MAX_BUZZER_LOCKED_TIME * 10.0)))) % 60;
			counter_minutes = (int8_t)((uint16_t)(trimmer_BUZZER_LOCKED / (MAX_ADC_VALUE / (MAX_BUZZER_LOCKED_TIME * 10.0)))) / 60;
			TCCR1B |= (1 << CS10); //set prescaler to 1 and enable timer/counter
		}
		
    }
}

