/*
 * File:    File:   Encapsula el control de los motores paso a paso .
 *
 * - Compiler:           Arduino 1.8.13
 * - Supported devices:  Mega
 *
 * \author               MV: luis.villacorta@alumnos.udemm.edu.ar
 *                       LC: leandro.cintioli@alumnos.udemm.edu.ar
 *                       PT: pablo.tavolaro@alumnos.udemm.edu.ar
 *
 * Date:  17-04-2021
 *
 *      Universidad de la Marina Mercante.
*/


  /*
   * TODO: Al definir metrodos unificar la generacion del pulso  en un metodo.
   */

// El motor nema 23HS5628 es de 2.8 A rms 
 
// Tb6600 Investigacion sujeta a corrobar por metodo empirico 

// Microstep config                   S1  S2  S3
//                                    OFF OFF OFF   32 Micro step 6400 pulso/rev (uso de Egresado mecanica)
// Microstep config                   S1  S2  S3 
//                                    ON  ON  OFF   1  Micro step 200  pulso/rev

// Currente Pk                        S4  S5  S6   
//                                    ON  ON  OFF  1.7A peak (uso de Egresado mecanica)
//                                    OFF OFF  OFF  4.0A peak 
 
#include "motor.h"

CMotor::CMotor()
{
   counter_m2_step = 0;
}
/*
 * Inicializa los pines pul y dir de los dos motores, el pin enable se maneja directamente por hardware pora panico. 
 */


bool CMotor::init( void )
{
    
    pinMode(PIN_DIR_M1, OUTPUT);
    pinMode(PIN_PUL_M1, OUTPUT);   
    pinMode(PIN_DIR_M2, OUTPUT);
    pinMode(PIN_PUL_M2, OUTPUT);
   
    
    digitalWrite(PIN_DIR_M1, HIGH);
    digitalWrite(PIN_PUL_M1, HIGH);     // Fija condiciones inicial de led indicador alarm, High= Apagado  

     digitalWrite(PIN_DIR_M2, HIGH);
    digitalWrite(PIN_PUL_M2, HIGH);     // Fija condiciones inicial de led indicador alarm, High= Apagado

    
}
/*
 *  Mueve la cantidad de passos solicitado en la direccion pedida
 *  De hoja de datos
 *  Tiempo entre  Ena y Dir t1 > 5 us 
 *  Tiempo entre  Dir y Pul t2 > 5 us 
 *  Tiempo min del pulso en alto/bajo t3=t4 >2.5 us  T=5 us F = 200KHz
 *  
 *  uint32_t pul  cantidad de pulsos
 *  uint8_t dir  CW horario CCW anti horario
 *  uint8_t mtr  M1 motor 1 M2 motor2
 */
void CMotor::step_mtr( uint32_t pul ,uint8_t dir ,uint8_t mtr )
{
  uint32_t i;
  uint8_t pin_pul_mtr ,pin_dir_mtr ;

  if ( mtr == M1 ) {
    
      pin_pul_mtr = PIN_PUL_M1;
      pin_dir_mtr = PIN_DIR_M1;
      
  }else if ( mtr == M2 ) {
    
            pin_pul_mtr = PIN_PUL_M2;
            pin_dir_mtr = PIN_DIR_M2;
  }
  
  // Setea el sentido de giro anti-horario, las bobinas deber estar conectadas segun .sch
  
  if ( dir == CCW ){
    
      digitalWrite(pin_dir_mtr, LOW);
    
  }else if ( dir == CW ){
    
            digitalWrite(pin_dir_mtr, HIGH);
  }
  
  delayMicroseconds(T_DIR_PUL);   // Espera t2 > 5 us.
  
   for ( i = 0; i < pul ; i++) {  

    // Un pulso para un paso.    
    digitalWrite(pin_pul_mtr, HIGH);
    delayMicroseconds(TON_PULSE);
    digitalWrite(pin_pul_mtr, LOW);
    delayMicroseconds(TON_PULSE);
  }
 
}

void CMotor::step_m1_fwd( uint32_t pul ){
  step_mtr( pul ,CW ,M1 );  
}

void CMotor::step_m1_rwd( uint32_t pul ){
  step_mtr( pul ,CCW, M1 );  
}

void CMotor::step_m2_down( uint32_t pul ){

#ifdef TEST_PROTOTIPE  

  step_mtr( pul ,CW ,M2 );  

#else 

 step_mtr( pul ,CCW ,M2 );  
 
#endif  TEST_PROTOTIPE

}

void CMotor::step_m2_up( uint32_t pul ){

#ifdef TEST_PROTOTIPE  

  step_mtr( pul ,CCW ,M2 );  

#else 

  step_mtr( pul ,CW, M2 );
  
#endif TEST_PROTOTIPE
}

void CMotor::rst_counter_m2( void ){
    counter_m2_step = 0;
}
    
    
uint32_t CMotor::get_counter_m2( void ){

  return(counter_m2_step);
 
}

float CMotor::calculate_flexion( float step_k ){

  float ret_val = counter_m2_step * step_k ;
  return(ret_val);
 
}

 void CMotor::inc_counter_m2( uint32_t value ){
  counter_m2_step  = counter_m2_step + value ;
 }


/*   
 *    Antes de aplicar la fuerza se mueve un offset para esta mas cerca de la viga
 */

 void CMotor::m2_offset_home(  uint32_t offset ){

   
     step_m2_down( offset );
   
  
  
 }

/*
 * Mueve cantidad de milimetro solicitado hacia atras del motor 1 
 */
 
   
void CMotor::rwd_m1( uint32_t distance ){
 uint32_t  count_cal;    

  count_cal = STEP_PER_MM_M1 * distance ;  
  
  step_m1_rwd( count_cal );
   
 
}

/*
 * Mueve cantidad de milimetro solicitado hacia adelante del motor 1 
 *  Si los pasos son 6400 por vueltas y el carro puede moverse 500mm  i= 6400 * 500 no puede ser uint_16t
 */


void CMotor::fwd_m1( uint32_t distance ){
  uint32_t  count_cal;    
  
  
  count_cal = STEP_PER_MM_M1 * distance ;  
 
  
  step_m1_fwd( count_cal );
   
}

/*
 * EL recorrido util del motor correcponde desde 0 a 295 (M1_DISTANCE_MAX) mm
 * la distancia de la viga correpondiente al recorrido util de motor es de 205(M1_OFFSET) mm a 500(BEEM_DISTANCE_MAX) mm 
 * se pone dos trampas una por max y otra por minimo
 * si esta dentro de la trampa se hace la conversion  ret_val = distance - M1_OFFSET;
 * 
 */

uint32_t CMotor::m1_convertion_distance( uint32_t distance ){
uint32_t ret_val = 0 ;

  if (distance <= M1_OFFSET) {
    ret_val = 0;
  }else if ( distance > BEEM_DISTANCE_MAX) {
    ret_val = M1_DISTANCE_MAX;
  }else {
    ret_val = distance - M1_OFFSET;
  }
#ifdef MOTOR_DEBUG
  Serial.print("distancia a moverse: "); 
  Serial.println(ret_val); 
#endif 
  return (ret_val);
}

/*
 * Mueve para arriba el motor 2 1m y sale 
 * Reemplazada por metordos pwm, para buscar el home
 */
   
void CMotor::up_m2( uint32_t distance ){
  uint32_t  count_cal;    

  count_cal = STEP_PER_MM_M2 * distance ;  
  
  step_m2_up( count_cal );
}

/*
 * Mueve cantidad de milimetro solicitado hace abajo  del motor 2
 */

void CMotor::down_m2(  uint32_t distance ){

 uint32_t  count_cal;    

  count_cal = STEP_PER_MM_M2 * distance ;  
  
  step_m2_down( count_cal );
  
}

 void CMotor::pwm_on_m1( void ){
    
   digitalWrite(PIN_DIR_M1, LOW);
   
   //Prende el pwm.
   analogWrite(PIN_PUL_M1,PWM_ON);
  
 }

  void CMotor::pwm_off_m1( void ){

    //Apaga el pwm.
    analogWrite(PIN_PUL_M1,PWM_OFF);
  
 }

 void CMotor::pwm_on_m2( void ){

#ifdef TEST_PROTOTIPE  
    
    digitalWrite(PIN_DIR_M2, LOW); 

#else    

   digitalWrite(PIN_DIR_M2, HIGH);

#endif // TEST_PROTOTIPE


   //Prende el pwm.
   analogWrite(PIN_PUL_M2,PWM_ON);
  
 }

  void CMotor::pwm_off_m2( void ){

    //Apaga el pwm.
    analogWrite(PIN_PUL_M2,PWM_OFF);
  
 }

 
 
