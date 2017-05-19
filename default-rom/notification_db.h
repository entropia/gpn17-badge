// vim: noai:ts=2:sw=2
#pragma once

#include <Arduino.h>
#include <FS.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>

struct Notification {
  int id;
  String summary;
  String description;
  String location;
  uint64_t valid_from;
  uint64_t valid_to;
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
};

enum class NotificationFilter {
  ALL,
  ACTIVE,
  NOT_NOTIFIED
};

class ChannelIterator {
public:
  ChannelIterator();
  bool next();
  File file(const char * name, const char * mode);
  String filename(const char * name);
private:
  Dir channels_dir;
  String channel_dir_base;
};

class NotificationStateIterator {
public:
  NotificationStateIterator(File state_file);
  bool next();
  NotificationStateEntry get();
private:
  File state_file;
  NotificationStateEntry current;
};

class NotificationIterator {
public:
  NotificationIterator(NotificationFilter filter);
  bool next();
  Notification get();

private:
  bool nextStatesFile();
  
  ChannelIterator channels;
  NotificationStateIterator notificationStateIterator;
  NotificationFilter filter;
  Notification currentNotification;
};

extern unsigned long lastNotificationPull;

void deleteTimestampFiles();

void pullNotifications();

void syncStatesWithData();
