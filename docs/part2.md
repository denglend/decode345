---
---
# Protocol
There were two seperate steps in decoding the incoming stream of data from gnuradio.  First, the "raw" bytes needed to be converted to bits, and then the protocol used by the sensors needed to be decoded.

### The Bitstream
Looking at the recorded FFT with inspectrum (see [Part 1](Part1.md)), it was clear that short vs long pulses were used to encode the data.

#### Pulse Length
While I thought I should be able to mathematically calculate the length of long and short pulses from the time scale in the inpsectrum FFT waterfall, I decided that would just add to debugging time and instead looked empirically at the length of the pulses to find the threshold that distinguished a short vs long pulse.
I used a short python script to display the length of pulses.

```
from __future__ import print_function
import os

FIFO = '/tmp/grcfifo'
os.mkfifo(FIFO)

zerocount = 0
onecount = 0
noiseceiling = 80
with open(FIFO, "rb") as f:
	while True:
		data = f.read(1)
		if len(data) == 0:
			print("Pipe closed")
			break
		if ord(data)<= noiseceiling:			# Below ceiling = a low or background noise
			zerocount += 1						# Track the length of the low pulse in # of samples
			if onecount >0:						# If transitioning from a high...
				print(" 1x"+str(onecount),end=" ")	#...disp length of high pulse
				onecount=0
		if ord(data) > noiseceiling:			# A high pulse
			onecount += 1
			if zerocount>0:
				print(" 0x"+str(zerocount),end=" "), disp length of low pulse
				zerocount=0
```

I found that for a sample rate of 1MHz and low pass decimation of 10, a short pulse was in the ballpark of 100 samples long, and a long pulse in the ballpark of 200.

#### The Incorrect Bit Encoding
I first made the mistake of assuming that the data was encoded in the width of the high pulse.  In other words, a short high pulse represents a 0 bit and long high pulse represents a 1.  I wrote a decoder in python based on that theory that yielded strings like

```
..............xx...x.x....xxx...x.......x..x..xx..x
..............xx...x.x....xxx....x..........xxxx.x.
..............xx...x.x....xxx....x....xx..xxxx..x
```

Where . is a short high and x is a long high.

This actually worked well enough to identify events.  The top event was the closing of my basement door, the second is the opening of the basement door, and the third was triggering the tamper sensor.  The decoded stream was consistent between multiple triggerings of the same event on the same sensor, so could have been usable as a "decoder" by triggering every event and recording its decoded string.
But it was clear that something was wrong because the messages were different lengths and not a multiple of 8.  Additionally, while it did look like there was a bit flip that indicated open vs closed or tampered vs not, it wasn't in a consistent bit position across different sensors.  So, in this form, I wasn't able to decode an unknown signal and I had no interest in triggering and recording every posible device state of every sensor in my house.

#### Bit Encoding: Take 2
Convinced that I made a mistake somewhere I took another look at the waveform and noticed that not only were the highs different lengths, so were the lows.  So I reworked the python script to record the length of the lows in addition to the highs.
This yielded strings like:

```
-_-_-_-_-_-_-_-_-_-_-_-_-_-_--__--_-_-_-__--_-__--_-__-_-_-_--__--__--_-__-_-_--_-_-_-_-_-_-_-__--_-_-__--_-__-_--__--__-_-_--


-_-_-_-_-_-_-_-_-_-_-_-_-_-_--__--_-_-_-__--_-__--_-__-_-_-_--__--__--_-__-_-_-_--_-_-_-_-_-_-_-_-_-_-__--__--__--__--__-_--__-
```
Where _ are lows and - are highs (doubled to indicate a long low or high)

This made a lot more sense.  Now the bit streams were nearly a sensible 128 symbols long (126 in the top example, 127 in the latter), and after a realization that an initial or terminal low would get lost in the background noise and preceeded or followed it (and therefore required manually prepending or appending), I got a consistent 128 symbol stream.

Since there were never more than two highs or lows consecutively, I assumed the transition between low and high encoded the bits.  Apparently encoding data in the low/high transition is a very common scheme (see [Manchester Encoding](https://en.wikipedia.org/wiki/Manchester_code)).  But it was news to me.

Since I was never going to deal with the data outside of RF, it didn't really matter whether I mapped low-high to 0 or to 1, so I decided on low-high as 0 and high-low as 1.  Later on in the process, based on the CRC polynomial, I figured  I probably had it backwards and ended up rewriting everything the other way around.

In other words, in the streams above \_- represents a one bit, and -\_ represents a zero bit.  With that in place, a little utility code converted the incoming RF data into 8-byte packets.

###The Bytestream Protocol

####Packet structure
Decoding the 8-byte packets was thankfully the simplest part of the project.  The packets look like:

|0|1|2|3|4|5|6|7|
|--|--|--|--|--|--|--|--|
|Sync|Sync2?|Device ID|Device ID|Device ID|Status|CRC|CRC|

 - **Sync** - The first byte is clearly a sync byte, as it contains nothing but a series of low-high transitions, which translates to 0xFF. 
 - **Sync2** - The second byte is very similar, but ends in a high-low (0xFE).  So perhaps the second byte actually encodes something, or perhaps the high-low is just an indicator that the payload is to follow. In either case, I ignore the contents of Sync and Sync2 in the code, instead opting to check for a stream of 64 bits as an indication of data.  If I were more enterprising, I would have used the sync bits to detect the clock frequency instead of hardcoding it.  But the decoding worked just fine without that extra step, so I didn't bother.
 - **Device ID** - The next three bytes encode the ID of the sensor.  The Device IDs don't seem to be completely random: all of mine have 08 as the first nibble, with the remaining 2.5 bytes looking pretty random.  Perhaps the device type is encoded as well?  Or perhaps it's just a sequential ID counter and all my devices were produced around the same time.  I don't have any other types of devices (motion sensors, etc.) or devices purchased at different time to check.
 - **Status** - Following the Device ID is a single status byte, which indicates the state of the sensor as well as the reason for the transmission.
 - **CRC** - Finally, the last two bytes are a CRC.  I assumed they were a CRC because they were consistent between all transmisisons that had six identical previous bytes, but their content seemed unpredictable otherwise.  

#### Status states
The status bits indicate the state of the different subsensors as well as whether the transmission was caused by a change in state or is just a periodic transmission.

| Bit | Status |
|--------|--------|
|0|Unused?|
|1|Unused?|
|2|Pulse Check|
|3|Battery|
|4|Unused?|
|5|Unknown|
|6|Tamper Sensor|
|7|Open/Closed State|

 - **Pulse Check** - The sensor will periodically transmit its state even if no event (e.g. door open) was triggered.  I presume this is an anti-jamming measure, or perhaps just a reliability tool, in case a sensor dies unexpectedly.  If Bit 2 is set, then the transmission is periodic, if Bit 2 is reset then the transmission was caused by a specific event (AKA a change in state of one of the following bits).
 - **Battery** - If Bit 3 is set, the sensor is in low battery state.  
 - **Tamper Sensor** - If Bit 6 is set, the tamper sensor has been triggered.  In other words, the case has been removed from the sensor.
 - **Open/Closed State** - If Bit 7 is set, the sensor is in the "open" state.  If Bit 7 is reset, the sensor is the "closed" state.
 - **Bit 5 & Unknown Bits** - There are four other bits remaining, which are typically all reset. Bit 5 I have seen reported as set on a real device, but it appeared to be a device in a neighboring house, so I don't know what the cause was.  I haven't seen bits 0, 1, or 4 set.  I would guess that these bits might be used for other types of sensors, and especially sensors that have multiple triggers (e.g. a dry contact sensor with two inputs).


#### CRC
I had some trouble determing the exact CRC parameters, until I tried flipping all the bits, which is how I ultimately decided \_- likely is intended as a 1 rather than vice versa.
[RevEng](http://reveng.sourceforge.net/) was incredibly helpful for identifying the CRC algorithm used, which in this case was CRC-16/BUYPASS.  I then used the [crcmod](http://crcmod.sourceforge.net/) library to generate CRCs to check packet integrity on the receiving end, and discard any packets with incorrect CRCs.
I also found this [CRC Calculator](http://www.sunshine2k.de/coding/javascript/crc/crc_js.html) helpful while verifying that I had interpreted the CRC parameters correctly.

I do get a fair amount of packets that fail CRC.  These seem all to be for devices that are located outside my home.  For example, I'll get 3 successful packets for an unknown device, and the 3 CRC failures.  So the device is probably right at the end of my receiveable range.  I probably could tweak the noise floor/ceiling part of my code and more reliably receive these packets sucessfully, but I have no need to receive packets originating outside of my house so I haven't tried.

#### Other mysteries
There does seem to be some other type of transmission that happens on the same frequency and using the same bitstream protocol.  I'll regularly see 12-byte packets decoded (instead of the normal 8-byte).  They have a similar set of sync bytes, though there appear to be three of them: 0xFFFFE instead of  0xFFFE.  The packets repeat consistently for the same 6 repetitions within a single transmisison, just like the Honeywell sensors, but otherwise there is nothing recognizable about the data, and the data doesn't seem to repeat between different transmissions.  So it could be a rolling code of some kind, but not much seems to use 344.94MHz, so I have no idea what device it would be.  One possible explanation is that the signal is not actually at 344.94MHz, but is close enough that I'm picking it up anyway through the lowpass filter.

<-\- [Part 1 (RF Capture)](part1.md)

[Part 3 (MQTT and OpenHAB)](part3.md) -\->

VV [Code](https://github.com/denglend/decode345) VV