
/**
 * File:   Defincion para soporte de precompilacion 
 *         dependiendo del ambiente de uso, prototipo o final 
 *         se seleccion las macros a definir
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


 // pines https://www.arduino.cc/en/Hacking/PinMapping2560
 
#ifndef PRECOMPILATION_H
#define PRECOMPILATION_H

#include "Arduino.h"

//#define TEST_PROTOTIPE  // Banco de prueba prototipo

#ifdef TEST_PROTOTIPE 

#define BUTTON_PRESENT

#define MOTOR_PRESENT
  
#define LED_PRESENT
     
#define CELL_PRESENT    
  

#else                   // Banco de prueba final.

#define BUTTON_PRESENT

#define MOTOR_PRESENT
  
#define LED_PRESENT
     
#define CELL_PRESENT       

#endif //TEST_PROTOTIPE

#endif // PRECOMPILATION_H
