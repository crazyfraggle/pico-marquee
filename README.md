# pico-marquee
Code for running my Raspberry Pi Pico driven 4k Led Marquee 

## LED driver

Use the PIO to drive the LEDs. 
The pico-examples repo has a driver for this that can be adapted for the purpose.

Drive the LEDs as two parallell displays to optimize output.
We can use 32bit integer flow through the PIO to drive 4 lines at the time, instead of 2.

## Frame transfer

??
