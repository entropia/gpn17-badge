// vim: noai:ts=2:sw=2
#pragma once

#include <Arduino.h>
#include <FS.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <functional>

struct Notification {
  int id;
  String summary;
  String description;
  String location;
};

enum class NotificationState {
  SCHEDULED = 1,
  ACTIVE_NOT_NOTIFIED = 2,
  NOTIFIED = 3,
  DISMISSED = 4,
  INACTIVE = 5
};

struct __attribute__((__packed__)) NotificationStateEntry {
  int id;
  NotificationState state;
  uint32_t valid_from;
  uint32_t valid_to;
  size_t data_file_position;

  void updateState(uint32_t server_timestamp);
};

struct NotificationHandle {
  int channel;
  int id;
};

enum class NotificationFilter {
  ALL,
  ACTIVE,
  NOT_NOTIFIED
};

extern const String channelsBaseDir;

class ChannelIterator {
public:
  ChannelIterator();
  bool next();
  File file(const char * name, const char * mode);
  String filename(const char * name);
  int channelNum();
  String host();
  String url();
  String fingerprint();
  String name();
private:
  Dir channels_dir;
  String channel_dir_base;
};

class NotificationStateIterator {
public:
  NotificationStateIterator(File state_file);
  bool next();
  NotificationStateEntry get();
  void update(NotificationStateEntry entry);
private:
  File state_file;
  NotificationStateEntry current;
  size_t currentFilePosition;
};

class NotificationIterator {
public:
  NotificationIterator(NotificationFilter filter);
  bool next();
  NotificationStateEntry getStateEntry();
  Notification getNotification();
  void setCurrentNotificationState(NotificationState state);
  NotificationHandle getHandle();

private:
  bool nextStatesFile();
  
  ChannelIterator channels;
  NotificationStateIterator notificationStateIterator;
  NotificationFilter filter;
  NotificationStateEntry current;
};

extern unsigned long lastNotificationPull;
extern long serverTimestamp;

void deleteTimestampFiles();

void pullNotifications();

void syncStatesWithData();

void recalculateStates(std::function<void()> inbetween);

bool getNotificationByHandle(NotificationHandle handle, Notification * notification);

void addChannel(const char* host, const char* url, const char* fingerprint);

bool deleteChannel(int num);
