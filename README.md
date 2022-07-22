# Dry-Cabinet

Open source herb/leaf dry/cure cabinet. Also applicable to meat or cheese curing.  Hardware and arduino firmware design, contains 2x analog temperature sensor ports, 2x external SHT-31 ports for temp/hum measurement. PWM control of circulation/exhaust, high-current DC switched outputs for humidifier, dehumidifier, heater, cooler as well as relay-driven outputs for AC devices. 

Essentially, just a versatile humidity/temperature controller, useable as either or as both. Inexpensive hardware design with hobbyist-friendly SMT components (0805 or larger). Utilizes Arduino Nano Every, the most inexpensive arduino suitable for this task, with a I2C 16x2 LCD. I used DFRobot external SHT31 sensors and LCD.

NOTE: D13 on-board LED on the Arduino Nano Every must be disabled. I did this by desoldering the LED resistor. I was unaware there was an on-board LED so the "UP" button will always read as depressed if this is not performed.
