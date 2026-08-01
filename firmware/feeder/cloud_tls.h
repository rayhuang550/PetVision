#pragma once
#include <Arduino.h>
#include <WiFiClientSecure.h>

using CloudTransport = WiFiClientSecure;

void configureCloudTransport(CloudTransport& client);
