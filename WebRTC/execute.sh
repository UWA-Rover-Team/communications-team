#!/bin/bash
# Opens two HTML files in Firefox

# Replace with the correct paths to your HTML files
echo "Starting sender scripts"
node server.js
firefox /home/uwa-rover/Documents/rover2025/communications-team/WebRTC/sender_main.html &
firefox /home/uwa-rover/Documents/rover2025/communications-team/WebRTC/sender_periph.html &
firefox /home/uwa-rover/Documents/rover2025/communications-team/WebRTC/receiver.html &
