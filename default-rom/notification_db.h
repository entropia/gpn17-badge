// vim: noai:ts=2:sw=2
#pragma once

#include <Arduino.h>
#include <FS.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>

struct Notification {
  unsigned int id;
  String summary;
  String description;
  String location;
  uint64_t valid_from;
  uint64_t valid_to;
};

enum class NotificationState {
  SCHEDULED,
  ACTIVE,
  NOTIFIED,
  DISMISSED
};

Notification * getAnyActiveNotification();

extern unsigned long lastNotificationPull;

void deleteTimestampFiles();

void pullNotifications();

void syncStatesWithData();
