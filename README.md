# cava-irl (Console Audio Visualizer for Alsa - In Real Life)

An audio visualizer for SMD5050 LEDs using Raspberry Pi (RPI).

![cava-irl demo](https://github.com/conradsmi/cava-irl/blob/master/cava-irl.gif "cava-irl demo")

## How it works

*cava-irl* (or *cirl* for short) utilizes [cava's](https://github.com/karlstav/cava) raw/fifo output mode to send raw waveform data to your RPI. The RPI uses this data to change the brightness of the LEDs accordingly.

## Features

During execution, cirl allows users to...

* change the color of the LEDs,
* switch between different formulas used to calculate brightness,
* and change the visibility of contrast between low and high points in an audio.

Most importantly, since cirl sends data over a network to the RPI, you can __control the LEDs via the internet__ so long as the RPI is running cirlserv.

## Installation

__NOTE:__ This build is in an experimental state; expect some bugs and frequent updates.

1. Firstly, make sure you read [this guide](https://dordnung.de/raspberrypi-ledstrip/) (in its entirety) on how to set up the hardware required (although the author forgot to mention it, you will also need some adapters to connect the male jumper cables to the LED, like [these](https://www.amazon.com/iCreating-Connector-Conductor-Controller-Solderless/dp/B074G48LWQ))

1. Naturally, you'll also need to [install cava and its prerequisites](https://github.com/karlstav/cava/#Installing) (which cava should handle automatically when you build it). You won't need cava on your RPI (unless you want to use it as both the client and the server; see details below).

1. Finally, [pigpiod](http://abyz.me.uk/rpi/pigpio/download.html) must be installed and configured on your RPI.

1. Clone this repository into your desired folder:

    `git clone https://github.com/conradsmi/cava-irl`

   You'll build either "cirl" (if installing on the machine with cava) or "cirlserv" (if installing on your RPI).

    `cd cava-irl`\
    `make cirl` &nbsp; &nbsp; &nbsp; &nbsp; `#or "make cirlserv"`

   Finally, execute the install script (which just moves the executable to /usr/local/bin).

    `make install_cirl` &nbsp; &nbsp; &nbsp; &nbsp; `#or "make install_cirlserv"`

## Usage

### cirl

A configuration file will be made available in the near future. For now, cirl requires at *least* two arguments:

* -i (num), where (num) is the ip address of your RPI, and
* -f (path/to/fifo), where (path/to/fifo) is the path to the fifo file that cava should output to.

Additionally, if cava's default config file is not the one you want to use for cirl, you also need to add:

* -c (path/to/config), where (path/to/config) is the path to the config file that cava should use for raw/fifo mode.

Example usage:

`cirl -c ~/.config/cava/config_fifo -i 192.168.1.3 -f ~/Music/cava/cava_fifo`

### cirlserv

cirlserv is much simpler. Firstly, ensure that the pigpio daemon is running by using `sudo pigpiod`. Then, simply execute `cirlserv` on your RPI and it will handle connections automatically. Note that it will only handle one connection at a time. Once a client disconnects, another one can reconnect without any additional commands on the RPI; thus, you can make pigpiod and cirlserv run everytime you boot your RPI, allowing you to use cirl without having to ssh to the pi or otherwise using its terminal.

### Notice on using the RPI for dual-purpose cirl

You can use your pi as both the cirl client and the cirl server. Just execute `cirl -i 127.0.0.1` with any other required options, which connects cirl to the server via localhost (assuming pigpiod and cirlserv are already running, of course).

## In-execution commands

cirl allows the user to change multiple properties during its execution via the provided command line. This is the help menu that displays when `h` is entered:

```txt
cava-irl commands:
------------------
Change color: "c (num) (num) (num)" (must be RGB value)

Change constraster: "a (num)" (between 0 and 5)
   - Makes higher peaks brighter, lower peaks dimmer

Toggle sigmoid mode: "s (0 or 1)"
   - Changes the way brightness is calculated. See the github page for details // to be written

Show this help message: "h"

Exit cava-irl: "q"
   - NOTE: This does not exit cirlservice; you must turn it off manually within the pi
```

More options, like offering a gradient mode, will be implemented in the future.

## Cava configuration file tips

This repository includes an example cava configuration file tailored for cirl. You can still alter it to your liking, but make sure NOT to change these fields:

* "method = raw" - cava obviously will not output to the fifo file otherwise
* "data_format = ascii"
* "bar_delimiter = 32"

Changes to these fields are untested and may lead to errors:

* "ascii_max_range = 1000" - Note that any value lower than the default (1000) should be okay, but you will lose some precision in the resulting brightness and lower values do not affect performance

Changes to any other fields not specified should probably be okay.

## Regarding Windows

cava does not offer true windows support, and thus, neither does cirl. However, theoretically, both programs should run quite nicely in WSL (Windows Subsystem for Linux). Unfortunately, it's apparently not that easy: __WSL does *not* come with built-in audio support.__ As you can imagine, audio support is pretty important for an audio visualizer.

On the brighter side, there is supposedly a way to jerryrig PulseAudio so that it works in WSL, which cava can then use for its audio input. I am looking into how this can be accomplished, and if successful, I will post the full Windows installation tutorial here. In the meantime, you'll have to use some unix-based environment. (Honestly, it would probably be much easier to switch from Windows to Linux *entirely* than to get audio support enabled in WSL).
