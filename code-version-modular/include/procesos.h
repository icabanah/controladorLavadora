#ifndef PROCESOS_H
#define PROCESOS_H

#include <Arduino.h>
#include "config.h"
#include "structs.h"
#include "funciones_utiles.h"
#include "comunicacion.h"

// Declaraciones de funciones de procesos
void procesarTanda(int numeroTanda);
void procesarPreparacionCentrifugado();
void procesarCentrifugado();
void procesarDesfogueFinal();
void procesarDetenimiento();
void procesarEmergencia();
void iniciarPrograma();
void detenerPrograma();
void activarEmergencia();

#endif // PROCESOS_H
