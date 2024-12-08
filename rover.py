# rover.py

import threading
import socket
import time
import logging
import ast
import copy
import gi
import sys
import os

# Configure logging
logging.basicConfig(
    filename='rover_log.log',
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s'
)

# Ensure the correct versions of Gst and GstRtspServer are used
gi.require_version('Gst', '1.0')
gi.require_version('GstRtspServer', '1.0')

from gi.repository import Gst, GstRtspServer, GLib

# Initialize GStreamer
Gst.init(None)

def start_logging():
    logging.info("Communication started.")
    print("Logging initialized and communication started.")

def stop_logging():
    logging.info("Communication stopped.")
    print("Logging stopped.")

def monitor_latency():
    latency_threshold = 2  # seconds
    heartbeat_port = 5006
    heartbeat_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    heartbeat_socket.bind(('', heartbeat_port))
    last_log_time = time.time()  # Initialize last_log_time to current time

    while True:
        try:
            data, addr = heartbeat_socket.recvfrom(1024)
            received_time = time.time()
            sent_time = float(data.decode())
            latency = received_time - sent_time

            current_time = time.time()
            # Log the latency only once every 60 seconds
            if current_time - last_log_time >= 60:
                logging.info(f"Latency: {latency:.3f} seconds")
                print(f"Latency: {latency:.3f} seconds")
                last_log_time = current_time

            # Log immediately if latency exceeds the threshold
            if latency > latency_threshold:
                logging.error(f"Latency threshold exceeded: {latency:.3f} seconds")
                print(f"Latency threshold exceeded: {latency:.3f} seconds")
        except Exception as e:
            logging.error(f"Latency monitoring error: {e}")
            print(f"Latency monitoring error: {e}")

def send_logs_to_base_station():
    base_station_ip = '192.168.180.137'  # Replace with the base station's IP
    log_port = 7000
    connected = False

    while True:
        log_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        try:
            if not connected:
                print(f"Attempting to connect to base station at {base_station_ip}:{log_port}")
                log_socket.connect((base_station_ip, log_port))
                logging.info(f"Connected to base station at {base_station_ip}:{log_port}")
                print(f"Connected to base station at {base_station_ip}:{log_port}")
                connected = True

            # Continuously read the log file and send new entries
            with open('rover_log.log', 'r') as log_file:
                # Move to the end of the file
                log_file.seek(0, 2)
                while True:
                    line = log_file.readline()
                    if line:
                        try:
                            log_socket.sendall(line.encode())

                        except Exception as e:
                            logging.error(f"Error sending log data: {e}")
                            print(f"Error sending log data: {e}")
                            log_socket.close()
                            connected = False
                            break
                    else:
                        time.sleep(1)
        except Exception as e:
            logging.error(f"Failed to connect to base station logs: {e}")
            print(f"Failed to connect to base station logs: {e}")
            time.sleep(5)  # Wait before retrying

def receive_joystick_commands():
    joystick_port = 5005
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(('', joystick_port))
    print(f"Listening for joystick data on port {joystick_port}")

    last_joystick_data = None  # Initialize last joystick data

    while True:
        try:
            data, addr = sock.recvfrom(4096)
            joystick_data = ast.literal_eval(data.decode())

            # Compare with last joystick data
            if joystick_data != last_joystick_data:
                # Log only if there is a change in joystick data
                logging.info(f"Received joystick data: {joystick_data}")
                print(f"Received joystick data: {joystick_data}")
                # Update last joystick data
                last_joystick_data = copy.deepcopy(joystick_data)

            # You can implement motor control logic here using joystick_data

        except Exception as e:
            logging.error(f"Joystick command reception error: {e}")
            print(f"Joystick command reception error: {e}")

def start_rtsp_server():
    try:
        class RTSPMediaFactory(GstRtspServer.RTSPMediaFactory):
            def __init__(self):
                super(RTSPMediaFactory, self).__init__()
                self.set_shared(True)

            def do_create_element(self, url):
                print("Creating GStreamer pipeline for RTSP stream")
                pipeline_str = (
                    'v4l2src device=/dev/video0 ! videoconvert ! '
                    'video/x-raw,format=I420,width=640,height=480,framerate=30/1 ! '
                    'queue ! x264enc tune=zerolatency bitrate=500 speed-preset=superfast ! '
                    'h264parse ! rtph264pay name=pay0 pt=96 config-interval=1'
                )
                try:
                    pipeline = Gst.parse_launch(pipeline_str)
                    print("GStreamer pipeline created successfully")
                    logging.info("GStreamer pipeline created successfully")
                    return pipeline
                except Exception as e:
                    print(f"Failed to create GStreamer pipeline: {e}")
                    logging.error(f"Failed to create GStreamer pipeline: {e}")
                    return None

        # Create RTSP server
        server = GstRtspServer.RTSPServer()
        factory = RTSPMediaFactory()
        mount_points = server.get_mount_points()
        mount_points.add_factory("/video", factory)
        server.attach(None)

        print("RTSP server is running at rtsp://0.0.0.0:8554/video")
        logging.info("RTSP server is running at rtsp://0.0.0.0:8554/video")

        # Start the main loop
        loop = GLib.MainLoop()
        loop.run()
    except Exception as e:
        print(f"Exception in RTSP server thread: {e}")
        logging.error(f"Exception in RTSP server thread: {e}")

def start_rover():
    start_logging()

    # Start threads for latency monitoring, log sending, joystick reception
    latency_thread = threading.Thread(target=monitor_latency, daemon=True)
    log_thread = threading.Thread(target=send_logs_to_base_station, daemon=True)
    joystick_thread = threading.Thread(target=receive_joystick_commands, daemon=True)

    latency_thread.start()
    log_thread.start()
    joystick_thread.start()

    # Start RTSP server in the main thread
    start_rtsp_server()

if __name__ == '__main__':
    start_rover()
