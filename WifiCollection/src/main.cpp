#include <Arduino.h>
#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "nvs_flash.h"
#include <serial_com.h>
#include <trilateralisation.h>
#include <wifsniff.h>


uint8_t target_macs[3][6] = {{0x3C, 0x58, 0x5D, 0x82, 0xB9, 0x75}, 
                            {0xF0, 0x7B, 0x65, 0xF6, 0xE8, 0x36}, 
                            {0x6C, 0xFF, 0xCE, 0x37, 0xD3, 0x76}};
uint8_t channels[3] = {1,6,11};
WifiUtils::SenderInfo senders[3];
uint8_t num_of_chan = 3;
uint8_t current_chan_index = 0;
//  3C:58:5D:82:B9:75 for my home wifi  Channel:6
//  F0:7B:65:F6:E8:36 for WiFi-26HBW    Channel:1
//  6C:FF:CE:37:D3:76 for WiFi-N5BYD    Channel:11

//Serial communication object
scom com;
//Current estimated position
TrilaterationUtils::Point3D current_position;

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
  // int num_targets= num_of_chan;// = sizeof(target_macs) / sizeof(target_macs[0]);
  int num_targets= sizeof(senders) / sizeof(senders[0]);
  int which_target=-1;

  for (int target = 0; target < num_targets; target++) { 
      bool match = true;
      for (int i = 0; i < 6; i++) {
          if (source_mac[i] != senders[target].mac[i]) { 
              match = false; // Current target is not a match, move to next target
              break;
          }
      }
      if (match) { 
        which_target=target; // Current target is a match! move to process packet
        break;
      }
  }

  
  if (which_target != -1) { // If the source mac matches one of our target macs, process the packet

    Serial.printf("%i ", rssi);
    senders[which_target].lastrssi = rssi;

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
  // senders[0].channel = 1;
  // senders[1].channel = 6; 
  // senders[2].channel = 11;
  senders[0].path_loss = 2;
  senders[1].path_loss = 2;
  senders[2].path_loss = 2;
  senders[0].unitrssi = -45;
  senders[1].unitrssi = -45;
  senders[2].unitrssi = -45;
  for (int i = 0; i < 3; i++) {
    senders[i].lastrssi = -100; // Initialize last RSSI to a low value
    senders[i].distance = 0.0;
    memcpy(senders[0].mac, target_macs[0], 6);
  }
  // memcpy(senders[0].mac, target_macs[0], 6);
  // memcpy(senders[1].mac, (uint8_t[]){0xF0, 0x7B, 0x65, 0xF6, 0xE8, 0x36}, 6);
  // memcpy(senders[2].mac, (uint8_t[]){0x6C, 0xFF, 0xCE, 0x37, 0xD3, 0x76}, 6);
  senders[0].position= {0.0, 0.0, 0.0};
  senders[1].position= {5.0, 0.0, 0.0};
  senders[2].position= {2.5, 4.33, 0.0};  

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
  com.comLoop();
  delay(1000);
  //calc current position
  current_position = TrilaterationUtils::Trilaterate(senders[0].position, senders[0].distance, 
                                                      senders[1].position, senders[1].distance, 
                                                      senders[2].position, senders[2].distance);
  // send pos over serial
  com.setpair("x", String(current_position.x));
  com.setpair("y", String(current_position.y));
  com.setpair("z", String(current_position.z));
  com.send( "trilateration","3DPoint");
}