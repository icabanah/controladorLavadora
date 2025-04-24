#include "../include/funciones_utiles.h"
#include "../include/comunicacion.h"

// Función modificada para muestreo más estable del nivel de agua
bool leerNivelAguaEstable()
{
  // Tomar varias muestras y promediar para mayor estabilidad
  const int NUM_MUESTRAS = 5;
  float sumaMuestras = 0;
  
  for (int i = 0; i < NUM_MUESTRAS; i++) {
    int valorADC = analogRead(NIVEL_AGUA_PIN);
    float voltajeLeido = (valorADC * VOLTAJE_MAX_ENTRADA) / VALOR_MAX_ADC;
    sumaMuestras += voltajeLeido;
    // Pequeña pausa entre muestras - Reemplazar con un enfoque no bloqueante
    delayMicroseconds(100); // Microsegundos en vez de delay para no bloquear
  }
  
  float promedioVoltaje = sumaMuestras / NUM_MUESTRAS;
  
  // En el caso del centrifugado usamos un umbral más bajo para evitar falsos positivos
  if (estadoLavado == CENTRIFUGADO || estadoLavado == PREPARACION_CENTRIFUGADO) {
    return promedioVoltaje >= (VOLTAJE_NIVEL - 0.5); // Umbral más bajo durante centrifugado
  }
  
  return promedioVoltaje >= VOLTAJE_NIVEL;
}

void actualizarEstadoEnPantalla(String nuevoEstado)
{
  if (nuevoEstado != estadoActual)
  {
    estadoActual = nuevoEstado;
    enviarComandoNextion("estado.txt=\"" + estadoActual + "\"");
  }
}

uint16_t calcularTiempoTotal()
{
  int total = 0;
  // Suma de tiempos de cada tanda (que ya incluyen sus desfogues)
  for (int i = 0; i < 3; i++)
  {
    total += tiemposTanda[programaSeleccionado - 1][i];
  }
  // Agregar tiempo de preparación, centrifugado y desfogue final
  total += TIEMPO_PREPARACION_CENTRIFUGADO + TIEMPO_CENTRIFUGADO + TIEMPO_DESFOGUE_FINAL;
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
        // Usar directamente la función formatearTiempo
        String tiempoFormateado = formatearTiempo(tiempos.tiempoRestante);
        enviarComandoNextion("tiempo.txt=\"" + tiempoFormateado + "\"");
      }
      tiempos.ultimaActualizacion = ahora;

      // Logging cada 5 segundos
      if ((ahora - tiempos.ultimaActualizacionSerial) >= INTERVALO_ACTUALIZACION_SERIAL)
      {
        tiempos.ultimaActualizacionSerial = ahora;
        // Implementar logging significativo aquí si es necesario
      }
    }
  }
}

String formatearTiempo(uint16_t segundos)
{
  uint16_t horas = segundos / 3600;
  uint16_t minutos = (segundos % 3600) / 60;
  uint8_t segs = segundos % 60;

  // Asegurar formato HH:MM:SS con ceros a la izquierda
  return (horas < 10 ? "0" : "") + String(horas) + ":" +
         (minutos < 10 ? "0" : "") + String(minutos) + ":" +
         (segs < 10 ? "0" : "") + String(segs);
}
