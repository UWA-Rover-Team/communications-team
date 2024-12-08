# base_station.py

import sys
import socket
import time
import pygame
import cv2
import traceback
import threading

from PyQt5 import QtWidgets, QtGui, QtCore
from PyQt5.QtCore import pyqtSignal, QThread

# Dark mode stylesheet
DARK_STYLE = """
QMainWindow {
    background-color: #2b2b2b;
    color: #ffffff;
}

QLabel {
    color: #ffffff;
}

QPushButton {
    background-color: #3c3c3c;
    color: #ffffff;
    border: 1px solid #555555;
    padding: 5px;
}

QPushButton:hover {
    background-color: #4c4c4c;
}

QPlainTextEdit {
    background-color: #1e1e1e;
    color: #ffffff;
    border: 1px solid #555555;
}

QLineEdit {
    background-color: #1e1e1e;
    color: #ffffff;
    border: 1px solid #555555;
}

QWidget {
    font-size: 12pt;
}
"""

class VideoWindow(QtWidgets.QWidget):
    def __init__(self, stream_url):
        super().__init__()
        self.setWindowTitle("Video Stream")
        self.setGeometry(150, 150, 640, 480)
        self.label = QtWidgets.QLabel(self)
        self.label.setFixedSize(640, 480)
        self.label.setStyleSheet("background-color: black;")
        layout = QtWidgets.QVBoxLayout()
        layout.addWidget(self.label)
        self.setLayout(layout)
        self.stream_url = stream_url
        self.cap = None
        self.running = True
        self.thread = threading.Thread(target=self.update_frame, daemon=True)
        self.thread.start()

    def update_frame(self):
        try:
            self.cap = cv2.VideoCapture(self.stream_url)
            if not self.cap.isOpened():
                self.show_error("Failed to open video stream.")
                return

            while self.running:
                ret, frame = self.cap.read()
                if not ret:
                    self.show_error("Failed to read frame from video stream.")
                    break

                # Convert frame to RGB
                frame = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)

                # Convert to QImage
                height, width, channel = frame.shape
                bytes_per_line = 3 * width
                q_image = QtGui.QImage(frame.data, width, height, bytes_per_line, QtGui.QImage.Format_RGB888)
                pixmap = QtGui.QPixmap.fromImage(q_image)

                # Update the label
                self.label.setPixmap(pixmap)

                # Sleep briefly to reduce CPU usage
                time.sleep(0.03)  # ~30 FPS

        except Exception as e:
            self.show_error(f"Video stream error: {e}\n{traceback.format_exc()}")

        finally:
            if self.cap:
                self.cap.release()

    def show_error(self, message):
        self.label.setText(message)
        self.label.setStyleSheet("color: red; background-color: black;")
        QtWidgets.QApplication.processEvents()

    def closeEvent(self, event):
        self.running = False
        event.accept()


class ReceiveLogsThread(QThread):
    log_received = pyqtSignal(str)
    connection_status = pyqtSignal(str)
    logging_status = pyqtSignal(str)

    def __init__(self, log_port=7000):
        super().__init__()
        self.log_port = log_port
        self.running = True

    def run(self):
        while self.running:
            try:
                log_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                log_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
                log_socket.bind(('', self.log_port))
                log_socket.listen(1)
                self.connection_status.emit("Connection Status: Waiting for Connection")
                self.log_received.emit("Waiting for rover logs...")

                conn, addr = log_socket.accept()
                self.log_received.emit(f"Connected to rover at {addr}")
                self.connection_status.emit("Connection Status: Connected")
                self.logging_status.emit("Logging Status: Active")

                while self.running:
                    data = conn.recv(4096)
                    if not data:
                        self.log_received.emit("Rover disconnected.")
                        self.connection_status.emit("Connection Status: Disconnected")
                        self.logging_status.emit("Logging Status: Inactive")
                        conn.close()
                        break
                    log_message = data.decode().strip()
                    if log_message:  # Only emit non-empty messages
                        self.log_received.emit(log_message)

            except Exception as e:
                self.log_received.emit(f"Log reception error: {e}\n{traceback.format_exc()}")
                self.connection_status.emit("Connection Status: Error")
                self.logging_status.emit(f"Logging Status: Error ({e})")
                time.sleep(5)  # Wait before retrying


class HeartbeatThread(QThread):
    terminal_message = pyqtSignal(str)

    def __init__(self, rover_ip='192.168.2.131', heartbeat_port=5006, interval=1):
        super().__init__()
        self.rover_ip = rover_ip
        self.heartbeat_port = heartbeat_port
        self.interval = interval
        self.running = True

    def run(self):
        heartbeat_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        while self.running:
            try:
                current_time = time.time()
                message = str(current_time).encode()
                heartbeat_socket.sendto(message, (self.rover_ip, self.heartbeat_port))
                time.sleep(self.interval)
            except Exception as e:
                self.terminal_message.emit(f"Heartbeat error: {e}\n{traceback.format_exc()}")
                break
        heartbeat_socket.close()

    def stop(self):
        self.running = False
        self.terminate()


class LatencyMonitorThread(QThread):
    latency_status = pyqtSignal(str)
    terminal_message = pyqtSignal(str)

    def __init__(self, rover_ip='192.168.2.131', rover_port=5006, threshold=2, interval=5):
        super().__init__()
        self.rover_ip = rover_ip
        self.rover_port = rover_port
        self.threshold = threshold
        self.interval = interval
        self.running = True

    def run(self):
        while self.running:
            try:
                heartbeat_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
                heartbeat_socket.settimeout(2)  # 2 seconds timeout
                sent_time = time.time()
                message = str(sent_time).encode()
                heartbeat_socket.sendto(message, (self.rover_ip, self.rover_port))
                data, addr = heartbeat_socket.recvfrom(1024)
                received_time = time.time()
                sent_time = float(data.decode())
                latency = received_time - sent_time

                if latency > self.threshold:
                    self.latency_status.emit(f"Latency Status: High ({latency:.3f} s)")
                else:
                    self.latency_status.emit(f"Latency Status: Normal ({latency:.3f} s)")

                heartbeat_socket.close()
            except socket.timeout:
                self.latency_status.emit("Latency Status: No Response")
            except Exception as e:
                self.latency_status.emit(f"Latency Status: Error ({e})")
                self.terminal_message.emit(f"Latency monitoring error: {e}\n{traceback.format_exc()}")
            time.sleep(self.interval)

    def stop(self):
        self.running = False
        self.terminate()


class JoystickThread(QThread):
    joystick_data = pyqtSignal(str)
    terminal_message = pyqtSignal(str)

    def __init__(self, rover_ip='192.168.2.131', rover_port=5005, threshold=0.1):
        super().__init__()
        self.rover_ip = rover_ip
        self.rover_port = rover_port
        self.threshold = threshold
        self.running = True
        self.prev_axes = {}

    def run(self):
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        pygame.init()
        pygame.joystick.init()
        if pygame.joystick.get_count() > 0:
            joystick = pygame.joystick.Joystick(0)
            joystick.init()
            self.terminal_message.emit("Joystick initialized.")
            # Initialize previous axes with current joystick state
            for i in range(joystick.get_numaxes()):
                axis_value = round(joystick.get_axis(i), 3)
                self.prev_axes[f'axis_{i}'] = axis_value
        else:
            self.terminal_message.emit("No joystick detected.")
            joystick = None

        while self.running:
            try:
                if joystick:
                    pygame.event.pump()
                    axes = {}
                    significant_change = False  # Flag to determine if any axis has significant change

                    for i in range(joystick.get_numaxes()):
                        axis_value = round(joystick.get_axis(i), 3)
                        axis_key = f'axis_{i}'
                        axes[axis_key] = axis_value

                        # Compare with previous axis value
                        prev_value = self.prev_axes.get(axis_key, 0)
                        if abs(axis_value - prev_value) > self.threshold:
                            significant_change = True

                    buttons = {}
                    for i in range(joystick.get_numbuttons()):
                        button_value = joystick.get_button(i)
                        buttons[f'button_{i}'] = button_value

                    if significant_change:
                        data = {'axes': axes, 'buttons': buttons}
                        message = str(data).encode()
                        sock.sendto(message, (self.rover_ip, self.rover_port))
                        self.terminal_message.emit(f"Sent joystick data: {data}")
                        self.joystick_data.emit(str(data))
                        # Update previous axes
                        self.prev_axes = axes.copy()

                time.sleep(0.1)  # Polling interval

            except Exception as e:
                self.terminal_message.emit(f"Joystick error: {e}\n{traceback.format_exc()}")
                break

        sock.close()
        if joystick:
            joystick.quit()
            pygame.joystick.quit()
        pygame.quit()

    def stop(self):
        self.running = False
        self.terminate()


class MainWindow(QtWidgets.QMainWindow):
    # Define custom signals
    update_terminal_signal = pyqtSignal(str)
    update_joystick_signal = pyqtSignal(str)
    update_connection_signal = pyqtSignal(str)
    update_logging_signal = pyqtSignal(str)
    update_video_signal = pyqtSignal(str)
    update_latency_signal = pyqtSignal(str)

    def __init__(self):
        super().__init__()
        self.init_ui()

        # Connect signals to slots
        self.update_terminal_signal.connect(self.update_terminal)
        self.update_joystick_signal.connect(self.update_joystick_display)
        self.update_connection_signal.connect(self.update_connection_status)
        self.update_logging_signal.connect(self.update_logging_status)
        self.update_video_signal.connect(self.update_video_status)
        self.update_latency_signal.connect(self.update_latency_status)

        # Apply dark mode
        self.apply_dark_mode()

        # Video window reference
        self.video_window = None

        # Start threads
        self.rover_ip = '192.168.2.131'  # Replace with your rover's IP address

        self.receive_logs_thread = ReceiveLogsThread(log_port=7000)
        self.receive_logs_thread.log_received.connect(self.update_terminal_signal.emit)
        self.receive_logs_thread.connection_status.connect(self.update_connection_signal.emit)
        self.receive_logs_thread.logging_status.connect(self.update_logging_signal.emit)
        self.receive_logs_thread.start()

        self.heartbeat_thread = HeartbeatThread(rover_ip=self.rover_ip, heartbeat_port=5006, interval=1)
        self.heartbeat_thread.terminal_message.connect(self.update_terminal_signal.emit)
        self.heartbeat_thread.start()

        self.latency_thread = LatencyMonitorThread(rover_ip=self.rover_ip, rover_port=5006, threshold=2, interval=5)
        self.latency_thread.latency_status.connect(self.update_latency_signal.emit)
        self.latency_thread.terminal_message.connect(self.update_terminal_signal.emit)
        self.latency_thread.start()

        self.joystick_thread = JoystickThread(rover_ip=self.rover_ip, rover_port=5005, threshold=0.1)
        self.joystick_thread.joystick_data.connect(self.update_joystick_signal.emit)
        self.joystick_thread.terminal_message.connect(self.update_terminal_signal.emit)
        self.joystick_thread.start()

    def init_ui(self):
        self.setWindowTitle('Lunar Rover Control Panel')
        self.setGeometry(100, 100, 800, 600)

        # Status Indicators
        self.connection_label = QtWidgets.QLabel("Connection Status: Disconnected", self)
        self.connection_label.setStyleSheet("color: red; font-weight: bold;")

        self.logging_label = QtWidgets.QLabel("Logging Status: Inactive", self)
        self.logging_label.setStyleSheet("color: red; font-weight: bold;")

        self.video_label = QtWidgets.QLabel("Video Streaming Status: Inactive", self)
        self.video_label.setStyleSheet("color: red; font-weight: bold;")

        self.latency_label = QtWidgets.QLabel("Latency Status: Unknown", self)
        self.latency_label.setStyleSheet("color: orange; font-weight: bold;")

        # Terminal Display
        self.terminal_display = QtWidgets.QPlainTextEdit(self)
        self.terminal_display.setReadOnly(True)
        self.terminal_display.setFixedHeight(200)

        # Joystick Input Display
        self.joystick_label = QtWidgets.QLabel("Joystick Input: None", self)
        self.joystick_label.setStyleSheet("font-weight: bold;")

        # Open Video Stream Button
        self.open_video_button = QtWidgets.QPushButton("Open Video Stream", self)
        self.open_video_button.clicked.connect(self.open_video_stream)

        # Layout
        central_widget = QtWidgets.QWidget()
        layout = QtWidgets.QVBoxLayout()

        # Status Layout
        status_layout = QtWidgets.QHBoxLayout()
        status_layout.addWidget(self.connection_label)
        status_layout.addWidget(self.logging_label)
        status_layout.addWidget(self.video_label)
        status_layout.addWidget(self.latency_label)
        layout.addLayout(status_layout)

        # Joystick and Video Button Layout
        joystick_video_layout = QtWidgets.QHBoxLayout()
        joystick_video_layout.addWidget(self.joystick_label)
        joystick_video_layout.addStretch()
        joystick_video_layout.addWidget(self.open_video_button)
        layout.addLayout(joystick_video_layout)

        # Terminal Display
        layout.addWidget(self.terminal_display)

        central_widget.setLayout(layout)
        self.setCentralWidget(central_widget)

    def apply_dark_mode(self):
        self.setStyleSheet(DARK_STYLE)

    def open_video_stream(self):
        stream_url = f'rtsp://{self.rover_ip}:8554/video'  # RTSP Stream URL
        try:
            if self.video_window is None:
                self.video_window = VideoWindow(stream_url)
                self.video_window.show()
                self.update_video_signal.emit("Video Streaming Status: Active")
                self.update_terminal_signal.emit("Video stream window opened.")
            else:
                self.video_window.activateWindow()
                self.update_terminal_signal.emit("Video stream window is already open.")
        except Exception as e:
            self.update_terminal_signal.emit(f"Failed to open video stream: {e}\n{traceback.format_exc()}")

    def update_terminal(self, message):
        if message:
            self.terminal_display.appendPlainText(message)

    def update_joystick_display(self, data):
        if data:
            self.joystick_label.setText(f"Joystick Input: {data}")

    def update_connection_status(self, status):
        if "Connected" in status:
            self.connection_label.setText(status)
            self.connection_label.setStyleSheet("color: green; font-weight: bold;")
        elif "Disconnected" in status or "Error" in status:
            self.connection_label.setText(status)
            self.connection_label.setStyleSheet("color: red; font-weight: bold;")
        elif "Waiting for Connection" in status:
            self.connection_label.setText(status)
            self.connection_label.setStyleSheet("color: orange; font-weight: bold;")
        else:
            self.connection_label.setText(status)
            self.connection_label.setStyleSheet("color: orange; font-weight: bold;")

    def update_logging_status(self, status):
        if "Active" in status:
            self.logging_label.setText(status)
            self.logging_label.setStyleSheet("color: green; font-weight: bold;")
        elif "Inactive" in status or "Error" in status:
            self.logging_label.setText(status)
            self.logging_label.setStyleSheet("color: red; font-weight: bold;")
        else:
            self.logging_label.setText(status)
            self.logging_label.setStyleSheet("color: orange; font-weight: bold;")

    def update_video_status(self, status):
        if "Active" in status:
            self.video_label.setText(status)
            self.video_label.setStyleSheet("color: green; font-weight: bold;")
        elif "Inactive" in status or "Failed" in status:
            self.video_label.setText(status)
            self.video_label.setStyleSheet("color: red; font-weight: bold;")
        else:
            self.video_label.setText(status)
            self.video_label.setStyleSheet("color: orange; font-weight: bold;")

    def update_latency_status(self, status):
        if "Normal" in status:
            self.latency_label.setText(status)
            self.latency_label.setStyleSheet("color: green; font-weight: bold;")
        elif "High" in status:
            self.latency_label.setText(status)
            self.latency_label.setStyleSheet("color: red; font-weight: bold;")
        elif "No Response" in status:
            self.latency_label.setText(status)
            self.latency_label.setStyleSheet("color: orange; font-weight: bold;")
        elif "Error" in status:
            self.latency_label.setText(status)
            self.latency_label.setStyleSheet("color: red; font-weight: bold;")
        else:
            self.latency_label.setText(status)
            self.latency_label.setStyleSheet("color: orange; font-weight: bold;")

    def closeEvent(self, event):
        # Gracefully stop all threads
        self.receive_logs_thread.running = False
        self.receive_logs_thread.quit()
        self.receive_logs_thread.wait()

        self.heartbeat_thread.stop()

        self.latency_thread.stop()

        self.joystick_thread.stop()

        # Close video window if open
        if self.video_window:
            self.video_window.close()

        event.accept()


def start_base_station():
    app = QtWidgets.QApplication(sys.argv)
    main_window = MainWindow()
    main_window.show()
    sys.exit(app.exec_())


if __name__ == '__main__':
    start_base_station()
