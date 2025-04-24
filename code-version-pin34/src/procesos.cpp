#include "../include/procesos.h"

void procesarTanda(int numeroTanda)
{
  unsigned long tiempoActual = millis();
  bool nivelLimiteAgua = leerNivelAguaEstable();
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
  if (numeroTanda == 3 && tiempos.tiempoRestante <= 0)
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
        // Preparar fase de preparación de centrifugado
        estadoLavado = PREPARACION_CENTRIFUGADO;
        tiempos.inicioPreparacionCentrifugado = tiempoActual;

        pines.reset();
        pines.desfogue = false; // Mantener desfogue abierto
        pines.giroDerecha = true;
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

void procesarPreparacionCentrifugado()
{
  unsigned long tiempoActual = millis();

  // Primera vez que entramos en este estado
  if (tiempos.inicioPreparacionCentrifugado == 0)
  {
    tiempos.inicioPreparacionCentrifugado = tiempoActual;
    pines.desfogue = false;      // Mantener desfogue abierto
    pines.giroDerecha = true;    // Giro a la derecha
    pines.giroIzquierda = false; // Asegurar que solo gira a la derecha
    pines.puertaBloqueada = true;
    pines.aplicar();
    actualizarEstadoEnPantalla("Preparando centrifugado");
  }

  // Verificar si terminó el tiempo de preparación
  if (tiempoActual - tiempos.inicioPreparacionCentrifugado >= (TIEMPO_PREPARACION_CENTRIFUGADO * 1000))
  {
    // Transición al estado de centrifugado manteniendo el giro derecho
    estadoLavado = CENTRIFUGADO;
    tiempos.inicioCentrifugado = tiempoActual; // Inicializar directamente

    // No resetear pines, solo activar el centrifugado manteniendo el giro derecho
    pines.centrifugado = true;
    pines.desfogue = false; // Mantener desfogue abierto
    pines.puertaBloqueada = true;
    // No modificar pines.giroDerecha para mantenerlo activado
    pines.aplicar();
  }
}

void procesarCentrifugado()
{
  unsigned long tiempoActual = millis();
  const unsigned long TIEMPO_ANTICIPADO_APAGADO = 30; // 30 segundos

  static unsigned long ultimaVerificacionNivel = 0;
  static unsigned int contadorIntentos = 0;
  const unsigned long INTERVALO_VERIFICACION = 5000; // 5 segundos entre verificaciones
  const unsigned int MAX_INTENTOS = 12; // 1 minuto máximo (12 * 5s)

  // Fase de inicio del centrifugado
  if (tiempos.inicioCentrifugado == 0)
  {
    // Verificaciones de seguridad antes de iniciar
    if (leerNivelAguaEstable() && estadoLavado != EMERGENCIA)
    {
      // Si aún hay agua, mantener desfogue abierto y esperar
      if (tiempoActual - ultimaVerificacionNivel >= INTERVALO_VERIFICACION) {
        ultimaVerificacionNivel = tiempoActual;
        contadorIntentos++;
        
        // Agregar log para verificación
        actualizarEstadoEnPantalla("Esperando desfogue " + String(contadorIntentos));
      }
      
      if (contadorIntentos >= MAX_INTENTOS) {
        actualizarEstadoEnPantalla("Forzando centrifugado");
        // Inicializar el centrifugado a pesar de la detección de agua
        tiempos.inicioCentrifugado = tiempoActual;
        contadorIntentos = 0;
      }
      
      pines.desfogue = false;
      pines.centrifugado = false;
      pines.giroDerecha = true;
      pines.giroIzquierda = false;
      pines.puertaBloqueada = true;
      pines.aplicar();
      return;
    }

    // Restablecer el contador de intentos
    contadorIntentos = 0;

    // Inicialización segura del centrifugado
    tiempos.inicioCentrifugado = tiempoActual;
    pines.desfogue = false;       // Mantener desfogue abierto durante centrifugado
    pines.centrifugado = true;    // Activar centrifugado
    pines.giroDerecha = true;     // Mantener giro derecho activado
    pines.giroIzquierda = false;  // Asegurar que no hay giro izquierdo
    pines.puertaBloqueada = true; // Asegurar que la puerta esté bloqueada
    pines.aplicar();
    actualizarEstadoEnPantalla("Centrifugado");
  }

  // Control del proceso de centrifugado
  unsigned long tiempoTranscurrido = tiempoActual - tiempos.inicioCentrifugado;

  if (tiempoTranscurrido < (TIEMPO_CENTRIFUGADO * 1000))
  {
    // Verificación continua de seguridad
    if (!banderas.emergencia && !banderas.primeraPausaActiva)
    {
      // Desactivar el centrifugado un tiempo antes de finalizar
      if (tiempoTranscurrido < ((TIEMPO_CENTRIFUGADO - TIEMPO_ANTICIPADO_APAGADO) * 1000))
      {
        pines.centrifugado = true;
        pines.giroDerecha = true; // Mantener giro derecho durante el centrifugado
      }
      else
      {
        // Último minuto sin centrifugado pero manteniendo el tiempo total
        pines.centrifugado = false;
        pines.giroDerecha = false; // Desactivar giro derecho en fase final
        actualizarEstadoEnPantalla("Finalizando");
      }

      pines.desfogue = false; // Mantener desfogue abierto
      pines.puertaBloqueada = true;
    }
    else
    {
      pines.centrifugado = false;
      pines.giroDerecha = false; // Desactivar giro en caso de emergencia
    }
    pines.aplicar();
  }
  else
  {
    // Finalización segura del centrifugado
    pines.centrifugado = false;
    pines.giroDerecha = false; // Desactivar giro derecho al finalizar
    pines.desfogue = false;    // Mantener desfogue abierto para fase final
    pines.aplicar();

    // Transición al siguiente estado
    estadoLavado = DESFOGUE_FINAL;
    tiempos.inicioDesfogueFinal = tiempoActual;
    // No resetear tiempos.inicioCentrifugado para mantener la referencia

    // Actualización de la interfaz
    enviarComandoNextion("page 3");
    enviarComandoNextion("t_mensajefinal.txt=\"DESFOGUE FINAL\"");
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
    banderas.reset(); // Esto debería reiniciar primeraPausaActiva
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
        pines.ingresoAgua = !leerNivelAguaEstable(); // Solo abrir si falta agua
      }
      break;
    case PREPARACION_CENTRIFUGADO:
      pines.desfogue = false;
      pines.giroDerecha = true;
      pines.giroIzquierda = false;
      break;
    case CENTRIFUGADO:
      pines.desfogue = false;
      pines.centrifugado = true;
      pines.giroDerecha = true;
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

    banderas.reset();
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

    // Cambiar a página de desfogue
    enviarComandoNextion("page 3");
    enviarComandoNextion("t_mensajefinal.pco=8");
    enviarComandoNextion("t_mensajefinal.txt=\"APERTURA DE PUERTA\"");
  }
}

void activarEmergencia()
{
  pines.reset();
  pines.desfogue = false;
  pines.puertaBloqueada = true;
  pines.aplicar();

  banderas.emergencia = true;
  banderas.enProgreso = false;
  banderas.primeraPausaActiva = false;

  estadoLavado = EMERGENCIA;
  subEstado = LAVADO;
  tiempos.inicioEmergencia = millis();

  // Asegurar que volvemos a la página correcta después de la banderas.emergencia
  enviarComandoNextion("page 4");
  enviarComandoNextion("t_emergencia.pco=63488");
  enviarComandoNextion("t_emergencia.txt=\"EMERGENCIA\"");
}
