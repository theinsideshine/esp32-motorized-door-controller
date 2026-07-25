/*
 * File:   Clase para controlar la confirguracion en la EEPROM.
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
 
#include "cfg.h"
#include "log.h"
#include "motor.h"
#include "cell.h"

#include <EEPROM.h>

/*
 * Para evitar crear estas clases se debe implemetar maquinas de estado que controlen el motor y las celdas en el main
 * asi poder intercactuar con ella mediante la memoria.
 */

CMotor   Motor_host;

CConfig::CConfig()
{
uint8_t magic_number;

    EEPROM.get( EEPROM_ADDRESS_MAGIC_NUMBER, magic_number );

    if( magic_number != MAGIC_NUMBER ){
        magic_number = MAGIC_NUMBER;
        EEPROM.put( EEPROM_ADDRESS_MAGIC_NUMBER, magic_number );

        set_distance( DISTANCE_DEFAULT );
        set_force( FORCE_DEFAULT );
        set_reaction1(REACTION1_DEFAULT );
        set_reaction2(REACTION2_DEFAULT );         
        set_flexion(FLEXION_DEFAULT);
        set_step_cal( STEP_CAL_DEFAULT );
        set_step_k( STEP_K_DEFAULT );        
        
        //set_log_level( LOG_DISABLED );
        set_log_level( LOG_MSG ); // cuando cambias el magic numbre setea los mensajes de logeo (Arduino 1,primer inicio")        
        set_st_test(ST_TEST_DEFAULT);
        set_st_mode(ST_MODE_DEFAULT );

        
    }else {
        EEPROM.get( EEPROM_ADDRESS_DISTANCE, distance );
        EEPROM.get( EEPROM_ADDRESS_FORCE, force);
        EEPROM.get( EEPROM_ADDRESS_REACTION_1, reaction1 );
        EEPROM.get( EEPROM_ADDRESS_REACTION_2, reaction2 );       
        EEPROM.get( EEPROM_ADDRESS_FLEXION, flexion );
        EEPROM.get( EEPROM_ADDRESS_STEP_CAL, step_cal );
        EEPROM.get( EEPROM_ADDRESS_STEP_K, step_k );

        EEPROM.get( EEPROM_ADDRESS_LOG_LEVEL, log_level );
        //EEPROM.get( EEPROM_ADDRESS_ST_MODE, st_mode );
        
       //EEPROM.get( EEPROM_ADDRESS_ST_TEST, st_test );
       //estos parametro inicializa en 0 independiente de como este en la eprrom ya que estos ejecutan el ST_LOOP
       set_st_test(ST_TEST_DEFAULT);
       set_st_mode(ST_MODE_DEFAULT );
       
        
    }
}

uint16_t CConfig::get_distance( void )
{
    return distance;
}

void CConfig::set_distance( uint16_t val )
{
    distance = val;
    EEPROM.put( EEPROM_ADDRESS_DISTANCE, val );
}

float CConfig::get_force( void )
{
    return force;
}

void CConfig::set_force( float val )
{
    force = val;
    EEPROM.put( EEPROM_ADDRESS_FORCE, val );
}

float CConfig::get_reaction1( void )
{
    return reaction1;
}

void CConfig::set_reaction1( float val )
{
   
    reaction1 = val;
    EEPROM.put( EEPROM_ADDRESS_REACTION_1, val );
}

float CConfig::get_reaction2( void )
{
    return reaction2;
}

void CConfig::set_reaction2( float val )
{
    reaction2 = val;
    EEPROM.put( EEPROM_ADDRESS_REACTION_2, val );
}



float CConfig::get_flexion( void )
{
    return flexion;
}

void CConfig::set_flexion( float val )
{
    flexion = val;
    EEPROM.put( EEPROM_ADDRESS_FLEXION, val );
}

uint32_t CConfig::get_step_cal( void )
{
    return step_cal;
}

void CConfig::set_step_cal( uint32_t val )
{
    step_cal = val;
    EEPROM.put( EEPROM_ADDRESS_STEP_CAL, val );
}

float CConfig::get_step_k( void )
{
    return step_k;
}

void CConfig::set_step_k ( float val )
{
    step_k = val;
    EEPROM.put( EEPROM_ADDRESS_STEP_K, val );
}

uint8_t CConfig::get_log_level( void )
{
    return log_level;
}

void CConfig::set_log_level( uint8_t enable )
{
    log_level = enable;
    EEPROM.put( EEPROM_ADDRESS_LOG_LEVEL, log_level );
}
/*
 * Estos deberia ser booleano !!!!!
 * 
 */
 
uint8_t CConfig::get_st_test( void )
{
    return st_test;
}

void CConfig::set_st_test( uint8_t enable )
{
    st_test = enable;
    EEPROM.put( EEPROM_ADDRESS_ST_TEST, st_test );
}

uint8_t CConfig::get_st_mode( void )
{
    return st_mode;
}

void CConfig::set_st_mode( uint8_t mode )
{
    st_mode = mode;
    EEPROM.put( EEPROM_ADDRESS_ST_MODE, st_mode );
}


// Lee por el puerto serie parametros de configuracion en formato json.
// {info:'all-params'}        Envia todos los parametros en formato json.
// {info:'all-calibration'}   Envia todos los parametros en formato json de calibracion de la flexion y el nivel de logeo.
// {info:'version'}           Envia  la version del firmware.
// {info:'status'}            Devuelve el estatus del ensayo.
// {info:'reaction_one'}      Devuelve la reaction1 del ensayo.
// {info:'reaction_two'}      Devuelve la reaction2 del ensayo.
// {info:'flexion'}           Devuelve la flexion del ensayo.
// {info:'st_mode'}           Devuelve el modo del ensayo.
// {info:'step_cal'}          Devuelve la cantidad de pasos contados para la calibracion de step_k.
// {info:'step_k'}            Constante de calibracion de flexion.
// {info:'log_level'}         Nivel de logeo por puerto serie.


// {log_level:'0'}       log_level:0=desactivado,
// {log_level:'1'}                 1=mensajes.
// {log_level:'2'}                 2=info control estandar.
// {log_level:'3'}                 3=info control arduino plotter.

// {cmd:'start'}       Comienza el ensayo.

// {m1_fwd:'50'}           Mueve 50 mm el motor 1 hacia adelante.
// {m1_rwd:'4'}            Mueve 4 mm el motor 1 hacia atras.
// {step_m1_fwd:'200'}     Mueve 200 pasos el motor 1 hacia adelante.
// {step_m1_rwd:'200'}     Mueve 200 pasos el motor 1 hacia atras.


// {m2_up:'5'}              Mueve 5 mm el motor 1 hacia arriba.
// {m2_down:'4'}            Mueve 4 mm el motor 1 hacia abajo.
// {step_m2_up:'200'}       Mueve 200 pasos el motor 2 hacia arriba.
// {step_m2_down:'200'}     Mueve 200 pasos el motor 2 hacia abajo.

 //   {cdd:'start',data:{distance:'20',force:'306'}} 


// {distance:'290'}      distance      Distancia en mm donde se aplica la fuerza.
// {force:'2000'}        force         Fuerza a aplicar en g.
// {reaction_one:'1'}    reaction_one  Fuerza de reaccion uno, en g.
// {reaction_two:'2'}    reaction_two  Fuerza de reaccion dos, en g.
// {flexion:'0.12630'}   flexion       Flexion del ensayo, en cm.
// {st_test:'1'}         st_test       0 ensayo desactivado. 
//                       st_test       1 ensayo activado. 
// {st_mode:'0'}         st_mode       ST_MODE_TEST                    0  ensayo activado.
//                                     ST_MODE_HOME_M2                 1 Va al home del motor 2.
//                                     ST_MODE_CELL                    2 Lee las celdas de carga.
//  {step_cal:'2000'}    step_cal      Devuelve la cantidad de pasos contados para la calibracion de step_k.
//  {step_k:'0.123456'}  step_k        Constante de calibracion de flexion.
                                   

void CConfig::host_cmd( void )
{
bool known_key = false;

    if ( Serial.available() ){
        StaticJsonDocument<512> doc;
        auto error = deserializeJson( doc, Serial );
        if ( !error ) {
          
            if ( doc.containsKey("distance") ) {
                set_distance( doc["distance"] );
                known_key = true;
            }

            if ( doc.containsKey("force") ) {
                float temp = doc["force"];
               
                if (temp > FORCE_MAX) {                   
                  doc["force"]=FORCE_MAX;
                  
                }else if (temp < FORCE_MIN) {                   
                  doc["force"]=FORCE_MIN;
                }
                
                set_force( doc["force"] );
                known_key = true;
            }

            if ( doc.containsKey("reaction_one") ) {
                set_reaction1( doc["reaction_one"] );
                known_key = true;
            }

            if ( doc.containsKey("reaction_two") ) {
                set_reaction2( doc["reaction_two"] );
                known_key = true;
            }

            if ( doc.containsKey("flexion") ) {
                set_flexion( doc["flexion"] );
                known_key = true;
            }

             if ( doc.containsKey("step_cal") ) {
                set_step_cal( doc["step_cal"] );
                known_key = true;
            }

            if ( doc.containsKey("step_k") ) {
                set_step_k( doc["step_k"] );
                known_key = true;
            }
                      
            /*             
             *              
            if ( doc.containsKey("st_test") ) {
                set_st_test( doc["st_test"] );
                known_key = true;
            }
            */ 
            
            if ( doc.containsKey("log_level") ) {
                set_log_level( doc["log_level"] );
                known_key = true;
            }  

             if ( doc.containsKey("st_mode") ) {
                set_st_mode( doc["st_mode"] );
                known_key = true;
            } 
                   

            if ( doc.containsKey("info") ) {
                String key = doc["info"];

                if( key == "all-params" ) {
                    send_all_params( doc );
                }else if( key == "all-calibration" ) {
                    send_all_calibration( doc );
                }else if( key == "version" ) {
                    send_version( doc );
                }else if( key == "status" ) {
                    send_status( doc );
                }else if( key == "reaction_one" ) {
                    send_reaction_one( doc );
                }else if( key == "reaction_two" ) {
                    send_reaction_two( doc );
                }else if( key == "flexion" ) {
                    send_flexion( doc );
                }else if( key == "st_mode" ) {
                    send_st_mode( doc );
                }else if( key == "step_cal" ) {
                    send_step_cal( doc );
                }else if( key == "step_k" ) {
                    send_step_k( doc );
                }else if( key == "log_level" ) {
                    send_log_level( doc );
                }
                
            }

            // Usamos cdd hasta definir que unifiquemos comandos, en realidad es cmd. 
            // Segun el uso https://arduinojson.org/v6/api/jsondocument/containskey/ 
            // para preguntar sobre la clave de un objeto  se usa  const char* key1 =doc["data"]["distance"];

            
            if ( doc.containsKey("cdd") ) { //Verifica "cdd".
                String key = doc["cdd"];
                 if( key == "start" ) {    //Verifica "start".
                     const char* key1 =doc["data"]["distance"];
                     const char* key2 =doc["data"]["force"];
                    if (key1 && key2) {   //Verifica "distance". "force".
                       set_distance( doc["data"]["distance"]);
                       set_force( doc["data"]["force"] );  
                       set_st_test( 1 );  // Comienza el ensayo.                      
                       send_ack( doc );
                     }
                  
                 }
             }           

              // Comandos de movimientos del motor 1.
              
             if ( doc.containsKey("m1_fwd") ) {                                            
                    
                    Motor_host.fwd_m1(doc["m1_fwd"]);                
                    send_ack( doc );                          
            } 
             if ( doc.containsKey("m1_rwd") ) {
                
                    Motor_host.rwd_m1(doc["m1_rwd"]);                
                    send_ack( doc );                          
            } 
             if ( doc.containsKey("step_m1_fwd") ) {
                
                    Motor_host.step_m1_fwd(doc["step_m1_fwd"]);                
                    send_ack( doc );                          
            }
            if ( doc.containsKey("step_m1_rwd") ) {
                
                    Motor_host.step_m1_rwd(doc["step_m1_rwd"]);                
                    send_ack( doc );                          
            }

            // Comandos de movimientos del motor 2.
              
             if ( doc.containsKey("m2_up") ) {                                            
                    
                    Motor_host.up_m2(doc["m2_up"]);                
                    send_ack( doc );                          
            } 
             if ( doc.containsKey("m2_down") ) {
                
                    Motor_host.down_m2(doc["m2_down"]);                
                    send_ack( doc );                          
            } 
             if ( doc.containsKey("step_m2_up") ) {
                
                    Motor_host.step_m2_up(doc["step_m2_up"]);                
                    send_ack( doc );                          
            }
            if ( doc.containsKey("step_m2_down") ) {
                
                    Motor_host.step_m2_down(doc["step_m2_down"]);                
                    send_ack( doc );                          
            }

                     
                      
            if ( doc.containsKey("cmd") ) {
                String key = doc["cmd"];                
                
                if( key == "start" ) {
                    set_st_test( 1 );  // Comienza el ensayo.
                    send_ack( doc );
                }          
            } else if( known_key == true ) {
                send_ok( doc );
            }        
        }
    }
}

/*
 *  Envia todos los parametros del experimento en formato json.
 */

void CConfig::send_all_params( JsonDocument& doc )
{
    doc["distance"] = get_distance();
    doc["force"] =  get_force();
    doc["reaction_one"] = get_reaction1();
    doc["reaction_two"] =  get_reaction2();
    doc["flexion"] = get_flexion();             
    doc["st_test"] = get_st_test();    
   
    serializeJsonPretty( doc, Serial );
}

/*
 *  Envia todos los parametros de calibracion de la flexion y el nivel de loggeo.
 */

void CConfig::send_all_calibration( JsonDocument& doc )
{
    doc["step_cal"] = get_step_cal();
    doc["step_k"] =  get_step_k();
    doc["log_level"] =  get_log_level();
    serializeJsonPretty( doc, Serial );
}

//Envia el status del test
void CConfig::send_test_finish( void )
{
    StaticJsonDocument<512> doc;   
    doc["st_test"] = get_st_test();  
    serializeJsonPretty( doc, Serial );
}



// Envia la version del firmware.
void CConfig::send_version( JsonDocument& doc )
{
    doc["version"] = FIRMWARE_VERSION;

    serializeJsonPretty( doc, Serial );
}

// Envia la reaccion 1
void CConfig::send_reaction_one( JsonDocument& doc )
{
    doc["reaction_one"] =  get_reaction1();;

    serializeJsonPretty( doc, Serial );
}

// Envia la reaccion 2
void CConfig::send_reaction_two( JsonDocument& doc )
{
    doc["reaction_two"] =  get_reaction2();;

    serializeJsonPretty( doc, Serial );
}

// Envia la flexion de contar los pasos del motor2
void CConfig::send_flexion( JsonDocument& doc )
{
    doc["flexion"] =  get_flexion();

    serializeJsonPretty( doc, Serial );
}

// Envia la cantidad de pasos contados para la calibracion de ktep.
void CConfig::send_step_cal( JsonDocument& doc )
{
    doc["step_cal"] =  get_step_cal();

    serializeJsonPretty( doc, Serial );
}

// Envia step_k
void CConfig::send_step_k( JsonDocument& doc )
{
    doc["step_k"] =  get_step_k();

    serializeJsonPretty( doc, Serial );
}

// Envia el nivel de logeo
void CConfig::send_log_level( JsonDocument& doc )
{
    doc["log_level"] =  get_log_level();

    serializeJsonPretty( doc, Serial );
}

// Envia el estatus del ensayo.
void CConfig::send_status( JsonDocument& doc )
{
    doc["status"] = get_st_test();;

    serializeJsonPretty( doc, Serial );
}

// Envia el modo del ensayo.
void CConfig::send_st_mode( JsonDocument& doc )
{
    doc["st_mode"] = get_st_mode();;

    serializeJsonPretty( doc, Serial );
}

// Envia el resultado en formato json
void CConfig::send_ok( JsonDocument& doc )
{
    doc[ "result" ] = "ok";

    serializeJsonPretty( doc, Serial );
}

// Envia el resultado en formato json
void CConfig::send_ack( JsonDocument& doc )
{
    doc[ "result" ] = "ack";

    serializeJsonPretty( doc, Serial );
}
