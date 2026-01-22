#ifndef MQTT_H
#define MQTT_H

#include <WiFi.h>
#include <PubSubClient.h>
#include <functional>
#include "Constantes.h"  
#include "GrupoMotores.h" // <--- IMPORTANTE: Necesario para conocer la clase grupo

class MqttHandler {
  private:
    WiFiClient espClient;
    PubSubClient client;
    
    // CAMBIO CLAVE: Ahora vigilamos un Grupo, no un Motor suelto
    GrupoMotores* _grupoVigilado = nullptr; 
    
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
    void vigilarGrupo() { // Renombrado para claridad
      if (_grupoVigilado == nullptr) return;
      
      // Revisar cada 100ms
      if (millis() - lastCheckTime > 100) {
         lastCheckTime = millis();
         
         // 1. CHEQUEAR ESTADO CONSOLIDADO (El grupo decide si es "error", "abriendo", etc.)
         String estadoActual = _grupoVigilado->getEstadoString();
         if (estadoActual != lastEstadoStr) {
            publish(estadoActual); 
            lastEstadoStr = estadoActual;
            lastPorcentaje = -1; // Forzar reenvío de posición al cambiar estado
         }

         // 2. CHEQUEAR POSICIÓN MEDIA (El grupo hace el promedio)
         int porcActual = _grupoVigilado->getPorcentajeEntero();
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
      
      // Callback limpio
      client.setCallback([this](char* topic, byte* payload, unsigned int length) {
        String msg = "";
        for (int i = 0; i < length; i++) msg += (char)payload[i];
        msg.trim();
        
        if (commandCallback) commandCallback(msg);
      });
    }

    // --- NUEVO MÉTODO DE VINCULACIÓN ---
    void vincularGrupo(GrupoMotores* grupo) {
      _grupoVigilado = grupo;
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
        vigilarGrupo(); // <--- Vigila al grupo entero
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
*/