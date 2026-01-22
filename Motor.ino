#include <Arduino.h>
#include "Motor.h"
#include "GrupoMotores.h"
#include "WiFiWebServer.h"
#include "Mqtt.h"       
#include "Constantes.h" 

// ==========================================
//           DEFINICIÓN DE PINES
// ==========================================

// --- MOTOR 1 (Principal) ---
#define PIN_M1_SETA       I0_1
#define PIN_M1_BTN_ABRIR  I0_4
#define PIN_M1_BTN_CERRAR I0_3
#define PIN_M1_BTN_CALIB  I0_2
#define PIN_M1_ENC_A      I0_6
#define PIN_M1_ENC_B      I0_5
#define PIN_M1_MOT_A      Q2_7
#define PIN_M1_MOT_C      Q2_6
#define PIN_M1_LUZ_ROJA   Q0_3
#define PIN_M1_LUZ_VERDE  Q0_5
#define PIN_M1_LUZ_NARANJA Q0_4

// --- MOTOR 2 (Secundario) ---
#define PIN_M2_SETA       I0_1  // Seta compartida
#define PIN_M2_BTN_ABRIR  I1_4
#define PIN_M2_BTN_CERRAR I1_3
#define PIN_M2_BTN_CALIB  I1_2
#define PIN_M2_ENC_A      I1_6
#define PIN_M2_ENC_B      I1_5
#define PIN_M2_MOT_A      Q1_7
#define PIN_M2_MOT_C      Q1_6
#define PIN_M2_LUZ_ROJA   Q1_3
#define PIN_M2_LUZ_VERDE  Q1_5
#define PIN_M2_LUZ_NARANJA Q1_4

// ==========================================
//             OBJETOS GLOBALES
// ==========================================

GrupoMotores* miGrupo = nullptr; 
WiFiWebServer wifiServer("MOVISTAR_D679", "7R8w77PGSkqcdK7", "esp32");
MqttHandler mqtt; // Asumo que la clase dentro de Mqtt.h se sigue llamando MqttHandler

// ==========================================
//               LOGGING
// ==========================================
void printLog(String msg) {
  Serial.println(msg);
  wifiServer.log(msg);
}

// ==========================================
//           EJECUTOR CENTRAL
// ==========================================
void ejecutarComando(String cmd) {
  if (cmd.length() == 0) return;
  char c = cmd.charAt(0);

  // Configuración Sistema
  if (c == 'T') { if (miGrupo) miGrupo->setModo(true); }
  else if (c == 'E') { if (miGrupo) miGrupo->setModo(false); }
  
  // Comandos de Grupo
  else if (miGrupo != nullptr) {
    if (c == 'a') miGrupo->abrir();
    else if (c == 'z') miGrupo->cerrar();
    else if (c == 's') miGrupo->parar();
    else if (c == 'c') miGrupo->calibrar();
    else if (c == '0') miGrupo->setZero();
    else if (c == '1') miGrupo->set100();
    else if (c == 'f') miGrupo->finCalibrado();
    else if (c == 'i') miGrupo->imprimirEstado();
    else if (c == 'r') miGrupo->intentarRearme(); // Reset errores
    
    // Comandos con valor
    else if (cmd.length() > 1) {
       int val = cmd.substring(1).toInt();
       if (c == 'A') miGrupo->setAnticipacion(val);
       else if (c == 'm') miGrupo->moverA(val);
    }
  }
}

// ==========================================
//                  SETUP
// ==========================================
void setup() {
  Serial.begin(115200);
  delay(1000);
  printLog("--- SISTEMA PLC MULTIMOTOR ---");
  
  // 1. Web
  wifiServer.setup();
  wifiServer.setCommandCallback([](String cmd) { ejecutarComando(cmd); });

  // 2. Grupo
  miGrupo = new GrupoMotores();

  // 3. Motor 1
  Motor* m1 = new Motor("mot1", 
                        PIN_M1_MOT_A, PIN_M1_MOT_C, PIN_M1_ENC_A, PIN_M1_ENC_B, 
                        PIN_M1_LUZ_ROJA, PIN_M1_LUZ_VERDE, PIN_M1_LUZ_NARANJA,
                        PIN_M1_BTN_ABRIR, PIN_M1_BTN_CERRAR, PIN_M1_BTN_CALIB, PIN_M1_SETA);
  m1->setLogger(printLog);
  m1->begin();
  miGrupo->agregarMotor(m1);

  // 4. Motor 2
  /*Motor* m2 = new Motor("mot2", 
                        PIN_M2_MOT_A, PIN_M2_MOT_C, PIN_M2_ENC_A, PIN_M2_ENC_B, 
                        PIN_M2_LUZ_ROJA, PIN_M2_LUZ_VERDE, PIN_M2_LUZ_NARANJA, // Comparten luces
                        PIN_M2_BTN_ABRIR, PIN_M2_BTN_CERRAR, PIN_M2_BTN_CALIB, PIN_M2_SETA);
  m2->setLogger(printLog);
  m2->begin();
  miGrupo->agregarMotor(m2);
  */
  
  // 5. MQTT
  mqtt.setup(MQTT_BROKER, MQTT_PORT, MQTT_USER, MQTT_PASS, 
             MQTT_TOPIC_IN, MQTT_TOPIC_OUT, MQTT_ID_PLC);
             
  mqtt.setCommandCallback([](String cmd) { 
      printLog("MQTT: " + cmd);
      ejecutarComando(cmd); 
  });
  
  // Vincular Grupo
  mqtt.vincularGrupo(miGrupo);
}

// ==========================================
//                  LOOP
// ==========================================
void loop() {
  wifiServer.loop();
  mqtt.loop(); 
  
  // Update del Grupo (Vigila desincronización y actualiza motores)
  if (miGrupo != nullptr) miGrupo->update();

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
*/