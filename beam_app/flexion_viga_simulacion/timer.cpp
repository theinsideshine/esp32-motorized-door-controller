/**
 * File:   Clase de timers por software basados en la funcion millis().
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


 /* La funcion millis y micros usan el timer 0
  *  
  */
  
#include "timer.h"

CTimer::CTimer()
{
    start();
}

void CTimer::start( void )
{
    timer = millis();
}

bool CTimer::expired( uint32_t ms )
{
    return ((millis() - timer) > ms);
}
