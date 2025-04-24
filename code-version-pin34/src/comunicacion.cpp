#include "../include/comunicacion.h"

// Función para enviar comandos al Nextion
void enviarComandoNextion(String comando)
{
  Serial.print(comando);
  Serial.write(0xFF);
  Serial.write(0xFF);
  Serial.write(0xFF);
  // No usar delay para evitar bloquear el loop principal
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
          if (!banderas.enProgreso)
          {
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
          if (!banderas.enProgreso)
          {
            enviarComandoNextion("page 1");
          }
        }
        comandoBuffer = "";
      }
    }
  }
}
