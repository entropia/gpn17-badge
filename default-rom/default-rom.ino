// vim: noai:ts=2:sw=2
#include <rboot.h>

#define DEBUG

#include <GPNBadge.hpp>

#define WEB_SERVER_BUFFER_SIZE 40
#define WEB_SERVER_CLIENT_TIMEOUT 100

#ifdef DEBUG
#define BADGE_PULL_INTERVAL 60000
#else
#define BADGE_PULL_INTERVAL 5*60*1000
#endif

#include <BadgeUI.h>
#include <UIThemes.h>
#include "url-encode.h"
#include <FS.h>
#include "notification_db.h"

#include "rboot.h"
#include "rboot-api.h"

const char* ssid = "entropia"; // Put your SSID here
const char* password = "pw"; // Put your PASSWORD here

// Color definitions
#define BLACK   0x0000
#define BLUE    0x001F
#define RED     0xF800
#define GREEN   0x07E0
#define CYAN    0x07FF
#define MAGENTA 0xF81F
#define YELLOW  0xFFE0
#define WHITE   0xFFFF

Badge badge;
WindowSystem* ui = new WindowSystem(&tft);
char writeBuf[WEB_SERVER_BUFFER_SIZE];
Menu * mainMenu = new Menu();
StatusOverlay * status = new StatusOverlay(BAT_CRITICAL, BAT_FULL);
bool initialSync = false;

void setup() {
  badge.init();

  if (!SPIFFS.begin()) { //initialize the spiffs
    // TODO
    Serial.print("Error init SPIFFS!");
  }

  badge.setBacklight(true);
  ui->root->setBmp("/system/load.bmp", 40, 12);
  ui->root->setSub("");
  ui->setOverlay(status);
  ui->draw();

  Serial.print("Battery Voltage:  ");
  Serial.println(badge.getBatVoltage());
  Serial.print("Light LDR Level:  ");
  Serial.println(badge.getLDRLvl());

  pixels.setPixelColor(0, pixels.Color(20, 0, 0));
  pixels.setPixelColor(1, pixels.Color(20, 0, 0));
  pixels.setPixelColor(2, pixels.Color(20, 0, 0));
  pixels.setPixelColor(3, pixels.Color(20, 0, 0));
  pixels.show();


  //setGPIO(VIBRATOR, HIGH);
  //delay(500);
  //setGPIO(VIBRATOR, LOW);

  badge.setGPIO(IR_EN, LOW);

  badge.setAnalogMUX(MUX_JOY);

  deleteTimestampFiles();

  if (SPIFFS.exists("/wifi.conf")) {
    Serial.print("Found wifi config");
    if(connectBadge()) {
      pullNotifications();
    }
    mainMenu->addMenuItem(new MenuItem("Badge", []() {}));
    mainMenu->addMenuItem(new MenuItem("Notifications", []() {
      Menu * notificationMenu = new Menu();
      notificationMenu->addMenuItem(new MenuItem("Back", []() {
        ui->closeCurrent();
      }));
      NotificationIterator notit(NotificationFilter::ACTIVE);
      while (notit.next()) {
        Notification noti = notit.getNotification();
        NotificationHandle notiHandl = notit.getHandle();
        notificationMenu->addMenuItem(new MenuItem(noti.summary, [notiHandl]() {
          Notification handNot;
          if(getNotificationByHandle(notiHandl, &handNot)) {
            ui->open(new NotificationScreen(handNot.summary, handNot.location, handNot.description));
          } else {
            ui->open(new NotificationScreen("Error", "", "Notification does not exist anymore."));
          }
        }));
      }
      ui->open(notificationMenu);
    }));
    mainMenu->addMenuItem(new MenuItem("Configuration", []() {
      initialConfig();
    }));
    mainMenu->addMenuItem(new MenuItem("Info", []() {
      Menu * infoMenu = new Menu();
      infoMenu->addMenuItem(new MenuItem("Back", []() {
        ui->closeCurrent();
      }));

      infoMenu->addMenuItem(new MenuItem("SSID: " + WiFi.SSID(), []() {})); 
      infoMenu->addMenuItem(new MenuItem("RSSI: " + String(WiFi.RSSI())+"dBm", []() {})); 
      infoMenu->addMenuItem(new MenuItem("IP: " + WiFi.localIP().toString(), []() {})); 
      infoMenu->addMenuItem(new MenuItem("Bat.:" + String(badge.getBatVoltage())+"mV", []() {})); 
      infoMenu->addMenuItem(new MenuItem("About", []() {
        NotificationScreen * about = new NotificationScreen("GPN17 Badge\nDefault ROM", "", "Developed by\nJanis (@dehexadop)\n&\nAnton");
        ui->open(about);
      })); 
      ui->open(infoMenu); 
    }));
    mainMenu->addMenuItem(new MenuItem("Factory reset", []() {}));
    ui->open(mainMenu);
    status->updateBat(badge.getBatVoltage());
    int wStat = WiFi.status();
    if(wStat == WL_CONNECTED) {
      status->updateWiFiState(WiFi.SSID());
    } else {
      status->updateWiFiState("No WiFi");
    }
  } else {
    initialConfig();
  }

}

unsigned long lastOneSecoundTask = 0;
bool isDark = false;
uint16_t lightAvg = 0;
int16_t batAvg = -1;
uint16_t pollDelay = 0;

void loop() {
  lightAvg = .99f*lightAvg + .01f*badge.getLDRLvl(); 
  if(lightAvg > 700) {
    if(!isDark) {
      Serial.println("Switching to dark theme");
      ui->setTheme(new ThemeDark());
      isDark = true;
    } 
  } else {
    if(isDark) {
      Serial.println("Switching to light theme");
      isDark = false;
      ui->setTheme(new ThemeLight());
    }
  }
  ui->dispatchInput(badge.getJoystickState());
  ui->draw();
  if (millis() - lastNotificationPull > BADGE_PULL_INTERVAL + pollDelay) {
    if(connectBadge()) {
      pollDelay = 0;
      status->updateWiFiState("Polling...");
      ui->draw();
      pullNotifications();
      Serial.println("Iterate notifications: ");
    } else {
      if(pollDelay < 5*60*1000) {
        Serial.println("Delaying poll by one minute...");
        pollDelay+=60*1000;
      }
    }
    lastNotificationPull = millis();
  }
  if (millis() - lastOneSecoundTask > 1000) {
    if(initialSync) {
      recalculateStates();
    }
    NotificationIterator notit(NotificationFilter::NOT_NOTIFIED);
    if (notit.next()) {
      Notification noti = notit.getNotification();
      notit.setCurrentNotificationState(NotificationState::NOTIFIED);
      NotificationScreen * notification = new NotificationScreen(String(noti.summary), String(noti.location), String(noti.description));
      ui->open(notification);
      badge.setVibrator(true);
      delay(200);
      badge.setVibrator(false);
      delay(300);
      badge.setVibrator(true);
      delay(400);
      badge.setVibrator(false);
    }
    Serial.printf("Free heap: %d\n", ESP.getFreeHeap());
    if(batAvg == -1) {
      batAvg = badge.getBatVoltage();
    } else {
      batAvg = .9f*batAvg + .1f*badge.getBatVoltage();
    }
    status->updateBat(batAvg);
    int wStat = WiFi.status();
    if(wStat == WL_CONNECTED) {
      status->updateWiFiState(WiFi.SSID());
    } else {
      status->updateWiFiState("No WiFi");
    }
    lastOneSecoundTask = millis();
  }
}

void setNick(const char* nick) {
  File nickStore = SPIFFS.open("nick.conf", "w");
  urlEncodeWriteKeyValue("nick", nick, nickStore);
  nickStore.close();
}

bool connectBadge() {
  bool ret = true;
  if(WiFi.status() == WL_CONNECTED) {
    return true;
  }
  status->updateWiFiState("Connecting...");
  ui->root->setSub("Loading...");
  ui->draw();
  int ledVal = 0;
  bool up = true;
  WiFi.mode(WIFI_STA);
  Serial.println("Loading config.");
  File wifiConf = SPIFFS.open("/wifi.conf", "r");
  String configString;
  while (wifiConf.available()) {
    configString += char(wifiConf.read());
  }
  wifiConf.close();
  UrlDecode confParse(configString.c_str());
  Serial.println(configString);
  configString = String();
  char* ssid = confParse.getKey("ssid");
  char* pw = confParse.getKey("pw");
  Serial.printf("Connecting ti wifi '%s' with password '%s'...\n", ssid, pw);
  unsigned long startTime = millis();
  WiFi.begin(ssid, pw);
  delete[] pw;
  while (WiFi.status() != WL_CONNECTED) {
    pixels.setPixelColor(1, pixels.Color(0, 0, ledVal));
    pixels.setPixelColor(2, pixels.Color(0, 0, ledVal));
    pixels.setPixelColor(3, pixels.Color(0, 0, ledVal));
    pixels.setPixelColor(0, pixels.Color(0, 0, ledVal));
    pixels.show();
    if (ledVal == 100) {
      up = false;
    }
    if (ledVal == 0) {
      up = true;
    }
    if (up) {
      ui->root->setSub("Connecting...");
      ui->draw();
      ledVal++;
    } else {
      ui->root->setSub(ssid);
      ui->draw();
      ledVal--;
    }
    if (millis() - startTime > 30 * 1000) {
      ui->root->setSub("Error ;(");
      pixels.setPixelColor(1, pixels.Color(50, 0, 0));
      pixels.setPixelColor(2, pixels.Color(50, 0, 0));
      pixels.setPixelColor(3, pixels.Color(50, 0, 0));
      pixels.setPixelColor(0, pixels.Color(50, 0, 0));
      pixels.show();
      ui->draw();
      ret = false;
      break;
    }
    delay(10);
  }
  delete[] ssid;
  ui->draw();
  delay(10);
  pixels.setPixelColor(1, pixels.Color(0, 0, 0));
  pixels.setPixelColor(2, pixels.Color(0, 0, 0));
  pixels.setPixelColor(3, pixels.Color(0, 0, 0));
  pixels.setPixelColor(0, pixels.Color(0, 0, 0));
  pixels.show();
  if(ret) {
    initialSync = true;
  }
  return ret;
}

void initialConfig() {
  Serial.printf("Free heap at init: %d\n", ESP.getFreeHeap());
  WiFi.disconnect();
  delay(100);
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAPConfig(IPAddress (10, 0, 0, 1), IPAddress(10, 0, 0, 1), IPAddress(255, 255, 255, 0));
  char pw[20];
  char ssid[20];
  sprintf(pw, "%d", random(10000000, 100000000)); // TODO richtig machen
  sprintf(ssid, "ESP%d", ESP.getChipId());
#ifdef DEBUG
  sprintf(pw, "%s", "hallohallo");
#endif
  Serial.println(WiFi.softAP(ssid, pw));
  FullScreenBMPStatus* webStatus = new FullScreenBMPStatus();
  connectWizard(ssid, pw, webStatus);
  WiFiServer server(80);
  server.begin();
  // Own Webserver. _slow_ but memory efficient
  String headBuf;
  String bodyBuf;
  String getValue;
  unsigned long lastByteReceived = 0;
  bool requestFinished = false;
  bool isPost = false;
  Serial.printf("Free before loop: %d\n", ESP.getFreeHeap());
  while (true) {
    if (WiFi.softAPgetStationNum() == 0) {
      connectWizard(ssid, pw, webStatus);
    }
    WiFiClient currentClient = server.available();
    if (!currentClient || !currentClient.connected())  {
      continue;
    }
    while ((lastByteReceived == 0 || millis() - lastByteReceived < WEB_SERVER_CLIENT_TIMEOUT) && !requestFinished) {
      while (currentClient.available()) {
        char r = char(currentClient.read());
        if (r == '\n') {
          isPost = headBuf.startsWith("POST");
          if (headBuf.startsWith("GET") || isPost) {
            getValue = headBuf.substring(4, headBuf.length() - 10);
            getValue.trim();
            Serial.print("GET/POST::");
            Serial.println(getValue);
            Serial.flush();
            requestFinished = true;
            break;
          }
        } else {
          headBuf += r;
        }
        lastByteReceived = millis();
      }
    }
    if (!requestFinished) {
      currentClient.stop();
      lastByteReceived = 0;
      Serial.println("Request timeout");
      continue;
    }
    headBuf = String();
    if (isPost) {
      requestFinished = false;
      bool wasNl = false;
      bool headerFinished = false;
      lastByteReceived = millis();
      while ((lastByteReceived == 0 || millis() - lastByteReceived < WEB_SERVER_CLIENT_TIMEOUT) && !requestFinished) {
        // Read until request body starts
        while (currentClient.available() && !headerFinished) {
          bodyBuf += char(currentClient.read());
          if (bodyBuf.endsWith("\r\n")) {
            bodyBuf = String();
            if (wasNl) {
              Serial.println("header finished");
              headerFinished = true;
              bodyBuf = String();
              break;
            }
            wasNl = true;
          } else {
            wasNl = false;
          }
        }
        while (currentClient.available() && headerFinished) {
          bodyBuf += currentClient.read();
        }
      }
      Serial.print("body: ");
      Serial.println(bodyBuf);
      if (getValue == "/api/conf/wifi") {
        UrlDecode dec(bodyBuf.c_str());
        Serial.println("decoder");
        char* confNet = dec.getKey("net");
        char* confPw = dec.getKey("pw");
        Serial.println("Open config file");
        File wifiStore = SPIFFS.open("/wifi.conf", "w");
        Serial.println("Write config file");
        urlEncodeWriteKeyValue("pw", confPw, wifiStore);
        urlEncodeWriteKeyValue("ssid", confNet, wifiStore);
        wifiStore.close();
        Serial.println("Done writing config");
        WiFi.begin(confNet, confPw); // quick fix.. chnage in js
        Serial.printf("Trying connect to %s...\n", confNet);
        int wStat = WiFi.status();
        int ledVal = 0;
        bool up = true;
        while (wStat != WL_CONNECTED) {
          if (wStat == WL_CONNECT_FAILED) {
            break;
          }
          pixels.setPixelColor(1, pixels.Color(0, 0, ledVal));
          pixels.setPixelColor(2, pixels.Color(0, 0, ledVal));
          pixels.setPixelColor(3, pixels.Color(0, 0, ledVal));
          pixels.setPixelColor(0, pixels.Color(0, 0, ledVal));
          pixels.show();
          if (ledVal == 100) {
            up = false;
          }
          if (ledVal == 0) {
            up = true;
          }
          if (up) {
            ledVal++;
          } else {
            ledVal--;
          }
          delay(10);
          wStat = WiFi.status();
          Serial.printf("Wstat: %d\n", wStat);
        }
        pixels.setPixelColor(1, pixels.Color(0, 0, 0));
        pixels.setPixelColor(2, pixels.Color(0, 0, 0));
        pixels.setPixelColor(3, pixels.Color(0, 0, 0));
        pixels.setPixelColor(0, pixels.Color(0, 0, 0));
        pixels.show();
        currentClient.write("HTTP/1.1 200\r\n");
        currentClient.write("Cache-Control: no-cache\r\n");
        if (wStat == WL_CONNECTED) {
          currentClient.write("true");
        } else {
          currentClient.write("false");
        }
        delete[] confNet;
        delete[] confPw;
      } else if (getValue == "/api/conf/nick") { // Nickname configuration
        UrlDecode nickUrlDecode(bodyBuf.c_str());
        char* nick = nickUrlDecode.getKey("nick");
        setNick(nick);
        delete[] nick;
        currentClient.write("HTTP/1.1 200\r\n");
        currentClient.write("Cache-Control: no-cache\r\n\r\ntrue");
      } else if (getValue == "/api/channels/add") { // Add channel
        UrlDecode channelAddDecode(bodyBuf.c_str());
        char* host = channelAddDecode.getKey("host");
        char* url = channelAddDecode.getKey("url");
        char* fingerprint = channelAddDecode.getKey("fingerprint");
        addChannel(host, url, fingerprint);
        delete[] host;
        delete[] url;
        delete[] fingerprint;
        currentClient.write("HTTP/1.1 200\r\n");
        currentClient.write("Cache-Control: no-cache\r\n\r\ntrue");
      } else if (getValue == "/api/channels/delete") { // Delete channel
        UrlDecode channelAddDecode(bodyBuf.c_str());
        char* num = channelAddDecode.getKey("num");
        int id = atoi(num);
        bool success = deleteChannel(id);
        delete[] num;
        currentClient.write("HTTP/1.1 200\r\n");
        currentClient.write("Cache-Control: no-cache\r\n\r\n");
        currentClient.print(success);
      } else {
        currentClient.write("HTTP/1.1 404");
        currentClient.write("\r\n\r\n");
        currentClient.write(getValue.c_str());
        currentClient.write(" Not Found!");
      }
    } else {
      if (getValue.startsWith("/api/")) {
        currentClient.write("HTTP/1.1 200\r\n");
        currentClient.write("Cache-Control: no-cache\r\n");
      }
      if (getValue == "/api/wifi/scan") {
        pixels.setPixelColor(0, pixels.Color(0, 0, 20));
        pixels.setPixelColor(1, pixels.Color(0, 0, 20));
        pixels.setPixelColor(2, pixels.Color(0, 0, 20));
        pixels.setPixelColor(3, pixels.Color(0, 0, 20));
        pixels.show();
        int n = WiFi.scanNetworks();
        delay(2000);
        pixels.setPixelColor(0, pixels.Color(0, 0, 0));
        pixels.setPixelColor(1, pixels.Color(0, 0, 0));
        pixels.setPixelColor(2, pixels.Color(0, 0, 0));
        pixels.setPixelColor(3, pixels.Color(0, 0, 0));
        pixels.show();
        currentClient.write("Content-Type: application/json");
        currentClient.write("\r\n\r\n");
        currentClient.write("[");
        for (int i = 0; i < n; ++i)
        {
          currentClient.printf("{\"id\": %d", i);
          currentClient.printf(",\"ssid\":\"%s\"", WiFi.SSID(i).c_str());
          currentClient.printf(",\"rssi\": %d", WiFi.RSSI(i));
          currentClient.printf(",\"encType\": %d", WiFi.encryptionType(i));
          Serial.printf("Check >0: %d\n", currentClient.write("}"));
          if (i + 1 < n) {
            currentClient.write(",");
          }
          Serial.printf("Free heap in json write: %d\n", ESP.getFreeHeap());
          currentClient.flush();
        }
        currentClient.write("]");
      } else if (getValue == "/api/wifi/status") {
        int stat = WiFi.status();
        currentClient.write("\r\n\r\n");
        if (stat == WL_DISCONNECTED) {
          currentClient.write("Disconnected");
        } else if (stat == WL_CONNECTION_LOST) {
          currentClient.write("Connection lost");
        } else if (stat == WL_CONNECTED) {
          currentClient.write("Connected to '");
          currentClient.write(WiFi.SSID().c_str());
          currentClient.write("'");
        } else if (stat == WL_IDLE_STATUS) {
          currentClient.write("Idle");
        } else if (stat == WL_CONNECT_FAILED) {
          currentClient.write("Connection failed");
        } else {
          currentClient.printf("Unknown (%d)", stat);
        }
      } else if (getValue == "/api/channels") {
        currentClient.write("\r\n\r\n");
        ChannelIterator channels;
        currentClient.write("[");
        bool first = true;
        while (channels.next()) {
          if (first) {
            first = false;
          } else {
            currentClient.write(",");
          }

          currentClient.write("{\"num\":");
          currentClient.print(channels.channelNum());
          currentClient.write(", \"host\":\"");
          currentClient.print(channels.host());
          currentClient.write("\", \"url\":\"");
          currentClient.print(channels.url());
          currentClient.write("\", \"fingerprint\":\"");
          currentClient.print(channels.fingerprint());
          currentClient.write("\"}");
        }
        currentClient.write("]");
      } else {
        if (getValue == "/") {
          getValue = "/index.html";
        }
        String path = "/system/web" + getValue;
        if (SPIFFS.exists(path) || SPIFFS.exists(path + ".gz")) {
          currentClient.write("HTTP/1.1 200\r\n");
          currentClient.write("Content-Type: ");
          currentClient.write(getContentType(path).c_str());
          currentClient.write("\r\nCache-Control: max-age=1800");
          if (SPIFFS.exists(path + ".gz")) {
            path += ".gz";
            currentClient.write("\r\nContent-Encoding: gzip");
          }
          currentClient.write("\r\n\r\n");
          File file = SPIFFS.open(path, "r");
          path = String();
          int pos = 0;
          while (file.available()) {
            writeBuf[pos++] = char(file.read());
            if (pos == WEB_SERVER_BUFFER_SIZE) {
              currentClient.write(&writeBuf[0], size_t(WEB_SERVER_BUFFER_SIZE));
              pos = 0;
            }
          }
          // Flush the remaining bytes
          currentClient.write(&writeBuf[0], size_t(pos));
          file.close();
        } else {
          currentClient.write("HTTP/1.1 404");
          currentClient.write("\r\n\r\n");
          currentClient.write(getValue.c_str());
          currentClient.write(" Not Found!");
        }
      }
    }
    getValue = String();
    currentClient.flush();
    currentClient.stop();
    requestFinished = false;
    lastByteReceived = 0;
    bodyBuf = String();
    Serial.println("Finished client.");
    Serial.printf("Free heap: %d\n", ESP.getFreeHeap());
  }
  server.close();
  ui->closeCurrent();
}

void connectWizard(char* ssid, char* pw, FullScreenBMPStatus* webStatus) {
  bool dispPw = false;
  webStatus->setBmp("/system/wifi.bmp", 13, 6);
  webStatus->setSub(ssid);
  ui->open(webStatus);
  ui->draw();
  int last_b = millis();
  JoystickState last_state = JoystickState::BTN_NOTHING;
  bool b_state = false;
  while (true) {
    if (millis() - last_b > 1000) {
      b_state = !b_state;
      pixels.setPixelColor(0, pixels.Color(0, b_state ? 0 : 20, 0));
      pixels.setPixelColor(1, pixels.Color(0, b_state ? 0 : 20, 0));
      pixels.setPixelColor(2, pixels.Color(0, b_state ? 0 : 20, 0));
      pixels.setPixelColor(3, pixels.Color(0, b_state ? 0 : 20, 0));
      pixels.show();
      last_b = millis();
    }
    JoystickState state = badge.getJoystickState();
    if (state != JoystickState::BTN_NOTHING && state != last_state) {
      dispPw = !dispPw;
      if (dispPw) {
        webStatus->setBmp("/system/lock.bmp", 35, 6);
        webStatus->setSub(pw);
      } else {
        webStatus->setBmp("/system/wifi.bmp", 13, 6);
        webStatus->setSub(ssid);
      }
      ui->draw();
      delay(50);
    }
    last_state = state;
    if (WiFi.softAPgetStationNum() > 0) {
      webStatus->setBmp("/system/pc.bmp", 10, 9);
      webStatus->setSub("10.0.0.1", 30, 52);
      ui->draw();
      break;
    }
    delay(20);
  }
  pixels.setPixelColor(0, pixels.Color(0, b_state ? 0 : 30, 0));
  pixels.setPixelColor(1, pixels.Color(0, b_state ? 0 : 30, 0));
  pixels.setPixelColor(2, pixels.Color(0, b_state ? 0 : 30, 0));
  pixels.setPixelColor(3, pixels.Color(0, b_state ? 0 : 30, 0));
  pixels.show();
}

String getContentType(String filename) {
  if (filename.endsWith(".htm")) return "text/html";
  else if (filename.endsWith(".html")) return "text/html";
  else if (filename.endsWith(".css")) return "text/css";
  else if (filename.endsWith(".js")) return "application/javascript";
  else if (filename.endsWith(".png")) return "image/png";
  else if (filename.endsWith(".gif")) return "image/gif";
  else if (filename.endsWith(".jpg")) return "image/jpeg";
  else if (filename.endsWith(".ico")) return "image/x-icon";
  else if (filename.endsWith(".xml")) return "text/xml";
  else if (filename.endsWith(".pdf")) return "application/x-pdf";
  else if (filename.endsWith(".zip")) return "application/x-zip";
  else if (filename.endsWith(".gz")) return "application/x-gzip";
  else if (filename.endsWith(".json")) return "application/json";
  return "text/plain";
}
