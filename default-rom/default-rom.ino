// vim: noai:ts=2:sw=2

//#define DEBUG

#include <GPNBadge.hpp>

#define WEB_SERVER_BUFFER_SIZE 40
#define WEB_SERVER_CLIENT_TIMEOUT 100

#ifdef DEBUG
#define BADGE_PULL_INTERVAL 60000
#else
#define BADGE_PULL_INTERVAL 5*60*1000
#endif

#define TIMEZONE_OFFSET 2 //hours
#include <time.h>

#include <BadgeUI.h>
#include <UIThemes.h>
#include "url-encode.h"
#include <FS.h>
#include "notification_db.h"
#include "WebServer.h"
#include <rboot-api.h>

#include "rboot.h"
#include "rboot-api.h"

// Color definitions
#define BLACK   0x0000
#define BLUE    0x001F
#define RED     0xF800
#define GREEN   0x07E0
#define CYAN    0x07FF
#define MAGENTA 0xF81F
#define YELLOW  0xFFE0
#define WHITE   0xFFFF

#define DEFAULT_THEME "Light"
#define CONFIG_PREFIX "/deflt/conf/"

Badge badge;
WindowSystem* ui = new WindowSystem(&tft);
char writeBuf[WEB_SERVER_BUFFER_SIZE];
Menu * mainMenu = new Menu();
StatusOverlay * status = new StatusOverlay(BAT_CRITICAL, BAT_FULL);
bool autoTheme = false;
bool isDark = false;
unsigned char wifiLight = 255;
int wifi_hue = 0;
int wifi_hue_target = 0;

//Sharing
String shareString;
bool receiveShare = false;

bool ennotif = true;

String getConfig(String key, String def) {
  if(!SPIFFS.exists(CONFIG_PREFIX+key)){
    setConfig(key, def);
    return def;
  }
  File f = SPIFFS.open(CONFIG_PREFIX+key, "r");
  String ret = "";
  while(f.available()) {
    ret += char(f.read());
  }
  f.close();
  if(ret == "" && ret != def) {
    setConfig(key, def);
    return def;
  }
  return ret;
}

void setConfig(String key, String value) {
  File f = SPIFFS.open(CONFIG_PREFIX+key, "w");
  Serial.printf("Conf save: %s value=%s\n", key.c_str(), value.c_str());
  f.print(value);
  f.flush();
  f.close();
} 

void setup() {
  badge.init();

  if (!SPIFFS.begin()) { //initialize the spiffs
    // TODO
    Serial.print("Error init SPIFFS!");
  }

  badge.setBacklight(true);
  ui->root->setBmp("/deflt/load.bmp", 40, 12);
  ui->root->setSub("");
  ui->setOverlay(status);
  ui->draw();

  Serial.print("Battery Voltage:  ");
  Serial.println(badge.getBatVoltage());
  Serial.print("Light LDR Level:  ");
  Serial.println(badge.getLDRLvl());



  //setGPIO(VIBRATOR, HIGH);
  //delay(500);
  //setGPIO(VIBRATOR, LOW);

  badge.setGPIO(IR_EN, LOW);

  badge.setAnalogMUX(MUX_JOY);

  deleteTimestampFiles();

  rboot_config rboot_config = rboot_get_config();
  Serial.print("I am in Slot ");
  Serial.println(rboot_config.current_rom);

  SPIFFS.begin();
  File f = SPIFFS.open("/rom" + String(rboot_config.current_rom), "w");
  f.println("Default ROM\n");

  if (SPIFFS.exists("/wifi.conf")) {
    Serial.print("Found wifi config");
    if(connectBadge()) {
      pullNotifications();
    }
    mainMenu->addMenuItem(new MenuItem("Badge", []() {
      FullScreenBMPDisplay* badge_screen = new FullScreenBMPDisplay();
      badge_screen->setBmp("/badge.bmp");
      ui->open(badge_screen);
    }));
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
      Menu * configMenu = new Menu();
      configMenu->addMenuItem(new MenuItem("Back", []() {
            ui->closeCurrent();
      }));
      MenuItem * themeItem = new MenuItem("Theme: "+getConfig("theme", DEFAULT_THEME), []() {});
      themeItem->setTrigger([themeItem]() {
       String current = getConfig("theme", DEFAULT_THEME); 
       autoTheme = false;
       if(current == "Light") {
        setConfig("theme", "Dark");
        ui->setTheme(new ThemeDark());
        isDark = true;
       } else if(current == "Dark") {
        setConfig("theme", "Auto");
        autoTheme = true;
       } else {
        isDark = false;
        ui->setTheme(new ThemeLight());
        setConfig("theme", "Light");
       }
       themeItem->setText("Theme: "+getConfig("theme", DEFAULT_THEME));
       });
      configMenu->addMenuItem(themeItem);

      MenuItem * wifiLightItem = new MenuItem("WifiLight: "+getConfig("wifilight", "255"), []() {});
      wifiLightItem->setTrigger([wifiLightItem]() {
       String current = getConfig("wifilight", "255"); 
       if (current == "0") {
        setConfig("wifilight", "255");
       } else if(current == "255") {
        setConfig("wifilight", "100");
       } else if(current == "100") {
        setConfig("wifilight", "50");
       } else if(current == "50") {
        setConfig("wifilight", "10");
       } else {
        setConfig("wifilight", "0");
       }
       wifiLightItem->setText("WifiLight: "+getConfig("wifilight", "255"));
       wifiLight = atoi(getConfig("wifilight", "255").c_str());
       });
      configMenu->addMenuItem(wifiLightItem);

      MenuItem * ennotifItem= new MenuItem("Notifications: "+getConfig("ennotif", "on"), []() {});
      ennotifItem->setTrigger([ennotifItem]() {
       String current = getConfig("ennotif", "on"); 
       if (current == "on") {
        setConfig("ennotif", "off");
       }  else {
        setConfig("ennotif", "on");
       }
       ennotifItem->setText("Notifications: "+getConfig("ennotif", "on"));
       ennotif = getConfig("ennotif", "on") == "on";
       });
      configMenu->addMenuItem(ennotifItem);

      configMenu->addMenuItem(new MenuItem("WiFi Config", []() {
          initialConfig();
      }));
      MenuItem * shareItem = new MenuItem("Share Channel", []() {
        Menu * shareMenu = new Menu();
        shareMenu->addMenuItem(new MenuItem("Back", []() {
          ui->closeCurrent();
        }));
        ChannelIterator channels;
        while(channels.next()) {
          String fingerprint = channels.fingerprint();
          String specificShareString = channels.host() + "\r" + channels.url() + "\r" + fingerprint + "\n";
          String channelName = channels.name();
          if (channelName == "") {
            channelName = "unnamed";
          }
          Serial.print("Add share menu item: \"");
          Serial.print(channelName);
          Serial.println("\"");
          shareMenu->addMenuItem(new MenuItem(channelName, [fingerprint, specificShareString]() {
            Serial.println("Share:");
            Serial.println(specificShareString);
            ClosableTextDisplay * shareScreen = new ClosableTextDisplay();
            shareScreen->setText(fingerprint);
            shareString = specificShareString;
            shareScreen->setOnClose([]() {
              ui->closeCurrent();
              shareString = String(); 
            });
            ui->open(shareScreen);
          }));
        }
        ui->open(shareMenu);
      });
      configMenu->addMenuItem(shareItem);

      MenuItem * receiveItem = new MenuItem("Receive Channel", []() {
          receiveShare = true;
          ClosableTextDisplay * receiveScreen = new ClosableTextDisplay();
          receiveScreen->setText("Receiving...");
          receiveScreen->setOnClose([]() {
            receiveShare = false;
            ui->closeCurrent();
          });
          ui->open(receiveScreen);
      });
      configMenu->addMenuItem(receiveItem);

      ui->open(configMenu);
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
    String themeConf = getConfig("theme", DEFAULT_THEME);
    if(themeConf == "Auto") {
      autoTheme = true;
    } else if(themeConf == "Light"){
      ui->setTheme(new ThemeLight());
    } else {
      ui->setTheme(new ThemeDark());
      isDark = true;
    }
    String wifiLightConf = getConfig("wifilight", "255");
    wifiLight = atoi(wifiLightConf.c_str());
    ennotif = getConfig("ennotif", "on") == "on";
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
uint16_t lightAvg = 0;
int16_t batAvg = -1;

void loop() {
  lightAvg = .99f*lightAvg + .01f*badge.getLDRLvl(); 
  if(autoTheme) {
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
  }
  ui->dispatchInput(badge.getJoystickState());
  ui->draw();
  if (millis() - lastNotificationPull > BADGE_PULL_INTERVAL) {
    if(connectBadge()) {
      status->updateWiFiState("Polling...");
      ui->draw();
      pullNotifications();
      Serial.println("Iterate notifications: ");
    }
    lastNotificationPull = millis();
  }
  if (millis() - lastOneSecoundTask > 1000) {
    recalculateStates([](){
      ui->dispatchInput(badge.getJoystickState());
      ui->draw();
    });

    // calculate current time
    if (serverTimestamp > 0) {
      time_t currentts = (time_t) serverTimestamp + TIMEZONE_OFFSET*3600;
      struct tm *datetime = localtime(&currentts);
      status->updateClock(datetime->tm_hour, datetime->tm_min);
      
    } else {
      status->updateClock(99,99);
    }
    if (ennotif) {
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
        delay(300);
        badge.setVibrator(true);
        delay(1000);
        badge.setVibrator(false);
      }
    } else {
      Serial.println("Noitif disabled");
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
      if (wifiLight) {
        Serial.println("MAC Address:");
        uint8_t hash = 0;
        uint8_t * bssid = WiFi.BSSID();  
        for (int i = 0; i < 6; i++) {
          hash += bssid[i];
          Serial.print(bssid[i], HEX);
          Serial.print(":");
        }
        Serial.println("");

        wifi_hue_target = hash * 10;
      }

    } else {
      status->updateWiFiState("No WiFi");
    }

    lastOneSecoundTask = millis();
    pixels.show();
  }

  if (wifiLight) {
    if (wifi_hue < wifi_hue_target) {
      wifi_hue++;
    } else if (wifi_hue > wifi_hue_target) {
      wifi_hue--;
    }
    int colors[3];
    HSVtoRGB(wifi_hue/10, 255, wifiLight, colors);

    pixels.setPixelColor(1, pixels.Color(colors[0], colors[1], colors[2]));
    pixels.setPixelColor(2, pixels.Color(colors[0], colors[1], colors[2]));
    pixels.setPixelColor(3, pixels.Color(colors[0], colors[1], colors[2]));
    pixels.setPixelColor(0, pixels.Color(colors[0], colors[1], colors[2]));
  } else {
    pixels.setPixelColor(1, pixels.Color(0, 0, 0));
    pixels.setPixelColor(2, pixels.Color(0, 0, 0));
    pixels.setPixelColor(3, pixels.Color(0, 0, 0));
    pixels.setPixelColor(0, pixels.Color(0, 0, 0));
  }
  pixels.show();


  //IR Sharing
  if (shareString.length() != 0) {
    badge.setGPIO(IR_EN, HIGH);
    Serial.println("NEC");
    uint32_t code = 0;
    uint8_t checksum = 0;
    for (int i = 0; i < shareString.length(); i++){
      ui->dispatchInput(badge.getJoystickState()); //Allow canceling
      checksum += shareString.charAt(i);
      code = code | shareString.charAt(i) << (i % 4)*8;
      if (i % 4 == 3) {
        irsend.sendNEC(code, 32);
        Serial.println(code, HEX);
        code = 0;
      }
    }
    if (code != 0) {
      irsend.sendNEC(code, 32);
      Serial.println(code, HEX);
    }
    Serial.print("Checksum: ");
    Serial.println(checksum); //224
    code = 0;
    code = checksum << 8 | 222;
    irsend.sendNEC(code, 32);
    badge.setGPIO(IR_EN, LOW);
  }


  //IR Receiving
  if (receiveShare) {
    badge.setGPIO(IR_EN, HIGH);
    Serial.println("Entering receive mode");
    String received = "";
    bool stringCompleted = false;
    bool dataCompleted = false;
    bool dataVerified = false;
    uint8_t checksumRec = 0;

    while (!dataCompleted && receiveShare) {
      int on_time = 0;
      decode_results  results;        // Somewhere to store the results
      if (irrecv.decode(&results)) {
        if (results.overflow) {
          Serial.println("IR code too long. Edit IRremoteInt.h and increase RAWBUF");
          return;
        }
        
        char * buf = reinterpret_cast<char*>(&results.value);

        if (millis() - on_time > 500) {
          pixels.setPixelColor(1, pixels.Color(0, 0, 0));
          pixels.show();
        }

        if (stringCompleted) {
          uint8_t checksum = buf[1];
          /*Serial.print("Checksum: ");
          Serial.println(checksum);


          Serial.print("Received Checksum: ");
          Serial.println(checksumRec);*/
          dataCompleted = true;

          if(checksum == checksumRec) {
            dataVerified = true;
            pixels.setPixelColor(1, pixels.Color(0, 100, 0));
          } else {
            pixels.setPixelColor(1, pixels.Color(100, 0, 0));
          }
          pixels.show();
          on_time = millis();
        }
        else {
          Serial.println(buf);
          received += String(buf[0]) + buf[1] + buf[2] + buf[3];
          checksumRec += buf[0] + buf[1] + buf[2] + buf[3];
          if (buf[0] == '\n' || buf[1] == '\n' || buf[2] == '\n' || buf[3] == '\n') {
            stringCompleted = true;
          }
        }
        irrecv.resume();              // Prepare for the next value
      }
      delay(0);
      ui->dispatchInput(badge.getJoystickState()); //Allow canceling
    }

    if (dataCompleted == true){
      if ( dataVerified == true) {
        Serial.print(received);
        receiveShare = false;
        ui->closeCurrent();

        size_t firstCR = received.indexOf("\r");
        size_t secondCR = received.indexOf("\r", firstCR + 1);

        String host = received.substring(0, firstCR);
        String url = received.substring(firstCR + 1, secondCR);
        String fingerprint = received.substring(secondCR + 1, received.length() - 1);

        Serial.println(host);
        Serial.println(url);
        Serial.println(fingerprint);

        ClosableTextDisplay * fingerprintScreen = new ClosableTextDisplay();
        fingerprintScreen->setText(fingerprint);
        fingerprintScreen->setOnClose([host, url, fingerprint]() {

          Menu * acceptMenu = new Menu();
          acceptMenu->addMenuItem(new MenuItem("Accept", [host, url, fingerprint]() {
            addChannel(host.c_str(), url.c_str(), fingerprint.c_str());
            ui->closeCurrent();
          }));
          acceptMenu->addMenuItem(new MenuItem("Decline", []() {
            ui->closeCurrent();
          }));
          ui->open(acceptMenu);
        });
        ui->open(fingerprintScreen);
      }
    }
    badge.setGPIO(IR_EN, LOW);
    Serial.println("Exiting receive mode");
  }
}



void HSVtoRGB(int hue, int sat, int val, int colors[3]) {
  // hue: 0-359, sat: 0-255, val (lightness): 0-255
  int r, g, b, base;
  if (sat == 0)
  {                     // Achromatic color (gray).
    colors[0] = val;
    colors[1] = val;
    colors[2] = val;
  }
  else 
  {
    base = ((255 - sat) * val) >> 8;
    switch(hue / 60)
    {
      case 0:
        r = val;
        g = (((val - base) * hue) / 60) + base;
        b = base;
        break;
      case 1:
        r = (((val - base) * (60 - (hue % 60))) / 60) + base;
        g = val;
        b = base;
        break;
      case 2:
        r = base;
        g = val;
        b = (((val - base) * (hue % 60)) / 60) + base;
        break;
      case 3:
        r = base;
        g = (((val - base) * (60 - (hue % 60))) / 60) + base;
        b = val;
        break;
      case 4:
        r = (((val - base) * (hue % 60)) / 60) + base;
        g = base;
        b = val;
        break;
      case 5:
        r = val;
        g = base;
        b = (((val - base) * (60 - (hue % 60))) / 60) + base;
        break;
    }
    colors[0] = r;
    colors[1] = g;
    colors[2] = b;
  }
}

void setNick(const char* nick) {
  File nickStore = SPIFFS.open("nick.conf", "w");
  urlEncodeWriteKeyValue("nick", nick, nickStore);
  nickStore.close();
}

bool connectBadge() {
  if(WiFi.status() == WL_CONNECTED) {
    return true;
  }
  status->updateWiFiState("Connecting...");
  ui->draw();
  WiFi.mode(WIFI_STA);
  delay(30);
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
  return WiFi.status() == WL_CONNECTED;
}

void initialConfig() {
  Serial.printf("Free heap at init: %d\n", ESP.getFreeHeap());
  WiFi.disconnect();
  delay(100);
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAPConfig(IPAddress (10, 0, 0, 1), IPAddress(10, 0, 0, 1), IPAddress(255, 255, 255, 0));
  char randpw[20];
  char ssid[20];
  int seed = millis();
  for (int i = 0; i < 10; ++i) {
    seed += badge.getBatLvl();
    seed += badge.getLDRLvl();
  }
  randomSeed(seed);
  sprintf(randpw, "%d", random(10000000, 100000000)); // TODO richtig machen
  sprintf(ssid, "ESP%d", ESP.getChipId());
  String pw = getConfig("configpw", String(randpw));
#ifdef DEBUG
  pw = "hallohallo";
#endif
  Serial.println(WiFi.softAP(ssid, pw.c_str()));
  FullScreenBMPStatus* webStatus = new FullScreenBMPStatus();
  connectWizard(ssid, pw.c_str(), webStatus);
  WebServer webServer(80, "/deflt/web");
  webServer.begin();
  webServer.registerPost("/api/conf/wifi", Page<WebServer::PostHandler>(CacheTime::NO_CACHE,
    [](Stream & currentClient) {
      String bodyBuf = readAll(currentClient);
      UrlDecode dec(bodyBuf.c_str());
      Serial.println("decoder");
      char* confNet = dec.getKey("net");
      char* confPw = dec.getKey("pw");
      if (confNet && confPw) {
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
        if (wStat == WL_CONNECTED) {
          currentClient.write("true");
        } else {
          currentClient.write("false");
        }
      } else {
        currentClient.write("false");
      }
      delete[] confNet;
      delete[] confPw;
    }
  ));

  webServer.registerPost("/api/conf/nick", Page<WebServer::PostHandler>(CacheTime::NO_CACHE,
    [](Stream & currentClient) {
      String bodyBuf = readAll(currentClient);
      UrlDecode nickUrlDecode(bodyBuf.c_str());
      char* nick = nickUrlDecode.getKey("nick");
      if (nick) {
        setNick(nick);
        delete[] nick;
        currentClient.write("true");
      } else {
        currentClient.write("false");
      }
    }
  ));

  webServer.registerPost("/api/channels/add", Page<WebServer::PostHandler>(CacheTime::NO_CACHE,
    [](Stream & currentClient) {
      String bodyBuf = readAll(currentClient);
      UrlDecode channelAddDecode(bodyBuf.c_str());
      char* host = channelAddDecode.getKey("host");
      char* url = channelAddDecode.getKey("url");
      char* fingerprint = channelAddDecode.getKey("fingerprint");
      if (host && url && fingerprint) {
        addChannel(host, url, fingerprint);
        currentClient.write("true");
      } else {
        currentClient.write("false");
      }
      delete[] host;
      delete[] url;
      delete[] fingerprint;
    }
  ));

  webServer.registerPost("/api/channels/delete", Page<WebServer::PostHandler>(CacheTime::NO_CACHE,
    [](Stream & currentClient) {
      String bodyBuf = readAll(currentClient);
      Serial.print("delrecv:");
      Serial.println(bodyBuf);
      UrlDecode channelAddDecode(bodyBuf.c_str());
      char* num = channelAddDecode.getKey("num");
      if (num) {
        int id = atoi(num);
        bool success = deleteChannel(id);
        delete[] num;
        currentClient.print(success);
      } else {
        currentClient.print(false);
      }
    }
  ));

  webServer.registerGet("/api/wifi/scan", Page<WebServer::GetHandler>(CacheTime::NO_CACHE,
    [](Stream & currentClient) {
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
    }
  ));

  webServer.registerGet("/api/wifi/status", Page<WebServer::GetHandler>(CacheTime::NO_CACHE,
    [](Stream & currentClient) {
      int stat = WiFi.status();
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
    }
  ));

  webServer.registerGet("/api/channels", Page<WebServer::GetHandler>(CacheTime::NO_CACHE,
    [](Stream & currentClient) {
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
    }
  ));


  while (true) {
    if (WiFi.softAPgetStationNum() == 0) {
      connectWizard(ssid, pw.c_str(), webStatus);
    }
    webServer.doWork();
  }
  ui->closeCurrent();
}

void connectWizard(const char* ssid, const char* pw, FullScreenBMPStatus* webStatus) {
  bool dispPw = false;
  webStatus->setBmp("/deflt/wifi.bmp", 13, 6);
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
        webStatus->setBmp("/deflt/lock.bmp", 35, 6);
        webStatus->setSub(pw);
      } else {
        webStatus->setBmp("/deflt/wifi.bmp", 13, 6);
        webStatus->setSub(ssid);
      }
      ui->draw();
      delay(50);
    }
    last_state = state;
    if (WiFi.softAPgetStationNum() > 0) {
      webStatus->setBmp("/deflt/pc.bmp", 10, 9);
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
