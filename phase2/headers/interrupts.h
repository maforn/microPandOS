#ifndef INTERRUPTS_H_INCLUDED
#define INTERRUPTS_H_INCLUDED

#define STATUS_MASK 0xFF

void handleIntervalTimer();
void handleLocalTimer();
void handleDeviceInterrupt(unsigned short device_number);

#endif
