// #include <HardwareSerial.h>

// HardwareSerial Serial(2);

// Configuración de pines
// #define RX_PIN 16
// #define TX_PIN 17
#define NIVEL_AGUA_PIN 34       // Pin analógico para el sensor de nivel
#define BTN_EMERGENCIA_PIN 15  // Usar el pin que tengas disponible
#define BLOQUEAR_PUERTA_PIN 13
#define GIRAR_DERECHA_PIN 12
#define GIRAR_IZQUIERDA_PIN 14
#define CENTRIFUGAR_PIN 27
#define INGRESAR_AGUA_PIN 26
#define DESFOGAR_PIN 25

// Configuración del ADC para el divisor de voltaje
const float VOLTAJE_MAX_ENTRADA = 3.8;  // Voltaje máximo de entrada (antes del divisor)
const float VOLTAJE_MIN_ENTRADA = 0.5;  // Voltaje mínimo de entrada
const float VOLTAJE_MAX_ADC = 3.3;      // Voltaje máximo del ADC
const float VOLTAJE_NIVEL = 3.0;        // Voltaje que indica nivel alcanzado (2.5V)
const uint8_t RESOLUCION_ADC = 12;          // Resolución del ADC
const uint16_t VALOR_MAX_ADC = 4095;         // Valor máximo del ADC (2^12 - 1)
// Factor de escala del divisor de voltaje
const float FACTOR_DIVISOR = VOLTAJE_MAX_ADC / VOLTAJE_MAX_ENTRADA;

// Tiempos en segundos para control de motor
// const uint8_t TIEMPO_GIRO_DERECHA = 180; // 180 segundos
// const uint8_t TIEMPO_GIRO_IZQUIERDA = 180;
// const uint8_t TIEMPO_PAUSA_GIRO = 60;
const uint8_t TIEMPO_GIRO_DERECHA = 3; // 180 segundos
const uint8_t TIEMPO_GIRO_IZQUIERDA = 3;
const uint8_t TIEMPO_PAUSA_GIRO = 1;
const uint16_t TIEMPO_CICLO_COMPLETO = TIEMPO_GIRO_DERECHA + TIEMPO_PAUSA_GIRO + TIEMPO_GIRO_IZQUIERDA + TIEMPO_PAUSA_GIRO;

// Tiempos fijos para procesos específicos
const uint8_t TIEMPO_DESFOGUE = 10;
// const uint16_t TIEMPO_CENTRIFUGADO = 420; // 7*60
// const uint8_t TIEMPO_BLOQUEO_FINAL = 120; // 2*60
const uint16_t TIEMPO_CENTRIFUGADO = 10; // 7*60
const uint8_t TIEMPO_BLOQUEO_FINAL = 7; // 2*60

// Tiempos de cada tanda en segundos (ya incluyen su tiempo de desfogue)
const uint16_t tiemposTanda[3][3] = {
  { 1690, 1510, 910},  // Programa 1: 28*60+10, 25*60+10, 15*60+10 + 7*60 = 4530
  { 1210, 910, 310},  // Programa 2: 20*60+10, 15*60+10, 5*60+10 + 7*60 = 
  // { 910, 610, 310 }   // Programa 3: 15*60+10, 10*60+10, 5*60+10 + 7*60 = 2250
  { 120, 60, 30 }   // Programa 3: 15*60+10, 10*60+10, 5*60+10 + 7*60 = 2250
};

// Variables de estado y control
uint8_t programaSeleccionado = 0;
uint16_t tiempoRestante = 0;
bool enProgreso = false;
bool emergencia = false;
bool primeraPausaActiva = false;
String comandoBuffer = "";
String estadoActual = "";
unsigned long ultimaActualizacionSerial = 0;
const uint16_t INTERVALO_ACTUALIZACION_SERIAL = 5000;
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
  Serial.print(comando);
  Serial.write(0xFF);
  Serial.write(0xFF);
  Serial.write(0xFF);
  // delay(10);
}

// Función para leer el nivel de agua
bool leerNivelAgua() {
  int valorADC = analogRead(NIVEL_AGUA_PIN);

  // Convertir el valor ADC a voltaje real (antes del divisor)
  float voltajeLeido = (valorADC * VOLTAJE_MAX_ENTRADA) / VALOR_MAX_ADC;

  // Para depuración
  static unsigned long ultimoLog = 0;
  if (millis() - ultimoLog > 1000) {  // Log cada segundo
    // Serial.print("Valor ADC: ");
    // Serial.print(valorADC);
    // Serial.print(" Voltaje: ");
    // Serial.print(voltajeLeido);
    // Serial.println("V");
    ultimoLog = millis();
  }

  // Comparar con el nivel deseado (2.5V)
  return voltajeLeido >= VOLTAJE_NIVEL;
}

void configurarPaginaLavado() {
  enviarComandoNextion("b_emergencia.en=0");  // emergencia
  enviarComandoNextion("b_parar.en=0");  // parar
  enviarComandoNextion("b_comenzar.en=1");  // comenzar
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
  static String estadoAnterior = "";

  unsigned long tiempoActual = millis();
  bool nivelLimiteAgua = leerNivelAgua();

  unsigned long tiempoTranscurridoEnCiclo = (tiempoActual - tiempoInicioSubEstado) % (TIEMPO_CICLO_COMPLETO * 1000);

  if (tiempoRestante <= 0) {
    enProgreso = false;
    estadoLavado = DESFOGUE_FINAL;
    tiempoInicioDesfogueFinal = tiempoActual;
    iniciarDesfogueFinal();
    return;
  }

  if (subEstado == LLENADO && tiempoInicioTanda == 0) {
    tiempoInicioTanda = tiempoActual;
    tiempoInicioSubEstado = tiempoActual;
    digitalWrite(INGRESAR_AGUA_PIN, HIGH);
    digitalWrite(DESFOGAR_PIN, HIGH);  // Cerrar desfogue durante llenado
  }

  String estadoActual = "Lavado " + String(numeroTanda) + " - ";

  switch (subEstado) {
    case LLENADO:
      if (!nivelLimiteAgua) {
        digitalWrite(INGRESAR_AGUA_PIN, HIGH);
        estadoActual += "llenado";
        
        // Procesamiento del motor durante el llenado
        if (tiempoTranscurridoEnCiclo < (TIEMPO_GIRO_DERECHA * 1000)) {
          digitalWrite(GIRAR_DERECHA_PIN, HIGH);
          digitalWrite(GIRAR_IZQUIERDA_PIN, LOW);
        } else if (tiempoTranscurridoEnCiclo < ((TIEMPO_GIRO_DERECHA + TIEMPO_PAUSA_GIRO) * 1000)) {
          digitalWrite(GIRAR_DERECHA_PIN, LOW);
          digitalWrite(GIRAR_IZQUIERDA_PIN, LOW);
        } else if (tiempoTranscurridoEnCiclo < ((TIEMPO_GIRO_DERECHA + TIEMPO_PAUSA_GIRO + TIEMPO_GIRO_IZQUIERDA) * 1000)) {
          digitalWrite(GIRAR_DERECHA_PIN, LOW);
          digitalWrite(GIRAR_IZQUIERDA_PIN, HIGH);
        } else {
          digitalWrite(GIRAR_DERECHA_PIN, LOW);
          digitalWrite(GIRAR_IZQUIERDA_PIN, LOW);
        }
      } else {
        digitalWrite(INGRESAR_AGUA_PIN, LOW);
        subEstado = LAVADO;
        tiempoInicioSubEstado = tiempoActual;
      }
      break;

    case LAVADO:
      if (tiempoTranscurridoEnCiclo < (TIEMPO_GIRO_DERECHA * 1000)) {
        digitalWrite(GIRAR_DERECHA_PIN, HIGH);
        digitalWrite(GIRAR_IZQUIERDA_PIN, LOW);
        estadoActual += "derecha";
      } else if (tiempoTranscurridoEnCiclo < ((TIEMPO_GIRO_DERECHA + TIEMPO_PAUSA_GIRO) * 1000)) {
        digitalWrite(GIRAR_DERECHA_PIN, LOW);
        digitalWrite(GIRAR_IZQUIERDA_PIN, LOW);
        estadoActual += "pausa";
      } else if (tiempoTranscurridoEnCiclo < ((TIEMPO_GIRO_DERECHA + TIEMPO_PAUSA_GIRO + TIEMPO_GIRO_IZQUIERDA) * 1000)) {
        digitalWrite(GIRAR_DERECHA_PIN, LOW);
        digitalWrite(GIRAR_IZQUIERDA_PIN, HIGH);
        estadoActual += "izquierda";
      } else {
        digitalWrite(GIRAR_DERECHA_PIN, LOW);
        digitalWrite(GIRAR_IZQUIERDA_PIN, LOW);
        estadoActual += "pausa";
      }

      if (tiempoActual - tiempoInicioTanda >= (tiemposTanda[programaSeleccionado - 1][numeroTanda - 1] - TIEMPO_DESFOGUE) * 1000) {
        subEstado = DESFOGUE;
        tiempoInicioDesfogue = tiempoActual;
        digitalWrite(GIRAR_DERECHA_PIN, LOW);
        digitalWrite(GIRAR_IZQUIERDA_PIN, LOW);
      }
      break;

    case DESFOGUE:
      estadoActual = "desfogue " + String(numeroTanda);
      digitalWrite(GIRAR_DERECHA_PIN, LOW);
      digitalWrite(GIRAR_IZQUIERDA_PIN, LOW);

      if (tiempoActual - tiempoInicioDesfogue < (TIEMPO_DESFOGUE * 1000)) {
        digitalWrite(DESFOGAR_PIN, LOW);  // Abrir desfogue (nivel bajo)
      } else {
        digitalWrite(DESFOGAR_PIN, HIGH);  // Cerrar desfogue (nivel alto)
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
    actualizarEstadoEnPantalla("centrifugado");
    digitalWrite(DESFOGAR_PIN, LOW);  // Mantener desfogue abierto durante centrifugado
  }

  if (tiempoActual - tiempoInicioCentrifugado < (TIEMPO_CENTRIFUGADO * 1000)) {
    digitalWrite(CENTRIFUGAR_PIN, HIGH);
    digitalWrite(DESFOGAR_PIN, LOW);  // Asegurar que el desfogue permanezca abierto
  } else {
    digitalWrite(CENTRIFUGAR_PIN, LOW);
    // No cerramos el desfogue aquí, se mantendrá abierto
    estadoLavado = ESPERA_FINAL;
    iniciarDesfogueFinal();
    tiempoInicioCentrifugado = 0;;
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
    digitalWrite(DESFOGAR_PIN, HIGH);  // Desfogue desactivado (nivel alto)
    iniciarDesfogueFinal();
  }

  if (tiempoActual - tiempoInicioEspera >= (TIEMPO_BLOQUEO_FINAL * 1000)) {
    digitalWrite(DESFOGAR_PIN, LOW);
    digitalWrite(BLOQUEAR_PUERTA_PIN, LOW);
    enProgreso = false;
    estadoLavado = ESPERA;
    tiempoInicioEspera = 0;
    enviarComandoNextion("page 1");
  }
}

void procesarComandosNextion() {
  while (Serial.available()) {
    char c = Serial.read();
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
  actualizarEstadoEnPantalla("Desfogue final");
}

void iniciarPrograma() {
  if (primeraPausaActiva) {
    primeraPausaActiva = false;
    enProgreso = true;
    // actualizarEstadoEnPantalla("reanudando");
  } else {
    // actualizarEstadoEnPantalla("iniciando");
    enProgreso = true;
    emergencia = false;
    estadoLavado = TANDA1;
    digitalWrite(BLOQUEAR_PUERTA_PIN, HIGH);
    digitalWrite(DESFOGAR_PIN, HIGH);  // Cerramos desfogue al iniciar
    tiempoRestante = calcularTiempoTotal();
    enviarComandoNextion("tiempo.txt=\"" + formatearTiempo(tiempoRestante) + "\"");
  }

  enviarComandoNextion("b_emergencia.en=1");  // emergencia
  enviarComandoNextion("b_parar.en=1");  // parar
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
    actualizarEstadoEnPantalla("Pausado");
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
    digitalWrite(DESFOGAR_PIN, LOW);  // Activar desfogue (nivel bajo)

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
  // digitalWrite(DESFOGAR_PIN, HIGH);

  // Cambiar a página de desfogue
  iniciarDesfogueFinal();
}

uint16_t calcularTiempoTotal() {
  int total = 0;
  // Suma de tiempos de cada tanda (que ya incluyen sus desfogues)
  for (int i = 0; i < 3; i++) {
    total += tiemposTanda[programaSeleccionado - 1][i];
  }
  // Agregar solo el tiempo de centrifugado
  total += TIEMPO_CENTRIFUGADO;
  return total;
}

void actualizarTiempo() {
  static unsigned long ultimaActualizacion = 0;
  unsigned long ahora = millis();
  bool nivelAgua = leerNivelAgua();

  // Verificamos si ha pasado 1 segundo desde la última actualización
  if (enProgreso && !emergencia && !primeraPausaActiva && 
      (ahora - ultimaActualizacion) >= 1000) {
    
    // Solo actualizamos el tiempo si el nivel de agua es adecuado
    // if (!nivelAgua) {
      if (tiempoRestante > 0) {
        tiempoRestante--;
        // Actualizamos el display con el nuevo tiempo formateado
        String tiempoFormateado = formatearTiempo(tiempoRestante);
        enviarComandoNextion("tiempo.txt=\"" + tiempoFormateado + "\"");
      }
    // }
    
    // Actualizamos el timestamp de la última actualización
    ultimaActualizacion = ahora;
    
    // Actualización del serial si es necesario
    if (ahora - ultimaActualizacionSerial >= INTERVALO_ACTUALIZACION_SERIAL) {
      ultimaActualizacionSerial = ahora;
    }
  }
}

void procesarDesfogueFinal() {
  unsigned long tiempoActual = millis();

  if (tiempoActual - tiempoInicioDesfogueFinal < (TIEMPO_BLOQUEO_FINAL * 1000)) {
    digitalWrite(DESFOGAR_PIN, LOW);  // Mantener desfogue abierto
  } else {
    // Al terminar el tiempo de desfogue, mantenemos el desfogue abierto
    digitalWrite(DESFOGAR_PIN, LOW);  // Mantener desfogue abierto
    estadoLavado = ESPERA;
    tiempoInicioDesfogueFinal = 0;

    enviarComandoNextion("page 1");
    digitalWrite(BLOQUEAR_PUERTA_PIN, LOW);
    actualizarEstadoEnPantalla("espera");
  }
}

String formatearTiempo(uint16_t segundos) {
  uint8_t minutos = segundos / 60;
  uint8_t segs = segundos % 60;
  return String(minutos) + ":" + (segs < 10 ? "0" : "") + String(segs);
}

void setup() {
  Serial.begin(9600);
  // Serial.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);
  analogReadResolution(RESOLUCION_ADC);

  pinMode(BTN_EMERGENCIA_PIN, INPUT_PULLUP);  // activo en nivel bajo
  pinMode(BLOQUEAR_PUERTA_PIN, OUTPUT);
  pinMode(GIRAR_DERECHA_PIN, OUTPUT);
  pinMode(GIRAR_IZQUIERDA_PIN, OUTPUT);
  pinMode(CENTRIFUGAR_PIN, OUTPUT);
  pinMode(INGRESAR_AGUA_PIN, OUTPUT);
  pinMode(DESFOGAR_PIN, OUTPUT);

  enviarComandoNextion("page 0");
}

void loop() {
  // Revisar el botón físico de emergencia
  static bool lastBtnState = HIGH;
  bool currentBtnState = digitalRead(BTN_EMERGENCIA_PIN);

  if (currentBtnState == LOW && lastBtnState == HIGH && (enProgreso || primeraPausaActiva)) {
    toggleEmergencia();
  }
  lastBtnState = currentBtnState;

  procesarComandosNextion();

  // Actualizar el tiempo siempre, independientemente del estado
  actualizarTiempo();

  if (enProgreso && !emergencia) {
    switch (estadoLavado) {
      case ESPERA:
        actualizarEstadoEnPantalla("espera");
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

  if (estadoLavado == DESFOGUE_FINAL) {
    procesarDesfogueFinal();
  }
}