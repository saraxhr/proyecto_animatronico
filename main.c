#define F_CPU 16000000UL
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <avr/eeprom.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "servo.h"

#define LED_MANUAL PB4
#define LED_EEPROM PB3
#define LED_UART   PB2

#define BTN_MODE PD7
#define BTN_SAVE PB0
#define BTN_SLOT PD2

#define SERVO_MIN_TICKS 1000
#define SERVO_MAX_TICKS 4000
#define SMOOTHING_ALPHA 0.1f

volatile uint8_t modo = 0;
volatile uint8_t last_mode_button_state = 1;
float smoothed_adc[4] = {0, 0, 0, 0};
uint8_t posicion_actual = 0;
uint8_t servo_angles[4] = {90, 90, 90, 90};
char ultima_pos[64] = "";

void ADC_init(void) {
	ADMUX = (1 << REFS0);
	ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
}

uint16_t ADC_read(uint8_t channel) {
	ADMUX = (ADMUX & 0xF0) | (channel & 0x0F);
	ADCSRA |= (1 << ADSC);
	while (ADCSRA & (1 << ADSC));
	return ADC;
}

uint16_t direccion_eeprom(uint8_t posicion, uint8_t servo) {
	return (posicion * 8) + (servo * 2);
}

void parpadear_led(uint8_t veces, uint8_t led_pin) {
	for (uint8_t i = 0; i < veces; i++) {
		PORTB &= ~(1 << led_pin);
		_delay_ms(200);
		PORTB |= (1 << led_pin);
		_delay_ms(200);
	}
	if ((modo == 0 && led_pin == LED_MANUAL) ||
	(modo == 1 && led_pin == LED_EEPROM) ||
	(modo == 2 && led_pin == LED_UART)) {
		PORTB |= (1 << led_pin);
	}
}

void uart_init(void) {
	uint16_t ubrr = F_CPU / 16 / 9600 - 1;
	UBRR0H = (ubrr >> 8);
	UBRR0L = ubrr;
	UCSR0B = (1 << RXEN0) | (1 << TXEN0);
	UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

char uart_receive(void) {
	while (!(UCSR0A & (1 << RXC0)));
	return UDR0;
}

void uart_transmit(char data) {
	while (!(UCSR0A & (1 << UDRE0)));
	UDR0 = data;
}

void uart_send_string(const char *s) {
	while (*s) uart_transmit(*s++);
}

void procesar_comando_uart(char *comando) {
	uart_send_string("Recibido: ");
	uart_send_string(comando);
	uart_send_string("\n");

	char *token = strtok(comando, ";");
	while (token != NULL) {
		if (strncmp(token, "servo", 5) == 0) {
			uint8_t s = token[5] - '1';
			char *v = strchr(token, ':');
			if (v && s < 4) {
				int angle = atoi(v + 1);
				if (angle >= 0 && angle <= 180) {
					servo_angles[s] = angle;
					uint16_t ticks = SERVO_MIN_TICKS + ((uint32_t)angle * (SERVO_MAX_TICKS - SERVO_MIN_TICKS)) / 180;
					servo_set_position(s, ticks);
				}
			}
		}
		token = strtok(NULL, ";");
	}
}

void enviar_estado_uart(void) {
	char buffer[64];
	sprintf(buffer, "p1:%d,p2:%d,p3:%d,p4:%d\n",
	servo_angles[0], servo_angles[1],
	servo_angles[2], servo_angles[3]);
	if (strcmp(buffer, ultima_pos) != 0) {
		uart_send_string(buffer);
		strcpy(ultima_pos, buffer);
	}
}

int main(void) {
	DDRC = 0x00;
	DDRB |= (1 << LED_MANUAL) | (1 << LED_EEPROM) | (1 << LED_UART);
	DDRD &= ~((1 << BTN_MODE) | (1 << BTN_SLOT));
	DDRB &= ~(1 << BTN_SAVE);
	PORTD |= (1 << BTN_MODE) | (1 << BTN_SLOT);
	PORTB |= (1 << BTN_SAVE);

	ADC_init();
	servo_init();
	uart_init();
	sei();

	PORTB |= (1 << LED_MANUAL);
	PORTB &= ~((1 << LED_EEPROM) | (1 << LED_UART));

	uint8_t modo_anterior = 255;
	char buffer[64];
	uint8_t buf_index = 0;
	uint16_t contador_uart = 0;

	while (1) {
		uint8_t mode_button_state = (PIND & (1 << BTN_MODE));
		if (!mode_button_state && last_mode_button_state) {
			_delay_ms(50);
			if (!(PIND & (1 << BTN_MODE))) {
				modo = (modo + 1) % 3;
				PORTB &= ~((1 << LED_MANUAL) | (1 << LED_EEPROM) | (1 << LED_UART));
				if (modo == 0) PORTB |= (1 << LED_MANUAL);
				else if (modo == 1) PORTB |= (1 << LED_EEPROM);
				else if (modo == 2) PORTB |= (1 << LED_UART);
			}
		}
		last_mode_button_state = mode_button_state;

		if (modo == 0) {
			for (uint8_t i = 0; i < 4; i++) {
				uint16_t raw = ADC_read(i);
				smoothed_adc[i] = (1 - SMOOTHING_ALPHA) * smoothed_adc[i] + SMOOTHING_ALPHA * raw;
				uint16_t val = (uint16_t)smoothed_adc[i];
				uint16_t ticks = SERVO_MIN_TICKS + ((uint32_t)val * (SERVO_MAX_TICKS - SERVO_MIN_TICKS)) / 1023;
				servo_set_position(i, ticks);
				servo_angles[i] = (val * 180) / 1023;
			}
			if (!(PINB & (1 << BTN_SAVE))) {
				_delay_ms(50);
				if (!(PINB & (1 << BTN_SAVE))) {
					for (uint8_t i = 0; i < 4; i++) {
						uint16_t raw = ADC_read(i);
						uint16_t ticks = SERVO_MIN_TICKS + ((uint32_t)raw * (SERVO_MAX_TICKS - SERVO_MIN_TICKS)) / 1023;
						uint16_t dir = direccion_eeprom(posicion_actual, i);
						eeprom_update_word((uint16_t*)dir, ticks);
					}
					while (!(PINB & (1 << BTN_SAVE)));
					_delay_ms(100);
				}
			}
		}
		else if (modo == 1) {
			if (modo_anterior != 1) {
				for (uint8_t i = 0; i < 4; i++) {
					uint16_t dir = direccion_eeprom(posicion_actual, i);
					uint16_t ticks = eeprom_read_word((uint16_t*)dir);
					if (ticks < SERVO_MIN_TICKS) ticks = SERVO_MIN_TICKS;
					if (ticks > SERVO_MAX_TICKS) ticks = SERVO_MAX_TICKS;
					servo_set_position(i, ticks);
					servo_angles[i] = ((ticks - SERVO_MIN_TICKS) * 180) / (SERVO_MAX_TICKS - SERVO_MIN_TICKS);
				}
				parpadear_led(posicion_actual + 1, LED_EEPROM);
			}
			if (!(PIND & (1 << BTN_SLOT))) {
				_delay_ms(50);
				if (!(PIND & (1 << BTN_SLOT))) {
					posicion_actual = (posicion_actual + 1) % 4;
					for (uint8_t i = 0; i < 4; i++) {
						uint16_t dir = direccion_eeprom(posicion_actual, i);
						uint16_t ticks = eeprom_read_word((uint16_t*)dir);
						if (ticks < SERVO_MIN_TICKS) ticks = SERVO_MIN_TICKS;
						if (ticks > SERVO_MAX_TICKS) ticks = SERVO_MAX_TICKS;
						servo_set_position(i, ticks);
						servo_angles[i] = ((ticks - SERVO_MIN_TICKS) * 180) / (SERVO_MAX_TICKS - SERVO_MIN_TICKS);
					}
					parpadear_led(posicion_actual + 1, LED_EEPROM);
					while (!(PIND & (1 << BTN_SLOT)));
					_delay_ms(100);
				}
			}
		}
		else if (modo == 2) {
			if (UCSR0A & (1 << RXC0)) {
				char c = uart_receive();
				if (c == '\n') {
					buffer[buf_index] = 0;
					procesar_comando_uart(buffer);
					buf_index = 0;
					} else if (buf_index < sizeof(buffer) - 1) {
					buffer[buf_index++] = c;
				}
			}
			contador_uart++;
			if (contador_uart >= 150) {
				enviar_estado_uart();
				contador_uart = 0;
			}
		}

		modo_anterior = modo;
		_delay_ms(20);
	}
}