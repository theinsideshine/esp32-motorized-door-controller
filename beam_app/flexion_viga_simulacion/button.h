/**
 * File:   Clase que controla los finales de carrera
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
#ifndef BUTTON_H
#define BUTTON_H

#include "Arduino.h"
#include "timer.h"
#include "precompilation.h"

#ifdef TEST_PROTOTIPE 

#define PIN_LIMIT_M1                  PIN_LIMIT_M2       // Pin del final de carrera del motor 1.
#define PIN_LIMIT_M2                  22                // Pin del final de carrera del motor 2.

#else 

#define PIN_LIMIT_M1                  52       // Pin del final de carrera del motor 1.
#define PIN_LIMIT_M2                  22       // Pin del final de carrera del motor 2.

#endif // TEST_PROTOTIPE

#ifdef TEST_PROTOTIPE

#define TIME_DEBOUNCE                 500       // Tiempo de espera en ms.

#else 

#define TIME_DEBOUNCE                 100       // Tiempo de espera en ms.

#endif 

class CButton
{
  public:
    CButton();
    void init( void );
    
    bool is_button_m2_low ( void );
    bool is_button_m1_low ( void );
    
  private:
     
};

#endif // BUTTON_H
