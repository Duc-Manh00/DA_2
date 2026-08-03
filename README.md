# Password and Fingerprint Door Unlocking System (ESP32 & Flutter App)

A secure, multi-method smart door lock system built with an ESP32 microcontroller, featuring biometric and PIN-based authentication, real-time logging, and remote control capabilities via a Flutter mobile application.

## Hardware Architecture
- **Microcontroller**: ESP32 (NodeMCU) - Handles core logic.
- **Biometric Scanner**: AS608 Fingerprint Sensor (UART communication).
- **Keypad Input**: 3x4 Matrix Keypad for PIN-code authentication (4-6 digits).
- **Display**: LCD 1602 integrated with an I2C Module for local system status alerts.
- **Actuator**: DC12V LY-03 Solenoid Door Lock driven by a Relay Module.
- **Power Management**: 18650 2S Battery Pack, BMS Circuit, and an LM2596 Buck Converter for a standalone backup power supply.

## Key Features & Functionalities
- **Multi-Factor Authentication**: Users can gain access using pre-enrolled fingerprints or a customizable security PIN.
- **Local Web Server Integration**: The ESP32 hosts an HTTP server that processes authenticated remote requests using secure API Keys.
- **Cross-Platform Mobile App**: A dedicated Flutter application providing an intuitive UI to remotely toggle the lock status.
- **Anti-Brute Force Security**: Built-in consecutive failed entry protection that triggers a buzzer alarm and progressive system lockouts (5s to 10s cooldowns).
- **Real-Time Access Logs**: Tracks the last 10 historical events (successful entries, failed attempts, and API triggers) viewable directly on the mobile app.

