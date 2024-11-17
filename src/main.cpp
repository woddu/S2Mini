#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <WiFi.h>
#include <WiFiAP.h>
#include <ESPmDNS.h>
#include <LittleFS.h>
#include <vector>
#include <string>

// #include <Preferences.h>
// #include <ArduinoJson.h>

using namespace std;

// Preferences preferences;

uint8_t redLED = 1;
uint8_t greenLED = 2;
uint8_t blueLED = 3;

const char* ssid = "S2-Mini";
const char* password = "123456789";

AsyncWebServer server(80);

AsyncEventSource events("/events");

typedef struct {
  IPAddress ip;
  string fields;
} Response;

// typedef struct {
//   String label;
//   String name;
// } Field;

// TODO Vector of form fields
string adminUsername = "admin";
string adminPassword = "admin";

const char* PARAM_FIRSTNAME = "firstname";
const char* PARAM_LASTNAME = "lastname";
const char* PARAM_USERNAME = "username";
const char* PARAM_PASSWORD = "password";

// TODO Boolean for if accepting responses
bool started = false;
bool ended = true;

static TimerHandle_t xTimer = NULL;

// array<Response, 70> s_responses;
vector<Response> responses;
// vector<IPAddress> respondents;

IPAddress hostIP;

// vector<Field> fields;

// TODO Clear vectors on End method called by the teacher 

void TimerCallBack(TimerHandle_t xTimer){
  ended = true;
  events.send("finished","finished",millis());
}

unsigned long getRemainingTime()
{
    TickType_t currentTime = xTaskGetTickCount();
    TickType_t remainingTime = 0;
    
    if (xTimerIsTimerActive(xTimer)) {
        // Get the timer's expiry time
        TickType_t expiryTime = xTimerGetExpiryTime(xTimer);
        
        // Calculate the remaining time
        if (expiryTime >= currentTime) {
            remainingTime = (expiryTime - currentTime) * portTICK_PERIOD_MS / 60000; // Convert to milliseconds
        }
    }
    
    return remainingTime;
}

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

  // preferences.begin("attendance", false); // Open Preferences

  // ended = preferences.getBool("ended", false);

  // started = preferenes.getBool("started", false);


  pinMode(redLED, OUTPUT);
  pinMode(greenLED, OUTPUT);
  pinMode(blueLED, OUTPUT);


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
  server.serveStatic("/qr.png", LittleFS, "/qr.png").setCacheControl("max-age=600");


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
        if (started){
          request->send(LittleFS, "/partial/admincontrol.html", String(), false, [](const String& var) -> String{
              String names, csv,download;
              if (var == "NAMES"){
                for (Response &responded : responses){
                  names += "<tr><td>"+String(responded.fields.c_str())+"</td></tr>";
                }
                return names;
              }
              if (var == "CSV"){
                for (Response &responded : responses){
                  csv += "{ name: \""+ String(responded.fields.c_str()) + "\"},";
                }
                return csv;
              }
              if (var == "DOWNLOAD" && ended){
                download = "downloadCSV('names.csv', data);";
                return download;
              }
              return String();
            });
        } else {
          request->send(LittleFS, "/partial/admintime.html");
        }
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
        string param_username = request->getParam(PARAM_USERNAME, true)->value().c_str();
        string param_password = request->getParam(PARAM_PASSWORD, true)->value().c_str();
        // Authenticate admin
        if (param_username == adminUsername && param_password == adminPassword){
          hostIP = clientIP;
          if(started){
            request->send(LittleFS, "/partial/admincontrol.html", String(), false, [](const String& var) -> String{
              String names;
              if (var == "NAMES"){
                for (Response &responded : responses){
                  names += "<tr><td>"+String(responded.fields.c_str())+"</td></tr>";
                  return names;
                }
                return names;
              }
              return String();
            });
          } else {
            Serial.println("sending admin time");
            request->send(LittleFS, "/partial/admintime.html");
          }
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
        Serial.println("sending admin form from /admin");
        request->send(LittleFS, "/partial/adminform.html");
      }
  });
  // Admin Start
  server.on("/start", HTTP_POST, [](AsyncWebServerRequest *request){
    IPAddress clientIP;

    if (request->hasParam("ip", true)){
      clientIP.fromString(request->getParam("ip",true)->value());
    }

    if (hostIP == clientIP){
      if (request->hasParam("time", true)){
        String time = request->getParam("time", true)->value();
        xTimer = xTimerCreate(
          "Timer",
          (time.toInt() * 60000) / portTICK_PERIOD_MS,
          pdFALSE,
          (void*)0,
          TimerCallBack
        );
        xTimerStart(xTimer, portMAX_DELAY);
        started = true;
        ended = !started;
        
        Serial.println("Timer Started");
        request->send(LittleFS, "/partial/admincontrol.html", String(), false, [](const String& var) -> String{
          String names, csv, download;
          if (var == "NAMES"){
            for (Response &responded : responses){
              names += "<tr><td>"+String(responded.fields.c_str())+"</td></tr>";
            }
            return names;
          }
          if (var == "CSV"){
            for (Response &responded : responses){
              csv += "{ name: \""+ String(responded.fields.c_str()) + "\"},";
            }
            return csv;
          }
          if (var == "DOWNLOAD" && ended){
            download = "downloadCSV('names.csv', data);";
            return download;
          }
          return String();
        });
      } else {
        Serial.println("sending admin time from /started");
        request->send(LittleFS, "/partial/admintime.html");
      }
    } else {
      Serial.println("Not the Host");
      // request->send(LittleFS, "/partial/adminform.html");
    }
  });
  // Admin End
  server.on("/end", HTTP_POST, [](AsyncWebServerRequest *request){
    IPAddress clientIP;

    if (request->hasParam("ip", true)){
      clientIP.fromString(request->getParam("ip",true)->value());
    }

    if (hostIP == clientIP){
      if(!ended){
        ended = true;

        events.send("finished","finished",millis());

        if(xTimer != NULL){
          xTimerStop(xTimer, 0);
        }

      }

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
  // Admin Manual Attendance
  server.on("/manual", HTTP_POST, [](AsyncWebServerRequest *request){
    IPAddress clientIP;
    // Check for ip param
    if (request->hasParam("ip", true)){
      if (!clientIP.fromString(request->getParam("ip",true)->value())){
        request->send(200, "text/html","<h1>Please Use Chrome or Firefox</h1>");
        // request->send(200, "text/plain", "Unsuccessful");
      }
    }

    if (hostIP == clientIP){
      string collect;

        if (request->hasParam(PARAM_FIRSTNAME, true) && request->hasParam(PARAM_LASTNAME, true)) {
            collect = request->getParam(PARAM_FIRSTNAME, true)->value().c_str();
            collect.append(" ");
            collect.append(request->getParam(PARAM_LASTNAME, true)->value().c_str());
        } else {
            collect = "Nothing sent";
            request->send(200,"text/plain");
        }
        responses.push_back({clientIP, collect});
        
        request->send(LittleFS, "/partial/admincontrol.html", String(), false, [](const String& var) -> String{
          String names, csv, download;
          if (var == "NAMES"){
            for (Response &responded : responses){
              names += "<tr><td>"+String(responded.fields.c_str())+"</td></tr>";
            }
            return names;
          }
          if (var == "CSV"){
            for (Response &responded : responses){
              csv += "{ name: \""+ String(responded.fields.c_str()) + "\"},";
            }
            return csv;
          }
          if (var == "DOWNLOAD" && ended){
            download = "downloadCSV('names.csv', data);";
            return download;
          }
          return String();
        });
    }

  });
  
  // Serve attendance.html
  server.on("/", HTTP_GET, [] (AsyncWebServerRequest *request) {
    request->send(LittleFS,"/attendance.html");
  });
  // HTMX attendanceform
  server.on("/partial/attendanceform", HTTP_POST, [] (AsyncWebServerRequest *request){

    if(!started){
      request->send(200, "text/html","<h1>Attendance is not yet open</h1>");
    } else {
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
    }
  });
  // Attendance Submition
  server.on("/attendance", HTTP_POST, [](AsyncWebServerRequest *request){
    if(!started){
      request->send(200, "text/html","<h1>Attendance is not yet open</h1>");
    } else {
      IPAddress clientIP;
      // Check for ip param
      if (request->hasParam("ip", true)){
        if (!clientIP.fromString(request->getParam("ip",true)->value())){
          request->send(200, "text/html", "Unsuccessful");
        }
      }
      // Check if the clientIp has already responded
      if (!responded(clientIP)){

        string collect;

        if (request->hasParam(PARAM_FIRSTNAME, true) && request->hasParam(PARAM_LASTNAME, true)) {
            collect = request->getParam(PARAM_FIRSTNAME, true)->value().c_str();
            collect.append(" ");
            collect.append(request->getParam(PARAM_LASTNAME, true)->value().c_str());
        } else {
            collect = "Nothing sent";
            request->send(200,"text/plain");
        }
        
        responses.push_back({clientIP, collect});

        // Debuging purposes
        // request->send(200, "text/plain", ("Hello, POST: " + collect +", IP:"+ clientIP.toString().c_str()).c_str());

        request->send(200, "text/html","<hgroup><h1>"+name(clientIP)+"</h1><p>Kindly disconnect form the <strong>Wifi</strong> to give way to others.</p></hgroup>");
      } else {
        request->send(200, "text/html","<hgroup><h1>"+name(clientIP)+"</h1><p>Kindly disconnect form the <strong>Wifi</strong> to give way to others.</p></hgroup>");
      }
    }
  });

  events.onConnect([](AsyncEventSourceClient *client){
    if(client->lastId()){
      Serial.printf("Client reconnected! Last message ID that it got is: %u\n", client->lastId());
    }
    // send event with message "hello!", id current millis
    // and set reconnect delay to 1 second
    client->send("hello!", NULL, millis(), 10000);
  });
  server.addHandler(&events);

  server.onNotFound(notFound);

  server.begin();

  // Add service to MDNS-SD
  MDNS.addService("http", "tcp", 80);

}

void loop() {
  // put your main code here, to run repeatedly:


  if (started && !ended){
    digitalWrite(greenLED, HIGH);
    digitalWrite(redLED, LOW);
    digitalWrite(blueLED, LOW);
  } else if(!started && !ended){
    digitalWrite(blueLED, HIGH);
    digitalWrite(redLED, LOW);
    digitalWrite(greenLED, LOW);
  } else if(!started && ended){
    digitalWrite(redLED, HIGH);
    digitalWrite(blueLED, LOW);
    digitalWrite(greenLED, LOW);
  } else if (started && ended){
    digitalWrite(redLED, HIGH);
    digitalWrite(blueLED, LOW);
    digitalWrite(greenLED, HIGH);
  }

  delay(5);
}
