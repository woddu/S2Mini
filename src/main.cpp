#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <WiFi.h>
#include <WiFiAP.h>
#include <ESPmDNS.h>
#include <LittleFS.h>
#include <vector>
#include <string>

const char* ssid = "S2-Mini";
const char* password = "123456789";

AsyncWebServer server(80);

typedef struct {
  IPAddress ip;
  std::string fields;
} Response;

// typedef struct {
//   String label;
//   String name;
// } Field;

// TODO Vector of form fields
std::string adminUsername = "admin";
std::string adminPassword = "admin";

const char* PARAM_FIRSTNAME = "firstname";
const char* PARAM_LASTNAME = "lastname";
const char* PARAM_USERNAME = "username";
const char* PARAM_PASSWORD = "password";

// TODO Boolean for if accepting responses
// TODO Countdown for time allocation for attendace

// TODO Vector of responses

std::vector<Response> responses;
// std::vector<IPAddress> respondents;

IPAddress hostIP;

// std::vector<Field> fields;

// TODO Clear vectors on End method called by the teacher 


bool responded(IPAddress &ip){
  for (Response &responded : responses){
    if (ip == responded.ip){
    // if (ip == responded){
      return true;
    }
  }

  return false;
}

String name(IPAddress &ip){
  for (Response &responded : responses){
    if (ip == responded.ip){
    // if (ip == responded){
      return responded.fields.c_str();
    }
  }
  return "Oops There is a Mistake";
}

void notFound(AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Not found");
}

void setup() {
  // put your setup code here, to run once:
   Serial.begin(115200);

  if(!LittleFS.begin(true)){
    Serial.println("An Error has occurred while mounting LittleFS");
    return;
  }

  if (!WiFi.softAP(ssid, password)) {
    log_e("Soft AP creation failed.");
    while(1);
  }

  if (!MDNS.begin("attendance")) {
        Serial.println("Error setting up MDNS responder!");
        while(1) {
            delay(1000);
        }
    }

  
  server.serveStatic("/style.css", LittleFS, "/style.css").setCacheControl("max-age=600");
  server.serveStatic("/htmx.min.js", LittleFS, "/htmx.min.js").setCacheControl("max-age=600");


  // Server admin.html
  server.on("/admin", HTTP_GET, [] (AsyncWebServerRequest *request) {
    // Check if the hostIP has not been set yet
    if (hostIP == IPAddress(0,0,0,0)){
      request->send(LittleFS,"/admin.html");
    } else {
      IPAddress clientIP = request->client()->remoteIP();
      // Check if the client is the Host 
        if (clientIP == hostIP){
          request->send(LittleFS,"/admin.html");
        } else {
          request->redirect("/");
        }
    }
  });
  // HTMX admin form
  server.on("/partial/adminform", HTTP_POST, [](AsyncWebServerRequest *request) {
    IPAddress clientIP;
    // Check for ip param
    if (request->hasParam("ip", true)){
      if (!clientIP.fromString(request->getParam("ip",true)->value())){
        request->send(200, "text/html","<h1>Please Use Chrome or Firefox</h1>");
        // request->send(200, "text/plain", "Unsuccessful");
      }
    }
    // Check if the hostIP has not been set yet for extra security
    if (hostIP == IPAddress(0,0,0,0)){
      request->send(LittleFS, "/partial/adminform.html");
    } else {
      // Check if the client is the Host or has already authenticated
      if (clientIP == hostIP){
        request->send(LittleFS, "/partial/admincontrol.html");
      } else {
        request->redirect("/");
      }
    }
  });
  // Admin authenticate
  server.on("/admin", HTTP_POST, [](AsyncWebServerRequest *request){
    IPAddress clientIP;
    // Check for ip param
    if (request->hasParam("ip", true)){
      if (!clientIP.fromString(request->getParam("ip",true)->value())){
        request->send(200, "text/html","<h1>Please Use Chrome or Firefox</h1>");
        // request->send(200, "text/plain", "Unsuccessful");
      }
    }
    if (request->hasParam(PARAM_USERNAME, true) && request->hasParam(PARAM_PASSWORD, true)){
        std::string param_username = request->getParam(PARAM_USERNAME, true)->value().c_str();
        std::string param_password = request->getParam(PARAM_PASSWORD, true)->value().c_str();
        // Authenticate admin
        if (param_username == adminUsername && param_password == adminPassword){
          hostIP = clientIP;
          request->send(LittleFS, "/partial/admincontrol.html");
        } else {
          String param_username = request->getParam(PARAM_USERNAME, true)->value();
          String param_password = request->getParam(PARAM_PASSWORD, true)->value();
          request->send(LittleFS, "/partial/adminerror.html", String(), false, [param_username, param_password](const String& var) -> String{
            if (var == "ERROR"){
              String inputs;
              inputs +=  "<label for=\"username\">Username<input type=\"text\" name=\"username\" id=\"username\" required autofocus aria-invalid=\"true\"value=\""+ param_username + "\"></label>";
              inputs += "<label for=\"password\">Password<input type=\"password\" name=\"password\" id=\"password\" required aria-invalid=\"true\"aria-describedby=\"error-password\" value=\""+ param_password +"\"><small id=\"error-password\">Wrong Credentials</small></label>";
              return inputs;
            }
            return String();
          });
        }
      } else {
        request->send(LittleFS, "/partial/adminform.html");
      }
  });
  // Admin Profile
  server.on("/partial/adminprofile", HTTP_POST, [](AsyncWebServerRequest *request){
    IPAddress clientIP;
    // Check for ip param
    if (request->hasParam("ip", true)){
      if (!clientIP.fromString(request->getParam("ip",true)->value())){
        request->send(200, "text/html","<h1>Please Use Chrome or Firefox</h1>");
        // request->send(200, "text/plain", "Unsuccessful");
      }
    }
    // Check if the hostIP has not been set yet for extra security
    if (hostIP == IPAddress(0,0,0,0)){
      request->send(LittleFS, "/partial/adminform.html");
    } else {
      // Check if the client is the Host or has already authenticated
      if (clientIP == hostIP){
        request->send(LittleFS, "/partial/adminprofile.html", String(), false, [](const String& var) -> String{
          if (var == "PROFILE"){
            String inputs;
              inputs +=  "<label for=\"username\">Username<input type=\"text\" name=\"username\" id=\"username\" required autofocus " + String(adminUsername.c_str()) + "\"></label>";
              inputs += "<label for=\"password\">Password<input type=\"password\" name=\"password\" id=\"password\" required " + String(adminPassword.c_str()) + "\"></label>";
              return inputs;
          }
          return String();
        });
      } else {
        request->redirect("/");
      }
    }
  });
  
  // Serve attendance.html
  server.on("/", HTTP_GET, [] (AsyncWebServerRequest *request) {
    request->send(LittleFS,"/attendance.html");
  });
  // HTMX attendanceform
  server.on("/partial/attendanceform", HTTP_POST, [] (AsyncWebServerRequest *request){
    IPAddress clientIP;
    // Check for ip param
    if (request->hasParam("ip", true)){
      if (!clientIP.fromString(request->getParam("ip",true)->value())){
        request->send(200, "text/html","<h1>Please Use Chrome or Firefox</h1>");
        // request->send(200, "text/plain", "Unsuccessful");
      }
    }
    // Check if the clientIp has already responded
    if (!responded(clientIP)){
      request->send(LittleFS, "/partial/attendanceform.html");
    } else {
      request->send(200, "text/html","<hgroup><h1>"+name(clientIP)+"</h1><p>Kindly disconnect form the <strong>Wifi</strong> to give way to others.</p></hgroup>");
    }
  });
  // Attendance Submition
  server.on("/attendance", HTTP_POST, [](AsyncWebServerRequest *request){
    IPAddress clientIP;
    // Check for ip param
    if (request->hasParam("ip", true)){
      if (!clientIP.fromString(request->getParam("ip",true)->value())){
        request->send(200, "text/html", "Unsuccessful");
      }
    }
    // Check if the clientIp has already responded
    if (!responded(clientIP)){

      std::string collect;

      if (request->hasParam(PARAM_FIRSTNAME, true) && request->hasParam(PARAM_LASTNAME, true)) {
          collect = request->getParam(PARAM_FIRSTNAME, true)->value().c_str();
          collect.append(" ");
          collect.append(request->getParam(PARAM_LASTNAME, true)->value().c_str());
      } else {
          collect = "Nothing sent";
          request->send(200,"text/plain");
      }
      responses.push_back({clientIP, collect});
      request->send(200, "text/plain", ("Hello, POST: " + collect +", IP:"+ clientIP.toString().c_str()).c_str());
    } else {
      request->send(200, "text/html","<hgroup><h1>"+name(clientIP)+"</h1><p>Kindly disconnect form the <strong>Wifi</strong> to give way to others.</p></hgroup>");
    }
  });

  server.onNotFound(notFound);

  server.begin();

  // Add service to MDNS-SD
  MDNS.addService("http", "tcp", 80);

}

void loop() {
  // put your main code here, to run repeatedly:
  delay(10);
}
