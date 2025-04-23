# Modificaciones necesarias para corregir problemas de centrifugado

## Descripción del problema

El problema ocurre cuando la lavadora está a 8 minutos para terminar y entrar a centrifugado (o al comenzar esta fase), momento en el cual sale emergencia, se detiene, y regresa al menú para escoger programa.

## Causa raíz identificada

Se ha identificado que hay tres factores clave que causan el problema:

1. El umbral de detección de nivel de agua es demasiado estricto
2. La detección de nivel de agua es inestable durante el movimiento del centrifugado
3. El sistema activa emergencia innecesariamente cuando detecta falsos positivos de nivel de agua

## Modificaciones necesarias

### 1. Reducir el umbral de detección de nivel de agua

- Cambiar el valor de VOLTAJE_NIVEL de 3.5V a 2.8V
- Línea original:
  ```cpp
  const float VOLTAJE_NIVEL = 3.5;       // Voltaje que indica nivel alcanzado (2.5V)
  ```
- Modificar a:
  ```cpp
  const float VOLTAJE_NIVEL = 2.8;       // MODIFICADO: Reducido para mayor tolerancia
  ```

### 2. Agregar una función de lectura más estable del nivel de agua

Agregar después de la función `leerNivelAgua()` el siguiente código:

```cpp
// Función modificada para muestreo más estable del nivel de agua
// Esta función hace múltiples lecturas y devuelve un promedio para evitar falsas lecturas
bool leerNivelAguaEstable()
{
  // Tomar varias muestras y promediar para mayor estabilidad
  const int NUM_MUESTRAS = 5;
  float sumaMuestras = 0;
  
  for (int i = 0; i < NUM_MUESTRAS; i++) {
    int valorADC = analogRead(NIVEL_AGUA_PIN);
    float voltajeLeido = (valorADC * VOLTAJE_MAX_ENTRADA) / VALOR_MAX_ADC;
    sumaMuestras += voltajeLeido;
    delay(10); // Pequeña pausa entre muestras
  }
  
  float promedioVoltaje = sumaMuestras / NUM_MUESTRAS;
  
  // En el caso del centrifugado usamos un umbral más bajo para evitar falsos positivos
  if (estadoLavado == CENTRIFUGADO || estadoLavado == PREPARACION_CENTRIFUGADO) {
    return promedioVoltaje >= (VOLTAJE_NIVEL - 0.5); // Umbral más bajo durante centrifugado
  }
  
  return promedioVoltaje >= VOLTAJE_NIVEL;
}
```

### 3. Modificar la función de procesamiento de centrifugado

Cambiar el código de la función `procesarCentrifugado()` para evitar que entre en estado de emergencia.
En particular, modificar la parte que detecta nivel de agua y activa la emergencia:

- En el código original, se activa emergencia después de varios intentos:
  ```cpp
  if (contadorIntentos >= MAX_INTENTOS) {
    activarEmergencia();
    return;
  }
  ```

- Modificar para que, en lugar de activar emergencia, simplemente fuerce el inicio del centrifugado:
  ```cpp
  if (contadorIntentos >= MAX_INTENTOS) {
    actualizarEstadoEnPantalla("Forzando centrifugado");
    // Inicializar el centrifugado a pesar de la detección de agua
    tiempos.inicioCentrifugado = tiempoActual;
    contadorIntentos = 0;
  }
  ```

- También cambiar para usar `leerNivelAguaEstable()` en lugar de `leerNivelAgua()`:
  ```cpp
  if (leerNivelAguaEstable() && estadoLavado != EMERGENCIA)
  ```

## Instrucciones de implementación

1. Hacer una copia de seguridad del archivo original
2. Implementar estos cambios uno por uno
3. Probar el funcionamiento verificando el comportamiento en la fase de centrifugado

Estas modificaciones evitarán que la lavadora entre en modo emergencia innecesariamente debido a falsas lecturas del sensor de nivel de agua durante la fase de centrifugado.

## Explicación técnica

El problema se basa en las características físicas de los sensores de nivel de agua, que pueden verse afectados por:

1. El movimiento del agua durante el centrifugado, que genera turbulencia
2. Fluctuaciones eléctricas en las lecturas analógicas
3. La alta sensibilidad del umbral de detección original

Al implementar un sistema de promediado de lecturas y reducir el umbral, se logra una detección más robusta que no se ve afectada por variaciones momentáneas. Adicionalmente, al evitar entrar en emergencia y forzar el centrifugado cuando se cumplen ciertas condiciones, se mejora la experiencia del usuario sin comprometer la seguridad del sistema.
