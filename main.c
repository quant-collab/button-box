/*
 * File:   main.c
 *
 * Created on August 16, 2010, 12:09 PM
 */

#include <stdint.h>
#include "p18cxxx.h"

struct buttons {
    unsigned int red:1;
    unsigned int green:1;
    unsigned int blue:1;
    unsigned int key:1;
};

static inline
void dumb_delay(unsigned long limit) {
    for (unsigned long i=0; i < limit; i++) {
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
    for (int i = SR_LENGTH - 1; i >= 0; i--) {
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

// PORTC
#define SQ_YELLOW_LED_PIN 0x02
#define SQ_RED_LED_PIN 0x04

#define RED_IPB_LED_PIN 0x20
#define PORTD_OUTPUT_PINS (0x01 | 0x02 | 0x04 | 0x08 | 0x10 | RED_IPB_LED_PIN)

#define BUILTIN_RED_LED_PIN 0x02 // PORTE
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
    TRISC &= ~(SQ_YELLOW_LED_PIN | SQ_RED_LED_PIN);
    
    // Port D
    // RD0: shift register data feed
    // RD1: shift register clock
    // RD2: shift register latch clock
    // RD3: shift register output enable
    // RD4: shift register reset
    // RD5: red LED (inside illuminated pushbutton)
    TRISD &= ~PORTD_OUTPUT_PINS;
    
    // Port E
    // RE0: (not connected)
    // RE1: on-board LED
    // RE2: on-board button (w/ external pull-up to +5V)
    TRISE &= ~BUILTIN_RED_LED_PIN;
}

// If blue buttons pressed, light up square red LED
// If green button pressed, light up square yellow LED
// If key is on, light up internal (round red) LED
void debug_inputs_with_internal_leds() {
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

    if (KEY_ON) {
        PORTE |= BUILTIN_RED_LED_PIN;
    } else {
        PORTE &= ~BUILTIN_RED_LED_PIN;
    }
}

void init_shift_register() {
    sr_output_enable(0);
    sr_reset();
    sr_write(0);
    sr_output_enable(1);
}

void led_test() {
    for (int i=0; i < SR_LENGTH; i++) {
        sr_output_enable(0);
        sr_write(1 << i);
        sr_output_enable(1);
        dumb_delay(50*1000UL);
    }
    sr_output_enable(0);
    sr_write(0);
    sr_output_enable(1);
    dumb_delay(50*1000UL);

    PORTD |= RED_IPB_LED_PIN;
    dumb_delay(50*1000UL);

    PORTD &= ~RED_IPB_LED_PIN;
}

void run_main_game(struct buttons buttons, uint24_t *count) {
    if (buttons.green) {
        if (buttons.key) {
            *count >>= 1; // shift LEDs away from center
        } else {
            *count <<= 1; // shift LEDs toward center
        }
    }
    if (buttons.blue) {
        *count ^= 0x0001; // toggle blue LED (outermost)
    }
    if (buttons.red) {
        *count ^= 0x20000; // toggle red-IBP LED (center)
    }

    sr_output_enable(0);
    sr_write(*count);
    sr_output_enable(1);
    // Treat red LED (inside IPB) as 18th bit
    if ((*count & 0x20000) == 0) {
        PORTD &= ~RED_IPB_LED_PIN;
    } else {
        PORTD |= RED_IPB_LED_PIN;
    }
}

void read_buttons(struct buttons *buttons) {
    buttons->red = RED_PRESSED ? 1 : 0;
    buttons->green = GREEN_PRESSED ? 1 : 0;
    buttons->blue = BLUE_PRESSED ? 1 : 0;
    buttons->key = KEY_ON ? 1 : 0;
}

void wait_and_watch_buttons(struct buttons *buttons, unsigned long delay_cycles) {
    // * loop until we've gone around `delay_cycles` times
    // * each time around check the buttons and increment to either a "pressed" or "not pressed" counter
    // * when done, see which counter for each button is higher and consider that one to be the "answer"
    struct buttons reading;
    unsigned long threshold = delay_cycles / 2;
    uint32_t red = 0, blue = 0, green = 0, key = 0;
    while (delay_cycles > 0) {
        read_buttons(&reading);
        if (reading.red) {
            red++;
        }
        if (reading.blue) {
            blue++;
        }
        if (reading.green) {
            green++;
        }
        if (reading.key) {
            key++;
        }
        delay_cycles--;
    }
    buttons->red = red >= threshold ? 1 : 0;
    buttons->green = green >= threshold ? 1 : 0;
    buttons->blue = blue >= threshold ? 1 : 0;
    buttons->key = key >= threshold ? 1 : 0;
}

#pragma config OSC = HSPLL // || HS || XT
#pragma config WDT = OFF
void main(void) {
    uint24_t count = 0;
    struct buttons buttons = { 0 };

    init_gpio();
    //init_shift_register();
    led_test();

    unsigned char state = 0x00;
    while (1) {
        debug_inputs_with_internal_leds();
        wait_and_watch_buttons(&buttons, 20*1000UL);
        run_main_game(buttons, &count);
    }
}


