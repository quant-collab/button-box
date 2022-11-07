/*
 * File:   main.c
 *
 * Created on August 16, 2010, 12:09 PM
 */

#include <stdint.h>
#include "p18cxxx.h"

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

// This turns out to be about a minute
#define IDLE_COUNTDOWN 72000
static uint32_t idle_time = IDLE_COUNTDOWN;

#define DEBOUNCE_COUNTDOWN 64
#define HOLD_COUNTDOWN 255
struct button {
    uint8_t debounce;
    uint8_t hold;
    unsigned int current:1;
    unsigned int last:1;
    unsigned int rising_edge:1; // keydown
    unsigned int falling_edge:1; // keyup
};

static inline
void debounce_button(struct button *b) {
    char trigger = 0;
    if (b->current != b->last) {
        b->debounce = DEBOUNCE_COUNTDOWN;
        b->last = b->current;
    } else if (b->debounce > 0) {
        b->debounce--;
        if (b->debounce == 0) {
            trigger = 1;
        }
    } else {
      if (b->hold > 0) {
        b->hold--;
        if (b->hold == 0) {
            trigger = 1;
        }
      }
    }
    if (trigger) {
        if (b->current) {
            b->rising_edge = 1;
        } else {
            b->falling_edge = 1;
        }
        b->hold = HOLD_COUNTDOWN;
    }
}

struct buttons {
    struct button red;
    struct button green;
    struct button blue;
    struct button key;
};


static inline
void read_buttons(struct buttons *buttons) {
    buttons->red.current = RED_PRESSED ? 1 : 0;
    buttons->green.current = GREEN_PRESSED ? 1 : 0;
    buttons->blue.current = BLUE_PRESSED ? 1 : 0;
    buttons->key.current = KEY_ON ? 1 : 0;
}


static inline
void debounce_buttons(struct buttons *buttons) {
    debounce_button(&buttons->red);
    debounce_button(&buttons->blue);
    debounce_button(&buttons->green);
    debounce_button(&buttons->key);
}


static inline
void dumb_delay(uint32_t limit) {
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
void sr_write(uint32_t bits) {
    for (int i = SR_LENGTH - 1; i >= 0; i--) {
        sr_data((bits >> i) & 1);
        sr_data_clock();
        //sr_latch_clock();
    }
    sr_latch_clock();
}

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

void run_main_game(struct buttons *buttons, uint24_t *count) {
    uint8_t is_active = 0;
    if (buttons->green.rising_edge == 1) {
        is_active = 1;
        buttons->green.rising_edge = 0;
        if (buttons->key.last) {
            *count >>= 1; // shift LEDs away from center
        } else {
            *count <<= 1; // shift LEDs toward center
        }
    }
    if (buttons->blue.rising_edge == 1) {
        is_active = 1;
        buttons->blue.rising_edge = 0;
        *count ^= 0x0001; // toggle blue LED (outermost)
    }
    if (buttons->red.rising_edge == 1) {
        is_active = 1;
        buttons->red.rising_edge = 0;
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

    if (is_active) {
        idle_time = IDLE_COUNTDOWN;
    }
}

void power_down_if_idle() {
    if (idle_time > 0) {
        idle_time--;
        if (idle_time == 0) {
            // DEBUG: "going to sleep now!)
            //sr_output_enable(0);
            //sr_write(0xAAAAAAAA);
            //sr_output_enable(1);
            //dumb_delay(500UL*1000UL);

            // Turn off main LEDs bye disabling output on shift register & turning off IPB
            sr_output_enable(0);
            PORTD &= ~RED_IPB_LED_PIN;
            // Turn off "debug LEDs"
            PORTC &= ~SQ_RED_LED_PIN;
            PORTC &= ~SQ_YELLOW_LED_PIN;
            PORTE &= ~BUILTIN_RED_LED_PIN;
            asm("sleep");
            sr_output_enable(1);
            idle_time = IDLE_COUNTDOWN;

            // DEBUG: "just woke up!)
            //sr_output_enable(0);
            //sr_write(0x55555555);
            //sr_output_enable(1);
            //dumb_delay(500UL*1000UL);
        }
    }
}



#pragma config OSC = HSPLL // | HS | XT | INTIO1 | INTIO2 ... (see p. 20 of datasheet)
#pragma config WDT = OFF
void main(void) {
    uint24_t count = 0;
    struct buttons buttons = { 0 }; // TODO: initialize less sloppily

    init_gpio();
    //init_shift_register();
    led_test();
    OSCCONbits.IDLEN = 0; // Device enters sleep mode on SLEEP instruction (vs. idle)
    //INTCONbits.RBIE = 1; // Interrupt on PORTB pin level changes (button press/release)
    INTCON3bits.INT1IE = 1; // Interrupt on INT1 pin (RB1 = green button)
    INTCON3bits.INT2IE = 1; // Interrupt on INT2 pin (RB2 = blue button)
    INTCONbits.GIE = 1;

    while (1) {
        read_buttons(&buttons);
        debug_inputs_with_internal_leds();
        debounce_buttons(&buttons);
        run_main_game(&buttons, &count);
        power_down_if_idle();
    }
}


void __interrupt() GlobalInterruptHandler(void) 
{
    static volatile uint8_t dummy;
    if(INTCONbits.RBIF == 1)
    {
        // PORTB change...
        dummy = PORTB;
        INTCONbits.RBIF = 0;
        //idle_time = IDLE_COUNTDOWN;
        //sr_output_enable(0);
        //sr_write(0xFFFFFFFF);
        //sr_output_enable(1);
    }
    if(INTCON3bits.INT1IF == 1)
    {
        // INT1 (RB1) interrupt
        INTCON3bits.INT1IF = 0;
        //idle_time = IDLE_COUNTDOWN;
        //sr_output_enable(1);
    }
    if(INTCON3bits.INT2IF == 1)
    {
        // INT2 (RB2) interrupt
        INTCON3bits.INT2IF = 0;
        //idle_time = IDLE_COUNTDOWN;
        //sr_output_enable(1);
    }
}

