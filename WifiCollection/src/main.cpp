#include <Arduino.h>
#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "nvs_flash.h"


uint8_t target_macs[3][6] = {{0x3C, 0x58, 0x5D, 0x82, 0xB9, 0x75}, 
                            {0xF0, 0x7B, 0x65, 0xF6, 0xE8, 0x36}, 
                            {0x6C, 0xFF, 0xCE, 0x37, 0xD3, 0x76}};
uint8_t channels[3] = {1,6,11};
uint8_t num_of_chan = 3;
uint8_t current_chan_index = 0;
//  3C:58:5D:82:B9:75 for my home wifi  Channel:6
//  F0:7B:65:F6:E8:36 for WiFi-26HBW    Channel:1
//  6C:FF:CE:37:D3:76 for WiFi-N5BYD    Channel:11

// Function run everytime a packet is recieved by wifi
void wifi_sniffer_packet_handler(void* buff, wifi_promiscuous_pkt_type_t type) {
  wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t *)buff;
  
  //Grab frame and radioheader parts
  int rssi = pkt->rx_ctrl.rssi;
  uint8_t *frame = pkt->payload;
  
  // If not a wifi packet, stop function
  if (pkt->rx_ctrl.sig_len < 24) {
    return;
  }
  
  // check if the source mac is within our target list
  uint8_t *source_mac = frame + 10;
  int num_targets = sizeof(target_macs) / sizeof(target_macs[0]);
  bool is_a_target = false;

  for (int target = 0; target < num_targets; target++) { 
      bool match = true;
      for (int i = 0; i < 6; i++) {
          if (source_mac[i] != target_macs[target][i]) { 
              match = false; // Current target is not a match, move to next target
              break;
          }
      }
      if (match) { 
        is_a_target = true; // Current target is a match! move to process packet
        break;
      }
  }

  
  if (is_a_target) {

    Serial.printf("%i ", rssi);

    // Print out MAC address
    for (int i = 0; i < 6; i++) {
      if (source_mac[i] < 16) Serial.print("0");  // Add leading zero if needed
      Serial.print(source_mac[i], HEX);
    }
    Serial.printf("\n");

    current_chan_index = (current_chan_index + 1) % num_of_chan;
    esp_wifi_set_channel(channels[current_chan_index], WIFI_SECOND_CHAN_NONE);
  }
}

// This runs once when the ESP32 starts up
void setup() {
  // Start serial communication at 115200 baud rate
  Serial.begin(115200);
  
  // Initialize the ESP32's flash memory system
  nvs_flash_init();
  
  // Initialize the WiFi system
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  esp_wifi_init(&cfg);
  

  esp_wifi_set_mode(WIFI_MODE_STA);   // Set wifichip to station mode
  esp_wifi_start();     // Start the WiFi functions 
  
  esp_wifi_set_channel(channels[current_chan_index], WIFI_SECOND_CHAN_NONE); //Which wifi channel to monitor
  esp_wifi_set_promiscuous(true);     // Enable monitor mode, apparenly also known as promiscuous mode
  esp_wifi_set_promiscuous_rx_cb(wifi_sniffer_packet_handler);   // Set wifi callback function
}


void loop() {
  delay(1000);
}