#ifndef CONFIG_H
#define CONFIG_H

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
const float VOLTAJE_NIVEL = 3.5;       // Voltaje que indica nivel alcanzado (2.5V)
const uint8_t RESOLUCION_ADC = 12;     // Resolución del ADC
const uint16_t VALOR_MAX_ADC = 4095;   // Valor máximo del ADC (2^12 - 1)
// Factor de escala del divisor de voltaje
const float FACTOR_DIVISOR = VOLTAJE_MAX_ADC / VOLTAJE_MAX_ENTRADA;

// Tiempos en segundos para control de motor
const uint8_t TIEMPO_GIRO_DERECHA = 90; // 90 segundos
const uint8_t TIEMPO_GIRO_IZQUIERDA = 90;
const uint8_t TIEMPO_PAUSA_GIRO = 30;
// const uint8_t TIEMPO_GIRO_DERECHA = 5;
// const uint8_t TIEMPO_GIRO_IZQUIERDA = 5;
// const uint8_t TIEMPO_PAUSA_GIRO = 3;
const uint16_t TIEMPO_CICLO_COMPLETO = TIEMPO_GIRO_DERECHA + TIEMPO_PAUSA_GIRO + TIEMPO_GIRO_IZQUIERDA + TIEMPO_PAUSA_GIRO;

// Tiempos fijos para procesos específicos
const uint8_t TIEMPO_DESFOGUE = 40;
const uint16_t TIEMPO_CENTRIFUGADO = 420;           // 7*60
const uint8_t TIEMPO_PREPARACION_CENTRIFUGADO = 60; // 60 segundos (1 minuto)
const uint8_t TIEMPO_DESFOGUE_FINAL = 5;            // 5 segundos
const uint8_t TIEMPO_DETENIDO = 5;                  // 5 segundos
const uint8_t TIEMPO_EMERGENCIA = 5;                // 5 segundos
// const uint8_t TIEMPO_DESFOGUE = 5;
// const uint16_t TIEMPO_CENTRIFUGADO = 5; // 7*60
// const uint8_t TIEMPO_EMERGENCIA = 5; // 2*60

// Tiempos de cada tanda en segundos (ya incluyen su tiempo de desfogue)
const uint16_t tiemposTanda[3][3] = {
    {1690, 1510, 910}, // Programa 1: 28*60+10, 25*60+10, 15*60+10 + 7*60 = 4530
    {1210, 910, 310},  // Programa 2: 20*60+10, 15*60+10, 5*60+10 + 7*60 =
    {910, 610, 310}    // Programa 3: 15*60+10, 10*60+10, 5*60+10 + 7*60 = 2250
};

// Constantes para comunicación serial
const uint16_t INTERVALO_ACTUALIZACION_SERIAL = 5000;

#endif // CONFIG_H
