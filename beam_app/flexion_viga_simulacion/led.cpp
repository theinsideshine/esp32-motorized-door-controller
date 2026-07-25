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
 
#include "led.h"

CLed::CLed()
{

}

void CLed::init(void){
  pinMode(PIN_LED_DEBUG, OUTPUT);
  pinMode(PIN_LED_DEBUG, HIGH); //Apaga led
}

void CLed::on(void){

   digitalWrite(PIN_LED_DEBUG, LOW); 
 
}

void CLed::off(void){

   digitalWrite(PIN_LED_DEBUG, HIGH);  
}
/*
 * n_blink numero de blinkeos
 * t_blink tiempo de blinkeos en ms
 */
 
void CLed::n_blink(uint8_t n_blink,uint8_t t_blink){

  for (uint8_t i=0; i<n_blink; i++){    
   digitalWrite(PIN_LED_DEBUG, LOW); 
   delay(t_blink);
   digitalWrite(PIN_LED_DEBUG, HIGH); 
   delay(t_blink); 
  }
}
