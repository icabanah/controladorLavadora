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
const uint8_t TIEMPO_PAUSA_GIRO = 30;
// const uint8_t TIEMPO_GIRO_DERECHA = 5;
// const uint8_t TIEMPO_GIRO_IZQUIERDA = 5;
// const uint8_t TIEMPO_PAUSA_GIRO = 3;
const uint16_t TIEMPO_CICLO_COMPLETO = TIEMPO_GIRO_DERECHA + TIEMPO_PAUSA_GIRO + TIEMPO_GIRO_IZQUIERDA + TIEMPO_PAUSA_GIRO;

// Tiempos fijos para procesos específicos
const uint8_t TIEMPO_DESFOGUE = 90;
const uint16_t TIEMPO_CENTRIFUGADO = 420; // 7*60
const uint16_t TIEMPO_CENTRIFUGADO_REAL = TIEMPO_CENTRIFUGADO - 60; // 7*60
const uint8_t TIEMPO_DESFOGUE_FINAL = 5;     // 2*60
const uint8_t TIEMPO_DETENIDO = 5;        // 5 segundos
const uint8_t TIEMPO_EMERGENCIA = 5;     // 2*60
// const uint8_t TIEMPO_DESFOGUE = 5;
// const uint16_t TIEMPO_CENTRIFUGADO = 5; // 7*60
// const uint8_t TIEMPO_EMERGENCIA = 5; // 2*60

// Tiempos de cada tanda en segundos (ya incluyen su tiempo de desfogue)
const uint16_t tiemposTanda[3][3] = {
    {1690, 1510, 910}, // Programa 1: 28*60+10, 25*60+10, 15*60+10 + 7*60 = 4530
    {1210, 910, 310},  // Programa 2: 20*60+10, 15*60+10, 5*60+10 + 7*60 =
    {910, 610, 310}    // Programa 3: 15*60+10, 10*60+10, 5*60+10 + 7*60 = 2250
                       // {120, 90, 60}, // Programa 1: 28*60+10, 25*60+10, 15*60+10 + 7*60 = 4530
                       // {90, 60, 45},  // Programa 2: 20*60+10, 15*60+10, 5*60+10 + 7*60 =
                       // {60, 45, 30}   // Programa 3: 15*60+10, 10*60+10, 5*60+10 + 7*60 = 2250
};

struct TiemposLavado
{
  unsigned long inicioSubEstado;
  unsigned long inicioTanda;
  unsigned long inicioDesfogue;
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
  DETENIMIENTO,
  EMERGENCIA
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
void procesarEmergencia();
void actualizarTiempo();
void configurarPaginaLavado();
void activarEmergencia();
void detenerPrograma();
void iniciarPrograma();

// Función para enviar comandos al Nextion
void enviarComandoNextion(String comando)
{
  Serial.print(comando);
  Serial.write(0xFF);
  Serial.write(0xFF);
  Serial.write(0xFF);
  // delay(10);
}

// Función para leer el nivel de agua
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

  // Inicialización de tiempos si es la primera vez
  if (tiempos.inicioTanda == 0)
  {
    tiempos.inicioTanda = tiempoActual;
    tiempos.inicioSubEstado = tiempoActual;
    // Asegurar que empiece con desfogue cerrado y entrada de agua abierta
    pines.desfogue = true;    // Cerrado
    pines.ingresoAgua = true; // Abierto
    pines.aplicar();
  }

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
  const unsigned long TIEMPO_ANTICIPADO_APAGADO = 60; // 1 minuto en segundos

  // Fase de inicio del centrifugado
  if (tiempos.inicioCentrifugado == 0)
  {
    // Verificaciones de seguridad antes de iniciar
    if (leerNivelAgua())
    {
      // Si aún hay agua, mantener desfogue abierto y esperar
      pines.desfogue = false;
      pines.centrifugado = false;
      pines.puertaBloqueada = true;
      pines.aplicar();
      return;
    }

    // Inicialización segura del centrifugado
    tiempos.inicioCentrifugado = tiempoActual;
    pines.desfogue = false;       // Mantener desfogue abierto durante centrifugado
    pines.puertaBloqueada = true; // Asegurar que la puerta esté bloqueada
    actualizarEstadoEnPantalla("Centrifugado");
  }

  // Control del proceso de centrifugado
  unsigned long tiempoTranscurrido = tiempoActual - tiempos.inicioCentrifugado;
  
  if (tiempoTranscurrido < (TIEMPO_CENTRIFUGADO * 1000))
  {
    // Verificación continua de seguridad
    if (!banderas.emergencia && !banderas.primeraPausaActiva)
    {
      // Desactivar el centrifugado un minuto antes de finalizar
      if (tiempoTranscurrido < ((TIEMPO_CENTRIFUGADO - TIEMPO_ANTICIPADO_APAGADO) * 1000))
      {
        pines.centrifugado = true;
      }
      else
      {
        // Último minuto sin centrifugado pero manteniendo el tiempo total
        pines.centrifugado = false;
        actualizarEstadoEnPantalla("Centrifugado - Finalizando");
      }
      
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
    enviarComandoNextion("t_mensajefinal.txt=\"DESFOGUE FINAL\"");
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
          enviarComandoNextion("bretroceder.tsw=1");  // retroceder
          enviarComandoNextion("t_programa.txt=\"" + String(programaSeleccionado) + "\"");
        }
        else if (comandoBuffer.indexOf("comenzar") >= 0)
        {
          if(!banderas.enProgreso){
            // enviarComandoNextion("page 2");
            enviarComandoNextion("b_emergencia.tsw=1"); // emergencia
            enviarComandoNextion("b_parar.tsw=1");      // parar
            enviarComandoNextion("b_comenzar.tsw=0");   // comenzar
            enviarComandoNextion("bretroceder.tsw=0");  // retroceder
            iniciarPrograma();
          }
        }
        else if (comandoBuffer.indexOf("parar") >= 0)
        {
          detenerPrograma();
        }
        else if (comandoBuffer.indexOf("emergencia") >= 0)
        {
          activarEmergencia();
        }
        else if (comandoBuffer.indexOf("regresar") >= 0)
        {
          if(!banderas.enProgreso){
            enviarComandoNextion("page 1");
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
  }

  // Fase de desfogue activo
  if (tiempoActual - tiempos.inicioDesfogueFinal < (TIEMPO_DESFOGUE_FINAL * 1000))
  {
    // Mantener el estado de desfogue constantemente
    pines.reset();
    pines.desfogue = false;       // Mantener desfogue abierto (LOW)
    pines.puertaBloqueada = true; // Mantener puerta bloqueada
    pines.aplicar();
  }
  // Fase de finalización
  else
  {
    tiempos.reset(); // Reiniciar todos los tiempos
    pines.reset();
    pines.desfogue = false; // Abrir desfogue
    pines.aplicar();

    estadoLavado = ESPERA;
    subEstado = LAVADO;
    banderas.reset();

    programaSeleccionado = 0;

    enviarComandoNextion("page 1");
  }
}

void procesarDetenimiento()
{
  unsigned long tiempoActual = millis();

  // Si es la primera vez que entramos al detenimiento
  if (tiempos.inicioDetenimiento == 0)
  {
    tiempos.inicioDetenimiento = tiempoActual;
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
  }
}

void procesarEmergencia()
{
  unsigned long tiempoActual = millis();

  // Si es la primera vez que entramos al detenimiento
  if (tiempos.inicioEmergencia == 0)
  {
    tiempos.inicioEmergencia = tiempoActual;
  }

  if (tiempoActual - tiempos.inicioEmergencia < (TIEMPO_EMERGENCIA * 1000))
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
    actualizarEstadoEnPantalla("Reanudado");

    // Restaurar estados de los pines según el estado actual
    switch (estadoLavado)
    {
    case TANDA1:
    case TANDA2:
    case TANDA3:
      if (subEstado == LAVADO)
      {
        pines.desfogue = true;                // Cerrar desfogue
        pines.ingresoAgua = !leerNivelAgua(); // Solo abrir si falta agua
      }
      break;
    case CENTRIFUGADO:
      pines.desfogue = false;
      pines.centrifugado = true;
      break;
    }
    pines.puertaBloqueada = true;
    pines.aplicar();
  }
  else
  {
    // Inicio nuevo del programa
    tiempos.reset();
    tiempos.inicioTanda = millis();
    tiempos.inicioSubEstado = millis();

    banderas.enProgreso = true;
    banderas.emergencia = false;
    estadoLavado = TANDA1;
    subEstado = LAVADO;

    pines.reset();
    pines.puertaBloqueada = true;
    pines.desfogue = true;    // Cerrar desfogue
    pines.ingresoAgua = true; // Iniciar llenado
    pines.giroDerecha = true; // Iniciar giro
    pines.aplicar();

    tiempos.tiempoRestante = calcularTiempoTotal();
    actualizarEstadoEnPantalla("Lavado 1 - derecha");
  }
}

void detenerPrograma()
{
  if (!banderas.primeraPausaActiva) // Pausar por primera vez
  {
    banderas.primeraPausaActiva = true;
    banderas.enProgreso = false;
    banderas.emergencia = false;

    pines.reset();
    pines.desfogue = true;
    pines.puertaBloqueada = true;
    pines.aplicar();
    actualizarEstadoEnPantalla("Pausado");
  }
  else // Segunda vez que se presiona parar
  {
    pines.reset();
    pines.desfogue = false;
    pines.puertaBloqueada = true;
    pines.aplicar();

    banderas.emergencia = false;
    banderas.enProgreso = true;
    banderas.primeraPausaActiva = false;
    errorTimeout = false;

    // Iniciar proceso de desfogue final
    estadoLavado = DETENIMIENTO;
    tiempos.inicioDetenimiento = millis();

    // Restaurar el programa seleccionado
    // programaSeleccionado = programaAnterior;

    // Cambiar a página de desfogue
    enviarComandoNextion("page 3");
    enviarComandoNextion("t_mensajefinal.pco=8");
    enviarComandoNextion("t_mensajefinal.txt=\"APERTURA DE PUERTA\"");
  }
}

void activarEmergencia()
{
  // tiempos.reset();
  pines.reset();
  pines.desfogue = false;
  pines.puertaBloqueada = true;
  pines.aplicar();

  banderas.emergencia = true;
  banderas.enProgreso = false;
  banderas.primeraPausaActiva = false;
  // errorTimeout = false;

  estadoLavado = EMERGENCIA;
  subEstado = LAVADO;
  tiempos.inicioEmergencia = millis();

  // Asegurar que volvemos a la página correcta después de la banderas.emergencia
  enviarComandoNextion("page 4");
  enviarComandoNextion("t_emergencia.pco=63488");
  enviarComandoNextion("t_emergencia.txt=\"EMERGENCIA\"");
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

void actualizarTiempo() {
    unsigned long ahora = millis();

    // Protección contra desbordamiento mejorada
    if (ahora < tiempos.ultimaActualizacion) {
        unsigned long diferencia = ULONG_MAX - tiempos.ultimaActualizacion + ahora;
        if (diferencia >= 1000) {
            tiempos.tiempoRestante--;
        }
        tiempos.ultimaActualizacion = ahora;
        tiempos.ultimaActualizacionSerial = ahora;
        return;
    }

    // Verificación de estado completa
    if (banderas.enProgreso && !banderas.emergencia && !banderas.primeraPausaActiva && !errorTimeout) {
        if ((ahora - tiempos.ultimaActualizacion) >= 1000) {
            if (tiempos.tiempoRestante > 0) {
                tiempos.tiempoRestante--;
                // Usar directamente la función formatearTiempo
                String tiempoFormateado = formatearTiempo(tiempos.tiempoRestante);
                enviarComandoNextion("tiempo.txt=\"" + tiempoFormateado + "\"");
            }
            tiempos.ultimaActualizacion = ahora;

            // Logging cada 5 segundos
            if ((ahora - tiempos.ultimaActualizacionSerial) >= INTERVALO_ACTUALIZACION_SERIAL) {
                tiempos.ultimaActualizacionSerial = ahora;
                // Implementar logging significativo aquí si es necesario
            }
        }
    }
}

String formatearTiempo(uint16_t segundos) {
    uint16_t horas = segundos / 3600;
    uint16_t minutos = (segundos % 3600) / 60;
    uint8_t segs = segundos % 60;
    
    // Asegurar formato HH:MM:SS con ceros a la izquierda
    return (horas < 10 ? "0" : "") + String(horas) + ":" +
           (minutos < 10 ? "0" : "") + String(minutos) + ":" +
           (segs < 10 ? "0" : "") + String(segs);
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
  pines.desfogue = false; // Por seguridad, desfogue abierto por defecto
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
    activarEmergencia();
  }
  lastBtnState = currentBtnState;

  procesarComandosNextion();

  // Actualizar el tiempo siempre, independientemente del estado
  actualizarTiempo();

  if (banderas.enProgreso && !banderas.emergencia)
  {
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

  if (estadoLavado == EMERGENCIA)
  {
    procesarEmergencia();
  }
}
