/**
 * File:   Clase que controla los finales de carrera
 *         Da soporte a la lectura de los finales de carrera 
 *          
 *         
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
 
#include "button.h"

CButton::CButton()
{

}

void CButton::init( void )
{
    pinMode( PIN_LIMIT_M1, INPUT_PULLUP );
    pinMode( PIN_LIMIT_M2, INPUT_PULLUP );   
    
}


// Devuelve el valor del true si el  del boton 2 esta en LOW

bool CButton::is_button_m2_low ( void ){

 return (digitalRead( PIN_LIMIT_M2 ) == LOW) ;
  
}

// Devuelve el valor del true si el  del boton 1 esta en LOW

bool CButton::is_button_m1_low ( void ){

  return (digitalRead( PIN_LIMIT_M1 ) == LOW) ;
  
}
