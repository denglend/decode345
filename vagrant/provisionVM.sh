#!/bin/bash

apt-get update
#Install dependencies
apt-get install -y swig doxygen gnuradio librtlsdr0 gr-osmosdr libmosquitto-dev

#if using python version of decode345
apt-get install -y python-pip 
pip install paho-mqtt
pip install crcmod

#not sure if we need these or not...
#mosquitto mosquitto-clients python-mosquitto 

git clone https://github.com/denglend/decode345.git
cd decode345/src
echo "Compiling decode345.c"
gcc decode345.c -o decode345 -lmosquitto

#If VNC i
#apt-get install -y xfce4 xfce4-goodies tightvncserver

echo "To get started, run 'vagrant ssh' to SSH into the running VM."
echo "Then run 'sudo python2.7 ~/decode345/bin/receive345.py'."
echo "Next you'll need to determine your deviceIDs and create the devicefile. See: https://github.com/denglend/decode345 for more info." 
echo "In another TTY (run 'vagrant ssh' again from another window), run '~/decode345/src/decode345 -f /tmp/grcfifo -d <yourMappingFile>'"
echo "In a third TTY, run 'sudo mosquitto_sub -d -t /security/#'. You should see your sensor alerts appear."
