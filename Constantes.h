#ifndef CONSTANTES_H
#define CONSTANTES_H

// ==========================================
//        MENSAJES DE ESTADO (Salida)
// ==========================================
// Estos son los textos que el PLC enviará por MQTT
const char* MSG_EMERGENCIA    = "status emergency_pressed";
const char* MSG_NO_CALIBRADO  = "status no_calibrado";
const char* MSG_CALIBRANDO    = "status calibrando";
const char* MSG_ABRIENDO      = "status abriendo";
const char* MSG_CERRANDO      = "status cerrando";
const char* MSG_ABIERTO       = "status opened"; 
const char* MSG_CERRADO       = "status closed"; 
const char* MSG_PARADO        = "status stopped";  
const char* MSG_ONLINE        = "ONLINE";
const char* MSG_OFFLINE       = "OFFLINE";
const char* MSG_ERROR_LIMITE  = "status error_limite";
const char* MSG_ERROR_ATASCO  = "status error_encoder"; 

// Prefijo para la posición (ej: "pos 50")
const char* PREFIJO_POS       = "pos "; 

// ==========================================
//        CONFIGURACIÓN MQTT
// ==========================================
const char* MQTT_BROKER       = "85.208.22.48";
const int   MQTT_PORT         = 1883;
const char* MQTT_USER         = "admin";
const char* MQTT_PASS         = "admin";
const char* MQTT_ID_PLC       = "PLC_TARRAGONA";

// Topics
const char* MQTT_TOPIC_IN     = "/30EDA0A6CCC4/toPLC"; 
const char* MQTT_TOPIC_OUT    = "/30EDA0A6CCC4/return";

#endif