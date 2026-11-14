# PCSK225 Software-defined decoder

This is an implementation of fully software-based solution to demodulate longwave signal on 225 kHz and decode phase-modulated PCSK225 signals, including eCzas-radio time signal using STM32 microcontroller.

## Background

The long-wave transmitter located in Solec Kujawski, Poland transmits amplitude modulated audio of Polish Radio 1 on 225 kHz, covering most of Europe. It is also audilble over the world if there is good propagation. additionally, the carrier sends data using phase modulation with system called PCSK225. 

For now the following informations are transmited:
* Time reference signal called *e-Czas radio* - transmitted since late 2023
* Frames identified as *Enea lighting control* - there is no information at all about frame format and exact purpose.

### Modulation deatils

Phase is shifted up by 36 degrees (1/10 of full period). Phase shift is zero, no phase shift is one. There is no manchester encoding - just pure bit-based shift keying. The baudrate is 50 bps.

Frames may be transmitted in 20 time slots per minute (every 3 seconds). First slot starts exactly at full minute.

The whole frame contains 16-bit sync sequence (0x5555 alternating zeros and ones), then 8-bit header which specifies type of frame (see above) and 72-bit payload. e-Czas utillizes it to transmit time code and additional time information, such as time zone (with change announcement twice a year) and leap seconds. One frame takes ~2 seconds to transmit.

## How does it work

RF signal from ferrite-rod active antenna (tuned to carrier freq) is pushed directly to ADC input (only with DC bias added) of STM32F303 MCU. It samples ADC with 2.25 MSPS and demodulates to I/Q signal. Next, the signals are low-pass filtered and the phase is calculated. The simple PLL stabilises low-freqency phase rolls and phase shifts are detected and converted to bits. The MCU has no sophisticated DSP features, so everything is software-based.

Received frames an decoded timestamp are transmitted over UART.

There is also AM detector which sends low-quality (but audible) Polish Radio 1 program to DAC output.


## TODO

This repo is still work-in-progress. The solution needs some features to be added:

- Add AGC
- implement precise 1PPS output
- implement NMEA 0103-compatible output
- add basic UI (display, maybe buttons)
