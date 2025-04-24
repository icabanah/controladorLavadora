#ifndef FUNCIONES_UTILES_H
#define FUNCIONES_UTILES_H

#include <Arduino.h>
#include "config.h"
#include "structs.h"

// Declaraciones de funciones de utilidad
bool leerNivelAguaEstable();
void actualizarEstadoEnPantalla(String nuevoEstado);
uint16_t calcularTiempoTotal();
void actualizarTiempo();
String formatearTiempo(uint16_t segundos);
bool leerBotonEmergencia();
void inicializarAntirreboteEmergencia();

#endif // FUNCIONES_UTILES_H
