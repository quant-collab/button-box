/*
 * File:   main.c
 *
 * Created on August 16, 2010, 12:09 PM
 */

#include <stdint.h>
#include "p18cxxx.h"

void delay();

void delay() {
    unsigned long counter = 0;
    for (counter = 0; counter < 120*1000UL; counter++) {
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

#define RED_BUTTON_BIT 0x01 // RB0 (red IPB)
#define GREEN_BUTTON_BIT 0x02 // RB1 (green button)
#define BLUE_BUTTON_BIT 0x04 // RB2 (blue button)
#define KEY_SWITCH_BIT 0x10 // RB4 (key switch)
#define PORTB_INPUT_PINS (RED_BUTTON_BIT | GREEN_BUTTON_BIT | BLUE_BUTTON_BIT | KEY_SWITCH_BIT)

#define RED_PRESSED ((PORTB & RED_BUTTON_BIT) == 0)
#define GREEN_PRESSED ((PORTB & GREEN_BUTTON_BIT) == 0)
#define BLUE_PRESSED ((PORTB & BLUE_BUTTON_BIT) == 0)
#define KEY_ON ((PORTB & KEY_SWITCH_BIT) == 0)

void init_gpio(void) {
    // TRISx = Tri-state / data direction for port x pins: 0 = output, 1 = input (default)
    
    // Port A
    // RA0-RA4: (not connected)
    //TRISA = 0;

    // Port B
    // 4 digital inputs: red, green, and blue pushbuttons plus key switch
    TRISB |= PORTB_INPUT_PINS;
    LATB |= PORTB_INPUT_PINS;
    INTCON2bits.RBPU = 0; // Enable pull-ups on PORTB
    ADCON1 |= 0x0F; // Disable ADC function on PORTB pins (use for digital input)
    
    // Port C
    // RC2 (yellow LED on board), RC3 (red LED on board)
    #define SQ_YELLOW_LED_PIN 0x02
    #define SQ_RED_LED_PIN 0x04
    TRISC &= ~(SQ_YELLOW_LED_PIN | SQ_RED_LED_PIN);
    
    // Port D
    // RD0: shift register data feed
    // RD1: shift register clock
    // RD2: shift register latch clock
    // RD3: shift register output enable
    // RD4: shift register reset
    // RD5: red LED (inside illuminated pushbutton)
    #define RED_IPB_LED_PIN 0x20
    #define PORTD_OUTPUT_PINS (0x01 | 0x02 | 0x04 | 0x08 | 0x10 | RED_IPB_LED_PIN)
    TRISD &= ~PORTD_OUTPUT_PINS;
    
    // Port E
    // RE0: (not connected)
    // RE1: on-board LED
    // RE2: on-board button (w/ external pull-up to +5V)
    #define BUILTIN_RED_LED_PIN 0x02
    TRISE &= ~BUILTIN_RED_LED_PIN;
}

#pragma config OSC = HSPLL // || HS || XT
#pragma config WDT = OFF
void main(void) {
    uint24_t count = 0;
    init_gpio();

    sr_output_enable(0);
    sr_reset();
    //sr_write(0x01A6);
    sr_write(0);
    dumb_delay(100UL);
    sr_output_enable(1);
       
    unsigned char state = 0x00;
    while (1) {
        // Flash red LED (inside IPB)
        //PORTD ^= INTERNAL_RED_LED_PIN;

        // If blue/green buttons pressed, light up red/yellow LEDs
        if (BLUE_PRESSED) {
          PORTC |= SQ_RED_LED_PIN;
        } else {
          PORTC &= ~SQ_RED_LED_PIN;
        }
        if (GREEN_PRESSED) {
          PORTC |= SQ_YELLOW_LED_PIN;
        } else {
          PORTC &= ~SQ_YELLOW_LED_PIN;
        }
        
        // If key is on, light up internal LED        
        if (KEY_ON) {
          PORTE |= BUILTIN_RED_LED_PIN;
        } else {
          PORTE &= ~BUILTIN_RED_LED_PIN;
        }

        // Blink internal/built-in LED each time around
        //PORTE ^= BUILTIN_RED_LED_PIN;
        delay();
        //dumb_delay(10000);

        //PORTB = 0x00;
        //PORTE &= ~0x01;
        //delay();
        //count = PORTB; // DEBUG: show input pins on LEDs
        sr_output_enable(0);
        sr_write(count);
        sr_output_enable(1);

        count++;
        if (count > 0x1FFFF) {
          count = 0;
        }
    }
}


