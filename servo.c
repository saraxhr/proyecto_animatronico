#include <avr/io.h>
#include <avr/interrupt.h>
#include "servo.h"

#define SERVO1 PD3
#define SERVO_COUNT 4
#define SERVO_SPACING_TICKS 10000
#define SERVO_MIN_TICKS 1000
#define SERVO_MAX_TICKS 4000

static volatile uint16_t servo_ticks[SERVO_COUNT] = {1500, 1500, 1500, 1500};
static volatile uint8_t current_servo = 0;
static volatile uint8_t phase = 0;

void servo_init(void) {
	DDRD |= (1 << SERVO1) | (1 << (SERVO1+1)) | (1 << (SERVO1+2)) | (1 << (SERVO1+3));

	TCCR1B |= (1 << WGM12);
	TCCR1B |= (1 << CS11); // Prescaler 8  2MHz
	OCR1A = SERVO_SPACING_TICKS;
	TIMSK1 |= (1 << OCIE1A);
}

void servo_set_position(uint8_t index, uint16_t ticks) {
	if (index >= SERVO_COUNT) return;
	if (ticks < SERVO_MIN_TICKS) ticks = SERVO_MIN_TICKS;
	if (ticks > SERVO_MAX_TICKS) ticks = SERVO_MAX_TICKS;
	servo_ticks[index] = ticks;
}

ISR(TIMER1_COMPA_vect) {
	if (phase == 0) {
		PORTD |= (1 << (SERVO1 + current_servo));
		OCR1A = TCNT1 + servo_ticks[current_servo];
		phase = 1;
		} else {
		PORTD &= ~(1 << (SERVO1 + current_servo));
		OCR1A = TCNT1 + SERVO_SPACING_TICKS;
		current_servo = (current_servo + 1) % SERVO_COUNT;
		phase = 0;
	}
}