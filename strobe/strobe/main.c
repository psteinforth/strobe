/*
 * strobe.c
 *
 * Created: 22.12.2016 19:00:13
 * Author : Patrick Steinforth
 */ 


#ifndef F_CPU
/* Define CPU frequency F_CPU if F_CPU was not already defined 
   (e.g. by command line parameter for compiler within 
   a Makefile). Warn if no frequency was passed by parameter. */
#warning "F_CPU was not defined and will be set to 1MHz"
#define F_CPU 1000000UL     /* Internal oscillator with 1.0000 Mhz (presecaled by 8) */
#endif

#include <avr/io.h>
#include <stdint.h> //int8_t and similar datatypes


int main(void)
{
    /* Replace with your application code */
    while (1) 
    {
    }
}

