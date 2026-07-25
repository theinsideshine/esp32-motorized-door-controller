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
 * 
 */

/*
 * Arduino Mega timers 
* El Timer0 controla las salidas PWM 4 y 13.
* El Timer1 controla las salidas PWM 11 y 12.
* El Timer2 controla las salidas PWM 9 y 10.
* El Timer3 controla las salidas PWM 2, 3 y 5.
* El Timer4 controla las salidas PWM 6, 7 y 8.
* El Timer5 controla las salidas PWM 44, 45 y 46
*
* pines disponibles de 4 a 13 y de 44 a 46 
* Todos los piness 490Hz excepto  4 y 13 980Hz 
* 
 */

 
#ifndef MOTOR_H
#define MOTOR_H

#include "Arduino.h"
#include "precompilation.h"
//Avance por vuelta 


#define M6    1    // 1    mm per revolution
#define M5    0.8  // 0,8  mm per revolution
#define M8    1.25 // 1.25 mm per revolution
#define M20   2    // 2    mm per revolution
#define MS   8    // 8    mm per revolution

/*
 *Motor 1 El pin 13 lo usa el mega para el pin de status,se mueve al reset y durante la grabacion, se aconseja no usarlo ,(dir del motor)
 *       para esto hay que precindir del uso del mismo como pwm ,este pin 13 y el pin 4 son los unicos que tienen como frecuencia tiena Fpwm=978Hz.  
 *       Usamos Fpwm=500Hz quedan todos lo otros pwm disponibles, y asi dejamos libre el pin 13
 *       
 */
 
#define PIN_DIR_M1 8              
#define PIN_PUL_M1 12           


#define STEP_PER_REVOLUTION_M1 200

#define STEP_PER_REVOLUTION_M2 200


/*
 * Los Pasos/s ,Rps ,Rpm se calculan en base a la cantidad de pulsos que necesita una vuelta, STEP_PER_REVOLUTION_M1
 * y  TON_PULSE ,se usan para el movimiento de los milimitros requeridos.
 * La busqueda del home la hacemos con pwm de 500 Hz disponibles.
 * 
 */

 
//#define TON_PULSE         1500   // Tiempo en microsegundo on para generacion de pulso de paso F = 333.3Hz   
#define TON_PULSE         1000   // Tiempo en microsegundo on para generacion de pulso de paso F = 500  Hz   2.5 Rps 150 Rpm
//#define TON_PULSE          500   // Tiempo en microsegundo on para generacion de pulso de paso F = 1000 Hz     
//#define TON_PULSE          250   // Tiempo en microsegundo on para generacion de pulso de paso F = 2000 Hz    


 //  De hoja de datos.
 //  Tiempo entre  Ena y Dir t1 > 5 us. 
 //  Tiempo entre  Dir y Pul t2 > 5 us.
 //  Tiempo min del pulso en alto/bajo t3=t4 >2.5 us  T=5 us F = 200KHz.


#define T_EN_DIR       15    // Tiempo entre  Ena y Dir t1 > 5 us. No se usa. 
#define T_DIR_PUL      15    // Tiempo entre  Dir y Pul t2 > 5 us 

#define PWM_ON             50     // Valor del ciclo de servicio para prender el pwm.
#define PWM_OFF            0      // Valor del ciclo de servicio para apagar  el pwm.




/*
 *       M20 mm ------------------------- STEP_PER_REVOLUTION 
 *       1   mm ------------------------- STEP_PER_REVOLUTION / 20   = 200 /2 =100 STEP POR mm
 *       
 *       1cm   10 mm
 *       50CM  500 mm entonces el parametro distancia debe ser un uint16_t
 *       
 *                      */

//#define STEP_PER_MM_M1     STEP_PER_REVOLUTION_M1/M20     
//#define STEP_PER_MM_M2     STEP_PER_REVOLUTION_M2/M20     


/*
 *Motor2.
 */

#ifdef TEST_PROTOTIPE 

#define PIN_DIR_M2 PIN_DIR_M1
#define PIN_PUL_M2 PIN_PUL_M1

#define STEP_PER_MM_M1     STEP_PER_REVOLUTION_M1/MS     
#define STEP_PER_MM_M2     STEP_PER_REVOLUTION_M2/MS  

#else   

#define PIN_DIR_M2 7
#define PIN_PUL_M2 11 

#define STEP_PER_MM_M1     STEP_PER_REVOLUTION_M1/M20     
#define STEP_PER_MM_M2     STEP_PER_REVOLUTION_M2/M6  

#endif // TEST_PROTOTIPE


#define CW   0 
#define CCW  1 
#define M1   2
#define M2   3

/*
 * El recorrido util de barra es de 205 a 294 mm ,entonces recorrido util : 294-205 = 291 mm = 29.1cm
 * offset 205 
 * distancia_max = 500 
 * Si distancia es < offset        entonces distancia = offset ; No se mueve. 
 * Si distancia es > distancia_max entonces distancia = 500    ; Se mueve hasta 500. 
 * distancia = distancia - offset  
 * 
 */

#define M1_OFFSET          206   // 205 mm offset del motor  con respecto al recorrido util.
#define M1_DISTANCE_MAX    294   // Maximo de recorrido swl motor.
#define BEEM_DISTANCE_MAX    500   // 500 mm maximo de recorrido de la viga.


#define M2_DOWN_FORCE_STEP   STEP_PER_REVOLUTION_M2/20  // En base #define STEP_PER_MM_M2     STEP_PER_REVOLUTION_M2/M6 
                                                       // se hace la cuenta  = 200 step/4 =50 M6/4 = 1mm/4 =0.25mm.
                                                       // Cantidad de pasos que baja cuando esta buscando la fuerza.

#define M2_HOME_OFFSET       STEP_PER_REVOLUTION_M2*4    // Mueve dos vueltas el Motor 2. 
#define M2_HOME_OFFSET_CERO  0                           // No mueve el Motor 2.         
//#define MOTOR_DEBUG                                      // Muestra informacion del motor por puerto serie.

 

class CMotor
{
  public:
    CMotor(); 
    bool CMotor::init( void );
    void CMotor::fwd_m1( uint32_t distance ); 
    uint32_t CMotor::m1_convertion_distance( uint32_t distance );
    void CMotor::rwd_m1( uint32_t distance );       
    void CMotor::up_m2 ( uint32_t distance );
    void CMotor::down_m2 ( uint32_t distance ) ;
    
    void CMotor::pwm_on_m1( void ); 
    void CMotor::pwm_off_m1( void );
    void CMotor::pwm_on_m2( void ); 
    void CMotor::pwm_off_m2( void );   
    void CMotor::step_m1_fwd( uint32_t pul );
    void CMotor::step_m1_rwd( uint32_t pul ); 
    void CMotor::step_m2_down( uint32_t pul );
    void CMotor::step_m2_up( uint32_t pul );

    void CMotor::rst_counter_m2( void );
    uint32_t CMotor::get_counter_m2( void );
    void CMotor::inc_counter_m2( uint32_t value );
    float CMotor::calculate_flexion( float step_k );
    
    void CMotor::m2_offset_home( uint32_t offset );
    
  private:
  uint32_t counter_m2_step;
  
  void CMotor::step_mtr( uint32_t pul ,uint8_t dir ,uint8_t mtr);
};

#endif // MOTOR_H
