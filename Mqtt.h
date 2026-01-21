#ifndef MQTT_H
#define MQTT_H

#include <WiFi.h>
#include <PubSubClient.h>
#include <functional>
#include "Constantes.h"  

class MqttHandler {
  private:
    WiFiClient espClient;
    PubSubClient client;
    Motor* _motorVigilado = nullptr; // Puntero al motor que vamos a espiar
    
    // Configuración
    const char* _server; int _port;
    const char* _user; const char* _pass;
    const char* _topicIn; const char* _topicOut;
    const char* _deviceId;

    // Variables de control interno
    unsigned long lastReconnectAttempt = 0;
    unsigned long lastCheckTime = 0;
    String lastEstadoStr = "";
    int lastPorcentaje = -1;
    
    std::function<void(String)> commandCallback;

    // --- LÓGICA PRIVADA DE VIGILANCIA ---
    void vigilarMotor() {
      if (_motorVigilado == nullptr) return;
      
      // Revisar cada 100ms
      if (millis() - lastCheckTime > 100) {
         lastCheckTime = millis();
         
         // 1. CHEQUEAR ESTADO (Texto: "status abriendo", etc.)
         String estadoActual = _motorVigilado->getEstadoString();
         if (estadoActual != lastEstadoStr) {
            publish(estadoActual); 
            lastEstadoStr = estadoActual;
            lastPorcentaje = -1; // Forzar reenvío de posición al cambiar estado
         }

         // 2. CHEQUEAR POSICIÓN (Número: "pos 50")
         int porcActual = _motorVigilado->getPorcentajeEntero();
         if (porcActual != lastPorcentaje) {
            String msgPos = String(PREFIJO_POS) + String(porcActual);
            publish(msgPos);
            lastPorcentaje = porcActual;
         }
      }
    }

  public:
    MqttHandler() : client(espClient) {}

    void setup(const char* server, int port, const char* user, const char* pass, 
               const char* topicIn, const char* topicOut, const char* deviceId) {
      _server = server; _port = port; _user = user; _pass = pass;
      _topicIn = topicIn; _topicOut = topicOut; _deviceId = deviceId;

      client.setServer(_server, _port);
      
      // Callback limpio: Recibe y entrega sin traducir nada
      client.setCallback([this](char* topic, byte* payload, unsigned int length) {
        String msg = "";
        for (int i = 0; i < length; i++) msg += (char)payload[i];
        msg.trim();
        
        if (commandCallback) commandCallback(msg);
      });
    }

    // --- VINCULAR MOTOR PARA VIGILANCIA ---
    void vincularMotor(Motor* motor) {
      _motorVigilado = motor;
    }

    void setCommandCallback(std::function<void(String)> callback) {
      commandCallback = callback;
    }

    // --- LOOP CON VIGILANCIA INTEGRADA ---
    void loop() {
      if (!client.connected()) {
        unsigned long now = millis();
        if (now - lastReconnectAttempt > 5000) {
          lastReconnectAttempt = now;
          if (reconnect()) lastReconnectAttempt = 0;
        }
      } else {
        client.loop();
        vigilarMotor(); // <--- Aquí vigila y publica si hay cambios
      }
    }

    bool reconnect() {
      if (client.connect(_deviceId, _user, _pass, _topicOut, 1, true, MSG_OFFLINE)) {
        client.subscribe(_topicIn);
        client.publish(_topicOut, MSG_ONLINE); 
        return true;
      }
      return false;
    }

    void publish(String msg) {
      if (client.connected()) {
        client.publish(_topicOut, msg.c_str());
      }
    }
};

#endif

/*
#ifndef MQTT_H
#define MQTT_H

#include <WiFi.h>
#include <PubSubClient.h>
#include <functional>

class MqttHandler {
  private:
    WiFiClient espClient;
    PubSubClient client;
    
    // Datos de conexión
    const char* _server;
    int _port;
    const char* _user;
    const char* _pass;
    const char* _topicIn;
    const char* _topicOut;
    const char* _deviceId;

    unsigned long lastReconnectAttempt = 0;
    
    // Puntero a la función del Main que ejecuta comandos
    std::function<void(String)> commandCallback;

  public:
    MqttHandler() : client(espClient) {}

    void setup(const char* server, int port, const char* user, const char* pass, 
               const char* topicIn, const char* topicOut, const char* deviceId) {
      _server = server; _port = port; _user = user; _pass = pass;
      _topicIn = topicIn; _topicOut = topicOut; _deviceId = deviceId;

      client.setServer(_server, _port);
      // Configurar buffer si los mensajes son largos (opcional)
      client.setBufferSize(512); 

      // Callback interno de la librería
      client.setCallback([this](char* topic, byte* payload, unsigned int length) {
        String msg = "";
        for (int i = 0; i < length; i++) msg += (char)payload[i];
        msg.trim();
        
        // --- TRADUCTOR DE COMANDOS ANTIGUOS (SI ES NECESARIO) ---
        // Si llega 'P50' (Posición), lo convertimos a 'm50' (Mover)
        if (msg.startsWith("P") || msg.startsWith("p")) {
           msg = "m" + msg.substring(1);
        }
        else if (msg.equalsIgnoreCase("S")) msg = "s"; // Stop
        else if (msg.equalsIgnoreCase("C")) msg = "c"; // Calibrar
        // --------------------------------------------------------

        if (commandCallback) commandCallback(msg);
      });
    }

    void setCommandCallback(std::function<void(String)> callback) {
      commandCallback = callback;
    }

    void loop() {
      if (!client.connected()) {
        unsigned long now = millis();
        // Intentar reconectar cada 5 segs (No bloqueante)
        if (now - lastReconnectAttempt > 5000) {
          lastReconnectAttempt = now;
          if (reconnect()) lastReconnectAttempt = 0;
        }
      } else {
        client.loop();
      }
    }

    bool reconnect() {
      // Conectamos con Last Will ("OFFLINE" si se muere)
      if (client.connect(_deviceId, _user, _pass, _topicOut, 1, true, "OFFLINE")) {
        client.subscribe(_topicIn);
        client.publish(_topicOut, "ONLINE"); 
        return true;
      }
      return false;
    }

    void publish(String msg) {
      if (client.connected()) {
        client.publish(_topicOut, msg.c_str());
      }
    }
};

#endif
*/