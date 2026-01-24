#ifndef GRUPOMOTORES_H
#define GRUPOMOTORES_H

#include <vector>
#include "Motor.h"
#include "Constantes.h"

class GrupoMotores {
  private:
    std::vector<Motor*> motores; // Lista dinámica de motores

  public:
    GrupoMotores() {}

    void agregarMotor(Motor* m) {
        motores.push_back(m);
    }

    // --- COMANDOS (Broadcast) ---
    // Simplemente reparte la orden a todos. Cada motor se gestiona a sí mismo.

    void abrir() {
        for (auto m : motores) m->abrir();
    }

    void cerrar() {
        for (auto m : motores) m->cerrar();
    }

    void parar() {
        for (auto m : motores) m->parar();
    }

    void moverA(int porcentaje) {
        for (auto m : motores) m->moverA(porcentaje);
    }

    void setAnticipacion(int valor) {
        for (auto m : motores) m->setAnticipacion(valor);
    }
    
    void setModo(bool esTiempo) {
        for (auto m : motores) m->setModo(esTiempo);
    }

    // Método para ver si alguien necesita actualizar MQTT
    bool checkMqttRequest() {
        bool algunMotorQuiere = false;
        for (auto m : motores) {
            // Usamos OR para que si uno quiere, devuelva true, 
            // pero ejecutamos la función en todos para limpiar sus banderas.
            if (m->checkMqttRequest()) algunMotorQuiere = true;
        }
        return algunMotorQuiere;
    }

    // Comandos de calibración
    void calibrar()     { for (auto m : motores) m->calibrar(); }
    void setZero()      { for (auto m : motores) m->setZero(); }
    void set100()       { for (auto m : motores) m->set100(); }
    void finCalibrado() { for (auto m : motores) m->finCalibrado(); }

    // El rearme ya no limpia errores de grupo (porque no hay), 
    // pero podemos usarlo para rearmar motores individuales si fuera necesario.
    void intentarRearme() {
         // (Opcional) Podríamos iterar sobre los motores si tuvieran un método rearmar() público
         // De momento no hace nada a nivel grupo.
    }

    // --- UPDATE ---
    void update() {
        // Solo actualizamos a los hijos. Ya no hay "policía" vigilando diferencias.
        for (auto m : motores) {
            m->update();
        }
    }

    // --- FEEDBACK: CONSENSO DE ESTADO ---
    String getEstadoString() {
        bool alguienEmergencia = false;
        bool alguienErrorLim   = false;
        bool alguienNoCal      = false;
        bool alguienCalibrando = false;
        bool alguienMoviendo   = false; // Esto es clave
        
        int contAbiertos = 0;
        int contCerrados = 0;

        for (auto m : motores) {
            String s = m->getEstadoString();
            
            // Prioridad 1: Errores Críticos
            if (s == MSG_EMERGENCIA) alguienEmergencia = true;
            if (s == MSG_ERROR_LIMITE) alguienErrorLim = true;
            
            // Prioridad 2: Configuración
            if (s == MSG_NO_CALIBRADO) alguienNoCal = true;
            if (s == MSG_CALIBRANDO) alguienCalibrando = true;
            
            // Prioridad 3: Movimiento
            // Si UNO se mueve, para el usuario el grupo "se está moviendo"
            if (s == MSG_ABRIENDO || s == MSG_CERRANDO) alguienMoviendo = true;
            
            // Contadores para estado final
            if (s == MSG_ABIERTO) contAbiertos++;
            if (s == MSG_CERRADO) contCerrados++;
        }

        // --- JERARQUÍA DE MENSAJES ---
        
        if (alguienEmergencia) return MSG_EMERGENCIA;
        if (alguienErrorLim)   return MSG_ERROR_LIMITE; // Nuevo error individual
        if (alguienNoCal)      return MSG_NO_CALIBRADO;
        if (alguienCalibrando) return MSG_CALIBRANDO;
        
        // Si uno corre más que otro:
        // El rápido llegará y dirá "Abierto". El lento dirá "Abriendo".
        // Como 'alguienMoviendo' será true, el MQTT dirá "status abriendo".
        // Cuando el lento llegue, ambos dirán "Abierto" -> MQTT dirá "status abierto".
        if (alguienMoviendo)   return MSG_ABRIENDO; 

        // Estados de Reposo (Solo si TODOS han llegado)
        if (contAbiertos == motores.size()) return MSG_ABIERTO;
        if (contCerrados == motores.size()) return MSG_CERRADO;

        return MSG_PARADO; // Algunos llegaron, otros no, o están en medio
    }

    // --- FEEDBACK: PROMEDIO DE POSICIÓN ---
    // Aunque vayan a distinta velocidad, mostramos la media para no volver loca la barra web
    int getPorcentajeEntero() {
        if (motores.empty()) return 0;
        long suma = 0;
        for (auto m : motores) {
            suma += m->getPorcentajeEntero();
        }
        return (int)(suma / motores.size());
    }
    
    void imprimirEstado() {
       Serial.println("[GRUPO] Estado: " + getEstadoString() + " | Media: " + String(getPorcentajeEntero()) + "%");
       for(auto m : motores) m->imprimirEstado();
    }
};

#endif