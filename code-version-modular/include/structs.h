#ifndef STRUCTS_H
#define STRUCTS_H

#include <Arduino.h>
#include "config.h"

// Estados del ciclo de lavado
enum EstadoLavado
{
  ESPERA,
  TANDA1,
  TANDA2,
  TANDA3,
  PREPARACION_CENTRIFUGADO,
  CENTRIFUGADO,
  DESFOGUE_FINAL,
  DETENIMIENTO,
  EMERGENCIA
};

enum SubEstadoTanda
{
  LAVADO,
  DESFOGUE
};

struct TiemposLavado
{
  unsigned long inicioSubEstado;
  unsigned long inicioTanda;
  unsigned long inicioDesfogue;
  unsigned long inicioPreparacionCentrifugado;
  unsigned long inicioCentrifugado;
  unsigned long inicioDesfogueFinal;
  unsigned long inicioDetenimiento;
  unsigned long inicioEmergencia;
  unsigned long ultimaActualizacion;
  unsigned long ultimaActualizacionSerial;
  unsigned long tiempoRestante;
  bool iniciado; // Flag para controlar la inicialización

  void reset()
  {
    inicioSubEstado = 0;
    inicioTanda = 0;
    inicioDesfogue = 0;
    inicioPreparacionCentrifugado = 0;
    inicioCentrifugado = 0;
    inicioDesfogueFinal = 0;
    inicioDetenimiento = 0;
    inicioEmergencia = 0;
    ultimaActualizacion = millis();
    ultimaActualizacionSerial = millis();
    tiempoRestante = 0;
    iniciado = false;
  }
};

struct EstadoPines
{
  bool puertaBloqueada;
  bool giroDerecha;
  bool giroIzquierda;
  bool centrifugado;
  bool ingresoAgua;
  bool desfogue;

  void reset()
  {
    puertaBloqueada = false;
    giroDerecha = false;
    giroIzquierda = false;
    centrifugado = false;
    ingresoAgua = false;
    desfogue = true; // Por seguridad, desfogue cerrado por defecto
  }

  void aplicar()
  {
    digitalWrite(BLOQUEAR_PUERTA_PIN, !puertaBloqueada);
    digitalWrite(GIRAR_DERECHA_PIN, giroDerecha);
    digitalWrite(GIRAR_IZQUIERDA_PIN, giroIzquierda);
    digitalWrite(CENTRIFUGAR_PIN, centrifugado);
    digitalWrite(INGRESAR_AGUA_PIN, ingresoAgua);
    digitalWrite(DESFOGAR_PIN, desfogue);
  }
};

struct BanderasProceso
{
  bool enProgreso;
  bool emergencia;
  bool primeraPausaActiva;

  void reset()
  {
    enProgreso = false;
    emergencia = false;
    primeraPausaActiva = false;
  }
};

// Variables globales (declaración externa)
extern TiemposLavado tiempos;
extern EstadoPines pines;
extern BanderasProceso banderas;
extern bool errorTimeout;
extern uint8_t programaSeleccionado;
extern String comandoBuffer;
extern String estadoActual;
extern EstadoLavado estadoLavado;
extern SubEstadoTanda subEstado;

#endif // STRUCTS_H
