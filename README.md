# decode345
This project decodes the 345 Mhz signals used by Honeywell 5800 series wireless security system components.  See the [decode345 project writeup](https://denglend.github.io/decode345/) for more background and technical operation of the project.

decide345 consistens of two parts:

1. A gnuradio flowgraph/python script to capture the RF signal and pipe it to Part 2:
2. A decoder (python and C verison available) that decodes the Honeywell protocol and transmits it via MQTT to a destination (e.g. to a home automation system like OpenHAB).

###Requirements
Required Hardware:
- SDR dongle (e.g. RTL or HackRF).
- PC to process and decide the RF data (Raspberry Pi 3 is a good choice).  Only tested to work on Linux.
- [if desired] PC to run OpenHAB (can be the same PC as above, if desired).

Required Software (to run binaries):
- gnuradio
- mosquitto or other MQTT broker (if you want to use this functionality)
- OpenHAB (if you want to use this functionality)
- python (and paho.mqtt and crcmod libraries, if using python decoder)

Required Software (to build C code):
- gcc
- libmosquitto

###Compiling
To compile the C code, try `gcc decode345.c  -o decode345 -lmosquitto`

###Usage
You will have to run two separate components:
1. The receiver, which is just a gnuradio/grc flowgraph in python.  The gnuradio python script can be run with no command line options required `python receive345.py`.
2. The decoder, which comes in two varieties: python and C.

If using the python decoder, the options (MQTT broker address, device IDs, etc) are set directly within the decode345.py file, which can be executed by `python decode345.py`

If using the C decoder, the options are set on the command line, as follows:
```
Usage: ./decode345 [options]
 -d, --devicefile FILE	file for device IDs and names
 -f, --fifoname FILE	file for FIFO pipe
 -h, --help			show this message
 -H, --host			mosquitto host
 -p, --port			mosquitto port
 -v, --verbose		show verbose messages
 ```
Other notes:
- Verbose mode (off by default) is probably required if you do not know the IDs of your sensors (which you would have no way of knowing in advance).  In Verbose mode, a variety of informational messages will be displayed about each packet received.
- The **devicefile** is a lits of all devices you would like decode345 to recognize and transmit via MQTT.  Any devices not included in this file will be displayed as "Unknown Device" and will not be transmitted to the MQTT broker.
- The default **port** is 1883, the same as the MQQT default.
- The default **host** is localhost.
- The **fifoname** option allows you to select a different named pipe to use for the communication channel between the gnuradio script and the decoder.  By default /tmp/grcfifo is used.  If changed here, you will also have to change it in the gnuradio flowgraph.
- Both the C and python decoders will always transmit on the MQTT topic /security/[DEVICE NAME] and will send the message OPEN or CLOSED.
- Since the RF receiver (gnuradio component) and protocol decoder (decode345.py or decoder345 binary) use named pipes to communicate, it is normal that their startup will block until both are running.

###Verbose Output
If you run in verbose mode, you'll get output that looks like this:
```
Packet Received: 0xfffe849ea7800aad
 DeviceID: 0x849ea7
 Status: 0x80
 CRC: 0xaad
Device: Basement Door Open 
Mosquitto Sending: /security/Basement Door OPEN to 192.168.1.100:1883
```
You can use this output to find the DeviceID of unknown devices so that you can create your own devicefile.
You may also see some lines that look like this in the verbose output:
```
String length 79: _-_-_-_-_-_--_-_-_--_-_-_-_--__--_-_-_-__--_-_-__-_--_-_--_-_-_-_-_-_-_-_-_-_--_-_-__-_--_-__-_--_--_-__--_-_--_-_-_-_-_-
String length 7d: _-_-_-_--_-_-_-_-_-_-_-_-_-_--__--_-_-_-__--_-_-__-_--_-__--_-_-_-_-_-_-_-_-__-_--_-_-__-_--_-__-_--_--_-__--_-_--_-_-__-_-_-
```
These indicate incomplete received packets; complete packets are 0x80 in length.  The _'s and -'s indicate lows and highs in the received signal, for debugging purposes.

###devicefile Format
To translate between Device IDs and a human-readable name for each sensor, edit the devicelist dict in the decode345.py file (if using Python) or pass a devicefile (if using the C binary).
The devicefile format is one line per device, with each line starting with the device ID in hexadecimal, followed by whitespace and then the device name.  For example:
```
0x812345 Basement Door
0x8AB432 Basement Window 1
0x86543A Basement Window 2
```