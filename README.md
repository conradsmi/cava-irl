# cava-irl (Console Audio Visualizer for Alsa - In Real Life)
An audio visualizer for SMD5050 LEDs using Raspberry Pi (RPI).

### How it works
cava-irl (or "cirl" for short) utilizes cava's raw/fifo output mode to send raw waveform data to your RPI. The RPI uses this data to change the brightness of the LEDs accordingly. 

### Features
During execution, cirl allows users to...
* change the color of the LEDs,
* switch between different formulas used to calculate brightness,
* and change the visibility of contrast between low and high points in an audio.

Most importantly, since cirl sends data over a network to the RPI, you can __control the LEDs via the internet__ so long as the RPI is running cirlservice.

## Installation
__NOTE:__ This build is in an experimental state. Expect some bugs.

1. Firstly, make sure you read this guide (in its entirety) on how to set up the hardware required: https://dordnung.de/raspberrypi-ledstrip/ (although the author forgot to mention it, you will also need some adapters to connect the male jumper cables to the LED, like this: https://www.amazon.com/iCreating-Connector-Conductor-Controller-Solderless/dp/B074G48LWQ)

1. (to be continued)
