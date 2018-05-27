# What is this?
There is a lot of fuss going on about IoT. Devices are ever more present in our day to day life, and all devices come with their proprietary connectivity and apps… But I’d like to have a central control over all devices in and around the house.

I’d like to have more control and develop an integrated approach. This way, a smartphone app can detect that I’m driving home and open the garage door when I arrive in front of it. It can disable the alarm and delay the gardening watering to make me enter with dry trousers. It can set heating and lights based on temperature and light conditions.
Heating can be controlled in zones based on space usage and room temperature, and the watering system can be diminished or disabled based on external temperature, humidity and rain (except for example for the zone on the porch). If the watering system triggers the alarm, the alarm sensors can be selectively turned off based on the active watering zone.

The system must have an intuitive interface that enables the user to combine devices. Using machine learning algorithm, the system can recognize patterns and propose to the user actions (turn off lights, insert alarm) based on behavior and user feedback (registered actions of the user).

This is the reason I developed this framework. Its aim is to be an open source and open standard platform providing a centralized control of devices. Both the development of hardware (devices) and front end (app, learning and prediction engine, html template for specific devices) must be community driven. Anyone developing a part will draw benefit of the whole platform, encouraging every contributor to leverage on the efforts of others. For example, a device manufacturer will be advantaged by the fact someone will use the device and integrate it is ways he did not think of.

Personally, I love the Arduino platform and I will use it for the MCU part of the architecture. But I strongly encourage (and will actively support) anyone who’d like to include other MCU platforms (such as esp8266). I am planning to do so myself, but not in the very near future.

What devices should you think about?
I started this project with a plants watering system… But I will extend it on short notice to:
-	Lights
-	Heating / cooling
-	Various sensors – proximity, temperature, humidity, motion, photoresistors, …
-	Alarm system
-	Gardening watering systems
-	Wherever my imagination takes me

Plug and Play?
The architecture has been set up to enable registered devices to be moved and controlled from different controlling units without losing their configuration information or status. However, including and setting up a new device can be cumbersome, as all MCU connected pins and what they will be controlling needs to be known to the central unit.

To overcome this, the protocol permits the use of the device EEPROM to store and retrieve the specific connected device information. This will allow anyone to develop (and sell) devices that will announce their make / model or pin layout when connected to the central unit, therefore integrating its capabilities in an automatic way, leveraging on the capabilities of the platform.

The MCU discovery function will run on each DCU and look for additional MCUs on their connected serial ports (including Bluetooth uarts). If found, a new DCU is launched and connected to the new MCU, registering it into the system. The Bluetooth pairing (and if needed the creation of the serial connection port) must be done manually (can we automate this?), and the new DCU process must be set to launch on startup.

The experience must get as close as possible to this: You have one or more installed DCUs (e.g. raspberry that control some devices). You buy a new device and power it on. The DCU sees there is a new Bluetooth device in range, and asserts it has uart capabilities. it connects to the device and see if it responds to the HELLO message. It asks the user if this is a new device to add, and based on make / model, it downloads all necessary device configurations and frontend plugins. You can start to use the device right away, and have it integrated in the frontend apps.

Terms explained
MCU – Micro Controller Unit – the actual microcontroller with any software running on it
DCU – Device Control Unit – microcontroller driver running on a cpu based system (arm/x386)
spDCU – Specific Purpose DCU. Custom DCU implementing the controls of a proprietary device
CCU – Central Control Unit which is composed of
	CCU RU – Registration Unit
	CCU QH – Queue Handler
	CCU RE – REST Engine
	CCU Backend – Communication bus connecting the CCU elements together
	CCU DB – Database backend for persistence
