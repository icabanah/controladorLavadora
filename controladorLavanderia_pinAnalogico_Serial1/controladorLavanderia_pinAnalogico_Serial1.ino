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
const int RESOLUCION_ADC = 12;          // Resolución del ADC
const int VALOR_MAX_ADC = 4095;         // Valor máximo del ADC (2^12 - 1)
// Factor de escala del divisor de voltaje
const float FACTOR_DIVISOR = VOLTAJE_MAX_ADC / VOLTAJE_MAX_ENTRADA;

// Tiempos en segundos para control de motor
const int TIEMPO_GIRO_DERECHA = 3;
const int TIEMPO_GIRO_IZQUIERDA = 3;
const int TIEMPO_PAUSA_GIRO = 3;
const int TIEMPO_CICLO_COMPLETO = TIEMPO_GIRO_DERECHA + TIEMPO_PAUSA_GIRO + TIEMPO_GIRO_IZQUIERDA + TIEMPO_PAUSA_GIRO;

// Tiempos fijos para procesos específicos
const int TIEMPO_DESFOGUE = 10;
const int TIEMPO_CENTRIFUGADO = 10;
const int TIEMPO_BLOQUEO_FINAL = 5;

// Tiempos de cada tanda en segundos (ya incluyen su tiempo de desfogue)
const int tiemposTanda[3][3] = {
  { 70, 55, 30 },  // Programa 1: 60+10, 45+10, 20+10
  { 55, 40, 25 },  // Programa 2: 45+10, 30+10, 15+10
  { 30, 25, 20 }   // Programa 3: 20+10, 15+10, 10+10
};

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
  delay(10);
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

  String estadoActual = "TANDA " + String(numeroTanda) + " - ";

  switch (subEstado) {
    case LLENADO:
      if (!nivelLimiteAgua) {
        digitalWrite(INGRESAR_AGUA_PIN, HIGH);
        estadoActual += "LLENADO";
        
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
        estadoActual += "LAVADO - DERECHA";
      } else if (tiempoTranscurridoEnCiclo < ((TIEMPO_GIRO_DERECHA + TIEMPO_PAUSA_GIRO) * 1000)) {
        digitalWrite(GIRAR_DERECHA_PIN, LOW);
        digitalWrite(GIRAR_IZQUIERDA_PIN, LOW);
        estadoActual += "LAVADO - PAUSA";
      } else if (tiempoTranscurridoEnCiclo < ((TIEMPO_GIRO_DERECHA + TIEMPO_PAUSA_GIRO + TIEMPO_GIRO_IZQUIERDA) * 1000)) {
        digitalWrite(GIRAR_DERECHA_PIN, LOW);
        digitalWrite(GIRAR_IZQUIERDA_PIN, HIGH);
        estadoActual += "LAVADO - IZQUIERDA";
      } else {
        digitalWrite(GIRAR_DERECHA_PIN, LOW);
        digitalWrite(GIRAR_IZQUIERDA_PIN, LOW);
        estadoActual += "LAVADO - PAUSA";
      }

      if (tiempoActual - tiempoInicioTanda >= (unsigned long)(tiemposTanda[programaSeleccionado - 1][numeroTanda - 1] - TIEMPO_DESFOGUE) * 1000) {
        subEstado = DESFOGUE;
        tiempoInicioDesfogue = tiempoActual;
        digitalWrite(GIRAR_DERECHA_PIN, LOW);
        digitalWrite(GIRAR_IZQUIERDA_PIN, LOW);
      }
      break;

    case DESFOGUE:
      estadoActual = "TANDA " + String(numeroTanda) + " - DESFOGUE";
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
    actualizarEstadoEnPantalla("CENTRIFUGADO");
    digitalWrite(DESFOGAR_PIN, LOW);  // Mantener desfogue abierto durante centrifugado
  }

  if (tiempoActual - tiempoInicioCentrifugado < (TIEMPO_CENTRIFUGADO * 1000)) {
    digitalWrite(CENTRIFUGAR_PIN, HIGH);
    digitalWrite(DESFOGAR_PIN, LOW);  // Asegurar que el desfogue permanezca abierto
  } else {
    digitalWrite(CENTRIFUGAR_PIN, LOW);
    // No cerramos el desfogue aquí, se mantendrá abierto
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
    digitalWrite(DESFOGAR_PIN, HIGH);  // Desfogue desactivado (nivel alto)
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
  actualizarEstadoEnPantalla("DESFOGUE FINAL");
}

void iniciarPrograma() {
  if (primeraPausaActiva) {
    primeraPausaActiva = false;
    enProgreso = true;
    actualizarEstadoEnPantalla("REANUDANDO");
  } else {
    actualizarEstadoEnPantalla("INICIANDO");
    enProgreso = true;
    emergencia = false;
    estadoLavado = TANDA1;
    digitalWrite(BLOQUEAR_PUERTA_PIN, HIGH);
    digitalWrite(DESFOGAR_PIN, HIGH);  // Cerramos desfogue al iniciar
    tiempoRestante = calcularTiempoTotal();
    enviarComandoNextion("tiempo.txt=\"" + String(tiempoRestante) + "\"");
  }

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
  digitalWrite(DESFOGAR_PIN, HIGH);

  // Cambiar a página de desfogue
  iniciarDesfogueFinal();
}

int calcularTiempoTotal() {
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

  if (enProgreso && !emergencia && !primeraPausaActiva && 
      ahora - ultimaActualizacion >= 1000) {  // Removida la condición de nivel de agua
    ultimaActualizacion = ahora;
    if (tiempoRestante > 0) {
      tiempoRestante--;
      enviarComandoNextion("tiempo.txt=\"" + String(tiempoRestante) + "\"");
      if (ahora - ultimaActualizacionSerial >= INTERVALO_ACTUALIZACION_SERIAL) {
        ultimaActualizacionSerial = ahora;
      }
    }
  }
}

void procesarDesfogueFinal() {
  unsigned long tiempoActual = millis();

  if (tiempoActual - tiempoInicioDesfogueFinal < (TIEMPO_DESFOGUE * 1000)) {
    digitalWrite(DESFOGAR_PIN, LOW);  // Mantener desfogue abierto
  } else {
    // Al terminar el tiempo de desfogue, mantenemos el desfogue abierto
    digitalWrite(DESFOGAR_PIN, LOW);  // Mantener desfogue abierto
    estadoLavado = ESPERA;
    tiempoInicioDesfogueFinal = 0;

    enviarComandoNextion("page 1");
    digitalWrite(BLOQUEAR_PUERTA_PIN, LOW);
    actualizarEstadoEnPantalla("ESPERA");
  }
}

void loop() {
  // Revisar el botón físico de emergencia
  static bool lastBtnState = HIGH;  // Para detección de flanco
  bool currentBtnState = digitalRead(BTN_EMERGENCIA_PIN);

  // Si el botón se presiona (flanco descendente) y el programa está en marcha
  if (currentBtnState == LOW && lastBtnState == HIGH && (enProgreso || primeraPausaActiva)) {
    toggleEmergencia();
    delay(50);  // Debounce
  }
  lastBtnState = currentBtnState;

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