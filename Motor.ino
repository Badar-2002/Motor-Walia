#include <Arduino.h>
#include "Motor.h"
#include "WiFiWebServer.h"
#include "Mqtt.h"
#include "Constantes.h"

// --- PINES ---
#define PIN_SETA       I0_1
#define PIN_BTN_ABRIR  I0_4
#define PIN_BTN_CERRAR I0_3
#define PIN_BTN_CALIB  I0_2
#define PIN_ENC_A      I0_6
#define PIN_ENC_B      I0_5
#define PIN_MOT_A      Q2_7
#define PIN_MOT_C      Q2_6
#define PIN_LUZ_ROJA   Q0_3
#define PIN_LUZ_VERDE  Q0_5
#define PIN_LUZ_NARANJA Q0_4

// --- OBJETOS ---
Motor* miMotor = nullptr; 
WiFiWebServer wifiServer("MOVISTAR_D679", "7R8w77PGSkqcdK7", "esp32");
MqttHandler mqtt; 

// --- LOGGER ---
void printLog(String msg) {
  Serial.println(msg);
  wifiServer.log(msg);
}

// --- EJECUTOR CENTRAL ---
// Recibe órdenes directas (m50, a, z, s) sin necesidad de traducir
void ejecutarComando(String cmd) {
  if (cmd.length() == 0) return;
  char c = cmd.charAt(0);

  // Configuración Sistema
  if (c == 'T') { if (miMotor) miMotor->setModo(true); }
  else if (c == 'E') { if (miMotor) miMotor->setModo(false); }
  
  // Comandos Motor
  else if (miMotor != nullptr) {
    if (c == 'a') miMotor->abrir();
    else if (c == 'z') miMotor->cerrar();
    else if (c == 's') miMotor->parar();
    else if (c == 'c') miMotor->calibrar();
    else if (c == '0') miMotor->setZero();
    else if (c == '1') miMotor->set100();
    else if (c == 'f') miMotor->finCalibrado();
    else if (c == 'i') miMotor->imprimirEstado();
    else if (cmd.length() > 1) {
       int val = cmd.substring(1).toInt();
       if (c == 'A') miMotor->setAnticipacion(val);
       else if (c == 'm') miMotor->moverA(val);
    }
  }
}

// --- SETUP ---
void setup() {
  Serial.begin(115200);
  delay(1000);
  printLog("--- SISTEMA PLC MATEO E HIJO ---");
  
  // 1. Web
  wifiServer.setup();
  wifiServer.setCommandCallback([](String cmd) { ejecutarComando(cmd); });

  // 2. Motor
  miMotor = new Motor("mot1", 
                      PIN_MOT_A, PIN_MOT_C, PIN_ENC_A, PIN_ENC_B, 
                      PIN_LUZ_ROJA, PIN_LUZ_VERDE, PIN_LUZ_NARANJA,
                      PIN_BTN_ABRIR, PIN_BTN_CERRAR, PIN_BTN_CALIB, PIN_SETA);
  miMotor->setLogger(printLog);
  miMotor->begin();

  // 3. MQTT
  mqtt.setup(MQTT_BROKER, MQTT_PORT, MQTT_USER, MQTT_PASS, 
             MQTT_TOPIC_IN, MQTT_TOPIC_OUT, MQTT_ID_PLC);
             
  // Vinculamos comandos entrantes
  mqtt.setCommandCallback([](String cmd) { 
      printLog("MQTT: " + cmd);
      ejecutarComando(cmd); 
  });
  
  // VINCULAMOS EL MOTOR PARA QUE EL MQTT LO VIGILE AUTOMÁTICAMENTE
  mqtt.vincularMotor(miMotor);
}

// --- LOOP ---
void loop() {
  // Servicios de fondo
  wifiServer.loop();
  
  // El MQTT gestiona dentro su conexión y la vigilancia del motor
  mqtt.loop(); 
  
  // Lógica de control del motor
  if (miMotor != nullptr) miMotor->update();

  // Comandos manuales por Serial
  if (Serial.available()) {
    char c = Serial.read();
    if(c != '\n' && c != '\r') {
       if (c == 'm' || c == 'A') ejecutarComando(String(c) + Serial.readStringUntil('\n'));
       else ejecutarComando(String(c));
    }
  }
}

/*#include <Arduino.h>
#include "Motor.h"
#include "WiFiWebServer.h"
#include "Mqtt.h"
#include "Constantes.h"

// ==========================================
//              DEFINICIÓN DE PINES
// ==========================================
// Entradas (Sensores / Botones)
#define PIN_SETA       I0_1  // Seta de Emergencia (NC normalmente)
#define PIN_BTN_ABRIR  I0_4  // Botón físico Abrir
#define PIN_BTN_CERRAR I0_3  // Botón físico Cerrar
#define PIN_BTN_CALIB  I0_2  // Botón físico Calibrar
#define PIN_ENC_A      I0_6  // Encoder Canal A
#define PIN_ENC_B      I0_5  // Encoder Canal B

// Salidas (Motor / Luces)
#define PIN_MOT_A      Q2_7  // Relé Abrir
#define PIN_MOT_C      Q2_6  // Relé Cerrar
#define PIN_LUZ_ROJA   Q0_3  // Piloto Rojo
#define PIN_LUZ_VERDE  Q0_5  // Piloto Verde
#define PIN_LUZ_NARANJA Q0_4 // Piloto Naranja

// ==========================================
//              OBJETOS GLOBALES
// ==========================================
// Puntero al motor (se crea dinámicamente en setup)
Motor* miMotor = nullptr; 

// Gestor de la interfaz Web (WiFi)
WiFiWebServer wifiServer("MOVISTAR_D679", "7R8w77PGSkqcdK7", "esp32");

// Gestor de comunicaciones MQTT
MqttHandler mqtt; 

// ==========================================
//        VARIABLES DE CONTROL DE ESTADO
// ==========================================
// Sirven para recordar lo último que enviamos y no repetir mensajes
String lastEstadoStr = "";
int lastPorcentaje = -1;
unsigned long lastCheckTime = 0;

// ==========================================
//            FUNCIONES AUXILIARES
// ==========================================

// Función para imprimir logs en Serial y en la Web a la vez
void printLog(String msg) {
  Serial.println(msg);
  wifiServer.log(msg);
}

// Cerebro de Comandos: Recibe órdenes de cualquier sitio (Serie, Web, MQTT)
void ejecutarComando(String cmd) {
  if (cmd.length() == 0) return;
  char c = cmd.charAt(0);

  // --- Comandos de Configuración de Sistema ---
  if (c == 'T') {
     if (miMotor) miMotor->setModo(true);  // Modo Tiempo
  }
  else if (c == 'E') {
     if (miMotor) miMotor->setModo(false); // Modo Encoder
  }
  
  // --- Comandos Directos al Motor ---
  else if (miMotor != nullptr) {
    // Movimientos Básicos
    if (c == 'a') miMotor->abrir();
    else if (c == 'z') miMotor->cerrar();
    else if (c == 's') miMotor->parar();
    
    // Calibración
    else if (c == 'c') miMotor->calibrar();
    else if (c == '0') miMotor->setZero();
    else if (c == '1') miMotor->set100();
    else if (c == 'f') miMotor->finCalibrado();
    
    // Información
    else if (c == 'i') miMotor->imprimirEstado();
    
    // Ajustes Avanzados (Requieren número después de la letra)
    else if (cmd.length() > 1) {
       int valor = cmd.substring(1).toInt();
       
       if (c == 'A') miMotor->setAnticipacion(valor); // Ajustar freno (Anticipación)
       else if (c == 'm') miMotor->moverA(valor);     // Mover a %
    }
  }
}

// ==========================================
//                  SETUP
// ==========================================
void setup() {
  Serial.begin(115200);
  delay(1000);
  printLog("--- INICIANDO SISTEMA PLC ---");
  
  // 1. Iniciar WiFi y Servidor Web
  wifiServer.setup();
  // Vinculamos los comandos web al ejecutor central
  wifiServer.setCommandCallback([](String cmd) { ejecutarComando(cmd); });

  // 2. Iniciar el Motor
  // "mot1" es el ID para guardar la configuración en memoria
  miMotor = new Motor("mot1", 
                      PIN_MOT_A, PIN_MOT_C, PIN_ENC_A, PIN_ENC_B, 
                      PIN_LUZ_ROJA, PIN_LUZ_VERDE, PIN_LUZ_NARANJA,
                      PIN_BTN_ABRIR, PIN_BTN_CERRAR, PIN_BTN_CALIB, PIN_SETA);
  
  miMotor->setLogger(printLog); // Para que el motor pueda escribir en el log web
  miMotor->begin();             // Carga calibración y configuración guardada

  // 3. Iniciar MQTT (Usando los datos de Constants.h)
  mqtt.setup(MQTT_BROKER, MQTT_PORT, MQTT_USER, MQTT_PASS, 
             MQTT_TOPIC_IN, MQTT_TOPIC_OUT, MQTT_ID_PLC);
             
  // Vinculamos los mensajes MQTT entrantes al ejecutor central
  mqtt.setCommandCallback([](String cmd) { 
      printLog("MQTT Recibido: " + cmd);
      ejecutarComando(cmd); 
  });
}

// ==========================================
//               LOOP PRINCIPAL
// ==========================================
void loop() {
  // 1. Mantener servicios activos
  wifiServer.loop();
  mqtt.loop();
  
  // 2. Actualizar lógica del motor (Luces, botones, movimiento, seguridad)
  if (miMotor != nullptr) {
    miMotor->update();

    // 3. GESTIÓN DE REPORTES MQTT (Solo si hay cambios)
    // Revisamos cada 100ms para tener buena respuesta sin saturar la red
    if (millis() - lastCheckTime > 100) {
       lastCheckTime = millis();
       
       // A) ¿Ha cambiado el ESTADO (texto)?
       // Ej: De "status abriendo" a "status parado"
       String estadoActual = miMotor->getEstadoString();
       if (estadoActual != lastEstadoStr) {
          mqtt.publish(estadoActual); 
          lastEstadoStr = estadoActual;
          
          // Truco: Si cambia el estado, forzamos a reenviar la posición 
          // en la siguiente vuelta para asegurar que la pantalla se actualiza bien
          lastPorcentaje = -1; 
       }

       // B) ¿Ha cambiado la POSICIÓN (número)?
       // Solo enviamos si el porcentaje cambia. Ej: 50 -> 51
       int porcActual = miMotor->getPorcentajeEntero();
       if (porcActual != lastPorcentaje) {
          // Construimos mensaje: "pos " + "50"
          String msgPos = String(PREFIJO_POS) + String(porcActual);
          mqtt.publish(msgPos);
          lastPorcentaje = porcActual;
       }
    }
  }

  // 4. Lectura del Puerto Serie (Para depuración manual)
  if (Serial.available()) {
    char c = Serial.read();
    // Filtramos saltos de línea para no enviar comandos vacíos
    if(c != '\n' && c != '\r') {
       // Si escribes "m50" en el monitor serie, lo captura aquí
       if (c == 'm' || c == 'A') {
          // Leer el resto del número
          String resto = Serial.readStringUntil('\n');
          ejecutarComando(String(c) + resto);
       } 
       else {
          // Comandos de una sola letra
          ejecutarComando(String(c));
       }
    }
  }
}
*/