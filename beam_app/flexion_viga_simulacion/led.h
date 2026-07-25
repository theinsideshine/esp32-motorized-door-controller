/**
 * File:   Clase que da soporte a los leds de debug
 *
 * - Compiler:           Arduino 1.8.13
 * - Supported devices:  Mega
 *
 * \author               MV: luis.villacorta@alumnos.udemm.edu.ar
 *                       LC: leandro.cintioli@alumnos.udemm.edu.ar
 *                       PT: pablo.tavolaro@alumnos.udemm.edu.ar
 *
 * Date:  10-05-2021
 *
 *      Universidad de la Marina Mercante.
 */
#ifndef LED_H
#define LED_H

#include "Arduino.h"
#include "timer.h"

#define PIN_LED_DEBUG                 16

class CLed
{
  public:
    CLed();
    void init( void );
    void on(void);
    void off(void);
    void n_blink(uint8_t n_blink,uint8_t t_blink);
        
  private:
      
};

#endif // LED_H
