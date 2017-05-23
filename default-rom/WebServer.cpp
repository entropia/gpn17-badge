// vim: noai:ts=2:sw=2
#include "WebServer.h"
#include <FS.h>
#undef min
#undef max
#include <vector>

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


void kmp(const String & needle, Stream & haystack, Stream * into) {
  int m = needle.length();
  std::vector<int> border(m + 1);
  border[0] = -1;
  for (int i = 0; i < m; ++i) {
    border[i+1] = border[i];
    while (border[i+1] > -1 and needle[border[i+1]] != needle[i]) {
      border[i+1] = border[border[i+1]];
    }
    border[i+1]++;
  }

  int seen = 0;
  char ringbuf[m];
  int p = 0;
  int l = 0;
  while (haystack.available()) {
    char c = haystack.read();
    while (seen > -1 and needle[seen] != c) {
      seen = border[seen];
    }
    if (++seen == m) {
	    //seen = border[m];
      Serial.print("l=");
      Serial.println(l);
      Serial.println("Rest: ");
        while (l > 0) {
          int idx = (p - l) % m;
          if (idx < 0) idx += m;

          Serial.write(ringbuf[idx]);
          l--;
        }
      Serial.println("");
      return;
    } else {
      if (into) {
        ringbuf[p] = c;
        l++;
        if (l > m) {
          Serial.println("Logic error in kmp ringbuf");
        }
        p = (p + 1) % m;
        while (seen < l && l > 0) {
          int idx = (p - l) % m;
          if (idx < 0) idx += m;

          into->write(ringbuf[idx]);
          l--;
        }
      }
    }
  }
  Serial.println("needle not found");
}

void WebServer::doWork() {
  String getValue;
  unsigned long lastByteReceived = 0;
  bool isPost = false;
  WiFiClient currentClient = server.available();
  if (!currentClient || !currentClient.connected())  {
    return;
  }
  Serial.println("got client");
  {
    bool requestFinished = false;
    String headBuf;
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
  }
  if (isPost) {
    String contentTypeHeader;
    String bodyBuf;
    bool headerFinished = false;
    lastByteReceived = millis();
    while ((lastByteReceived == 0 || millis() - lastByteReceived < WEB_SERVER_CLIENT_TIMEOUT) && !headerFinished) {
      // Read until request body starts
      while (currentClient.available() && !headerFinished) {
        bodyBuf += char(currentClient.read());
        if (bodyBuf.endsWith("\r\n")) {
          if (bodyBuf.startsWith("Content-Type:")) {
            Serial.println(bodyBuf);
            contentTypeHeader = bodyBuf;
          }
          if (bodyBuf == "\r\n") {
            Serial.println("header finished");
            headerFinished = true;
            bodyBuf = String();
            break;
          }
          bodyBuf = String();
        }
        lastByteReceived = millis();
      }
    }
    if (getValue == "/upload") {
      int bound_start = contentTypeHeader.indexOf("boundary=");
      int bound_end = contentTypeHeader.indexOf(" ", bound_start);
      String boundary = contentTypeHeader.substring(bound_start + 9, bound_end);
      if (boundary.endsWith("\r\n")) {
        boundary = boundary.substring(0, boundary.length() - 4);
      }
      boundary = String("--") + boundary;
      Serial.print("boundary:");
      Serial.println(boundary);

      kmp(boundary, currentClient, nullptr);
      Serial.println("Found first boundary");
      kmp("\r\n\r\n", currentClient, nullptr);
      Serial.println("Read part headers");
      {
        File file = SPIFFS.open("/system/web/uptest", "w");
        kmp(String("\r\n") + boundary, currentClient, &file);
      }
      Serial.println("Read part body");
    } else {
      PostPageMap::const_iterator page_it = postHandlers.find(getValue);
      if (page_it != postHandlers.end()) {
        const Page<PostHandler> & page = (*page_it).second;
        currentClient.write("HTTP/1.1 200\r\n");
        writeCacheHeader(currentClient, page.cacheTime);
        currentClient.write("\r\n");
        page.handler(currentClient);
      } else {
        currentClient.write("HTTP/1.1 404");
        currentClient.write("\r\n\r\n");
        currentClient.write(getValue.c_str());
        currentClient.write(" Not Found!");
      }
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
        uint8_t writeBuf[WEB_SERVER_BUFFER_SIZE];
        while (file.available()) {
          size_t len = file.read(writeBuf, sizeof(writeBuf));
          if (!currentClient.write(&writeBuf[0], len)) {
            break;
          }
        }
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

String readAll(Stream & stream) {
  String string;
  while(stream.available()) {
    string += char(stream.read());
  }
  return string;
}
