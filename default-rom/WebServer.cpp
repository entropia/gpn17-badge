// vim: noai:ts=2:sw=2
#include "WebServer.h"
#include <FS.h>

WebServer::~WebServer() {
  server.close();
}

void WebServer::begin() {
  server.begin();
}

void WebServer::registerPost(String url, Page<PostHandler> page) {
  postHandlers[url] = page;
}

void WebServer::registerGet(String url, Page<GetHandler> page) {
  getHandlers[url] = page;
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

void writeCacheHeader(Stream & stream, CacheTime cacheTime) {
  switch(cacheTime) {
    case CacheTime::NO_CACHE:
      stream.write("Cache-Control: no-cache\r\n");
      break;
    case CacheTime::LONG_CACHE:
      stream.write("Cache-Control: max-age=1800\r\n");
      break;
  }
}

void WebServer::doWork() {
  String headBuf;
  String bodyBuf;
  String getValue;
  unsigned long lastByteReceived = 0;
  bool requestFinished = false;
  bool isPost = false;
  WiFiClient currentClient = server.available();
  if (!currentClient || !currentClient.connected())  {
    return;
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
    return;
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
    PostPageMap::const_iterator page_it = postHandlers.find(getValue);
    if (page_it != postHandlers.end()) {
      const Page<PostHandler> & page = (*page_it).second;
      currentClient.write("HTTP/1.1 200\r\n");
      writeCacheHeader(currentClient, page.cacheTime);
      currentClient.write("\r\n");
      page.handler(currentClient, bodyBuf);
    } else {
      currentClient.write("HTTP/1.1 404");
      currentClient.write("\r\n\r\n");
      currentClient.write(getValue.c_str());
      currentClient.write(" Not Found!");
    }
  } else {
    GetPageMap::const_iterator page_it = getHandlers.find(getValue);
    if (page_it != getHandlers.end()) {
      const Page<GetHandler> & page = (*page_it).second;
      currentClient.write("HTTP/1.1 200\r\n");
      writeCacheHeader(currentClient, page.cacheTime);
      currentClient.write("\r\n");
      page.handler(currentClient);
    } else {
      if (getValue == "/") {
        getValue = "/index.html";
      }
      String path = staticPath + getValue;
      if (SPIFFS.exists(path) || SPIFFS.exists(path + ".gz")) {
        char writeBuf[WEB_SERVER_BUFFER_SIZE];
        currentClient.write("HTTP/1.1 200\r\n");
        currentClient.write("Content-Type: ");
        currentClient.write(getContentType(path).c_str());
        currentClient.write("\r\n");
        writeCacheHeader(currentClient, CacheTime::LONG_CACHE);
        if (SPIFFS.exists(path + ".gz")) {
          path += ".gz";
          currentClient.write("Content-Encoding: gzip\r\n");
        }
        currentClient.write("\r\n");
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
  currentClient.flush();
  currentClient.stop();
  Serial.println("Finished client.");
  Serial.printf("Free heap: %d\n", ESP.getFreeHeap());
}
