
#include "file_transfer.h"
#include "sd_library.h"
#include <WiFi.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include <SD_MMC.h>

const char* AP_SSID = "Auffline-Transfer";
const char* AP_PASS = "auffline123";

static AsyncWebServer server(80);
static DNSServer dnsServer;

void file_transfer_init() {
  
  WiFi.softAP(AP_SSID, AP_PASS);
  IPAddress IP = WiFi.softAPIP();
  Serial.print("[WiFi] AP IP address: ");
  Serial.println(IP);

  
  dnsServer.start(53, "*", IP);

  
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    String html = "<!DOCTYPE html><html><head>";
    html += "<title>Auffline - Song Library</title>";
    html += "<style>body{font-family:Arial;max-width:600px;margin:40px auto;padding:20px;}";
    html += "h1{color:#333;}li{padding:8px;border-bottom:1px solid #eee;}</style>";
    html += "</head><body>";
    html += "<h1>🎵 Auffline Song Library</h1>";
    html += "<form method='POST' action='/upload' enctype='multipart/form-data' style='margin:20px 0;'>";
    html += "<input type='file' name='file' accept='.wav' style='margin-right:10px;'>";
    html += "<input type='submit' value='Upload Song' style='background:#333;color:white;border:none;padding:8px 16px;cursor:pointer;border-radius:4px;'>";
    html += "</form>";
    html += "<p>Connected to device. Your songs:</p><ul>";

    int count = sd_library_get_count();
    for (int i = 0; i < count; i++) {
      const Track* t = sd_library_get_track(i);
      if (t) {
        html += "<li>";
        html += t->filename;
        html += "</li>";
      }
    }

    html += "</ul></body></html>";
    request->send(200, "text/html", html);
  });

  
  server.onNotFound([](AsyncWebServerRequest *request) {
    request->redirect("/");
  });

  server.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", "<h2>Upload complete!</h2><a href='/'>Go back</a>");
}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
    static File uploadFile;
    if(index == 0){
        String filename = "/music/" + request->getParam("file", true, true)->value();
        uploadFile = SD_MMC.open(filename, FILE_WRITE);
    }
    if(uploadFile){
        uploadFile.write(data, len);
    }
    if(index + len == total){
        uploadFile.close();
        sd_library_scan();
    }
});

  server.begin();
  Serial.println("[WiFi] Web server started!");
  Serial.print("[WiFi] Connect to: ");
  Serial.println(AP_SSID);
}

void file_transfer_loop() {
  dnsServer.processNextRequest();
}