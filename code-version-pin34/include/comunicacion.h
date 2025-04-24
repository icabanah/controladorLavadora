#ifndef COMUNICACION_H
#define COMUNICACION_H

#include <Arduino.h>
#include "config.h"
#include "structs.h"
#include "procesos.h"

// Declaraciones de funciones de comunicación con Nextion
void enviarComandoNextion(String comando);
void procesarComandosNextion();

#endif // COMUNICACION_H
