// vim: noai:ts=2:sw=2
#include "notification_db.h"
#include "url-encode.h"

unsigned long lastNotificationPull = 0;

void pullNotifications() {
  Serial.println("pullNotifications()");
  lastNotificationPull = millis();
  Dir dir = SPIFFS.openDir("/notif/chan");

  while(dir.next()){
    File entry = dir.openFile("r");
    String channelDir(entry.name());
    entry.close();

    if (channelDir.endsWith("/url")) {
      Serial.print("Channel dir: ");
      Serial.println(channelDir);
      String url_file_name = channelDir;
      String channel_dir_base = url_file_name.substring(0, url_file_name.length() - 3);

      String host;
      String url;

      {
        File url_file = SPIFFS.open(url_file_name, "r");
        bool read_host = true;
        while (url_file.available()) {
          char r = char(url_file.read());
          if (r == '\n') {
            read_host = false;
          } else {
            if (read_host) {
              host += r;
            } else {
              url += r;
            }
          }
        }
      }
      Serial.print("host: ");
      Serial.println(host);
      Serial.print("url: ");
      Serial.println(url);

      String fingerprint;
      {
        String fingerprint_file_name = channel_dir_base + "fingerprint";
        File fingerprint_file = SPIFFS.open(fingerprint_file_name, "r");

        while (fingerprint_file.available()) {
          char r = char(fingerprint_file.read());
          if (r == '\n') break;
          fingerprint += r;
        }
      }

      Serial.print("Expected fingerprint: ");
      Serial.println(fingerprint);

      WiFiClientSecure client;
      Serial.print("connecting to ");
      Serial.println(host);
      if (!client.connect(host.c_str(), 443)) {
        Serial.println("connection failed");
        continue;
      }

      if (client.verify(fingerprint.c_str(), host.c_str())) {
        Serial.println("fingerprint matches");
      } else {
        Serial.println("fingerprint doesn't match");
        continue;
      }

      Serial.print("requesting URL: ");
      Serial.println(url);

      client.print(String("GET ") + url + " HTTP/1.1\r\n" +
          "Host: " + host + "\r\n" +
          "User-Agent: BuildFailureDetectorESP8266\r\n" +
          "Connection: close\r\n\r\n");

      Serial.println("request sent");
      while (client.connected()) {
        String line = client.readStringUntil('\n');
        if (line == "\r") {
          Serial.println("headers received");
          break;
        }
      }

      {
        String timestamp_str = client.readStringUntil('\n');
        String local_timestamp_str(millis());
        Serial.print("server timestamp: ");
        Serial.println(timestamp_str);
        Serial.print("local timestamp: ");
        Serial.println(local_timestamp_str);
        String timestamp_file_name = channel_dir_base + "timestamp";
        File timestamp_file = SPIFFS.open(timestamp_file_name, "w");
        timestamp_file.println(timestamp_str);
        timestamp_file.println(local_timestamp_str);
      }

      {
        String data_file_name = channel_dir_base + "data";
        File data_file = SPIFFS.open(data_file_name, "w");
        while (client.available()) {
          uint8_t buf[128];
          int n = client.read(buf, 128);
          data_file.write(buf, n);
          Serial.write(buf, n);
        }
      }
      Serial.println("Data written to file");
    }

  }

}

void deleteTimestampFiles() {
  Serial.println("deleteTimestampFiles()");
  Dir dir = SPIFFS.openDir("/notif/chan");
  while(dir.next()){
    File entry = dir.openFile("r");
    String channelDir(entry.name());
    entry.close();
    if (channelDir.endsWith("/timestamp")) {
      Serial.print("removing: ");
      Serial.println(channelDir);
      SPIFFS.remove(channelDir);
    }
  }
}

Notification * getAnyActiveNotification() {
  return nullptr;
}
