// #include <HardwareSerial.h>
// HardwareSerial Serial(2);

// Configuración de pines
// #define RX_PIN 16
// #define TX_PIN 17
#define NIVEL_AGUA_PIN 34     // Pin analógico para el sensor de nivel
#define BTN_EMERGENCIA_PIN 15 // Usar el pin que tengas disponible
#define BLOQUEAR_PUERTA_PIN 13
#define GIRAR_DERECHA_PIN 12
#define GIRAR_IZQUIERDA_PIN 14
#define CENTRIFUGAR_PIN 27
#define INGRESAR_AGUA_PIN 26
#define DESFOGAR_PIN 25

// Configuración del ADC para el divisor de voltaje
const float VOLTAJE_MAX_ENTRADA = 3.8; // Voltaje máximo de entrada (antes del divisor)
const float VOLTAJE_MIN_ENTRADA = 0.5; // Voltaje mínimo de entrada
const float VOLTAJE_MAX_ADC = 3.3;     // Voltaje máximo del ADC
const float VOLTAJE_NIVEL = 3.0;       // Voltaje que indica nivel alcanzado (2.5V)
const uint8_t RESOLUCION_ADC = 12;     // Resolución del ADC
const uint16_t VALOR_MAX_ADC = 4095;   // Valor máximo del ADC (2^12 - 1)
// Factor de escala del divisor de voltaje
const float FACTOR_DIVISOR = VOLTAJE_MAX_ADC / VOLTAJE_MAX_ENTRADA;

// Tiempos en segundos para control de motor
const uint8_t TIEMPO_GIRO_DERECHA = 90; // 180 segundos
const uint8_t TIEMPO_GIRO_IZQUIERDA = 90;
const uint8_t TIEMPO_PAUSA_GIRO = 60;
// const uint8_t TIEMPO_GIRO_DERECHA = 5;
// const uint8_t TIEMPO_GIRO_IZQUIERDA = 5;
// const uint8_t TIEMPO_PAUSA_GIRO = 3;
const uint16_t TIEMPO_CICLO_COMPLETO = TIEMPO_GIRO_DERECHA + TIEMPO_PAUSA_GIRO + TIEMPO_GIRO_IZQUIERDA + TIEMPO_PAUSA_GIRO;

// Tiempos fijos para procesos específicos
const uint8_t TIEMPO_DESFOGUE = 120;      // 2 minutos
const uint16_t TIEMPO_CENTRIFUGADO = 420; // 7*60
const uint8_t TIEMPO_BLOQUEO_FINAL = 120; // 2*60
const uint8_t TIEMPO_DETENIDO = 5; // 5 segundos
// const uint8_t TIEMPO_DESFOGUE = 5;
// const uint16_t TIEMPO_CENTRIFUGADO = 5; // 7*60
// const uint8_t TIEMPO_BLOQUEO_FINAL = 5; // 2*60

// Tiempos de cada tanda en segundos (ya incluyen su tiempo de desfogue)
const uint16_t tiemposTanda[3][3] = {
    {1690, 1510, 910}, // Programa 1: 28*60+10, 25*60+10, 15*60+10 + 7*60 = 4530
    {1210, 910, 310},  // Programa 2: 20*60+10, 15*60+10, 5*60+10 + 7*60 =
    {910, 610, 310}    // Programa 3: 15*60+10, 10*60+10, 5*60+10 + 7*60 = 2250
                       // {120, 90, 60}, // Programa 1: 28*60+10, 25*60+10, 15*60+10 + 7*60 = 4530
                       // {90, 60, 45},  // Programa 2: 20*60+10, 15*60+10, 5*60+10 + 7*60 =
                       // {60, 45, 30}   // Programa 3: 15*60+10, 10*60+10, 5*60+10 + 7*60 = 2250
};

// Constantes de timeout y seguridad (en millisegundos)
const unsigned long TIMEOUT_LLENADO = 300000;           // 5 minutos máximo de llenado
const unsigned long TIMEOUT_DESFOGUE = 240000;          // 3 minutos máximo de desfogue
const unsigned long TIMEOUT_MOTOR = 600000;             // 10 minutos máximo de centrifugado
const unsigned long MAX_TIEMPO_DESFOGUE_FINAL = 300000; // 5 minutos máximo de desfogue final
const unsigned long MAX_TIEMPO_DETENIMIENTO = 60000; // 1 minuto máximo de desfogue final

// Estructura para manejar timeouts
struct TimeoutControl
{
  bool errorTimeout;
  unsigned long inicioOperacion;

  void iniciarTimeout()
  {
    errorTimeout = false;
    inicioOperacion = millis();
  }

  bool verificarTimeout(unsigned long limiteTimeout)
  {
    return (millis() - inicioOperacion) > limiteTimeout;
  }
};
TimeoutControl controlTimeout;

struct TiemposLavado
{
  unsigned long inicioSubEstado;
  unsigned long inicioTanda;
  unsigned long inicioDesfogue;
  unsigned long inicioCentrifugado;
  unsigned long inicioDesfogueFinal;
  unsigned long inicioDetenimiento;
  unsigned long ultimaActualizacion;
  unsigned long ultimaActualizacionSerial;
  unsigned long tiempoRestante;
  bool iniciado; // Flag para controlar la inicialización

  void reset()
  {
    inicioSubEstado = 0;
    inicioTanda = 0;
    inicioDesfogue = 0;
    inicioCentrifugado = 0;
    inicioDesfogueFinal = 0;
    inicioDetenimiento = 0;
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
    digitalWrite(BLOQUEAR_PUERTA_PIN, puertaBloqueada);
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

// Variables globales a agregar
TiemposLavado tiempos;
EstadoPines pines;
BanderasProceso banderas;
bool errorTimeout = false;

// Variables de estado y control
uint8_t programaSeleccionado = 0;

String comandoBuffer = "";
String estadoActual = "";
const uint16_t INTERVALO_ACTUALIZACION_SERIAL = 5000;

// Estados del ciclo de lavado
enum EstadoLavado
{
  ESPERA,
  TANDA1,
  TANDA2,
  TANDA3,
  CENTRIFUGADO,
  DESFOGUE_FINAL,
  DETENIMIENTO
};
EstadoLavado estadoLavado = ESPERA;

enum SubEstadoTanda
{
  LAVADO,
  DESFOGUE
};
SubEstadoTanda subEstado = LAVADO;

// Declaraciones anticipadas de funciones
void enviarComandoNextion(String comando);
void actualizarEstadoEnPantalla(String nuevoEstado);
void iniciarDesfogueFinal();
void procesarTanda(int numeroTanda);
void procesarCentrifugado();
void procesarDesfogueFinal();
void procesarDetenimiento();
void actualizarTiempo();
void configurarPaginaLavado();
void activarEmergencia(String mensaje);
void detenerPrograma();
void iniciarPrograma();

bool verificarTimeouts()
{
  unsigned long tiempoActual = millis();
  bool timeout = false;
  String mensajeError = "";

  switch (estadoLavado)
  {
  case TANDA1:
  case TANDA2:
  case TANDA3:
    if (subEstado == LAVADO && !leerNivelAgua())
    {
      // Verificar timeout de llenado
      if (pines.ingresoAgua && controlTimeout.verificarTimeout(TIMEOUT_LLENADO))
      {
        timeout = true;
        mensajeError = "Timeout Llenado";
        // actualizarEstadoEnPantalla("Timeout Llenado");
      }
    }
    else if (subEstado == DESFOGUE)
    {
      // Verificar timeout de desfogue
      if (!pines.desfogue && controlTimeout.verificarTimeout(TIMEOUT_DESFOGUE))
      {
        timeout = true;
        mensajeError = "Timeout Desfogue";
        // actualizarEstadoEnPantalla("Timeout Desfogue");
      }
    }
    break;

  case CENTRIFUGADO:
    // Verificar timeout de motor en centrifugado
    if (pines.centrifugado && controlTimeout.verificarTimeout(TIMEOUT_MOTOR))
    {
      timeout = true;
      mensajeError = "Timeout Motor";
      // actualizarEstadoEnPantalla("Timeout Motor");
    }
    break;

  case DESFOGUE_FINAL:
    // Verificar timeout de desfogue final
    if (!pines.desfogue && controlTimeout.verificarTimeout(MAX_TIEMPO_DESFOGUE_FINAL))
    {
      timeout = true;
      mensajeError = "Timeout Desfogue";
      // actualizarEstadoEnPantalla("Error: Timeout Desfogue Final");
    }
    break;

  case DETENIMIENTO:
    // Verificar timeout de desfogue final
    if (!pines.desfogue && controlTimeout.verificarTimeout(MAX_TIEMPO_DETENIMIENTO))
    {
      timeout = true;
      mensajeError = "Timeout detenimiento";
      // actualizarEstadoEnPantalla("Error: Timeout Desfogue Final");
    }
    break;
  }

  if (timeout)
  {
    controlTimeout.errorTimeout = true;
    activarEmergencia(mensajeError);
  }

  return timeout;
}

void enviarComandoNextion(String comando)
{
  Serial.print(comando);
  Serial.write(0xFF);
  Serial.write(0xFF);
  Serial.write(0xFF);
  // delay(10);
}

bool leerNivelAgua()
{
  int valorADC = analogRead(NIVEL_AGUA_PIN);

  // Convertir el valor ADC a voltaje real (antes del divisor)
  float voltajeLeido = (valorADC * VOLTAJE_MAX_ENTRADA) / VALOR_MAX_ADC;

  // Para depuración
  static unsigned long ultimoLog = 0;
  if (millis() - ultimoLog > 1000)
  {
    ultimoLog = millis();
  }

  // Comparar con el nivel deseado (2.5V)
  return voltajeLeido >= VOLTAJE_NIVEL;
}

void actualizarEstadoEnPantalla(String nuevoEstado)
{
  if (nuevoEstado != estadoActual)
  {
    estadoActual = nuevoEstado;
    enviarComandoNextion("estado.txt=\"" + estadoActual + "\"");
  }
}

void procesarTanda(int numeroTanda)
{
  unsigned long tiempoActual = millis();
  bool nivelLimiteAgua = leerNivelAgua();
  static String estadoAnterior = "";

  // // Debug logging
  // static unsigned long ultimoLog = 0;
  // if (tiempoActual - ultimoLog > 1000)
  // { // Log cada segundo
  //   Serial.println("Estado: " + String(estadoLavado) +
  //                  " SubEstado: " + String(subEstado) +
  //                  " Tiempo restante: " + String(tiempos.tiempoRestante) +
  //                  " Nivel agua: " + String(nivelLimiteAgua));
  //   ultimoLog = tiempoActual;
  // }

  // Inicialización de tiempos si es la primera vez
  if (tiempos.inicioTanda == 0)
  {
    tiempos.inicioTanda = tiempoActual;
    tiempos.inicioSubEstado = tiempoActual;

    controlTimeout.iniciarTimeout(); // Iniciar timeout para el nuevo estado

    // Asegurar que empiece con desfogue cerrado y entrada de agua abierta
    pines.desfogue = true;    // Cerrado
    pines.ingresoAgua = true; // Abierto
    pines.aplicar();
  }

  // Al cambiar de subEstado
  if (subEstado == LAVADO && tiempoActual - tiempos.inicioTanda >=
                                 (tiemposTanda[programaSeleccionado - 1][numeroTanda - 1] - TIEMPO_DESFOGUE) * 1000)
  {
    subEstado = DESFOGUE;
    tiempos.inicioDesfogue = tiempoActual;
    tiempos.inicioSubEstado = tiempoActual;
    controlTimeout.iniciarTimeout(); // Reiniciar timeout para desfogue

    pines.giroDerecha = false;
    pines.giroIzquierda = false;
    pines.aplicar();
  }

  // enviarComandoNextion("b3.tsw = 0"); // Ocultar botón de volver a lavar

  // Cálculo del tiempo transcurrido en el ciclo actual
  unsigned long tiempoTranscurridoEnCiclo = (tiempoActual - tiempos.inicioSubEstado) % (TIEMPO_CICLO_COMPLETO * 1000);

  // Verificación de fin de programa
  if (tiempos.tiempoRestante <= 0)
  {
    procesarDesfogueFinal();
    return;
  }

  String estadoActual = "Lavado " + String(numeroTanda) + " - ";

  switch (subEstado)
  {
  case LAVADO:
    // Control del nivel de agua
    if (nivelLimiteAgua)
    {
      pines.ingresoAgua = false;
      pines.aplicar();
    }

    // Control de giro del motor
    if (tiempoTranscurridoEnCiclo < (TIEMPO_GIRO_DERECHA * 1000))
    {
      // Giro derecha
      pines.giroDerecha = true;
      pines.giroIzquierda = false;
      estadoActual += "derecha";
    }
    else if (tiempoTranscurridoEnCiclo < ((TIEMPO_GIRO_DERECHA + TIEMPO_PAUSA_GIRO) * 1000))
    {
      // Pausa entre giros
      pines.giroDerecha = false;
      pines.giroIzquierda = false;
      estadoActual += "pausa";
    }
    else if (tiempoTranscurridoEnCiclo < ((TIEMPO_GIRO_DERECHA + TIEMPO_PAUSA_GIRO + TIEMPO_GIRO_IZQUIERDA) * 1000))
    {
      // Giro izquierda
      pines.giroDerecha = false;
      pines.giroIzquierda = true;
      estadoActual += "izquierda";
    }
    else
    {
      // Pausa final
      pines.giroDerecha = false;
      pines.giroIzquierda = false;
      estadoActual += "pausa";
    }

    // Verificar si es tiempo de cambiar a desfogue
    if (tiempoActual - tiempos.inicioTanda >= (tiemposTanda[programaSeleccionado - 1][numeroTanda - 1] - TIEMPO_DESFOGUE) * 1000)
    {
      subEstado = DESFOGUE;
      tiempos.inicioDesfogue = tiempoActual;
      tiempos.inicioSubEstado = tiempoActual;

      // Detener motores antes de desfogue
      pines.giroDerecha = false;
      pines.giroIzquierda = false;
      pines.aplicar();
    }
    break;

  case DESFOGUE:
    estadoActual = "Desfogue " + String(numeroTanda);

    if (tiempoActual - tiempos.inicioDesfogue < (TIEMPO_DESFOGUE * 1000))
    {
      // Fase de desfogue activo
      pines.reset();
      pines.desfogue = false; // Abrir desfogue
      pines.puertaBloqueada = true;
      pines.aplicar();
    }
    else
    {
      // Fin del desfogue - preparar siguiente fase
      if (numeroTanda < 3)
      {
        // Preparar siguiente tanda
        estadoLavado = (EstadoLavado)((int)estadoLavado + 1);
        subEstado = LAVADO;
        tiempos.inicioTanda = 0; // Se inicializará en la siguiente iteración
        tiempos.inicioSubEstado = 0;

        pines.reset();
        pines.desfogue = true;    // Cerrar desfogue
        pines.ingresoAgua = true; // Iniciar nuevo llenado
        pines.puertaBloqueada = true;
        pines.aplicar();

        banderas.enProgreso = true;
      }
      else
      {
        // Preparar centrifugado
        estadoLavado = CENTRIFUGADO;
        tiempos.inicioTanda = 0;

        pines.reset();
        pines.desfogue = false; // Mantener desfogue abierto
        pines.centrifugado = true;
        pines.puertaBloqueada = true;
        pines.aplicar();

        banderas.enProgreso = true;
      }
    }
    break;
  }

  // Actualizar estado en pantalla si cambió
  if (estadoActual != estadoAnterior)
  {
    actualizarEstadoEnPantalla(estadoActual);
    estadoAnterior = estadoActual;
  }

  // Aplicar cambios en los pines si hubo modificaciones
  pines.aplicar();
}

void procesarCentrifugado()
{
  unsigned long tiempoActual = millis();

  // Fase de inicio del centrifugado
  if (tiempos.inicioCentrifugado == 0)
  {
    if (tiempos.inicioCentrifugado == 0)
    {
      if (leerNivelAgua())
      {
        pines.desfogue = false;
        pines.centrifugado = false;
        pines.puertaBloqueada = true;
        pines.aplicar();
        return;
      }

      tiempos.inicioCentrifugado = tiempoActual;
      controlTimeout.iniciarTimeout(); // Iniciar timeout para centrifugado

      pines.desfogue = false;
      pines.puertaBloqueada = true;
      actualizarEstadoEnPantalla("Centrifugado");
    }

    // Inicialización segura del centrifugado
    tiempos.inicioCentrifugado = tiempoActual;
    pines.desfogue = false;       // Mantener desfogue abierto durante centrifugado
    pines.puertaBloqueada = true; // Asegurar que la puerta esté bloqueada
    actualizarEstadoEnPantalla("Centrifugado");
  }

  // Control del proceso de centrifugado
  if (tiempoActual - tiempos.inicioCentrifugado < (TIEMPO_CENTRIFUGADO * 1000))
  {
    // Verificación continua de seguridad
    if (!banderas.emergencia && !banderas.primeraPausaActiva)
    {
      pines.centrifugado = true;
      pines.desfogue = false; // Mantener desfogue abierto
      pines.puertaBloqueada = true;
    }
    else
    {
      pines.centrifugado = false;
    }
  }
  else
  {
    // Finalización segura del centrifugado
    pines.centrifugado = false;
    pines.desfogue = false; // Mantener desfogue abierto para fase final

    // Transición al siguiente estado
    estadoLavado = DESFOGUE_FINAL;
    tiempos.inicioDesfogueFinal = tiempoActual;
    tiempos.inicioCentrifugado = 0;

    // Actualización de la interfaz
    enviarComandoNextion("page 3");
    enviarComandoNextion("t0.txt=\"DESFOGUE FINAL\"");
  }

  // Aplicar todos los cambios de estado
  pines.aplicar();
}

void procesarComandosNextion()
{
  while (Serial.available())
  {
    char c = Serial.read();
    if (c >= 32 && c <= 126)
    {
      comandoBuffer += c;
    }
    if (c == 0xFF || comandoBuffer.length() > 20)
    {
      if (comandoBuffer.length() > 0)
      {
        if (comandoBuffer.indexOf("programa") >= 0)
        {
          programaSeleccionado = comandoBuffer.charAt(comandoBuffer.length() - 1) - '0';

          enviarComandoNextion("page 2");
          enviarComandoNextion("b_emergencia.tsw=0"); // emergencia
          enviarComandoNextion("b_parar.tsw=0");      // parar
          enviarComandoNextion("b_comenzar.tsw=1");   // comenzar
        }
        else if (comandoBuffer.indexOf("comenzar") >= 0)
        {
          enviarComandoNextion("page 2");
          enviarComandoNextion("b3.tsw = 0");           // Ocultar botón de volver a lavar
          enviarComandoNextion("btn_comenzar.tsw = 0"); // Ocultar botón de reanudar
          iniciarPrograma();
        }
        else if (comandoBuffer.indexOf("parar") >= 0)
        {
          if (banderas.enProgreso || banderas.primeraPausaActiva)
          {
            enviarComandoNextion("page 2");
            enviarComandoNextion("btn_comenzar.tsw = 1"); // Mostrar botón de reanudar
            detenerPrograma();
            // enviarComandoNextion("b3.tsw = 1");           // Mostrar botón de volver a lavar
          }
        }
        else if (comandoBuffer.indexOf("emergencia") >= 0)
        {
          if (banderas.enProgreso || banderas.primeraPausaActiva)
          {
            enviarComandoNextion("page 2");
            enviarComandoNextion("b3.tsw = 1"); // Mostrar botón de volver   a lavar
            enviarComandoNextion("btn_comenzar.tsw = 1"); // Mostrar botón de reanudar
            activarEmergencia("EMEGENCIA");
          }
        }
        comandoBuffer = "";
      }
    }
  }
}

void procesarDesfogueFinal()
{
  unsigned long tiempoActual = millis();

  // Si es la primera vez que entramos al desfogue final
  if (tiempos.inicioDesfogueFinal == 0)
  {
    tiempos.inicioDesfogueFinal = tiempoActual;
    controlTimeout.iniciarTimeout(); // Iniciar timeout para desfogue final
  }

  if (tiempoActual - tiempos.inicioDesfogueFinal < (TIEMPO_BLOQUEO_FINAL * 1000))
  {
    pines.reset();
    pines.desfogue = false;
    pines.puertaBloqueada = true;
    pines.aplicar();
  }
  else
  {
    tiempos.reset();
    pines.reset();
    pines.desfogue = false;
    pines.aplicar();

    estadoLavado = ESPERA;
    subEstado = LAVADO;
    banderas.reset();
    programaSeleccionado = 0;

    enviarComandoNextion("page 1");
    enviarComandoNextion("b3.tsw = 1");
  }
}

void procesarDetenimiento()
{
  unsigned long tiempoActual = millis();

  // Si es la primera vez que entramos al desfogue final
  if (tiempos.inicioDetenimiento == 0)
  {
    tiempos.inicioDetenimiento = tiempoActual;
    controlTimeout.iniciarTimeout(); // Iniciar timeout para desfogue final
  }

  if (tiempoActual - tiempos.inicioDetenimiento < (TIEMPO_DETENIDO * 1000))
  {
    pines.reset();
    pines.desfogue = false;
    pines.puertaBloqueada = true;
    pines.aplicar();
  }
  else
  {
    tiempos.reset();
    pines.reset();
    pines.desfogue = false;
    pines.aplicar();

    estadoLavado = ESPERA;
    subEstado = LAVADO;
    banderas.reset();
    programaSeleccionado = 0;

    enviarComandoNextion("page 1");
    enviarComandoNextion("b3.tsw = 1");
  }
}

void iniciarPrograma()
{
  // enviarComandoNextion("b3.tsw = 0"); // Deshabilitar pulsacción de botón de vuelta

  if (banderas.primeraPausaActiva)
  {
    // Si estamos reanudando desde pausa, solo restauramos las banderas
    banderas.primeraPausaActiva = false;
    banderas.enProgreso = true;
    controlTimeout.iniciarTimeout(); // Reiniciar timeout al reanudar

    // Restaurar estados de los pines según el estado actual
    switch (estadoLavado)
    {
    case TANDA1:
    case TANDA2:
    case TANDA3:
      if (subEstado == LAVADO)
      {
        pines.desfogue = true; // Cerrar desfogue
        // pines.ingresoAgua = !leerNivelAgua(); // Solo abrir si falta agua
      }
      break;
    case CENTRIFUGADO:
      pines.desfogue = false;
      pines.centrifugado = true;
      break;
    }
    pines.puertaBloqueada = true;
    pines.aplicar();
    // enviarComandoNextion("btn_comenzar.tsw = 0"); // Deshabilitar pulsacción de botón en pantalla
  }
  else
  {
    // Inicio nuevo del programa
    tiempos.reset();
    tiempos.inicioTanda = millis();
    tiempos.inicioSubEstado = millis();
    controlTimeout.iniciarTimeout(); // Iniciar timeout para nuevo programa

    banderas.enProgreso = true;
    banderas.emergencia = false;
    estadoLavado = TANDA1;
    subEstado = LAVADO;

    pines.reset();
    pines.puertaBloqueada = true;
    pines.desfogue = true;    // Cerrar desfogue
    pines.ingresoAgua = true; // Iniciar llenado
    pines.aplicar();

    tiempos.tiempoRestante = calcularTiempoTotal();
    actualizarEstadoEnPantalla("Lavado 1");
    // enviarComandoNextion("btn_comenzar.tsw = 0");
  }
}

void detenerPrograma()
{
  if (!banderas.primeraPausaActiva)
  {
    banderas.primeraPausaActiva = true;
    banderas.enProgreso = false;
    banderas.emergencia = false;
    // No reiniciamos el timeout aquí ya que el programa está pausado

    pines.reset();
    pines.desfogue = true;
    pines.puertaBloqueada = true;
    pines.aplicar();
    actualizarEstadoEnPantalla("Pausado");
    enviarComandoNextion("btn_comenzar.tsw = 1");
  }
  else
  {
    pines.reset();
    pines.desfogue = false;
    pines.puertaBloqueada = true;
    pines.aplicar();

    banderas.emergencia = false;
    banderas.enProgreso = true;
    banderas.primeraPausaActiva = false;
    errorTimeout = false;

    estadoLavado = DETENIMIENTO;
    tiempos.inicioDetenimiento = millis();
    controlTimeout.iniciarTimeout(); // Iniciar timeout para desfogue final

    enviarComandoNextion("page 3");
    enviarComandoNextion("t0.txt=\"DETENIDO\"");
  }
}

void activarEmergencia(String mensaje = "EMERGENCIA")
{
  // tiempos.reset();
  pines.reset();
  pines.desfogue = false;
  pines.puertaBloqueada = true;
  pines.aplicar();

  banderas.emergencia = true;
  banderas.enProgreso = false;
  banderas.primeraPausaActiva = false;
  errorTimeout = false;

  estadoLavado = DESFOGUE_FINAL;
  subEstado = LAVADO;
  tiempos.inicioDesfogueFinal = millis();

  // Asegurar que volvemos a la página correcta después de la banderas.emergencia
  enviarComandoNextion("page 3");
  enviarComandoNextion("t0.txt=\"" + mensaje + "\"");
}

uint16_t calcularTiempoTotal()
{
  int total = 0;
  // Suma de tiempos de cada tanda (que ya incluyen sus desfogues)
  for (int i = 0; i < 3; i++)
  {
    total += tiemposTanda[programaSeleccionado - 1][i];
  }
  // Agregar solo el tiempo de centrifugado
  total += TIEMPO_CENTRIFUGADO;
  return total;
}

void actualizarTiempo()
{
  unsigned long ahora = millis();

  // Protección contra desbordamiento mejorada
  if (ahora < tiempos.ultimaActualizacion)
  {
    unsigned long diferencia = ULONG_MAX - tiempos.ultimaActualizacion + ahora;
    if (diferencia >= 1000)
    {
      tiempos.tiempoRestante--;
    }
    tiempos.ultimaActualizacion = ahora;
    tiempos.ultimaActualizacionSerial = ahora;
    return;
  }

  // Verificación de estado completa
  if (banderas.enProgreso && !banderas.emergencia && !banderas.primeraPausaActiva && !errorTimeout)
  {
    if ((ahora - tiempos.ultimaActualizacion) >= 1000)
    {
      if (tiempos.tiempoRestante > 0)
      {
        tiempos.tiempoRestante--;
        char buffer[10];
        snprintf(buffer, sizeof(buffer), "%02d:%02d",
                 tiempos.tiempoRestante / 60, tiempos.tiempoRestante % 60);
        enviarComandoNextion("tiempo.txt=\"" + String(buffer) + "\"");
      }
      tiempos.ultimaActualizacion = ahora;

      // Logging cada 5 segundos
      if ((ahora - tiempos.ultimaActualizacionSerial) >= INTERVALO_ACTUALIZACION_SERIAL)
      {
        tiempos.ultimaActualizacionSerial = ahora;
        // Implementar logging significativo aquí
      }
    }
  }
}

String formatearTiempo(uint16_t segundos)
{
  uint16_t minutos = segundos / 60;
  uint8_t segs = segundos % 60;
  // Asegurar formato MM:SS con ceros a la izquierda
  return (minutos < 10 ? "0" : "") + String(minutos) + ":" + (segs < 10 ? "0" : "") + String(segs);
}

bool verificarSeguridadPuerta()
{
  // La puerta solo debe desbloquearse cuando:
  // 1. El programa no está en progreso
  // 2. No hay agua en la lavadora
  // 3. No hay movimiento de motor

  if (banderas.enProgreso ||
      leerNivelAgua() ||
      digitalRead(GIRAR_DERECHA_PIN) == HIGH ||
      digitalRead(GIRAR_IZQUIERDA_PIN) == HIGH ||
      digitalRead(CENTRIFUGAR_PIN) == HIGH)
  {

    digitalWrite(BLOQUEAR_PUERTA_PIN, HIGH);
    return false;
  }
  return true;
}

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
  // Agregar después del código actual de setup
  pines.reset();
  pines.desfogue = false; // Por seguridad, desfogue cerrado por defecto
  pines.aplicar();
  banderas.reset();
  tiempos.reset();
}

void loop()
{
  // Revisar el botón físico de banderas.emergencia
  static bool lastBtnState = HIGH; // Estado anterior del botón
  bool currentBtnState = digitalRead(BTN_EMERGENCIA_PIN);

  if (currentBtnState == LOW && lastBtnState == HIGH && (banderas.enProgreso || banderas.primeraPausaActiva)) // Botón presionado
  {
    activarEmergencia("EMERGENCIA");
  }
  lastBtnState = currentBtnState;

  procesarComandosNextion();

  // Actualizar el tiempo siempre, independientemente del estado
  actualizarTiempo();

  if (banderas.enProgreso && !banderas.emergencia)
  {
    if (verificarTimeouts())
    {
      return;
    }

    switch (estadoLavado)
    {
    case ESPERA:
      // actualizarEstadoEnPantalla("Espera");
      break;
    case TANDA1:
    case TANDA2:
    case TANDA3:
      procesarTanda(estadoLavado - TANDA1 + 1);
      break;
    case CENTRIFUGADO:
      procesarCentrifugado();
      break;
    }
  }

  if (estadoLavado == DESFOGUE_FINAL)
  {
    procesarDesfogueFinal();
  }

  if (estadoLavado == DETENIMIENTO)
  {
    procesarDetenimiento();
  }
}