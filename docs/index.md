---
title: Honeywell 345 Mhz Protocol
---

#Honeywell 345 Mhz Protocol

####[Part 1 - RF Capture & Demodulation](part1.md) 

####[Part 2 - Protocol](part2.md)

####[Part 3 - MQTT & OpenHab](part3.md)

####[Part 4 - Up Next](part4.md)

I've been assembling some home automation with [OpenHAB 2](http://www.openhab.org/).  I had a few existing devices that were supported by OpenHAB, and purchased a zwave devices to fit other needs I had (under sink water leak sensing, basement humidity sensing, etc).  But, looking through online zwave catalogs, it seemed like I probably exhausted the list of devices worth purchasing rather quickly.  The devices are not cheap, and the costs add up, especially if you need to buy multiples.  For example, door and window sensors cost around $40, and you likely have a lot of doors and windows.

Besides, who needs door/window sensors?  I certainly don't, but I realized I already have a system of door & window sensors, they're not just not zwave.  Every window and door in the basement or first floor of my house has a wireless sensor that transmits to my security system.  Why not integrate them into OpenHAB?

The sensors in question are from [Honeywell's 5800](https://www.security.honeywell.com/hsc/products/intruder-detection-systems/wireless/index.html) series of wireless products.  A google search for information reading info from those sensors turned up nothing.  There does appear to be a device ([AD2PI](http://www.alarmdecoder.com/wiki/index.php/Main_Page)) which can interface directly with the alarm system panel, but A) This would require much time and attention be spent on aesthetics, as the panel is in a visible place in my house; and B) it doesn't appear to support my panel anyway.

So, the purpose of this project is to decode the wireless signals from the Honeywell door/window sensors already installed in my house and then to relay this information to Openhab.  It also served to satiate my curiousity about how the sensors communicate with the panel.

I've presented the code as well as the process I used to accomplish this, in case it is ultimately useful to others.  [Part 1](part1.md) describes the process I used to decode the rf signal and convert it to a bytestream.  In [Part 2](part2.md), I decode the protocol used by the sensors.  Using MQTT, [Part 3](part3.md) describes the interface  to OpenHAB.  Finally, [Part 4](part4.md) describes where I'd like to go next, time permitting.

I've posted this documentation in case it's useful for anyone who has Honeywell sensors, but also to document the process I went through to identify, decode, understand, and ultimately use an unknown signal using an SDR device.  I started from zero experience with SDR and with RF in general, so I hope a few of the lessons I learned may be helpful for others who are themselves starting to explore.