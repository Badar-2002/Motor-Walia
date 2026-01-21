#ifndef MOTOR_H
#define MOTOR_H

#include <ESP32Encoder.h>
#include <Preferences.h>
#include <functional>
#include "Constantes.h"

struct DatosMotor {
  long valor;       
  float porcentaje; 
};

class Motor {
  private:
    String id; 
    
    // --- PINES ---
    int pinAbrir, pinCerrar;
    int pinRojo, pinVerde, pinNaranja;
    int pinBtnAbrir, pinBtnCerrar, pinBtnCalib, pinSeta;

    ESP32Encoder enc;
    Preferences prefs; 

    // --- ESTADO INTERNO ---
    bool modoTiempo = false;           
    bool motorEnMovimiento = false;
    int sentidoGiro = 0;          

    // Variables Encoder/Tiempo
    long pulsos100 = 0;           
    long pulsosObjetivo = 0;      
    bool moviendoAutomatico = false; 
    unsigned long tiempoTotalRecorrido = 0; 
    unsigned long posicionActualTiempo = 0; 
    unsigned long tiempoInicioMovimiento = 0; 

    // Variable de Anticipación (Freno)
    long pulsosAnticipacion = 50; 

    // Variables Calibración
    bool calibrando = false;
    bool esperandoInicio = false; 
    bool midiendoTiempo = false; 
    bool tiempoInvertido = false; 
    bool buscandoBajar = false;
    
    // Estados Botones
    bool lastBtnAbrir = false;
    bool lastBtnCerrar = false;
    bool lastBtnCalib = false;
    int pasoCalibracion = 0; 

    // Luces y Errores
    unsigned long lastBlinkTime = 0;
    bool blinkState = false;           
    bool enEmergencia = false;
    bool errorLimite = false; // Seguridad Software
    unsigned long finCalibracionTime = 0; 
    bool mostrandoExito = false;       

    // Inercia Stop
    bool esperandoParadaReal = false;   
    long lastEncValStop = 0;            
    unsigned long lastEncTimeStop = 0;  
    const int TIEMPO_ESTABILIZACION = 300; 

    // Logger
    typedef std::function<void(String)> LogCallback;
    LogCallback externalLog = nullptr;

    void debug(String msg) {
      Serial.println("[" + id + "] " + msg); 
      if (externalLog) externalLog("[" + id + "] " + msg);
    }

    // --- VERIFICACIÓN DE LÍMITES (Modo Encoder) ---
    void verificarLimitesSeguridad() {
       if (calibrando || modoTiempo) return;
       if (!motorEnMovimiento) return;

       long pos = enc.getCount();

       // OPCIÓN A: Sistema de Pulsos POSITIVO (0 ... 10000)
       if (pulsos100 > 0) {
           // Se pasó del 100% por arriba (ej: 10050)
           if (pos > pulsos100 && sentidoGiro == 1) {
               debug("!!! ERROR: Limite Superior (+) !!!");
               parar(); errorLimite = true; 
           }
           // Se bajó del 0% (ej: -10)
           if (pos < 0 && sentidoGiro == -1) {
               debug("!!! ERROR: Limite Inferior (+) !!!");
               parar(); errorLimite = true; 
           }
       }
       // OPCIÓN B: Sistema de Pulsos NEGATIVO (0 ... -19000) -> TU CASO
       else {
           // Se pasó del 100% por "abajo" (ej: -20000 es menor que -19000)
           // Nota: Al ser negativo, "mayor recorrido" es un número "menor"
           if (pos < pulsos100 && sentidoGiro == 1) {
               debug("!!! ERROR: Limite Superior (-) !!!");
               parar(); errorLimite = true; 
           }
           // Se pasó del 0% hacia positivos (ej: 10 es mayor que 0)
           if (pos > 0 && sentidoGiro == -1) {
               debug("!!! ERROR: Limite Inferior (-) !!!");
               parar(); errorLimite = true; 
           }
       }
    }

    void gestionarLuces() {
      unsigned long now = millis();
      if (now - lastBlinkTime > 300) { blinkState = !blinkState; lastBlinkTime = now; }

      if (enEmergencia || errorLimite) { // Rojo parpadeando en fallo
        digitalWrite(pinRojo, blinkState); digitalWrite(pinVerde, LOW); digitalWrite(pinNaranja, LOW); return;
      }
      if (mostrandoExito) {
        if (now - finCalibracionTime < 3000) {
           digitalWrite(pinRojo, blinkState); digitalWrite(pinVerde, blinkState); digitalWrite(pinNaranja, blinkState);
        } else { mostrandoExito = false; }
        return;
      }
      if (calibrando) {
        if (pasoCalibracion == 2) digitalWrite(pinNaranja, HIGH);
        else digitalWrite(pinNaranja, blinkState);
        digitalWrite(pinRojo, LOW); digitalWrite(pinVerde, LOW); return;
      }
      if (motorEnMovimiento) {
        digitalWrite(pinVerde, blinkState); digitalWrite(pinRojo, LOW); digitalWrite(pinNaranja, LOW); return;
      }
      
      bool sistemaCalibrado = (!modoTiempo && pulsos100 != 0) || (modoTiempo && tiempoTotalRecorrido > 0);
      if (sistemaCalibrado) {
        digitalWrite(pinVerde, HIGH); digitalWrite(pinRojo, HIGH); digitalWrite(pinNaranja, LOW);
      } else {
        digitalWrite(pinVerde, LOW); digitalWrite(pinRojo, HIGH); digitalWrite(pinNaranja, LOW); 
      }
    }

    void gestionarEntradas() {
      if (digitalRead(pinSeta) == LOW) {
         if (!enEmergencia) { debug("!!! EMERGENCIA !!!"); parar(); enEmergencia = true; }
         return; 
      } else { 
        if (enEmergencia) { 
          debug("Emergencia OFF."); 
          enEmergencia = false;
          // REARME DEL ERROR DE LÍMITE
             // Si había un error de software, al soltar la seta lo limpiamos.
          if (errorLimite) {
              errorLimite = false;
              debug(">> REARME DE SISTEMA: Error de límite limpiado.");
          }
        } 
      }

      bool btnA = digitalRead(pinBtnAbrir);
      bool btnC = digitalRead(pinBtnCerrar);
      bool btnCal = digitalRead(pinBtnCalib);

      if (btnCal && !lastBtnCalib) {
         if (pasoCalibracion == 0) { debug(">> Calibrar..."); calibrar(); } 
         else if (pasoCalibracion == 1) { debug(">> Set 0..."); setZero(); } 
         else if (pasoCalibracion == 2) { debug(">> Set 100..."); set100(); finCalibrado(); }
      }
      lastBtnCalib = btnCal;

      if (calibrando || modoTiempo) {
         if (btnA) abrirManual();
         else if (btnC) cerrarManual();
         else {
            if (motorEnMovimiento && !moviendoAutomatico) parar();
         }
      }
      else {
         if (btnA && !lastBtnAbrir) { debug("Btn: Auto Abrir"); moverA(100); }
         if (!btnA && lastBtnAbrir) { debug("Btn: Stop"); parar(); }
         if (btnC && !lastBtnCerrar) { debug("Btn: Auto Cerrar"); moverA(0); }
         if (!btnC && lastBtnCerrar) { debug("Btn: Stop"); parar(); }
      }
      lastBtnAbrir = btnA;
      lastBtnCerrar = btnC;
    }

    bool debeSumarTiempo() {
       if (calibrando && midiendoTiempo) return true;
       if (!tiempoInvertido) return (sentidoGiro == 1); 
       else return (sentidoGiro == -1); 
    }

    // Cálculo en vivo (para que la barra web se mueva suave)
    unsigned long getPosicionTiempoEstimada() {
        if (!modoTiempo) return 0;
        if (!motorEnMovimiento) return posicionActualTiempo; 
        unsigned long delta = millis() - tiempoInicioMovimiento;
        long estimada;
        if (debeSumarTiempo()) estimada = posicionActualTiempo + delta;
        else estimada = (long)posicionActualTiempo - (long)delta;
        
        if (estimada < 0) return 0;
        if (estimada > tiempoTotalRecorrido) return tiempoTotalRecorrido;
        return (unsigned long)estimada;
    }

    void cargarDatosDeMemoria() {
       pulsosAnticipacion = prefs.getLong("antiEnc", 50);

       if (modoTiempo) {
          tiempoTotalRecorrido = prefs.getULong("timeTotal", 0);
          posicionActualTiempo = prefs.getULong("posTime", 0);
          debug("Inicio: Restaurado TIEMPO (Pos: " + String(posicionActualTiempo) + " / Total: " + String(tiempoTotalRecorrido) + ")");
       } else {
          pulsos100 = prefs.getLong("pulsos100", 0);
          long savedPulsos = prefs.getLong("posEnc", 0);
          enc.setCount(savedPulsos);
          debug("Inicio: Restaurado ENCODER (Pos: " + String(savedPulsos) + " / Total: " + String(pulsos100) + ")");
          debug("Anticipación Freno: " + String(pulsosAnticipacion) + " pulsos");
       }
    }

  public:
    Motor(String nombreUnico, int pA, int pC, int encA, int encB, int pR, int pV, int pN, int bA, int bC, int bCal, int bSeta) {
      id = nombreUnico;
      pinAbrir = pA; pinCerrar = pC;
      pinRojo = pR; pinVerde = pV; pinNaranja = pN;
      pinBtnAbrir = bA; pinBtnCerrar = bC; pinBtnCalib = bCal; pinSeta = bSeta;

      pinMode(pinAbrir, OUTPUT); pinMode(pinCerrar, OUTPUT);
      pinMode(pinRojo, OUTPUT); pinMode(pinVerde, OUTPUT); pinMode(pinNaranja, OUTPUT);
      digitalWrite(pinAbrir, LOW); digitalWrite(pinCerrar, LOW);

      pinMode(pinBtnAbrir, INPUT); pinMode(pinBtnCerrar, INPUT);
      pinMode(pinBtnCalib, INPUT); pinMode(pinSeta, INPUT); 

      enc.attachHalfQuad(encA, encB); 
    }

    void setLogger(LogCallback callback) { externalLog = callback; }

    void begin() {
      String nsp = id.substring(0, 15);
      prefs.begin(nsp.c_str(), false);
      modoTiempo = prefs.getBool("isTimeMode", false); 
      cargarDatosDeMemoria();
    }

    void setAnticipacion(long pulsos) {
       pulsosAnticipacion = pulsos;
       prefs.putLong("antiEnc", pulsosAnticipacion);
       debug("Anticipación actualizada a: " + String(pulsosAnticipacion) + " pulsos");
    }

    void setModo(bool esTiempo) {
       parar(); 
       modoTiempo = esTiempo;
       prefs.putBool("isTimeMode", modoTiempo); 
       pulsos100 = 0;
       tiempoTotalRecorrido = 0;
       posicionActualTiempo = 0;
       enc.clearCount(); 
       debug("CAMBIO DE MODO: Variables reseteadas. Sistema NO CALIBRADO.");
    }

    void update() {
      gestionarLuces();
      gestionarEntradas();
      
      // Chequeo constante de límites
      verificarLimitesSeguridad(); 

      if (esperandoParadaReal && !modoTiempo) {
          long lecturaActual = enc.getCount();
          if (lecturaActual != lastEncValStop) {
              lastEncValStop = lecturaActual;
              lastEncTimeStop = millis(); 
          } 
          else {
              if (millis() - lastEncTimeStop > TIEMPO_ESTABILIZACION) {
                  prefs.putLong("posEnc", lecturaActual);
                  esperandoParadaReal = false; 
                  debug(">> Motor FRENADO. Pos guardada: " + String(lecturaActual));
              }
          }
      }

      // Si hay cualquier error o parada, no ejecutamos lógica de movimiento automático
      if (!motorEnMovimiento || calibrando || enEmergencia || errorLimite) return;

      if (modoTiempo && moviendoAutomatico) {
        unsigned long delta = millis() - tiempoInicioMovimiento;
        long posEstimada;
        if (debeSumarTiempo()) posEstimada = posicionActualTiempo + delta;
        else posEstimada = posicionActualTiempo - delta;
        
        if (debeSumarTiempo() && posEstimada >= tiempoTotalRecorrido) { debug("Fin carrera (Abierto)."); parar(); } 
        else if (!debeSumarTiempo() && posEstimada <= 0) { debug("Fin carrera (Cerrado)."); parar(); }
      }
      else if (!modoTiempo && moviendoAutomatico) {
        long pulsosActuales = enc.getCount();
        // Lógica de frenado anticipado
        if (buscandoBajar) { 
            if (pulsosActuales <= (pulsosObjetivo + pulsosAnticipacion)) { 
                debug("Destino alcanzado (Anticipado)."); parar(); 
            } 
        } 
        else { 
            if (pulsosActuales >= (pulsosObjetivo - pulsosAnticipacion)) { 
                debug("Destino alcanzado (Anticipado)."); parar(); 
            } 
        }
      }
    }

    // --- ACCIONES AUTOMÁTICAS (BLOQUEAN SI HAY ERROR) ---
    void abrir() {
      esperandoParadaReal = false;
      if (enEmergencia || errorLimite) return; // BLOQUEO

      if (calibrando) {
         if (modoTiempo && esperandoInicio) {
            tiempoInicioMovimiento = millis(); esperandoInicio = false; midiendoTiempo = true; tiempoInvertido = false;
            debug(">> CAL (CMD): Midiendo (Abriendo)...");
         }
         moviendoAutomatico = true; 
      }
      else if (!modoTiempo) { moverA(100); return; }
      else {
         if (!tiempoInvertido && posicionActualTiempo >= tiempoTotalRecorrido) return;
         if (tiempoInvertido && posicionActualTiempo == 0) return;
         tiempoInicioMovimiento = millis(); moviendoAutomatico = true;
      }
      digitalWrite(pinAbrir, HIGH); digitalWrite(pinCerrar, LOW);
      motorEnMovimiento = true; sentidoGiro = 1; 
    }

    void cerrar() {
      esperandoParadaReal = false;
      if (enEmergencia || errorLimite) return; // BLOQUEO

      if (calibrando) {
         if (modoTiempo && esperandoInicio) {
            tiempoInicioMovimiento = millis(); esperandoInicio = false; midiendoTiempo = true; tiempoInvertido = true;
            debug(">> CAL (CMD): Midiendo (Cerrando)...");
         }
         moviendoAutomatico = true;
      }
      else if (!modoTiempo) { moverA(0); return; }
      else {
         if (!tiempoInvertido && posicionActualTiempo == 0) return;
         if (tiempoInvertido && posicionActualTiempo >= tiempoTotalRecorrido) return;
         tiempoInicioMovimiento = millis(); moviendoAutomatico = true;
      }
      digitalWrite(pinAbrir, LOW); digitalWrite(pinCerrar, HIGH);
      motorEnMovimiento = true; sentidoGiro = -1; 
    }

    void moverA(int porcentaje) {
       esperandoParadaReal = false;
       if(modoTiempo || calibrando || enEmergencia || errorLimite) { // BLOQUEO
           debug("Accion rechazada (Error o Estado)."); return;
       }
       //long porcentajeLim = constrain(porcentaje, 0, 100);
       long porcentajeLim = porcentaje;
       pulsosObjetivo = (pulsos100 * porcentajeLim) / 100;
       long pulsosActuales = enc.getCount();
       debug("Auto a: " + String(porcentajeLim) + "%");
       
       if (pulsosActuales < pulsosObjetivo) { 
           buscandoBajar = false; 
           digitalWrite(pinAbrir, LOW); digitalWrite(pinCerrar, HIGH); 
           motorEnMovimiento = true; sentidoGiro = -1; moviendoAutomatico = true; 
       } 
       else if (pulsosActuales > pulsosObjetivo) { 
           buscandoBajar = true; 
           digitalWrite(pinAbrir, HIGH); digitalWrite(pinCerrar, LOW); 
           motorEnMovimiento = true; sentidoGiro = 1; moviendoAutomatico = true; 
       }
    }

    // --- ACCIONES MANUALES (RESETEAN ERROR) ---
    void abrirManual() {
      esperandoParadaReal = false;
      errorLimite = false; // REARME MANUAL
      
      if (enEmergencia) return;
      if (calibrando && modoTiempo && esperandoInicio) {
         tiempoInicioMovimiento = millis(); esperandoInicio = false; midiendoTiempo = true; tiempoInvertido = false; 
         debug(">> CAL (BTN): Midiendo (Abriendo)...");
      }
      digitalWrite(pinAbrir, HIGH); digitalWrite(pinCerrar, LOW);
      motorEnMovimiento = true; sentidoGiro = 1; moviendoAutomatico = false;
    }

    void cerrarManual() {
      esperandoParadaReal = false;
      errorLimite = false; // REARME MANUAL
      
      if (enEmergencia) return;
      if (calibrando && modoTiempo && esperandoInicio) {
         tiempoInicioMovimiento = millis(); esperandoInicio = false; midiendoTiempo = true; tiempoInvertido = true; 
         debug(">> CAL (BTN): Midiendo (Cerrando)...");
      }
      digitalWrite(pinAbrir, LOW); digitalWrite(pinCerrar, HIGH);
      motorEnMovimiento = true; sentidoGiro = -1; moviendoAutomatico = false;
    }

    void parar() {
      digitalWrite(pinAbrir, LOW); digitalWrite(pinCerrar, LOW);
      
      if (modoTiempo && motorEnMovimiento && !calibrando && moviendoAutomatico) {
        unsigned long delta = millis() - tiempoInicioMovimiento;
        if (debeSumarTiempo()) { posicionActualTiempo += delta; if (posicionActualTiempo > tiempoTotalRecorrido) posicionActualTiempo = tiempoTotalRecorrido; } 
        else { if (delta > posicionActualTiempo) posicionActualTiempo = 0; else posicionActualTiempo -= delta; }
      }

      if (calibrando && modoTiempo && midiendoTiempo && motorEnMovimiento) {
         tiempoTotalRecorrido = millis() - tiempoInicioMovimiento;
         posicionActualTiempo = tiempoTotalRecorrido; 
         set100(); midiendoTiempo = false; finCalibrado(); 
      }
      
      motorEnMovimiento = false; moviendoAutomatico = false; sentidoGiro = 0;

      if (!calibrando) {
        if (modoTiempo) {
          prefs.putULong("posTime", posicionActualTiempo); 
        } else {
          esperandoParadaReal = true;
          lastEncValStop = enc.getCount();
          lastEncTimeStop = millis();
          debug("Frenando...");
        }
      }
    }

    // --- MÉTODOS DE CALIBRACIÓN Y GETTERS ---
    void calibrar() { 
       calibrando = true; midiendoTiempo = false; esperandoInicio = false; posicionActualTiempo = 0; 
       pasoCalibracion = 1; 
       debug("MODO CALIBRACIÓN ACTIVADO."); 
    }
    void setZero() { 
       if (!calibrando) return;
       pasoCalibracion = 2; 
       if (modoTiempo) { 
          esperandoInicio = true; midiendoTiempo = false; tiempoInvertido = false; 
          prefs.putULong("posTime", 0); debug(">> Punto 0 FIJADO (Tiempo)."); 
       } else { 
          enc.clearCount(); prefs.putLong("posEnc", 0); debug(">> Punto 0 FIJADO (Encoder)."); 
       }
    }
    void set100() { 
       if (!calibrando) return;
       if (!modoTiempo) { 
          pulsos100 = enc.getCount(); prefs.putLong("pulsos100", pulsos100); debug(">> 100% FIJADO: " + String(pulsos100)); 
       } else { 
          prefs.putULong("timeTotal", tiempoTotalRecorrido); debug(">> 100% FIJADO: " + String(tiempoTotalRecorrido) + " ms"); 
       }
    }
    void finCalibrado() { 
       if (!calibrando) return;
       calibrando = false; pasoCalibracion = 0; mostrandoExito = true; finCalibracionTime = millis(); 
       debug("FIN CALIBRACIÓN."); 
    }
    
    DatosMotor getPosicionActual() {
      DatosMotor datos;
      if (modoTiempo) { datos.valor = (long)posicionActualTiempo; datos.porcentaje = -1.0; } 
      else { datos.valor = enc.getCount(); if (pulsos100 != 0) datos.porcentaje = ((float)datos.valor / (float)pulsos100) * 100.0; else datos.porcentaje = -1.0; }
      return datos;
    }

    String getEstadoString() {
      if (enEmergencia) return MSG_EMERGENCIA;
      if (errorLimite)  return MSG_ERROR_LIMITE; // Aviso de límite superado

      bool calibrado = (modoTiempo && tiempoTotalRecorrido > 0) || (!modoTiempo && pulsos100 != 0);
      if (!calibrado) return MSG_NO_CALIBRADO;
      if (calibrando) return MSG_CALIBRANDO;

      if (motorEnMovimiento) {
         if (sentidoGiro == 1) return MSG_ABRIENDO; 
         if (sentidoGiro == -1) return MSG_CERRANDO;
      }

      float p = 0.0;
      if (modoTiempo) {
          unsigned long posViva = getPosicionTiempoEstimada();
          if (tiempoTotalRecorrido > 0) p = ((float)posViva / (float)tiempoTotalRecorrido) * 100.0;
      } else {
          DatosMotor datos = getPosicionActual();
          p = datos.porcentaje;
      }

      if (p >= 98.0) return MSG_ABIERTO;
      if (p <= 2.0)  return MSG_CERRADO;
      
      return MSG_PARADO;
    }

    int getPorcentajeEntero() {
       if (modoTiempo) {
          if (tiempoTotalRecorrido == 0) return 0;
          unsigned long posViva = getPosicionTiempoEstimada();
          float p = ((float)posViva / (float)tiempoTotalRecorrido) * 100.0;
          return constrain((int)p, 0, 100);
       } else {
          DatosMotor datos = getPosicionActual();
          if (datos.porcentaje < 0) return 0; 
          return (int)datos.porcentaje;
       }
    }

    void imprimirEstado() {
      String msg = "";
      if (modoTiempo) {
        msg = "[TIEMPO] Actual: " + String(posicionActualTiempo) + " ms / Total: " + String(tiempoTotalRecorrido) + " ms";
        if (tiempoTotalRecorrido == 0) msg += " (NO CALIBRADO)";
      } 
      else {
        long pulsosActuales = enc.getCount();
        String strPorc = "";
        if (pulsos100 != 0) {
          float porcentaje = ((float)pulsosActuales / (float)pulsos100) * 100.0;
          strPorc = String(porcentaje, 1) + "%"; 
        } else { strPorc = "NO CAL"; }
        msg = "[ENCODER] " + strPorc + " | Pulsos: " + String(pulsosActuales) + " / " + String(pulsos100);
        msg += " | Anticip: " + String(pulsosAnticipacion);
      }
      debug(msg);
    }
};

#endif

/*
#include <ESP32Encoder.h>
#include <Preferences.h>
#include <functional>
#include "Constantes.h"

struct DatosMotor {
  long valor;       
  float porcentaje; 
};

class Motor {
  private:
    String id; 
    
    // --- PINES ---
    int pinAbrir, pinCerrar;
    int pinRojo, pinVerde, pinNaranja;
    int pinBtnAbrir, pinBtnCerrar, pinBtnCalib, pinSeta;

    ESP32Encoder enc;
    Preferences prefs; 

    // --- ESTADO INTERNO ---
    bool modoTiempo = false;           
    bool motorEnMovimiento = false;
    int sentidoGiro = 0;          

    // Variables Encoder/Tiempo
    long pulsos100 = 0;           
    long pulsosObjetivo = 0;      
    bool moviendoAutomatico = false; 
    unsigned long tiempoTotalRecorrido = 0; 
    unsigned long posicionActualTiempo = 0; 
    unsigned long tiempoInicioMovimiento = 0; 

    // --- NUEVO: VARIABLE DE ANTICIPACIÓN (Ya no es const) ---
    long pulsosAnticipacion = 500; // Valor por defecto

    // Variables Calibración
    bool calibrando = false;
    bool esperandoInicio = false; 
    bool midiendoTiempo = false; 
    bool tiempoInvertido = false; 
    bool buscandoBajar = false;
    
    // Estados Botones
    bool lastBtnAbrir = false;
    bool lastBtnCerrar = false;
    bool lastBtnCalib = false;
    int pasoCalibracion = 0; 

    // Luces
    unsigned long lastBlinkTime = 0;
    bool blinkState = false;           
    bool enEmergencia = false;         
    unsigned long finCalibracionTime = 0; 
    bool mostrandoExito = false;       

    // Inercia Stop
    bool esperandoParadaReal = false;   
    long lastEncValStop = 0;            
    unsigned long lastEncTimeStop = 0;  
    const int TIEMPO_ESTABILIZACION = 300; 

    // Logger
    typedef std::function<void(String)> LogCallback;
    LogCallback externalLog = nullptr;

    void debug(String msg) {
      Serial.println("[" + id + "] " + msg); 
      if (externalLog) externalLog("[" + id + "] " + msg);
    }

    void gestionarLuces() {
      unsigned long now = millis();
      if (now - lastBlinkTime > 300) { blinkState = !blinkState; lastBlinkTime = now; }

      if (enEmergencia) {
        digitalWrite(pinRojo, blinkState); digitalWrite(pinVerde, LOW); digitalWrite(pinNaranja, LOW); return;
      }
      if (mostrandoExito) {
        if (now - finCalibracionTime < 3000) {
           digitalWrite(pinRojo, blinkState); digitalWrite(pinVerde, blinkState); digitalWrite(pinNaranja, blinkState);
        } else { mostrandoExito = false; }
        return;
      }
      if (calibrando) {
        if (pasoCalibracion == 2) digitalWrite(pinNaranja, HIGH);
        else digitalWrite(pinNaranja, blinkState);
        digitalWrite(pinRojo, LOW); digitalWrite(pinVerde, LOW); return;
      }
      if (motorEnMovimiento) {
        digitalWrite(pinVerde, blinkState); digitalWrite(pinRojo, LOW); digitalWrite(pinNaranja, LOW); return;
      }
      
      bool sistemaCalibrado = (!modoTiempo && pulsos100 != 0) || (modoTiempo && tiempoTotalRecorrido > 0);
      if (sistemaCalibrado) {
        digitalWrite(pinVerde, HIGH); digitalWrite(pinRojo, HIGH); digitalWrite(pinNaranja, LOW);
      } else {
        digitalWrite(pinVerde, LOW); digitalWrite(pinRojo, HIGH); digitalWrite(pinNaranja, LOW); 
      }
    }

    void gestionarEntradas() {
      if (digitalRead(pinSeta) == LOW) {
         if (!enEmergencia) { debug("!!! EMERGENCIA !!!"); parar(); enEmergencia = true; }
         return; 
      } else { if (enEmergencia) { debug("Emergencia OFF."); enEmergencia = false; } }

      bool btnA = digitalRead(pinBtnAbrir);
      bool btnC = digitalRead(pinBtnCerrar);
      bool btnCal = digitalRead(pinBtnCalib);

      if (btnCal && !lastBtnCalib) {
         if (pasoCalibracion == 0) { debug(">> Calibrar..."); calibrar(); } 
         else if (pasoCalibracion == 1) { debug(">> Set 0..."); setZero(); } 
         else if (pasoCalibracion == 2) { debug(">> Set 100..."); set100(); finCalibrado(); }
      }
      lastBtnCalib = btnCal;

      if (calibrando || modoTiempo) {
         if (btnA) abrirManual();
         else if (btnC) cerrarManual();
         else {
            if (motorEnMovimiento && !moviendoAutomatico) parar();
         }
      }
      else {
         if (btnA && !lastBtnAbrir) { debug("Btn: Auto Abrir"); moverA(100); }
         if (!btnA && lastBtnAbrir) { debug("Btn: Stop"); parar(); }
         if (btnC && !lastBtnCerrar) { debug("Btn: Auto Cerrar"); moverA(0); }
         if (!btnC && lastBtnCerrar) { debug("Btn: Stop"); parar(); }
      }
      lastBtnAbrir = btnA;
      lastBtnCerrar = btnC;
    }

    bool debeSumarTiempo() {
       if (calibrando && midiendoTiempo) return true;
       if (!tiempoInvertido) return (sentidoGiro == 1); 
       else return (sentidoGiro == -1); 
    }

    void cargarDatosDeMemoria() {
       // Cargar la anticipación guardada (o 50 por defecto)
       pulsosAnticipacion = prefs.getLong("antiEnc", 50);

       if (modoTiempo) {
          tiempoTotalRecorrido = prefs.getULong("timeTotal", 0);
          posicionActualTiempo = prefs.getULong("posTime", 0);
          debug("Inicio: Restaurado TIEMPO (Pos: " + String(posicionActualTiempo) + " / Total: " + String(tiempoTotalRecorrido) + ")");
       } else {
          pulsos100 = prefs.getLong("pulsos100", 0);
          long savedPulsos = prefs.getLong("posEnc", 0);
          enc.setCount(savedPulsos);
          debug("Inicio: Restaurado ENCODER (Pos: " + String(savedPulsos) + " / Total: " + String(pulsos100) + ")");
          debug("Anticipación Freno: " + String(pulsosAnticipacion) + " pulsos");
       }
    }

  public:
    Motor(String nombreUnico, int pA, int pC, int encA, int encB, int pR, int pV, int pN, int bA, int bC, int bCal, int bSeta) {
      id = nombreUnico;
      pinAbrir = pA; pinCerrar = pC;
      pinRojo = pR; pinVerde = pV; pinNaranja = pN;
      pinBtnAbrir = bA; pinBtnCerrar = bC; pinBtnCalib = bCal; pinSeta = bSeta;

      pinMode(pinAbrir, OUTPUT); pinMode(pinCerrar, OUTPUT);
      pinMode(pinRojo, OUTPUT); pinMode(pinVerde, OUTPUT); pinMode(pinNaranja, OUTPUT);
      digitalWrite(pinAbrir, LOW); digitalWrite(pinCerrar, LOW);

      pinMode(pinBtnAbrir, INPUT); pinMode(pinBtnCerrar, INPUT);
      pinMode(pinBtnCalib, INPUT); pinMode(pinSeta, INPUT); 

      enc.attachHalfQuad(encA, encB); 
    }

    void setLogger(LogCallback callback) { externalLog = callback; }

    void begin() {
      String nsp = id.substring(0, 15);
      prefs.begin(nsp.c_str(), false);
      modoTiempo = prefs.getBool("isTimeMode", false); 
      cargarDatosDeMemoria();
    }

    // --- NUEVO MÉTODO PARA AJUSTAR ANTICIPACIÓN ---
    void setAnticipacion(long pulsos) {
       pulsosAnticipacion = pulsos;
       prefs.putLong("antiEnc", pulsosAnticipacion); // Guardamos en memoria
       debug("Anticipación actualizada a: " + String(pulsosAnticipacion) + " pulsos");
    }
    // ----------------------------------------------

    void setModo(bool esTiempo) {
       parar(); 
       modoTiempo = esTiempo;
       prefs.putBool("isTimeMode", modoTiempo); 
       pulsos100 = 0;
       tiempoTotalRecorrido = 0;
       posicionActualTiempo = 0;
       enc.clearCount(); 
       debug("CAMBIO DE MODO: Variables reseteadas. Sistema NO CALIBRADO.");
    }

    void update() {
      gestionarLuces();
      gestionarEntradas();

      if (esperandoParadaReal && !modoTiempo) {
          long lecturaActual = enc.getCount();
          if (lecturaActual != lastEncValStop) {
              lastEncValStop = lecturaActual;
              lastEncTimeStop = millis(); 
          } 
          else {
              if (millis() - lastEncTimeStop > TIEMPO_ESTABILIZACION) {
                  prefs.putLong("posEnc", lecturaActual);
                  esperandoParadaReal = false; 
                  debug(">> Motor FRENADO. Pos guardada: " + String(lecturaActual));
              }
          }
      }

      if (!motorEnMovimiento || calibrando || enEmergencia) return;

      if (modoTiempo && moviendoAutomatico) {
        unsigned long delta = millis() - tiempoInicioMovimiento;
        long posEstimada;
        if (debeSumarTiempo()) posEstimada = posicionActualTiempo + delta;
        else posEstimada = posicionActualTiempo - delta;
        if (debeSumarTiempo() && posEstimada >= tiempoTotalRecorrido) { debug("Fin carrera (Abierto)."); parar(); } 
        else if (!debeSumarTiempo() && posEstimada <= 0) { debug("Fin carrera (Cerrado)."); parar(); }
      }
      else if (!modoTiempo && moviendoAutomatico) {
        long pulsosActuales = enc.getCount();
        
        // --- AQUÍ USAMOS LA VARIABLE DE ANTICIPACIÓN ---
        if (buscandoBajar) { 
            if (pulsosActuales <= (pulsosObjetivo + pulsosAnticipacion)) { 
                debug("Destino alcanzado (Anticipado)."); 
                parar(); 
            } 
        } 
        else { 
            if (pulsosActuales >= (pulsosObjetivo - pulsosAnticipacion)) { 
                debug("Destino alcanzado (Anticipado)."); 
                parar(); 
            } 
        }
      }
    }

    void abrir() {
      esperandoParadaReal = false;
      if (enEmergencia) return;
      if (calibrando) {
         if (modoTiempo && esperandoInicio) {
            tiempoInicioMovimiento = millis(); esperandoInicio = false; midiendoTiempo = true; tiempoInvertido = false;
            debug(">> CAL (CMD): Midiendo (Abriendo)...");
         }
         moviendoAutomatico = true; 
      }
      else if (!modoTiempo) { moverA(100); return; }
      else {
         if (!tiempoInvertido && posicionActualTiempo >= tiempoTotalRecorrido) return;
         if (tiempoInvertido && posicionActualTiempo == 0) return;
         tiempoInicioMovimiento = millis(); moviendoAutomatico = true;
      }
      digitalWrite(pinAbrir, HIGH); digitalWrite(pinCerrar, LOW);
      motorEnMovimiento = true; sentidoGiro = 1; 
    }

    void cerrar() {
      esperandoParadaReal = false;
      if (enEmergencia) return;
      if (calibrando) {
         if (modoTiempo && esperandoInicio) {
            tiempoInicioMovimiento = millis(); esperandoInicio = false; midiendoTiempo = true; tiempoInvertido = true;
            debug(">> CAL (CMD): Midiendo (Cerrando)...");
         }
         moviendoAutomatico = true;
      }
      else if (!modoTiempo) { moverA(0); return; }
      else {
         if (!tiempoInvertido && posicionActualTiempo == 0) return;
         if (tiempoInvertido && posicionActualTiempo >= tiempoTotalRecorrido) return;
         tiempoInicioMovimiento = millis(); moviendoAutomatico = true;
      }
      digitalWrite(pinAbrir, LOW); digitalWrite(pinCerrar, HIGH);
      motorEnMovimiento = true; sentidoGiro = -1; 
    }

    void abrirManual() {
      esperandoParadaReal = false;
      if (enEmergencia) return;
      if (calibrando && modoTiempo && esperandoInicio) {
         tiempoInicioMovimiento = millis(); esperandoInicio = false; midiendoTiempo = true; tiempoInvertido = false; 
         debug(">> CAL (BTN): Midiendo (Abriendo)...");
      }
      digitalWrite(pinAbrir, HIGH); digitalWrite(pinCerrar, LOW);
      motorEnMovimiento = true; sentidoGiro = 1; moviendoAutomatico = false;
    }

    void cerrarManual() {
      esperandoParadaReal = false;
      if (enEmergencia) return;
      if (calibrando && modoTiempo && esperandoInicio) {
         tiempoInicioMovimiento = millis(); esperandoInicio = false; midiendoTiempo = true; tiempoInvertido = true; 
         debug(">> CAL (BTN): Midiendo (Cerrando)...");
      }
      digitalWrite(pinAbrir, LOW); digitalWrite(pinCerrar, HIGH);
      motorEnMovimiento = true; sentidoGiro = -1; moviendoAutomatico = false;
    }

    void parar() {
      digitalWrite(pinAbrir, LOW); digitalWrite(pinCerrar, LOW);
      
      if (modoTiempo && motorEnMovimiento && !calibrando && moviendoAutomatico) {
        unsigned long delta = millis() - tiempoInicioMovimiento;
        if (debeSumarTiempo()) { posicionActualTiempo += delta; if (posicionActualTiempo > tiempoTotalRecorrido) posicionActualTiempo = tiempoTotalRecorrido; } 
        else { if (delta > posicionActualTiempo) posicionActualTiempo = 0; else posicionActualTiempo -= delta; }
      }

      if (calibrando && modoTiempo && midiendoTiempo && motorEnMovimiento) {
         tiempoTotalRecorrido = millis() - tiempoInicioMovimiento;
         posicionActualTiempo = tiempoTotalRecorrido; 
         set100(); midiendoTiempo = false; finCalibrado(); 
      }
      
      motorEnMovimiento = false; moviendoAutomatico = false; sentidoGiro = 0;

      if (!calibrando) {
        if (modoTiempo) {
          prefs.putULong("posTime", posicionActualTiempo); 
        } else {
          esperandoParadaReal = true;
          lastEncValStop = enc.getCount();
          lastEncTimeStop = millis();
          debug("Frenando...");
        }
      }
    }

    void calibrar() { 
       calibrando = true; midiendoTiempo = false; esperandoInicio = false; posicionActualTiempo = 0; 
       pasoCalibracion = 1; 
       debug("MODO CALIBRACIÓN ACTIVADO."); 
    }
    void setZero() { 
       if (!calibrando) return;
       pasoCalibracion = 2; 
       if (modoTiempo) { 
          esperandoInicio = true; midiendoTiempo = false; tiempoInvertido = false; 
          prefs.putULong("posTime", 0); 
          debug(">> Punto 0 FIJADO (Tiempo)."); 
       } 
       else { 
          enc.clearCount(); 
          prefs.putLong("posEnc", 0);
          debug(">> Punto 0 FIJADO (Encoder)."); 
       }
    }
    void set100() { 
       if (!calibrando) return;
       if (!modoTiempo) { 
          pulsos100 = enc.getCount(); 
          prefs.putLong("pulsos100", pulsos100); 
          debug(">> 100% FIJADO: " + String(pulsos100)); 
       } 
       else { 
          prefs.putULong("timeTotal", tiempoTotalRecorrido); 
          debug(">> 100% FIJADO: " + String(tiempoTotalRecorrido) + " ms"); 
       }
    }
    void finCalibrado() { 
       if (!calibrando) return;
       calibrando = false; pasoCalibracion = 0; mostrandoExito = true; finCalibracionTime = millis(); 
       debug("FIN CALIBRACIÓN."); 
    }
    
    DatosMotor getPosicionActual() {
      DatosMotor datos;
      if (modoTiempo) { datos.valor = (long)posicionActualTiempo; datos.porcentaje = -1.0; } 
      else { datos.valor = enc.getCount(); if (pulsos100 != 0) datos.porcentaje = ((float)datos.valor / (float)pulsos100) * 100.0; else datos.porcentaje = -1.0; }
      return datos;
    }
    
    void moverA(int porcentaje) {
       esperandoParadaReal = false;
       if(modoTiempo || calibrando || enEmergencia) return;
       long porcentajeLim = constrain(porcentaje, 0, 100);
       pulsosObjetivo = (pulsos100 * porcentajeLim) / 100;
       long pulsosActuales = enc.getCount();
       debug("Auto a: " + String(porcentajeLim) + "%");
       
       if (pulsosActuales < pulsosObjetivo) { 
           buscandoBajar = false; 
           digitalWrite(pinAbrir, LOW); digitalWrite(pinCerrar, HIGH); 
           motorEnMovimiento = true; sentidoGiro = -1; moviendoAutomatico = true; 
       } 
       else if (pulsosActuales > pulsosObjetivo) { 
           buscandoBajar = true; 
           digitalWrite(pinAbrir, HIGH); digitalWrite(pinCerrar, LOW); 
           motorEnMovimiento = true; sentidoGiro = 1; moviendoAutomatico = true; 
       }
    }
    
    
    // --- REPORTE DE ESTADO (Actualizado para Tiempo Real) ---
    String getEstadoString() {
      // 1. Emergencia
      if (enEmergencia) return MSG_EMERGENCIA;
      
      // 2. Calibración
      bool calibrado = (modoTiempo && tiempoTotalRecorrido > 0) || (!modoTiempo && pulsos100 != 0);
      if (!calibrado) return MSG_NO_CALIBRADO;
      if (calibrando) return MSG_CALIBRANDO;

      // 3. Movimiento
      if (motorEnMovimiento) {
         if (sentidoGiro == 1) return MSG_ABRIENDO; 
         if (sentidoGiro == -1) return MSG_CERRANDO;
      }

      // 4. Calculamos porcentaje (Usando la ESTIMACIÓN si es modo tiempo)
      float p = 0.0;

      if (modoTiempo) {
          unsigned long posViva = getPosicionTiempoEstimada(); // <--- EL CAMBIO CLAVE
          if (tiempoTotalRecorrido > 0) {
             p = ((float)posViva / (float)tiempoTotalRecorrido) * 100.0;
          }
      } else {
          DatosMotor datos = getPosicionActual();
          p = datos.porcentaje;
      }

      // Finales de carrera virtuales
      if (p >= 98.0) return MSG_ABIERTO;
      if (p <= 2.0)  return MSG_CERRADO;
      
      return MSG_PARADO;
    }

    // --- PORCENTAJE ENTERO (Actualizado para Tiempo Real) ---
    int getPorcentajeEntero() {
       if (modoTiempo) {
          if (tiempoTotalRecorrido == 0) return 0;
          
          unsigned long posViva = getPosicionTiempoEstimada(); // <--- EL CAMBIO CLAVE
          float p = ((float)posViva / (float)tiempoTotalRecorrido) * 100.0;
          
          return constrain((int)p, 0, 100);
       } else {
          DatosMotor datos = getPosicionActual();
          if (datos.porcentaje < 0) return 0; 
          return (int)datos.porcentaje;
       }
    }

    // --- NUEVO: Calcula la posición en vivo mientras se mueve ---
    unsigned long getPosicionTiempoEstimada() {
        if (!modoTiempo) return 0;
        
        // Si está parado, devolvemos la última posición guardada real
        if (!motorEnMovimiento) return posicionActualTiempo; 

        // Si se está moviendo, calculamos dónde debería ir según el reloj
        unsigned long delta = millis() - tiempoInicioMovimiento;
        long estimada;

        if (debeSumarTiempo()) {
            estimada = posicionActualTiempo + delta;
        } else {
            estimada = (long)posicionActualTiempo - (long)delta;
        }

        // Protecciones para que no de valores locos
        if (estimada < 0) return 0;
        if (estimada > tiempoTotalRecorrido) return tiempoTotalRecorrido;
        
        return (unsigned long)estimada;
    }

    void imprimirEstado() {
      String msg = "";

      if (modoTiempo) {
        // MODO TIEMPO
        msg = "[TIEMPO] Actual: " + String(posicionActualTiempo) + " ms";
        msg += " / Total: " + String(tiempoTotalRecorrido) + " ms";
        
        if (tiempoTotalRecorrido == 0) msg += " (NO CALIBRADO)";
      } 
      else {
        // MODO ENCODER
        long pulsosActuales = enc.getCount();
        String strPorc = "";
        
        // Calcular porcentaje
        if (pulsos100 != 0) {
          float porcentaje = ((float)pulsosActuales / (float)pulsos100) * 100.0;
          strPorc = String(porcentaje, 1) + "%"; 
        } else {
          strPorc = "NO CAL";
        }

        // Montar mensaje: Porcentaje | Pulsos Actuales / Totales | Anticipación
        msg = "[ENCODER] " + strPorc;
        msg += " | Pulsos: " + String(pulsosActuales) + " / " + String(pulsos100);
        msg += " | Anticip: " + String(pulsosAnticipacion);
      }
      
      debug(msg);
    }
};
*/

