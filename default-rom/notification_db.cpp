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
  syncStatesWithData();

}

void syncStatesWithData() {
  Dir dir = SPIFFS.openDir("/notif/chan");

  while(dir.next()){
    File entry = dir.openFile("r");
    String channelDir(entry.name());
    entry.close();

    if (channelDir.endsWith("/url")) {
      String channel_dir_base = channelDir.substring(0, channelDir.length() - 3);
      String data_file_name = channel_dir_base + "data";
      String states_file_name = channel_dir_base + "states";
      String new_states_file_name = channel_dir_base + "new_states";
      
      {
        //Add new notifications to states
        //Iterate data - find in states
        //If found: copy to new states file
        //Delete old states file
        File data_file = SPIFFS.open(data_file_name, "r");
        File new_states_file = SPIFFS.open(new_states_file_name, "w");
        while (data_file.available()) {
          String line = data_file.readStringUntil('\n');
          UrlDecode decoder(line.c_str());
          char * id_str = decoder.getKey("id");
          Serial.print("data line id: ");
          Serial.println(id_str);
          if (id_str) {
            File states_file = SPIFFS.open(states_file_name, "r");
            bool found = false;
            while (states_file.available()) {
              String state_line = states_file.readStringUntil('\n');
              UrlDecode state_decoder(state_line.c_str());
              char * state_id_str = decoder.getKey("id");
              if (state_id_str) {
                if (strcmp(id_str, state_id_str) == 0) {
                  Serial.println("Found id in states");
                  new_states_file.println(state_line);
                  delete state_id_str;
                  found = true;
                  break;
                } else {
                  delete state_id_str;
                }
              } else {
                Serial.println("state line without id:");
                Serial.println(state_line);
              }
            }
            if (!found) {
              //Not found in states file
              Serial.println("Did not find id in states");
              urlEncodeWriteKeyValue("id", id_str, new_states_file);
              String notification_state_str(int(NotificationState::SCHEDULED));
              urlEncodeWriteKeyValue("state", notification_state_str.c_str(), new_states_file);
              new_states_file.println("");
            }
            
            delete id_str;
          } else {
            Serial.println("data line without id:");
            Serial.println(line);
          }

        }
      }
      SPIFFS.remove(states_file_name);
      SPIFFS.rename(new_states_file_name, states_file_name);
      {
        Serial.println("states file:");
        File states_file = SPIFFS.open(states_file_name, "r");
        while (states_file.available()) {
          String line = states_file.readStringUntil('\n');
          Serial.println(line);
        }
      }
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
