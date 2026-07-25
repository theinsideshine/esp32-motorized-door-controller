/**
 * File:   Encapsula el control del la celdas de carga.
 *
 * - Compiler:           Arduino 1.8.13
 * - Supported devices:  Mega
 *
 * \author               MV: luis.villacorta@alumnos.udemm.edu.ar
 *                       LC: leandro.cintioli@alumnos.udemm.edu.ar
 *                       PT: pablo.tavolaro@alumnos.udemm.edu.ar
 *
 * Date:  04-25-2021
 *
 *      Universidad de la Marina Mercante.
 */
 
#ifndef CELL_H
#define CELL_H

#include "Arduino.h"
#include "precompilation.h"
#include <HX711.h>

#ifdef TEST_PROTOTIPE 

#define PIN_CELL_REACTION1_DAT          6
#define PIN_CELL_REACTION1_CLK          5
#define K1_CELL_REACTION1               -63932                 // Factor obtenido con Calibrar().                  
#define STANDARD_WEIGHT_CELL_REACTION1  306                    // Peso de calibracion.   


#define PIN_CELL_REACTION2_DAT          PIN_CELL_REACTION1_DAT
#define PIN_CELL_REACTION2_CLK          PIN_CELL_REACTION1_CLK
#define K1_CELL_REACTION2               K1_CELL_REACTION1                  
#define STANDARD_WEIGHT_CELL_REACTION2  STANDARD_WEIGHT_CELL_REACTION1   

#else 

#define PIN_CELL_REACTION2_DAT          4
#define PIN_CELL_REACTION2_CLK          3
#define K1_CELL_REACTION2               -229668            //Factor obtenido con cal_hx711.ino.
#define STANDARD_WEIGHT_CELL_REACTION2  2205               //Peso de calibracion.

#define PIN_CELL_REACTION1_DAT          6
#define PIN_CELL_REACTION1_CLK          5
#define K1_CELL_REACTION1               -282565          //Factor obtenido con cal_hx711.ino.       
#define STANDARD_WEIGHT_CELL_REACTION1  2205              //Peso de calibracion.


#endif //TEST_PROTOTIPE


#define GET_UNITS                    10                   // Cantidad de lecturas a realizar.
#define CELL_FORCE_WINDOWS           10                   // Ventana de comparacion para celda de fuerza, en gramos.
#define CELL_LOAD                    500                  // Fuerza en gramos ,donde se considera el sistema cargado para empezar a contar pasos
//#define CELL_DEBUG                                        // Muestra informacion del las fuerza-pasos por el puerto serie.
#define CELL_TIME_READ_FORCE         500               // Tiempo en ms de espera entre lectura y lectura. 

class CCell
{
  public:
    CCell();
    void init( void );   
    bool is_force(uint16_t force );
    void read_cell_force( void );
    float get_cell_reaction1(void);
    float get_cell_reaction2(void);
    float get_cell_force( void );
    bool is_cell_load(void );
    void reset_cell_load( void);
    
  private:
  HX711 cell_force;
  float weight_cell_force ;
  HX711 cell_reaction1;
  float weight_cell_reaction1;
  HX711 cell_reaction2;
  float weight_cell_reaction2;
  bool  cell_load ;
     
};


#endif // CELL_H
