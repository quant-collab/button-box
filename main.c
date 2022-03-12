/*
 * File:   main.c
 *
 * Created on August 16, 2010, 12:09 PM
 */

#include "p18cxxx.h"

void delay();

void delay() {
    unsigned long counter = 0;
    for (counter = 0; counter < 100*1000UL; counter++) {
        NOP();
    }
}

static inline
void dumb_delay(unsigned long limit) {
    for (unsigned long i=0; i<limit; i++) {
        NOP();
    }
}

static inline
void sr_reset() {
    PORTD &= ~0x10; // clears when low
    dumb_delay(10);
    PORTD |= 0x10;
}

static inline
void sr_output_enable(char enable) {
    if (enable) {
        PORTD &= ~0x08; // output enabled when low
    } else {
        PORTD |= 0x08;
    }
}


static inline
void sr_data(char bit) {
    if (bit) {
        PORTD |= 0x01;
    } else {
        PORTD &= ~0x01;
    }
}

// positive edge triggered
static inline
void sr_data_clock() {
    PORTD |= 0x02;
    dumb_delay(10);
    PORTD &= ~0x02;
    dumb_delay(10);
}

// positive edge triggered
static inline
void sr_latch_clock() {
    PORTD |= 0x04;
    dumb_delay(10);
    PORTD &= ~0x04;
    dumb_delay(10);
}

#define SR_LENGTH (17)
void sr_write(unsigned long bits) {
    for (int i=SR_LENGTH; i > 0; i--) {
        sr_data((bits >> i) & 1);
        sr_data_clock();
        //sr_latch_clock();
    }
    sr_latch_clock();
}

#pragma config OSC = HSPLL
//#pragma config OSC = HS
//#pragma config OSC = XT
#pragma config WDT = OFF
void main(void) {
    int count = 0;
    // Tri-state / data direction registers: 0 = output, 1 = input (default)
    //TRISA = 0;
    // RB0 (red/IPB), RB1 (green button), RB2 (blue button), RB4 (key switch)
    TRISB |= 0x01 | 0x02 | 0x04 | 0x10;
    LATB |= 0x01 | 0x02 | 0x04 | 0x10;
    INTCON2 &= 0x80; // Set /RBPU to zero so that we can enable pull-ups on PORTB
    // RC2 (yellow LED on board), RC3 (red LED on board)
    TRISC &= ~(0x02 | 0x04);
    // RD0: shift register data feed
    // RD1: shift register clock
    // RD2: shift register latch clock
    // RD3: shift register output enable
    // RD4: shift register reset
    // RD5: red LED on illuminated pushbutton
    TRISD &= ~(0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20);
    //TRISE &= ~0x01;
    TRISE = 0;

    sr_output_enable(0);
    sr_reset();
    //sr_write(0x01A6);
    sr_write(0);
    dumb_delay(100UL);
    sr_output_enable(1);
       
    unsigned char state = 0x00;
    while (1) {
        state ^= 0xFF;
        if (state) {
            PORTC |= 0x02;
            PORTC &= ~0x04;
            //sr_output_enable(0);
        } else {
            PORTC &= ~0x02;
            PORTC |= 0x04;
            //sr_output_enable(1);
        }
        //PORTA = state;
        //PORTB = 0x0F;
        PORTD ^= 0x20;
        //PORTE |= 0x01;
        PORTE = state;
        delay();
        //dumb_delay(10000);

        //PORTB = 0x00;
        //PORTE &= ~0x01;
        //delay();
        sr_output_enable(0);
        sr_write(count);
        sr_output_enable(1);

        count++;
        if (count > 0x1FFFF) {
            count = 0;
        }
    }
}


