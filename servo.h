#ifndef SERVO_H
#define SERVO_H

#include <stdint.h>

// Inicializa Timer1 y pines de servos
void servo_init(void);

// Asigna posición del servo i (0–3) con ancho de pulso en ticks (1000–4000)
void servo_set_position(uint8_t index, uint16_t ticks);

#endif