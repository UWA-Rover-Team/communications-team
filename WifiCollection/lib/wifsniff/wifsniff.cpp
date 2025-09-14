#include <cmath>
#include <stdexcept>
#include <Arduino.h>
#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "nvs_flash.h"
#include "wifsniff.h"

void WifiUtils::distance_estimation(SenderInfo* sender) {
    // Constants for the distance estimation formula
    // const double A = 45.0; // RSSI value at 1 meter
    // const double n = 2.0;  // Path-loss exponent (environmental factor)

    // Calculate distance using the logarithmic path loss model
    sender->distance = pow(10.0, (sender->unitrssi -sender->lastrssi ) / (10.0 * sender->path_loss));
}

// void WifiUtils::wifi_sniffer_packet_handler(void* buff, wifi_promiscuous_pkt_type_t type) {
//   wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t *)buff;
  
//   //Grab frame and radioheader parts
//   int rssi = pkt->rx_ctrl.rssi;
//   uint8_t *frame = pkt->payload;
  
//   // If not a wifi packet, stop function
//   if (pkt->rx_ctrl.sig_len < 24) {
//     return;
//   }
  
//   // check if the source mac is within our target list
//   uint8_t *source_mac = frame + 10;
//   int num_targets = sizeof(target_macs) / sizeof(target_macs[0]);
//   bool is_a_target = false;

//   for (int target = 0; target < num_targets; target++) { 
//       bool match = true;
//       for (int i = 0; i < 6; i++) {
//           if (source_mac[i] != target_macs[target][i]) { 
//               match = false; // Current target is not a match, move to next target
//               break;
//           }
//       }
//       if (match) { 
//         is_a_target = true; // Current target is a match! move to process packet
//         break;
//       }
//   }

  
//   if (is_a_target) {

//     Serial.printf("%i ", rssi);

//     // Print out MAC address
//     for (int i = 0; i < 6; i++) {
//       if (source_mac[i] < 16) Serial.print("0");  // Add leading zero if needed
//       Serial.print(source_mac[i], HEX);
//     }
//     Serial.printf("\n");

//     current_chan_index = (current_chan_index + 1) % num_of_chan;
//     esp_wifi_set_channel(channels[current_chan_index], WIFI_SECOND_CHAN_NONE);
//   }
// }
