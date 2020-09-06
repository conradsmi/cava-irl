# cava-irl (Console Audio Visualizer for Alsa - In Real Life)
An audio visualizer for SMD5050 LEDs using Raspberry Pi (RPI).

### How it works
cava-irl (or "cirl" for short) utilizes cava's raw/fifo output mode to send waveform data to your RPI. The RPI uses this data to change the brightness of the LEDs accordingly. 

### Features
During execution, cirl allows users to...
* change the color of the LEDs,
* switch between different formulas used to calculate brightness,
* and change the visibility of contrast between low and high points in an audio

Most importantly, since cirl sends data over a network to the RPI, you can __control the LEDs via the internet__ so long as the RPI is running cirlservice.

## Installation
(to be written)
