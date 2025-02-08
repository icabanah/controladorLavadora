const int leds[] = {13, 12, 14, 27, 26, 25};
const int numLeds = 6;

void setup() {
  // Configurar todos los pines como salidas
  for(int i = 0; i < numLeds; i++) {
    pinMode(leds[i], OUTPUT);
  }
}

void loop() {
  // Secuencia 1: Encender LEDs uno por uno
  for(int i = 0; i < numLeds; i++) {
    digitalWrite(leds[i], HIGH);
    delay(500);
    digitalWrite(leds[i], LOW);
  }
  
  delay(1000);
  
  // Secuencia 2: Todos los LEDs parpadean juntos
  for(int i = 0; i < 3; i++) {
    // Encender todos
    for(int j = 0; j < numLeds; j++) {
      digitalWrite(leds[j], HIGH);
    }
    delay(500);
    
    // Apagar todos
    for(int j = 0; j < numLeds; j++) {
      digitalWrite(leds[j], LOW);
    }
    delay(500);
  }
  
  delay(1000);
}