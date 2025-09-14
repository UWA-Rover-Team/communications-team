#include <cmath>
#include <stdexcept>
#include <Arduino.h>
#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "nvs_flash.h"
#include <trilateralisation.h>

namespace WifiUtils {
    struct SenderInfo {
        uint8_t mac[6];
        // int channel;
        int lastrssi;
        int unitrssi;
        unsigned long path_loss;
        double distance;
        TrilaterationUtils::Point3D position;
    };
    // void wifi_sniffer_packet_handler(void* buff, wifi_promiscuous_pkt_type_t type);
    void distance_estimation(SenderInfo* sender);
}