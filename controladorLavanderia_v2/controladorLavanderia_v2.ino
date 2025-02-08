#include <HardwareSerial.h>

// Configuración Serial para Nextion
HardwareSerial NextionSerial(2);
#define RX_PIN 16
#define TX_PIN 17

// Pin Analógico para sensor de nivel
#define SENSOR_NIVEL 34

// Pines de salida
#define PIN_BLOQUEAR_PUERTA 13
#define PIN_GIRAR_DERECHA 12
#define PIN_GIRAR_IZQUIERDA 14
#define PIN_CENTRIFUGAR 27
#define PIN_INGRESAR_AGUA 26
#define PIN_DESFOGAR 25

// Estados del sistema
enum Estado {
  ESPERA,
  TANDA1,
  TANDA2,
  TANDA3,
  CENTRIFUGADO,
  ESPERA_FINAL
};

// Variables de control
Estado estadoActual = ESPERA;
int programaSeleccionado = 0;
bool enPausa = false;
bool emergencia = false;
bool llenando = false;
bool desfogando = false;
unsigned long tiempoInicio = 0;
unsigned long tiempoGiroInicio = 0;
int tiempoTotal = 0;
int tiempoTranscurrido = 0;

// Estructura para los tiempos de cada programa (en minutos)
struct TiemposPrograma {
  int tanda1;
  int tanda2;
  int tanda3;
  const int centrifugado = 7;
  const int esperaFinal = 2;
};

TiemposPrograma programas[3] = {
  {28, 25, 15}, // Programa 1
  {20, 15, 5},  // Programa 2
  {15, 10, 5}   // Programa 3
};

void setup() {
  Serial.begin(9600);
  NextionSerial.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);
  
  // Configurar pines
  pinMode(PIN_BLOQUEAR_PUERTA, OUTPUT);
  pinMode(PIN_GIRAR_DERECHA, OUTPUT);
  pinMode(PIN_GIRAR_IZQUIERDA, OUTPUT);
  pinMode(PIN_CENTRIFUGAR, OUTPUT);
  pinMode(PIN_INGRESAR_AGUA, OUTPUT);
  pinMode(PIN_DESFOGAR, OUTPUT);
  
  apagarTodo();
}

void loop() {
  leerComandosNextion();
  if (!emergencia && !enPausa) {
    controladorPrincipal();
  }
  actualizarDisplay();
}

float leerNivel() {
  int valorADC = analogRead(SENSOR_NIVEL);
  float voltaje = (valorADC * 3.3) / 4095.0;
  return voltaje;
}

void controladorPrincipal() {
  if (estadoActual == ESPERA) return;
  
  float nivelAgua = leerNivel();
  unsigned long tiempoActual = millis() - tiempoInicio;
  
  // Control de llenado/desfogue
  switch(estadoActual) {
    case TANDA1:
    case TANDA2:
    case TANDA3:
      if (!desfogando) {
        if (nivelAgua < 2.5) {
          digitalWrite(PIN_INGRESAR_AGUA, HIGH);
          llenando = true;
        } else {
          digitalWrite(PIN_INGRESAR_AGUA, LOW);
          llenando = false;
        }
        realizarGiroMotor(); // Los giros continúan durante todo el proceso
      }
      break;
      
    case CENTRIFUGADO:
      digitalWrite(PIN_CENTRIFUGAR, HIGH);
      break;
      
    case ESPERA_FINAL:
      digitalWrite(PIN_CENTRIFUGAR, LOW);
      break;
  }
  
  // Control de tiempos y transiciones
  int tiempoTandaActual = 0;
  switch(estadoActual) {
    case TANDA1:
      tiempoTandaActual = programas[programaSeleccionado-1].tanda1 * 60000;
      if(tiempoActual >= tiempoTandaActual) {
        iniciarDesfogue();
        if(nivelAgua <= 0.5) {
          desfogando = false;
          estadoActual = TANDA2;
          tiempoInicio = millis();
        }
      }
      break;
      
    case TANDA2:
      tiempoTandaActual = programas[programaSeleccionado-1].tanda2 * 60000;
      if(tiempoActual >= tiempoTandaActual) {
        iniciarDesfogue();
        if(nivelAgua <= 0.5) {
          desfogando = false;
          estadoActual = TANDA3;
          tiempoInicio = millis();
        }
      }
      break;
      
    case TANDA3:
      tiempoTandaActual = programas[programaSeleccionado-1].tanda3 * 60000;
      if(tiempoActual >= tiempoTandaActual) {
        iniciarDesfogue();
        if(nivelAgua <= 0.5) {
          desfogando = false;
          estadoActual = CENTRIFUGADO;
          tiempoInicio = millis();
        }
      }
      break;
      
    case CENTRIFUGADO:
      if(tiempoActual >= 7 * 60000) { // 7 minutos
        estadoActual = ESPERA_FINAL;
        tiempoInicio = millis();
      }
      break;
      
    case ESPERA_FINAL:
      if(tiempoActual >= 2 * 60000) { // 2 minutos
        finalizarPrograma();
      }
      break;
  }
}

void realizarGiroMotor() {
  unsigned long tiempoGiroActual = (millis() - tiempoGiroInicio) % (7 * 60000); // Ciclo de 7 minutos
  
  if(tiempoGiroActual < 3 * 60000) { // 3 minutos derecha
    digitalWrite(PIN_GIRAR_DERECHA, HIGH);
    digitalWrite(PIN_GIRAR_IZQUIERDA, LOW);
  }
  else if(tiempoGiroActual < 4 * 60000) { // 1 minuto pausa
    digitalWrite(PIN_GIRAR_DERECHA, LOW);
    digitalWrite(PIN_GIRAR_IZQUIERDA, LOW);
  }
  else { // 3 minutos izquierda
    digitalWrite(PIN_GIRAR_DERECHA, LOW);
    digitalWrite(PIN_GIRAR_IZQUIERDA, HIGH);
  }
}

void iniciarDesfogue() {
  digitalWrite(PIN_INGRESAR_AGUA, LOW);
  digitalWrite(PIN_DESFOGAR, HIGH);
  digitalWrite(PIN_GIRAR_DERECHA, LOW);
  digitalWrite(PIN_GIRAR_IZQUIERDA, LOW);
  desfogando = true;
  llenando = false;
}

void finalizarPrograma() {
  apagarTodo();
  estadoActual = ESPERA;
  programaSeleccionado = 0;
  enviarComandoNextion("page 1"); // Volver al menú
}

void leerComandosNextion() {
  if (NextionSerial.available()) {
    String comando = "";
    while(NextionSerial.available()) {
      char c = NextionSerial.read();
      if(c >= 32 && c <= 126) {
        comando += c;
      }
    }
    
    if(comando != "") {
      procesarComando(comando);
    }
  }
}

void procesarComando(String comando) {
  if (comando.indexOf("programa1") >= 0) {
    programaSeleccionado = 1;
    calcularTiempoTotal();
  }
  else if (comando.indexOf("programa2") >= 0) {
    programaSeleccionado = 2;
    calcularTiempoTotal();
  }
  else if (comando.indexOf("programa3") >= 0) {
    programaSeleccionado = 3;
    calcularTiempoTotal();
  }
  else if (comando.indexOf("comenzar") >= 0) {
    if (emergencia) {
      emergencia = false;
      enPausa = false;
    } else if (programaSeleccionado > 0 && estadoActual == ESPERA) {
      iniciarPrograma();
    }
  }
  else if (comando.indexOf("emergencia") >= 0) {
    emergencia = true;
    enPausa = true;
  }
  else if (comando.indexOf("parar") >= 0) {
    if (emergencia) {
      pararPrograma();
    }
  }
}

void iniciarPrograma() {
  digitalWrite(PIN_BLOQUEAR_PUERTA, HIGH);
  estadoActual = TANDA1;
  tiempoInicio = millis();
  tiempoGiroInicio = millis();
  enPausa = false;
  llenando = true;
  desfogando = false;
}

void pararPrograma() {
  iniciarDesfogue();
  estadoActual = ESPERA;
  programaSeleccionado = 0;
  emergencia = false;
  enPausa = false;
}

void apagarTodo() {
  digitalWrite(PIN_BLOQUEAR_PUERTA, LOW);
  digitalWrite(PIN_GIRAR_DERECHA, LOW);
  digitalWrite(PIN_GIRAR_IZQUIERDA, LOW);
  digitalWrite(PIN_CENTRIFUGAR, LOW);
  digitalWrite(PIN_INGRESAR_AGUA, LOW);
  digitalWrite(PIN_DESFOGAR, LOW);
}

void calcularTiempoTotal() {
  if(programaSeleccionado > 0) {
    TiemposPrograma prog = programas[programaSeleccionado-1];
    tiempoTotal = (prog.tanda1 + prog.tanda2 + prog.tanda3 + 
                   prog.centrifugado + prog.esperaFinal) * 60;
  }
}

void actualizarDisplay() {
  // Actualizar temporizador MM:SS
  int segundosRestantes = tiempoTotal - (tiempoTranscurrido / 1000);
  int minutos = segundosRestantes / 60;
  int segundos = segundosRestantes % 60;
  
  String tiempoFormateado = 
    (minutos < 10 ? "0" : "") + String(minutos) + ":" + 
    (segundos < 10 ? "0" : "") + String(segundos);
  
  enviarComandoNextion("tiempo.txt=\"" + tiempoFormateado + "\"");
  
  // Actualizar estado
  String estadoTexto;
  switch(estadoActual) {
    case ESPERA:
      estadoTexto = "ESPERANDO";
      break;
    case TANDA1:
      estadoTexto = "TANDA 1";
      if(llenando) estadoTexto += " - LLENANDO";
      if(desfogando) estadoTexto += " - DESFOGANDO";
      break;
    case TANDA2:
      estadoTexto = "TANDA 2";
      if(llenando) estadoTexto += " - LLENANDO";
      if(desfogando) estadoTexto += " - DESFOGANDO";
      break;
    case TANDA3:
      estadoTexto = "TANDA 3";
      if(llenando) estadoTexto += " - LLENANDO";
      if(desfogando) estadoTexto += " - DESFOGANDO";
      break;
    case CENTRIFUGADO:
      estadoTexto = "CENTRIFUGADO";
      break;
    case ESPERA_FINAL:
      estadoTexto = "ESPERA FINAL";
      break;
  }
  
  if (emergencia) {
    estadoTexto += " (EMERGENCIA)";
  } else if (enPausa) {
    estadoTexto += " (PAUSA)";
  }
  
  enviarComandoNextion("estado.txt=\"" + estadoTexto + "\"");
}

void enviarComandoNextion(String comando) {
  NextionSerial.print(comando);
  NextionSerial.write(0xFF);
  NextionSerial.write(0xFF);
  NextionSerial.write(0xFF);
}