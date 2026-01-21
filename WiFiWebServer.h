#ifndef WIFIWEBSERVER_H
#define WIFIWEBSERVER_H

#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>

String webLogBuffer = "";

class WiFiWebServer {
public:
  WiFiWebServer(const char* ssid, const char* password, const char* host) 
    : ssid(ssid), password(password), host(host), server(80) {}

  // --- CAMBIO AQUÍ: Ahora aceptamos String (texto completo) en vez de char (letra) ---
  typedef std::function<void(String)> CommandHandler;
  CommandHandler onCommandReceived;

  void setCommandCallback(CommandHandler handler) {
    onCommandReceived = handler;
  }

  void log(String text) {
    String timePrefix = "[" + String(millis()/1000) + "s] ";
    webLogBuffer = timePrefix + text + "\n" + webLogBuffer; 
    if (webLogBuffer.length() > 2000) webLogBuffer = webLogBuffer.substring(0, 2000);
  }

  void setup() {
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
    }
    Serial.println("");
    Serial.print("WiFi Conectado. IP: "); Serial.println(WiFi.localIP());

    if (!MDNS.begin(host)) {
      Serial.println("Error mDNS");
    }

    server.on("/", HTTP_GET, std::bind(&WiFiWebServer::handleRoot, this));
    server.on("/serverIndex", HTTP_GET, std::bind(&WiFiWebServer::handleServerIndex, this));
    server.on("/login", HTTP_POST, std::bind(&WiFiWebServer::handleLogin, this));
    server.on("/update", HTTP_POST, std::bind(&WiFiWebServer::handleUpdateEnd, this), std::bind(&WiFiWebServer::handleDoUpdate, this));
    server.on("/cmd", HTTP_GET, std::bind(&WiFiWebServer::handleCommand, this));
    server.on("/logs", HTTP_GET, std::bind(&WiFiWebServer::handleGetLogs, this));

    server.begin();
  }

  void loop() {
    server.handleClient();
  }

private:
  const char* ssid;
  const char* password;
  const char* host;
  WebServer server;

  // ESTILOS CSS
  String getStyle() {
    return F(
      "<style>"
      "@import url('https://fonts.googleapis.com/css2?family=Poppins:wght@300;400;500;700&display=swap');"
      "body { background: #f0f2f5; font-family: 'Poppins', sans-serif; display: flex; flex-direction: column; align-items: center; min-height: 100vh; margin: 0; color: #4a5568; padding: 20px; }"
      ".container { width: 100%; max-width: 500px; }"
      ".card { background: white; padding: 2rem; border-radius: 1rem; box-shadow: 0 10px 25px -5px rgba(0,0,0,0.1); margin-bottom: 20px; text-align: center; }"
      ".company-name { font-size: 1.8rem; font-weight: 700; margin-bottom: 0; background: -webkit-linear-gradient(45deg, #3182ce, #2c5282); -webkit-background-clip: text; -webkit-text-fill-color: transparent; }"
      ".subtitle { font-size: 0.9rem; color: #718096; margin-bottom: 1.5rem; font-weight: 500; text-transform: uppercase; letter-spacing: 1px; }"
      "h2 { font-size: 1.2rem; font-weight: 600; margin-bottom: 1rem; color: #2d3748; border-bottom: 2px solid #edf2f7; padding-bottom: 5px; display: inline-block; }"
      ".cmd-group { display: flex; gap: 10px; margin-bottom: 15px; }"
      "#cmdInput { flex: 1; padding: 12px; font-size: 1.1rem; text-align: center; border: 2px solid #e2e8f0; border-radius: 8px; outline: none; transition: border-color 0.2s; }" 
      "#cmdInput:focus { border-color: #4299e1; }"
      ".btn-send { background: linear-gradient(135deg, #4299e1 0%, #3182ce 100%); color: white; border: none; border-radius: 8px; padding: 0 20px; font-weight: 600; cursor: pointer; transition: transform 0.1s; }"
      ".btn-send:active { transform: scale(0.95); }"
      "#console { background: #1a202c; color: #48bb78; font-family: 'Courier New', monospace; padding: 15px; border-radius: 8px; text-align: left; height: 200px; overflow-y: auto; font-size: 0.85rem; margin-top: 10px; border: 1px solid #2d3748; box-shadow: inset 0 2px 4px rgba(0,0,0,0.3); }"
      "input { width: 100%; padding: 0.8rem; margin-bottom: 1rem; border: 1px solid #e2e8f0; border-radius: 0.5rem; box-sizing: border-box; background: #f7fafc; }"
      ".btn-login { width: 100%; background: linear-gradient(135deg, #4299e1 0%, #3182ce 100%); color: white; padding: 0.8rem; border: none; border-radius: 0.5rem; font-weight: 600; cursor: pointer; }"
      ".file-upload-label { display: block; padding: 1rem; color: #4a5568; background: #edf2f7; border-radius: 0.5rem; cursor: pointer; border: 2px dashed #cbd5e0; margin-bottom: 10px; }"
      "#prgbar { width: 100%; background-color: #edf2f7; border-radius: 10px; height: 10px; overflow: hidden; }"
      "#bar { width: 0%; height: 100%; background: #48bb78; transition: width 0.3s; }"
      "</style>"
    );
  }

  void handleRoot() {
    String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1'>" + getStyle() + "</head><body><div class='container'><div class='card'>";
    html += "<div class='company-name'>MATEO E HIJO</div><div class='subtitle'>PLC de Tarragona</div>";
    html += "<h2>Carga de Código</h2>";
    html += "<form name='loginForm'>";
    html += "<input type='text' name='userid' placeholder='Usuario' autocomplete='off'>";
    html += "<input type='password' name='pwd' placeholder='Contraseña'>";
    html += "<input type='button' onclick='check()' class='btn-login' value='Entrar'>";
    html += "</form></div></div>";
    html += "<script>function check() { var u = document.getElementsByName('userid')[0].value; var p = document.getElementsByName('pwd')[0].value; fetch('/login', { method: 'POST', headers: {'Content-Type': 'application/x-www-form-urlencoded'}, body: 'userid='+encodeURIComponent(u)+'&pwd='+encodeURIComponent(p) }).then(r => { if(r.ok) window.location.href='/serverIndex'; else alert('Acceso Denegado'); }); }</script></body></html>";
    server.send(200, "text/html", html);
  }

  void handleServerIndex() {
    String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1'>" + getStyle() + "<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js'></script></head><body><div class='container'>";
    html += "<div class='card'>";
    html += "<div class='company-name'>MATEO E HIJO</div><div class='subtitle'>PLC de Tarragona</div>";
    html += "<h2>Enviar Comando</h2>";
    html += "<div class='cmd-group'>";
    html += "<input type='text' id='cmdInput' placeholder='Ej: a, z, m20...' maxlength='10' onkeydown='if(event.key===\"Enter\") sendCmd()'>";
    html += "<button class='btn-send' onclick='sendCmd()'>Enviar</button>";
    html += "</div>";
    html += "<h2>Registro (Logs)</h2>";
    html += "<div id='console'>Conectando con PLC...</div>";
    html += "</div>";
    html += "<div class='card'>";
    html += "<h2>Actualizar Firmware</h2>";
    html += "<form method='POST' action='#' enctype='multipart/form-data' id='upload_form'>";
    html += "<input type='file' name='update' id='file' onchange='sub(this)' style='display:none;'>";
    html += "<label for='file' class='file-upload-label' id='file-label'>📂 Toca para elegir archivo .bin</label>";
    html += "<input type='submit' class='btn-login' value='Instalar Actualización'>";
    html += "<div id='prgbar' style='margin-top:10px;'><div id='bar'></div></div>";
    html += "<div id='prg'></div>";
    html += "</form></div>";
    html += "</div>"; 
    html += "<script>";
    html += "function sendCmd() { var input = document.getElementById('cmdInput'); var val = input.value; if(val.length > 0) { fetch('/cmd?c=' + val); input.value = ''; } }";
    html += "setInterval(function() { fetch('/logs').then(r => r.text()).then(data => { document.getElementById('console').innerHTML = data.replace(/\\n/g, '<br>'); }); }, 1000);";
    html += "function sub(obj){ var fileName = obj.value.split('\\\\'); document.getElementById('file-label').innerHTML = '📄 ' + fileName[fileName.length-1]; };";
    html += "$('form').submit(function(e){ e.preventDefault(); var form = $('#upload_form')[0]; var data = new FormData(form); $.ajax({ url: '/update', type: 'POST', data: data, contentType: false, processData:false, xhr: function() { var xhr = new window.XMLHttpRequest(); xhr.upload.addEventListener('progress', function(evt) { if (evt.lengthComputable) { var per = evt.loaded / evt.total; $('#prg').html(Math.round(per*100) + '%'); $('#bar').css('width',Math.round(per*100) + '%'); } }, false); return xhr; }, success:function(d, s) { console.log('success!') }, error: function (a, b, c) { } }); });";
    html += "</script></body></html>";
    server.send(200, "text/html", html);
  }

  void handleLogin() {
    if (server.hasArg("userid") && server.hasArg("pwd")) {
      if (server.arg("userid") == "admin" && server.arg("pwd") == "admin") {
        server.sendHeader("Location", "/serverIndex");
        server.send(303);
      } else { server.send(401, "text/plain", "Error"); }
    } else { server.send(400, "text/plain", "Bad Request"); }
  }

  // --- CAMBIO AQUÍ: Enviamos el string completo (cmd) ---
  void handleCommand() {
    if (server.hasArg("c")) {
      String cmd = server.arg("c");
      if (cmd.length() > 0 && onCommandReceived) {
        // Antes era: char c = cmd.charAt(0);
        // AHORA: Pasamos todo el string
        onCommandReceived(cmd);
      }
    }
    server.send(200, "text/plain", "OK");
  }

  void handleGetLogs() {
    server.send(200, "text/plain", webLogBuffer);
  }

  void handleUpdateEnd() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", (Update.hasError()) ? "FALLO" : "OK");
    ESP.restart();
  }

  void handleDoUpdate() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      log("Actualizando: " + upload.filename);
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) Update.printError(Serial);
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) Update.printError(Serial);
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) log("Exito OTA: " + String(upload.totalSize) + " bytes");
      else Update.printError(Serial);
    }
  }
};

#endif

/*#ifndef WIFIWEBSERVER_H
#define WIFIWEBSERVER_H

#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>

class WiFiWebServer {
public:
  WiFiWebServer(const char* ssid, const char* password, const char* host) 
    : ssid(ssid), password(password), host(host), server(80) {}

  void setup() {
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
    }
    Serial.println("");
    Serial.print("WiFi Conectado. IP: "); Serial.println(WiFi.localIP());

    if (!MDNS.begin(host)) {
      Serial.println("Error mDNS");
    }

    // Rutas del servidor
    server.on("/", HTTP_GET, std::bind(&WiFiWebServer::handleRoot, this));
    server.on("/serverIndex", HTTP_GET, std::bind(&WiFiWebServer::handleServerIndex, this));
    server.on("/login", HTTP_POST, std::bind(&WiFiWebServer::handleLogin, this));
    server.on("/update", HTTP_POST, 
      std::bind(&WiFiWebServer::handleUpdateEnd, this), 
      std::bind(&WiFiWebServer::handleDoUpdate, this)
    );

    server.begin();
  }

  void loop() {
    server.handleClient();
  }

private:
  const char* ssid;
  const char* password;
  const char* host;
  WebServer server;

  // =========================================================
  // ESTILOS CSS MODERNOS (Con efecto de texto degradado)
  // =========================================================
  String getStyle() {
    return F(
      "<style>"
      "@import url('https://fonts.googleapis.com/css2?family=Poppins:wght@300;400;700&display=swap');"
      "body { background: #f0f2f5; font-family: 'Poppins', sans-serif; display: flex; justify-content: center; align-items: center; min-height: 100vh; margin: 0; color: #4a5568; }"
      ".card { background: white; padding: 2.5rem; border-radius: 1rem; box-shadow: 0 10px 25px -5px rgba(0, 0, 0, 0.1), 0 10px 10px -5px rgba(0, 0, 0, 0.04); width: 100%; max-width: 400px; text-align: center; }"
      
      // Estilo para el nombre de la empresa 
      ".company-name { font-size: 2rem; font-weight: 700; margin-bottom: 0.5rem; background: -webkit-linear-gradient(45deg, #3182ce, #2c5282); -webkit-background-clip: text; -webkit-text-fill-color: transparent; letter-spacing: -0.5px; }"
      
      "h2 { font-size: 1.1rem; font-weight: 400; margin-bottom: 2rem; color: #a0aec0; margin-top: 0; }"
      "input[type='text'], input[type='password'] { width: 100%; padding: 0.8rem; margin-bottom: 1rem; border: 1px solid #e2e8f0; border-radius: 0.5rem; font-size: 1rem; outline: none; transition: border-color 0.2s, box-shadow 0.2s; box-sizing: border-box; background: #f7fafc; }"
      "input:focus { border-color: #4299e1; box-shadow: 0 0 0 3px rgba(66, 153, 225, 0.2); background: white; }"
      ".btn { width: 100%; background: linear-gradient(135deg, #4299e1 0%, #3182ce 100%); color: white; padding: 0.8rem; border: none; border-radius: 0.5rem; font-size: 1rem; font-weight: 600; cursor: pointer; transition: transform 0.1s, box-shadow 0.2s; margin-top: 10px; }"
      ".btn:hover { box-shadow: 0 4px 6px rgba(66, 153, 225, 0.4); }"
      ".btn:active { transform: scale(0.98); }"
      ".file-upload { position: relative; display: inline-block; width: 100%; margin-bottom: 1rem; }"
      ".file-upload-label { display: flex; align-items: center; justify-content: center; padding: 1rem; color: #4a5568; background: #edf2f7; border-radius: 0.5rem; cursor: pointer; border: 2px dashed #cbd5e0; transition: all 0.2s; font-size: 0.9rem; }"
      ".file-upload-label:hover { background: #e2e8f0; border-color: #a0aec0; }"
      "#prgbar { width: 100%; background-color: #edf2f7; border-radius: 9999px; margin-top: 1.5rem; height: 0.6rem; overflow: hidden; }"
      "#bar { width: 0%; height: 100%; background: linear-gradient(90deg, #48bb78 0%, #38a169 100%); transition: width 0.3s ease; }"
      "</style>"
    );
  }

  // --- PÁGINA DE LOGIN ---
  void handleRoot() {
    String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += getStyle();
    html += "</head><body><div class='card'>";
    
    // Nombre de la empresa
    html += "<div class='company-name'>MATEO E HIJO</div>";
    html += "<h2>Acceso Técnico</h2>";
    
    html += "<form name='loginForm'>";
    html += "<input type='text' name='userid' placeholder='Usuario' autocomplete='off'>";
    html += "<input type='password' name='pwd' placeholder='Contraseña'>";
    html += "<input type='button' onclick='check()' class='btn' value='Iniciar Sesión'>";
    html += "</form></div>";
    
    html += "<script>";
    html += "function check() {";
    html += "  var u = document.getElementsByName('userid')[0].value;";
    html += "  var p = document.getElementsByName('pwd')[0].value;";
    html += "  fetch('/login', { method: 'POST', headers: {'Content-Type': 'application/x-www-form-urlencoded'}, body: 'userid='+encodeURIComponent(u)+'&pwd='+encodeURIComponent(p) })";
    html += "  .then(r => { if(r.ok) window.location.href='/serverIndex'; else alert('Acceso Denegado'); });";
    html += "}";
    html += "</script></body></html>";
    
    server.send(200, "text/html", html);
  }

  // --- PÁGINA DE ACTUALIZACIÓN ---
  void handleServerIndex() {
    String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += getStyle();
    html += "<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js'></script>";
    html += "</head><body><div class='card'>";
    
    // Nombre de la empresa también aquí
    html += "<div class='company-name'>MATEO E HIJO</div>";
    html += "<h2>Actualizar Firmware</h2>";

    html += "<form method='POST' action='#' enctype='multipart/form-data' id='upload_form'>";
    
    html += "<div class='file-upload'>";
    html += "<input type='file' name='update' id='file' onchange='sub(this)' style='display:none;'>";
    html += "<label for='file' class='file-upload-label' id='file-label'>📂 Toca para seleccionar archivo .bin</label>";
    html += "</div>";
    
    html += "<input type='submit' class='btn' value='Instalar Actualización'>";
    
    html += "<div id='prgbar'><div id='bar'></div></div>";
    html += "<div id='prg' style='margin-top:10px; font-weight:600; color:#2d3748; font-size: 0.9rem;'></div>";
    html += "</form></div>";

    html += "<script>";
    html += "function sub(obj){";
    html += "  var fileName = obj.value.split('\\\\');";
    html += "  var label = document.getElementById('file-label');";
    html += "  label.innerHTML = '📄 ' + fileName[fileName.length-1];";
    html += "  label.style.borderColor = '#4299e1';";
    html += "  label.style.color = '#2b6cb0';";
    html += "  label.style.background = '#ebf8ff';";
    html += "};";
    
    html += "$('form').submit(function(e){";
    html += "  e.preventDefault();";
    html += "  var form = $('#upload_form')[0];";
    html += "  var data = new FormData(form);";
    html += "  $.ajax({";
    html += "    url: '/update', type: 'POST', data: data, contentType: false, processData:false,";
    html += "    xhr: function() {";
    html += "      var xhr = new window.XMLHttpRequest();";
    html += "      xhr.upload.addEventListener('progress', function(evt) {";
    html += "        if (evt.lengthComputable) {";
    html += "          var per = evt.loaded / evt.total;";
    html += "          $('#prg').html('Subiendo: ' + Math.round(per*100) + '%');";
    html += "          $('#bar').css('width',Math.round(per*100) + '%');";
    html += "        }";
    html += "      }, false);";
    html += "      return xhr;";
    html += "    },";
    html += "    success:function(d, s) { console.log('success!') },";
    html += "    error: function (a, b, c) { }";
    html += "  });";
    html += "});";
    html += "</script></body></html>";

    server.send(200, "text/html", html);
  }

  void handleLogin() {
    if (server.hasArg("userid") && server.hasArg("pwd")) {
      if (server.arg("userid") == "admin" && server.arg("pwd") == "admin") {
        server.sendHeader("Location", "/serverIndex");
        server.send(303);
      } else {
        server.send(401, "text/plain", "Error de credenciales");
      }
    } else {
      server.send(400, "text/plain", "Bad Request");
    }
  }

  void handleUpdateEnd() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", (Update.hasError()) ? "FALLO" : "OK");
    ESP.restart();
  }

  void handleDoUpdate() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.printf("Actualizando: %s\n", upload.filename.c_str());
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { 
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) { 
        Serial.printf("Exito: %u bytes\n", upload.totalSize);
      } else {
        Update.printError(Serial);
      }
    }
  }
};

#endif
*/

