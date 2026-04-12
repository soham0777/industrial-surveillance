🚀 IoT-Based Industrial Security System
📌 Project Title

IoT-Enabled Industrial Security System Using ESP32-S3 CAM with Real-Time Monitoring and 360° Surveillance

📖 Overview

This project presents a low-cost IoT-based industrial security system designed to provide real-time monitoring, environmental sensing, and remote control. The system integrates a camera module with multiple sensors and provides a web-based dashboard for live monitoring and control.

🎯 Objectives
Provide real-time video surveillance
Enable remote monitoring via web interface
Integrate environmental sensors
Achieve 360° area coverage
Develop a low-cost and scalable solution
⚙️ System Components
🔹 Hardware:
ESP32-S3 CAM
PIR Motion Sensor
DHT11 (Temperature & Humidity)
LDR (Light Sensor)
Rain Sensor
Continuous Servo Motor (360°)
Buzzer & LED
🔹 Software:
Arduino IDE
ESP32 Libraries
Web Server (HTML/CSS/JS)
REST API
🧠 Working Principle

According to the system design explained in your report :

System powers ON and initializes components
ESP32 connects to Wi-Fi
Camera starts live streaming
Sensors collect environmental data
Data is displayed on web dashboard
Motion detection triggers alert system
Servo motor rotates for 360° coverage
🌐 Features
📹 Live video streaming (~15 FPS)
📡 Real-time sensor monitoring
🔔 Motion detection alerts (<0.5 sec response)
🔄 360° camera rotation
🌍 Web-based remote control
⚡ Low latency (~200 ms)
💰 Cost-effective (< ₹3000)
🖥️ Web Dashboard Functionalities
Live video feed
Sensor data display (Temp, Humidity, Light, Rain)
Manual camera control
Auto-pan feature
Speed control
LED & buzzer control
🔄 Code Implementation Modules

As shown in your flowchart (Page 11) :

Camera Initialization
Wi-Fi Connection
Web Server Setup
Sensor Reading Functions
Servo Control Logic
API Handling
REST API Communication
Real-time Data Updates
MJPEG Video Streaming
📊 Results
🔍 Observations:
Live video streaming achieved (~15 FPS)
Real-time sensor updates
Fast motion detection (<0.5 sec)
Smooth servo operation
📈 Performance:
Latency: ~200 ms
Range: ~30 meters
Stable continuous operation
✅ Functional Testing:
Motion detection → Working
Rain detection → Working
Light sensing → Working
Manual control → Working
🏁 Conclusion

The project successfully demonstrates a low-cost smart surveillance system integrating video monitoring, sensor data, and remote control. It provides:

360° monitoring
Real-time environmental tracking
Instant alert system
User-friendly interface

It is suitable for industries, warehouses, farms, and smart homes.

🔮 Future Scope
📱 Mobile app integration
☁️ Cloud storage (Firebase, AWS)
🤖 AI-based object detection
☀️ Solar-powered system
📷 Multi-camera setup
📚 References
ESP32 Technical Reference Manual
MQTT Protocol Documentation
IoT Literature Reviews
ESP32 Camera Web Server (GitHub)
DHT Sensor Library
Research Papers on IoT Surveillance Systems
👨‍💻 Author

Soham Prabhakar Kadu
B.Tech (CSE - AIML)
Sanjivani University, Kopargaon
