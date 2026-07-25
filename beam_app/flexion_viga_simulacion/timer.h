/**
 * File:   Clase de timers por software basados en la funcion millis()
 *         de arduino que al estar basada en un timer de la cpu por irq,
           no bloquea la ejecucion. 
 *
 * - Compiler:           Arduino 1.8.13
 * - Supported devices:  Mega
 *
 * \author               MV: luis.villacorta@alumnos.udemm.edu.ar
 *                       LC: leandro.cintioli@alumnos.udemm.edu.ar
 *                       PT: pablo.tavolaro@alumnos.udemm.edu.ar
 *
 * Date:  21-04-2021
 *
 *      Universidad de la Marina Mercante.
 */
 
#ifndef TIMER_H
#define TIMER_H

#include "Arduino.h"

class CTimer
{
  public:
    CTimer();
    void start();
    bool expired(uint32_t);

  private:
      uint32_t timer;
};

#endif // TIMER_H
