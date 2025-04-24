#include "include/config.h"
#include "include/structs.h"
#include "include/funciones_utiles.h"
#include "include/comunicacion.h"
#include "include/procesos.h"

// Variables globales
TiemposLavado tiempos;
EstadoPines pines;
BanderasProceso banderas;
bool errorTimeout = false;

// Variables de estado y control
uint8_t programaSeleccionado = 0;
String comandoBuffer = "";
String estadoActual = "";
EstadoLavado estadoLavado = ESPERA;
SubEstadoTanda subEstado = LAVADO;

void setup()
{
  Serial.begin(9600);
  // Serial.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);
  analogReadResolution(RESOLUCION_ADC);

  pinMode(BTN_EMERGENCIA_PIN, INPUT_PULLUP); // Botón de emergencia
  pinMode(BLOQUEAR_PUERTA_PIN, OUTPUT);
  pinMode(GIRAR_DERECHA_PIN, OUTPUT);
  pinMode(GIRAR_IZQUIERDA_PIN, OUTPUT);
  pinMode(CENTRIFUGAR_PIN, OUTPUT);
  pinMode(INGRESAR_AGUA_PIN, OUTPUT);
  pinMode(DESFOGAR_PIN, OUTPUT);

  enviarComandoNextion("page 0");
  tiempos.ultimaActualizacion = millis(); // Inicialización correcta
  tiempos.ultimaActualizacionSerial = millis();
  // Inicialización segura de los componentes
  pines.reset();
  pines.desfogue = false; // Por seguridad, desfogue abierto por defecto
  pines.aplicar();
  banderas.reset();
  tiempos.reset();
}

void loop()
{
  // Revisar el botón físico de emergencia
  static bool lastBtnState = HIGH; // Estado anterior del botón
  bool currentBtnState = digitalRead(BTN_EMERGENCIA_PIN);

  // Detección de flanco descendente para activar la emergencia
  if (currentBtnState == LOW && lastBtnState == HIGH)
  {
    activarEmergencia();
  }
  lastBtnState = currentBtnState;

  // Procesar comandos recibidos desde la pantalla Nextion
  procesarComandosNextion();

  // Actualizar el tiempo siempre, independientemente del estado
  actualizarTiempo();

  // Control principal del proceso de lavado
  if (banderas.enProgreso && !banderas.emergencia)
  {
    switch (estadoLavado)
    {
    case ESPERA:
      // En espera de comandos, no hace nada específico
      break;
    case TANDA1:
    case TANDA2:
    case TANDA3:
      procesarTanda(estadoLavado - TANDA1 + 1);
      break;
    case PREPARACION_CENTRIFUGADO:
      procesarPreparacionCentrifugado();
      break;
    case CENTRIFUGADO:
      procesarCentrifugado();
      break;
    }
  }

  // Estados especiales que se procesan independientemente de las banderas
  if (estadoLavado == DESFOGUE_FINAL)
  {
    procesarDesfogueFinal();
  }

  if (estadoLavado == DETENIMIENTO)
  {
    procesarDetenimiento();
  }

  if (estadoLavado == EMERGENCIA)
  {
    procesarEmergencia();
  }
}
