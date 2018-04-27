# Sonoff-with-Nextion-LCD-TouchScreen
Sonoff using modified Tasmota firmware version 5.8.0 to be able to use Nextion display without any extra dev board

https://github.com/arendst/Sonoff-Tasmota
https://github.com/aderusha/HASwitchPlate

Modified https://github.com/aderusha/HASwitchPlate and Sonoff-Tasmota firmware version 5.8.0 to use Nextion displays with Sonoff devices using serial communication and MQTT to provide TouchScreen-based control.

I use this project with OpenHAB but you can use any open-source automation software you like.

The starting point of this project is the Tasmota firmware. All the initial steps of setting up your device are documented in the wiki of that project. Link: https://github.com/arendst/Sonoff-Tasmota/wiki

Additional Steps to get Nextion working with Sonoff:
1. Clone the Sonoff Tasmota firmware version 5.8.0 to your computer.
2. Replace the original sonoff file with the sonoff file in this project.
3. Change the necessary parameters in the user_config.h file like IP Address of MqttHost/broker, WiFi SSId and password.
