from __future__ import print_function
import os
import errno
import re
import paho.mqtt.client as mqtt
import crcmod



SETTINGS = {
	"Verbose":   		False,
    "ShowPulseChecks":	False,
    "Show192":  		False,
    "MQTTHostName": 	"127.0.0.1",
    "FIFOName":		"/tmp/grcfifo"
}


decimation = 10
sample_rate = 1e6
noiseceiling = 25						#anything below this is considered a 0 (60 orig value)
noisefloor = 50							#anything above this is considered a 1 (60 orig value)
Run1Len = sample_rate / 1e6 * 230 / decimation			#230 orig value
Run0Len = sample_rate / 1e6 * 100 / decimation			#100 orig value
RunErrLen = Run0Len						#100 orig value



Stats_High = [0]*10000
print("Opening FIFO...")
devicelist = {
	0x812345:"Example Device 1"
}

statuslist = {
1<<7: ["Closed ","Open "],
1<<6: ["","Tamper-Sensor "],
1<<5: ["","Bit5-0 "],
1<<4: ["","Bit4-0 "],
1<<3: ["","Battery-Low "],
1<<2: ["","Pulse-Check "],
1<<1: ["","Bit1-0 "],
1<<0: ["","Bit0-0 "]
}

client = mqtt.Client()
crcfunc = crcmod.predefined.mkCrcFun('crc-16-buypass')

try:
	os.mkfifo(SETTINGS["FIFOName"])
except OSError as oe: 
	if oe.errno != errno.EEXIST:
		raise

with open(SETTINGS["FIFOName"], "rb") as f:
	curstate=0		# 0 = waiting for bit .... 1 = mid-bit
	curlen=0		# length of current stream of symbols
	parsedmostrecent=0	# flag that end of transimission has been reached
	curstr=""		# current bitstream
	while True:
		data = f.read(1)
		if len(data) == 0:
			print("Writer closed")
			break
		if curstate == 0:
			if ord(data)>noisefloor:					#signals change of bitstate
				if curlen >=1000/2 and parsedmostrecent==0:		#end of bitstream
					curstr = re.sub(r'^_*','',curstr)		#remove potentially many extraneous leading lows
					curstr = "_"+curstr				#add back a single leading low (sync pattern always starts with a low)
					if len(curstr) == 127 or len(curstr) == 191:
						curstr = curstr + "_"			#add back an ending low if needed
					if len(curstr) == 192 and SETTINGS["Show192"]:
						print("String length: 192")		# temp decoding 192 bit strings
						bytes = [0,0,0,0,0,0,0,0,0,0,0,0]
						for curbyte in range(0,12):
							for curbit in range(0,8):
								bytes[curbyte] *= 2
								if curstr[curbyte*16+curbit*2:curbyte*16+curbit*2+2] == "_-": 
									bytes[curbyte] +=1
						print ('['+','.join(format(x, '02X') for x in bytes)+']')			
					elif len(curstr) != 128:
						if SETTINGS["Verbose"]:
							print("String length: "+str(len(curstr)))
							print(curstr)
					else:				
						bytes = [0,0,0,0,0,0,0,0]
						for curbyte in range(0,8):
							for curbit in range(0,8):
								bytes[curbyte] *= 2
								if curstr[curbyte*16+curbit*2:curbyte*16+curbit*2+2] == "_-": #low-high transition = 1
									bytes[curbyte] +=1						
						device = (bytes[2]<<16) + (bytes[3] <<8) +  bytes[4]
						status = bytes[5]
						#CRC Check
						crc = crcfunc("".join(map(chr,bytes[2:6])))
						if (crc & 0xFF) == bytes[7] and ((crc & 0xFF00)>>8) == bytes[6]:
							if device in devicelist:
								client.connect(SETTINGS["MQTTHostName"], 1883, 60)	
								client.publish("/security/"+devicelist[device],"OPEN" if 1<<7 & status >0 else "CLOSED")
								client.disconnect()
							if SETTINGS["ShowPulseChecks"] or (1<<2 & status) ==0: 
								if device in devicelist:
									print(devicelist[device],end=" ")
								else:
									print("Unknown Device "+format(device,'06X'),end=" ")
								for statusbit in statuslist:
									if (status & statusbit)==0:
										print(statuslist[statusbit][0],end="")
									else:
										print(statuslist[statusbit][1],end="")
								print ('['+','.join(format(x, '02X') for x in bytes)+']')
						elif SETTINGS["Verbose"]:
							# CRC Fail
							print("CRC Fail: ",end="")
							print ('['+','.join(format(x, '02X') for x in bytes)+']')
					curstr=""
					parsedmostrecent=1
					if SETTINGS["Verbose"]:
						for hc in range(0,10000):
							if Stats_High[hc]>0:print(str(hc)+": "+str(Stats_High[hc]),end=" ")
						print("")
				elif curlen>Run1Len:
					curstr +="__"
				elif curlen>Run0Len:
					curstr+="_"
				curstate = 1
				curlen = 1
			else:
				curlen += 1
		else:
			if ord(data)>noiseceiling:
				curlen += 1
			else:
				if curlen>Run1Len:
					curstr+="--"
					parsedmostrecent=0
				elif curlen>Run0Len:
					curstr+="-"
					parsedmostrecent=0
				elif curlen>RunErrLen:		#interpret as an error
					print("\n  short - "+str(curlen))
					curstr=""
				if curlen <= 10000 and SETTINGS["Verbose"]: Stats_High[curlen]+=1
				curlen=0
				curstate=0			#wait for next bit
