# Electro-Chlorinator

This Arduino code controls a device that produces sodium hypochlorite (bleach) by electrolosys of salt water. This process is known as electrochlorination.

Through the Arduino serial monitor, the user can specify how long the device will run, and during the process, the saved pH, supply voltage, voltage across the electrodes, and current through the solution is displayed. The process can be paused with a remote in order to measure the pH with an analog pH sensor. The pH reading can then be updated and saved to the display.

# Components
The device utilizes an Arduino Mega2560, DFRobot SEN0161 Analog pH Sensor, and an Elegoo DS1307 Real Time Clock Module, a .100 OHM shunt resistor, and an IR Reciever. Electrolysis is controlled by an NMOS switch circuit.

# Documentation
Complete project documentation and demos are at [t3rwin.github.io/Electrochlorination](https://t3rwin.github.io/Electrochlorination)
