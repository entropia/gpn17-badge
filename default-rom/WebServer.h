// vim: noai:ts=2:sw=2
#pragma once 
#include <WiFiClient.h>
#include <WiFiServer.h>
#include <map>

#define WEB_SERVER_CLIENT_TIMEOUT 100
#define WEB_SERVER_BUFFER_SIZE 40

enum class CacheTime {
  NO_CACHE,
  LONG_CACHE
};

template<typename Handler>
struct Page {
  Page() {}
  Page(CacheTime cacheTime, Handler handler)
    : cacheTime(cacheTime), handler(handler) {}
  Handler handler;
  CacheTime cacheTime;
};

class WebServer {
public:
  typedef std::function<void(Stream &)> GetHandler;
  typedef std::function<void(Stream &)> PostHandler;
  typedef std::map<String, Page<GetHandler>> GetPageMap;
  typedef std::map<String, Page<PostHandler>> PostPageMap;
  WebServer(int port, String staticPath)
    : server(port), staticPath(staticPath) {}
  ~WebServer();
  void begin();
  void registerGet(String url, Page<GetHandler> page);
  void registerPost(String url, Page<PostHandler> page);
  void doWork();
private:
  WiFiServer server;
  WiFiClient currentClient;
  GetPageMap getHandlers;
  PostPageMap postHandlers;
  String staticPath;
};

String readAll(Stream & stream);
