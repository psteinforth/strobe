# strobe
ATtiny microcontroller project to control a cheap stroboscope or similar device. 

### Introduction
This piece of software was designed to enable a cheap LED stroboscope by people on a danceflor. The strobe flashes as long as it is powered. The idea was to enable the strobe for a fixed time period as soon as a dancer presses a red push-button. To avoid nearly continuous flashing due to drunken people :dancers: the strobe can not be reenabled before a configurable time period passed.

### Features
- Based on Atmel ATtiny 44A-PU microcontroller.
- Interrupt-driven recognition of push-button presses.
- Strobe activity time period is configurable by a potentiometer.
- Time period between two strobe activites is potentiometer controllable.
- Status LED for "push button active/ strobe can be enabled" and "strobe active".
- Automatic reset if microcontroller hang-up by watchdog timer within 0.125 seconds.

### Details
- Some options are configurable by **preprocessor defines**. Default values will be used if they are not given. Meaning of preprocessor defines can be found in source code comments.
- Program sequence is controlled by a **Finite State Machine**
    - Legend (0 = no/not required, 1 = yes/required, -1 = unspecified/don't care)
        - buzzer_enabled (output): push-button is active and can enable the strobe
        - relais_enabled (output): relais is active and powers the strobe
        - buzzer_pressed (input): push-button was pressed
        - timer_counting (input): timer is active (strobe active or buzzer locked and time is not over)
        - next_state: new state if all inputs met the current state
    - State transition matrix:

State | Name | buzzer_enabled | relais_enabled | buzzer_pressed | timer_counting | next_state
------|------|----------------|----------------|----------------|----------------|------------
S_0 | WAIT_FOR_KEYPRESS | 1 | 0 | 1 | -1 | BLINKING
S_1 | BLINKING | 0 | 1 | -1 | 0 | BUZZER_LOCKED
S_2 | BUZZER_LOCKED | 0 | 0 | -1 | 0 | WAIT_FOR_KEYPRESS

State transition diagram (arrows are labeled with inputs that are required for state change):
![](doc/FSM.png?raw=true)

- All **time periods** are counted  by 16-bit Timer/Counter 1
- **Push-putton** presses are detected by external interrupt INT0
- **Potentiometers** are sampled by analog/digital converter (ADC)
    - Sampling is conducted only once after each power on/reset. So the adjustment of potentiometers is not detected until next microcontroller reset/restart.
    - This program does not use the low noise mode while sampling potentiometer values. However for better sampling results an averaged value of several (configurable by preprocessor define) samplings is calculated. In addition for every potentiometer an additional sampling is done first to stabilize the ADC, the value of this conversion is thrown.
- The circuit should be **power**ed with 5 V (DC). In other cases maybe other and/or additional components are required. It's recommended to enable the brown-out detector to reset the microcontroller if the current falls below a certain level (maybe 4.3 V).

### Microcontroller connections
Legend:
- *Italic* pins are required for In-System-Programming (ISP) via SPI. Pins printed grey are only required for ISP.
- AREF is not connected but maybe will be used for more precise potentiometer values later. 
![](doc/microcontroller.png?raw=true)

### Circuit Diagram
![](doc/layout/circuit_diagram.png?raw=true)

### Conductor board example
![](doc/pictures/conductor_board_case.JPG?raw=true)
