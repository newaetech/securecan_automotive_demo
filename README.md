## USB Can Interface

The USB Can interface is a PEAK CAN USB FD: https://www.peak-system.com/PCAN-USB-FD.365.0.html?&L=1

Software of importance:

* PCAN-View (used to send/receive packets)
* PCAN-Basic API (includes Python API)

secure-can/ contains all of the firmware for the project. To build for the STM32 that reads from the pedal, run make ISMASTER=NO. To build for the STM32 that controls the motor, run make.
