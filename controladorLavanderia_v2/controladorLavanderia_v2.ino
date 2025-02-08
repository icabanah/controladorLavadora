#include <HardwareSerial.h>

HardwareSerial NextionSerial(2);

// Configuración de pines
#define RX_PIN 16
#define TX_PIN 17
#define NIVEL_AGUA_PIN 5  // Pin analógico para el sensor de nivel
#define BLOQUEAR_PUERTA_PIN 13
#define GIRAR_DERECHA_PIN 12
#define GIRAR_IZQUIERDA_PIN 14
#define CENTRIFUGAR_PIN 27
#define INGRESAR_AGUA_PIN 26
#define DESFOGAR_PIN 25

// Configuración del sensor de nivel
const float VOLTAJE_REFERENCIA = 3.3;
const float VOLTAJE_NIVEL = 2.5;
const int RESOLUCION_ADC = 12;
const int VALOR_MAX_ADC = 4095;
const int UMBRAL_NIVEL = (VOLTAJE_NIVEL / VOLTAJE_REFERENCIA) * VALOR_MAX_ADC;

// Tiempos en segundos
const int TIEMPO_DESFOGUE = 10;
const int TIEMPO_CENTRIFUGADO = 10;
const int TIEMPO_GIRO_DERECHA = 3;
const int TIEMPO_GIRO_IZQUIERDA = 3;
const int TIEMPO_PAUSA_GIRO = 3;
const int TIEMPO_BLOQUEO_FINAL = 5;
const int TIEMPO_CICLO_COMPLETO = TIEMPO_GIRO_DERECHA + TIEMPO_PAUSA_GIRO + TIEMPO_GIRO_IZQUIERDA + TIEMPO_PAUSA_GIRO;

// Variables de estado y control
uint8_t programaSeleccionado = 0;
uint8_t tiempoRestante = 0;
bool enProgreso = false;
bool emergencia = false;
bool primeraPausaActiva = false;
String comandoBuffer = "";
String estadoActual = "";
unsigned long ultimaActualizacionSerial = 0;
const unsigned long INTERVALO_ACTUALIZACION_SERIAL = 5000;
unsigned long tiempoInicioDesfogueFinal = 0;

// Estados del ciclo de lavado
enum EstadoLavado {
  ESPERA,
  TANDA1,
  TANDA2,
  TANDA3,
  CENTRIFUGADO,
  ESPERA_FINAL,
  DESFOGUE_FINAL
};

EstadoLavado estadoLavado = ESPERA;

// Tiempos de cada programa en segundos
const int tiemposTanda[3][3] = {
  { 60, 45, 20 },  // Programa 1
  { 45, 30, 15 },  // Programa 2
  { 20, 15, 10 }   // Programa 3
};

// Declaraciones anticipadas de funciones
void enviarComandoNextion(String comando);
void actualizarEstadoEnPantalla(String nuevoEstado);
void iniciarDesfogueFinal();
void procesarTanda(int numeroTanda);
void procesarCentrifugado();
void procesarDesfogueFinal();
void actualizarTiempo();
void configurarPaginaLavado();
void toggleEmergencia();
void detenerPrograma();
void iniciarPrograma();

// Función para enviar comandos al Nextion
void enviarComandoNextion(String comando) {
  NextionSerial.print(comando);
  NextionSerial.write(0xFF);
  NextionSerial.write(0xFF);
  NextionSerial.write(0xFF);
  delay(10);
}

// Función para leer el nivel de agua
bool leerNivelAgua() {
  int valorADC = analogRead(NIVEL_AGUA_PIN);
  float voltaje = (valorADC * VOLTAJE_REFERENCIA) / VALOR_MAX_ADC;

  static unsigned long ultimoLog = 0;
  if (millis() - ultimoLog > 1000) {
    Serial.print("Valor ADC: ");
    Serial.print(valorADC);
    Serial.print(" Voltaje: ");
    Serial.println(voltaje);
    ultimoLog = millis();
  }

  return valorADC >= UMBRAL_NIVEL;
}

void configurarPaginaLavado() {
  enviarComandoNextion("b1.en=0");  // emergencia
  enviarComandoNextion("b2.en=0");  // parar
  enviarComandoNextion("b0.en=1");  // comenzar
}

void actualizarEstadoEnPantalla(String nuevoEstado) {
  if (nuevoEstado != estadoActual) {
    estadoActual = nuevoEstado;
    enviarComandoNextion("estado.txt=\"" + estadoActual + "\"");
  }
}

void procesarTanda(int numeroTanda) {
  static enum SubEstadoTanda { LLENADO,
                               LAVADO,
                               DESFOGUE } subEstado = LLENADO;
  static unsigned long tiempoInicioSubEstado = 0;
  static unsigned long tiempoInicioTanda = 0;
  static unsigned long tiempoInicioDesfogue = 0;
  static int ciclosLavado = 0;
  static String estadoAnterior = "";

  unsigned long tiempoActual = millis();

  // Verificar si aún hay tiempo disponible
  if (tiempoRestante <= 0) {
    enProgreso = false;
    estadoLavado = DESFOGUE_FINAL;
    tiempoInicioDesfogueFinal = tiempoActual;
    iniciarDesfogueFinal();
    return;
  }

  // Leer el nivel de agua usando el sensor analógico
  bool nivelLimiteAgua = leerNivelAgua();

  // Iniciamos el temporizador de la tanda si estamos en el estado de llenado y aún no se ha iniciado
  if (subEstado == LLENADO && tiempoInicioTanda == 0) {
    tiempoInicioTanda = tiempoActual;
    tiempoInicioSubEstado = tiempoActual;
    // Aseguramos que inicie con giro a la derecha
    digitalWrite(GIRAR_DERECHA_PIN, HIGH);
    digitalWrite(GIRAR_IZQUIERDA_PIN, LOW);
  }

  String estadoActual = "TANDA " + String(numeroTanda) + " - ";

  // El motor solo gira si NO estamos en estado de desfogue
  if (subEstado != DESFOGUE) {
    unsigned long tiempoTranscurridoEnCiclo = (tiempoActual - tiempoInicioSubEstado) % (TIEMPO_CICLO_COMPLETO * 1000);

    if (tiempoTranscurridoEnCiclo < (TIEMPO_GIRO_DERECHA * 1000)) {
      digitalWrite(GIRAR_DERECHA_PIN, HIGH);
      digitalWrite(GIRAR_IZQUIERDA_PIN, LOW);
      estadoActual += (subEstado == LAVADO ? "LAVADO - " : "") + String("DERECHA");
    } else if (tiempoTranscurridoEnCiclo < ((TIEMPO_GIRO_DERECHA + TIEMPO_PAUSA_GIRO) * 1000)) {
      digitalWrite(GIRAR_DERECHA_PIN, LOW);
      digitalWrite(GIRAR_IZQUIERDA_PIN, LOW);
      estadoActual += "PAUSA";
    } else if (tiempoTranscurridoEnCiclo < ((TIEMPO_GIRO_DERECHA + TIEMPO_PAUSA_GIRO + TIEMPO_GIRO_IZQUIERDA) * 1000)) {
      digitalWrite(GIRAR_DERECHA_PIN, LOW);
      digitalWrite(GIRAR_IZQUIERDA_PIN, HIGH);
      estadoActual += (subEstado == LAVADO ? "LAVADO - " : "") + String("IZQUIERDA");
    } else {
      digitalWrite(GIRAR_DERECHA_PIN, LOW);
      digitalWrite(GIRAR_IZQUIERDA_PIN, LOW);
      estadoActual += "PAUSA";
    }
  }

  switch (subEstado) {
    case LLENADO:
      if (!nivelLimiteAgua) {
        digitalWrite(INGRESAR_AGUA_PIN, HIGH);
        estadoActual = "TANDA " + String(numeroTanda) + " - LLENADO";
      } else {
        digitalWrite(INGRESAR_AGUA_PIN, LOW);
        subEstado = LAVADO;
      }
      break;

    case LAVADO:
      if (tiempoActual - tiempoInicioTanda >= (unsigned long)tiemposTanda[programaSeleccionado - 1][numeroTanda - 1] * 1000) {
        subEstado = DESFOGUE;
        tiempoInicioDesfogue = tiempoActual;
        digitalWrite(GIRAR_DERECHA_PIN, LOW);
        digitalWrite(GIRAR_IZQUIERDA_PIN, LOW);
      }
      break;

    case DESFOGUE:
      digitalWrite(GIRAR_DERECHA_PIN, LOW);
      digitalWrite(GIRAR_IZQUIERDA_PIN, LOW);

      estadoActual = "TANDA " + String(numeroTanda) + " - DESFOGUE";

      if (tiempoActual - tiempoInicioDesfogue < (TIEMPO_DESFOGUE * 1000)) {
        digitalWrite(DESFOGAR_PIN, HIGH);
      } else {
        digitalWrite(DESFOGAR_PIN, LOW);
        if (numeroTanda < 3) {
          estadoLavado = (EstadoLavado)((int)estadoLavado + 1);
          subEstado = LLENADO;
          tiempoInicioTanda = 0;
          tiempoInicioSubEstado = 0;
        } else {
          estadoLavado = CENTRIFUGADO;
          tiempoInicioTanda = 0;
        }
      }
      break;
  }

  if (estadoActual != estadoAnterior) {
    actualizarEstadoEnPantalla(estadoActual);
    estadoAnterior = estadoActual;
  }
}

void procesarCentrifugado() {
  static unsigned long tiempoInicioCentrifugado = 0;
  unsigned long tiempoActual = millis();

  if (tiempoInicioCentrifugado == 0) {
    tiempoInicioCentrifugado = tiempoActual;
    actualizarEstadoEnPantalla("CENTRIFUGADO");
  }

  if (tiempoActual - tiempoInicioCentrifugado < (TIEMPO_CENTRIFUGADO * 1000)) {
    digitalWrite(CENTRIFUGAR_PIN, HIGH);
  } else {
    digitalWrite(CENTRIFUGAR_PIN, LOW);
    estadoLavado = ESPERA_FINAL;
    tiempoInicioCentrifugado = 0;
  }
}

void procesarEsperaFinal() {
  static unsigned long tiempoInicioEspera = 0;
  unsigned long tiempoActual = millis();

  if (tiempoInicioEspera == 0) {
    tiempoInicioEspera = tiempoActual;
    digitalWrite(GIRAR_DERECHA_PIN, LOW);
    digitalWrite(GIRAR_IZQUIERDA_PIN, LOW);
    digitalWrite(CENTRIFUGAR_PIN, LOW);
    digitalWrite(INGRESAR_AGUA_PIN, LOW);
    digitalWrite(DESFOGAR_PIN, LOW);
    actualizarEstadoEnPantalla("ESPERA FINAL");
  }

  if (tiempoActual - tiempoInicioEspera >= (TIEMPO_BLOQUEO_FINAL * 1000)) {
    digitalWrite(BLOQUEAR_PUERTA_PIN, LOW);
    enProgreso = false;
    estadoLavado = ESPERA;
    tiempoInicioEspera = 0;
    enviarComandoNextion("page 1");
    actualizarEstadoEnPantalla("ESPERA");
  }
}

void setup() {
  Serial.begin(9600);
  NextionSerial.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);
  analogReadResolution(RESOLUCION_ADC);

  pinMode(BLOQUEAR_PUERTA_PIN, OUTPUT);
  pinMode(GIRAR_DERECHA_PIN, OUTPUT);
  pinMode(GIRAR_IZQUIERDA_PIN, OUTPUT);
  pinMode(CENTRIFUGAR_PIN, OUTPUT);
  pinMode(INGRESAR_AGUA_PIN, OUTPUT);
  pinMode(DESFOGAR_PIN, OUTPUT);

  enviarComandoNextion("page 0");
}

void procesarComandosNextion() {
  while (NextionSerial.available()) {
    char c = NextionSerial.read();
    if (c >= 32 && c <= 126) {
      comandoBuffer += c;
    }
    if (c == 0xFF || comandoBuffer.length() > 20) {
      if (comandoBuffer.length() > 0) {
        if (comandoBuffer.indexOf("programa") >= 0) {
          programaSeleccionado = comandoBuffer.charAt(comandoBuffer.length() - 1) - '0';
          enviarComandoNextion("page 2");
          configurarPaginaLavado();
        } else if (comandoBuffer.indexOf("comenzar") >= 0) {
          iniciarPrograma();
        } else if (comandoBuffer.indexOf("parar") >= 0) {
          if (enProgreso || primeraPausaActiva) {
            detenerPrograma();
          }
        } else if (comandoBuffer.indexOf("emergencia") >= 0) {
          if (enProgreso || primeraPausaActiva) {
            toggleEmergencia();
          }
        }
        comandoBuffer = "";
      }
    }
  }
}

void iniciarDesfogueFinal() {
  // Cambiar a la página de desfogue (página 3)
  enviarComandoNextion("page 3");
  actualizarEstadoEnPantalla("DESFOGUE FINAL");
}

void iniciarPrograma() {
  if (primeraPausaActiva) {
    // Si estaba en pausa, reanudamos
    primeraPausaActiva = false;
    enProgreso = true;
    actualizarEstadoEnPantalla("REANUDANDO");
  } else {
    // Inicio normal del programa
    actualizarEstadoEnPantalla("INICIANDO");
    enProgreso = true;
    emergencia = false;
    estadoLavado = TANDA1;
    digitalWrite(BLOQUEAR_PUERTA_PIN, HIGH);
    tiempoRestante = calcularTiempoTotal();
    enviarComandoNextion("tiempo.txt=\"" + String(tiempoRestante) + "\"");
  }

  // Al iniciar el programa, habilitar los botones de parar y emergencia
  enviarComandoNextion("b1.en=1");  // emergencia
  enviarComandoNextion("b2.en=1");  // parar
}

void detenerPrograma() {
  if (!primeraPausaActiva) {
    // Primera vez que se presiona parar
    primeraPausaActiva = true;
    enProgreso = false;

    // Detiene los motores y el ingreso de agua
    digitalWrite(GIRAR_DERECHA_PIN, LOW);
    digitalWrite(GIRAR_IZQUIERDA_PIN, LOW);
    digitalWrite(CENTRIFUGAR_PIN, LOW);
    digitalWrite(INGRESAR_AGUA_PIN, LOW);
    actualizarEstadoEnPantalla("PAUSADO");
  } else {
    // Segunda vez que se presiona parar
    primeraPausaActiva = false;
    enProgreso = false;
    emergencia = false;

    // Detener motores e ingreso de agua
    digitalWrite(GIRAR_DERECHA_PIN, LOW);
    digitalWrite(GIRAR_IZQUIERDA_PIN, LOW);
    digitalWrite(CENTRIFUGAR_PIN, LOW);
    digitalWrite(INGRESAR_AGUA_PIN, LOW);

    // Iniciar proceso de desfogue final
    estadoLavado = DESFOGUE_FINAL;
    tiempoInicioDesfogueFinal = millis();
    digitalWrite(DESFOGAR_PIN, HIGH);

    // Cambiar a página de desfogue
    iniciarDesfogueFinal();
  }
}

void toggleEmergencia() {
  // Detener motores e ingreso de agua
  digitalWrite(GIRAR_DERECHA_PIN, LOW);
  digitalWrite(GIRAR_IZQUIERDA_PIN, LOW);
  digitalWrite(CENTRIFUGAR_PIN, LOW);
  digitalWrite(INGRESAR_AGUA_PIN, LOW);

  // Restablecer variables de estado
  emergencia = false;
  enProgreso = false;
  primeraPausaActiva = false;

  // Iniciar proceso de desfogue final
  estadoLavado = DESFOGUE_FINAL;
  tiempoInicioDesfogueFinal = millis();
  digitalWrite(DESFOGAR_PIN, HIGH);

  // Cambiar a página de desfogue
  iniciarDesfogueFinal();
}

int calcularTiempoTotal() {
  int total = 0;
  // Suma de tiempos de cada tanda
  for (int i = 0; i < 3; i++) {
    total += tiemposTanda[programaSeleccionado - 1][i];
    // Agregar tiempo de desfogue por cada tanda
    total += TIEMPO_DESFOGUE;
  }
  // Agregar tiempo de centrifugado y bloqueo final
  total += TIEMPO_CENTRIFUGADO + TIEMPO_BLOQUEO_FINAL;
  return total;
}

void actualizarTiempo() {
  static unsigned long ultimaActualizacion = 0;
  unsigned long ahora = millis();

  if (enProgreso && !emergencia && !primeraPausaActiva && ahora - ultimaActualizacion >= 1000) {
    ultimaActualizacion = ahora;
    if (tiempoRestante > 0) {
      tiempoRestante--;
      enviarComandoNextion("tiempo.txt=\"" + String(tiempoRestante) + "\"");
      if (ahora - ultimaActualizacionSerial >= INTERVALO_ACTUALIZACION_SERIAL) {
        Serial.println("Tiempo restante: " + String(tiempoRestante));
        ultimaActualizacionSerial = ahora;
      }
    }
  }
}

void procesarDesfogueFinal() {
  unsigned long tiempoActual = millis();

  if (tiempoActual - tiempoInicioDesfogueFinal < (TIEMPO_DESFOGUE * 1000)) {
    digitalWrite(DESFOGAR_PIN, HIGH);
  } else {
    digitalWrite(DESFOGAR_PIN, LOW);
    estadoLavado = ESPERA;
    tiempoInicioDesfogueFinal = 0;

    // Cambiar a la página del menú
    enviarComandoNextion("page 1");
    // Desbloquear la puerta
    digitalWrite(BLOQUEAR_PUERTA_PIN, LOW);
    actualizarEstadoEnPantalla("ESPERA");
  }
}

void loop() {
  procesarComandosNextion();

  if (enProgreso && !emergencia) {
    switch (estadoLavado) {
      case ESPERA:
        actualizarEstadoEnPantalla("ESPERA");
        break;
      case TANDA1:
      case TANDA2:
      case TANDA3:
        procesarTanda(estadoLavado - TANDA1 + 1);
        break;
      case CENTRIFUGADO:
        procesarCentrifugado();
        break;
      case ESPERA_FINAL:
        procesarEsperaFinal();
        break;
    }
  }

  // Procesar desfogue final independientemente del estado de enProgreso
  if (estadoLavado == DESFOGUE_FINAL) {
    procesarDesfogueFinal();
  }

  actualizarTiempo();
}