#include <Arduino.h>
#include <avr/interrupt.h>

#include "button.hpp"
#include "pinout.hpp"

volatile static bool button_pressed = false;
volatile static bool button_released = false;
volatile static unsigned long last_btn_check = 0;

ISR(INT0_vect) {
	unsigned long now = millis();

	if (now - last_btn_check < DEBOUNCE_TIME)
		return;

	last_btn_check = now;

	if (!(PIND & (1 << FREQ_SEL_BTN)))
		button_pressed = true;
	else 
		button_released = true;
}

enum button_state handle_button_input(void) {
	static unsigned long press_start = millis();

    if (button_pressed) {
        button_pressed = false;
		press_start = millis();
    }

	if (button_released) {
		button_released = false;
		unsigned long press_duration = millis() - press_start;

		if (press_duration < DEBOUNCE_TIME)
			return NOP;
		
		if (press_duration < SHORT_PRESS_TRESHOLD)
			return PRESS;
		return HOLD;
	}

	return NOP;
}