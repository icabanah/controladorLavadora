const int presostato_pin = 34;  // GPIO34

void setup() {
  Serial.begin(9600);
  analogReadResolution(12);
}

void loop() {
  // Múltiples lecturas para estabilidad
  int suma = 0;
  for(int i = 0; i < 10; i++) {
    suma += analogRead(presostato_pin);
    delay(10);
  }
  int valor_presostato = suma / 10;
  
  Serial.print("GPIO34 | Valor: ");
  Serial.print(valor_presostato);
  Serial.print(" | Voltaje: ");
  Serial.print((valor_presostato * 3.3) / 4095.0, 2);
  Serial.println("V");
  
  delay(500);
}