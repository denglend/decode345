#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <getopt.h>
#include <unistd.h>
#include <stdint.h>
#include <inttypes.h>
#include <mosquitto.h>

#define MAX_BUF 1024
#define MAX_PID 8
#define MAX_STRING_LENGTH 50
#define MAX_BITSTREAM_LEN 65536
#define DECIMATION  10
#define SAMPLE_RATE  1000000
#define NOISE_CEILING 25					//anything below this is considered a 0 (60 orig value)
#define NOISE_FLOOR 50						//anything above this is considered a 1 (60 orig value)
#define RUN_1_LEN SAMPLE_RATE / 1000000 * 230 / DECIMATION	//230 orig value
#define RUN_0_LEN SAMPLE_RATE / 1000000 * 100 / DECIMATION	//100 orig value
#define RUN_ERR_LEN RUN_0_LEN	

	/*
	/ To enable support within one file for multiple sensor types, set the "sensorModel" variable to the value that corresponds to the following models:
	/	0 = Honeywell 5811 
	/ 	1 = Honeywell 5816
	*/
	int sensorModel=0;
	const char** StatusList = NULL;
	const char* StatusList5811[] = {"Closed ","Open ","","Tamper-Sensor ","","Bit5-0 ","","Bit4-0 ","","Battery-Low ","","Pulse-Check ","","Bit1-0 ","","Bit0-0 "};
        /*
        / bit 0: Unknown (usually 0)
        / bit 1: Unknown (usually 0)
        / bit 2: Pulse-Check (1=true)
        / bit 3: Unknown (usually 0)
        / bit 4: Unknown (usually 0)
        / bit 5: Sensor Open/Closed (0=Door/Window Closed; 1=Door/Window Open)
        / bit 6: Tamper Sensor (0=cover on; 1=cover off [tampered])
        / bit 7: Unknown (always 1?)
        */
	const char* StatusList5816[] = {"","","Tamper-Sensor Closed "," Tamper-Sensor Open ","Entry Closed! ","Entry Open! ","","Bit4-0 ","","Battery-Low ","","Pulse-Check ","","Bit1-0 ","","Bit0-0 "};

	//Just setting defaults - these will be overridden later
	int OpenCloseBit=7;
	int TestBitAdjustment=0;

	uint32_t * DeviceIDs = NULL;
	char** DeviceNames = NULL;
	int NumDevices = 0;
	
	struct mosquitto* MQ = NULL;
	char* MQTT_Host = "127.0.0.1";
	int MQTT_Port = 1883;
	char* FIFOName = "/tmp/grcfifo";
	

void ParseByte(unsigned char Val);
void prepend(char* s, const char* t);
uint16_t gen_crc16(const uint8_t *data, uint16_t size);
void MQ_Callback(struct mosquitto * MQ, void * obj, int MessageID);
static int Verbose = 0;

int main (int argc, char **argv)
{
	int fd;
	int bytesread;
	unsigned char buf[MAX_BUF];
	mosquitto_lib_init();
	MQ = mosquitto_new(NULL,true,NULL);
	mosquitto_loop_start(MQ);
	int c,CurChar,LineCount=0,DeviceNameLen=0,DeviceNameMaxLen=0,i;
	while (1) {
      		static struct option long_options[] =
        	{
          		/* These options set a flag. */
          		{"verbose",	no_argument,		0,	'v'},
			{"host",	required_argument,	0,	'H'},
			{"port",	required_argument,	0,	'p'},
			{"help",	no_argument,		0,	'h'},
			{"devicefile",	required_argument,	0,	'd'},
			{"fifoname",	required_argument,	0,	'f'},
			{0, 		0, 			0, 	0}
		};
		int option_index = 0;
		c = getopt_long (argc, argv, "H:p:d:vhf:",long_options, &option_index);

		if (c == -1) break;

 		switch (c) {
        case 0:
		break;

        case 'H':
		MQTT_Host = optarg;
		break;

        case 'p':
		MQTT_Port = strtol(optarg,NULL,10);
		break;

        case 'd':
		;FILE *dfp = fopen(optarg,"r");
		if (dfp == NULL) {
			printf("Unable to open device file\n");
			exit(1);
		}
		do {
			CurChar = fgetc(dfp);
			if (CurChar == '\n') {
				if (DeviceNameLen>DeviceNameMaxLen) DeviceNameMaxLen = DeviceNameLen;
				DeviceNameLen = 0;
				LineCount++;
			}
			else DeviceNameLen++;
		} while(CurChar != EOF);
		DeviceNames = malloc(LineCount*sizeof(char*));
		DeviceIDs = malloc(sizeof(uint32_t)*LineCount);
		rewind(dfp);
		for (i=0;i<LineCount;i++) {
			DeviceNames[i]= malloc(DeviceNameMaxLen+1);
			fscanf(dfp,"%x %[^\n]",&DeviceIDs[i],DeviceNames[i]);
		}
		NumDevices = LineCount;
		break;
	case 'v':
		Verbose = 1;
		break;
	case 'f':
		FIFOName = optarg;
		break;
	case 'h':
		printf("Usage: %s [options]\n",argv[0]);
		printf(	" -d, --devicefile FILE\tfile for device IDs and names\n"
			" -f, --fifoname FILE\tfile for FIFO pipe\n"
			" -h, --help\t\tshow this message\n"
			" -H, --host\t\tmosquitto host\n"
			" -p, --port\t\tmosquitto port\n"
			" -v, --verbose\t\tshow verbose messages\n");
		return 0; 

        case '?':
 		break;

        default:
		abort ();
        }
}
        switch (sensorModel) {
		case 0:
			StatusList = StatusList5811;
			//Set this to the bit that's flipped to 1 if the door/window is open
			OpenCloseBit = 7;
			break;
		case 1:
			StatusList = StatusList5816;
			//Set this to the bit that's flipped to 1 if the door/window is open
			OpenCloseBit = 5;
			//If the 7th bit is always/usually 1, set this to 1.
			TestBitAdjustment = 1;
                        break;
		default:
			printf("Please specify a valid sensor model");
			abort();
        }
	
	if (Verbose) {
		printf("# of Devices: %d\n",NumDevices);
		for(i=0;i<NumDevices;i++) printf(" Device %d (0x%x): %s\n",i,DeviceIDs[i],DeviceNames[i]);
	}
	mosquitto_connect_async(MQ,MQTT_Host,MQTT_Port,1);
	mkfifo(FIFOName,DEFFILEMODE);
	printf("Waiting for FIFO data...\n");
	fd = open( FIFOName, O_RDONLY );
	printf("Receiving data...\n");
	while( 1 )
	{
		if((bytesread = read( fd, buf, MAX_BUF - 1)) > 0)
		{
			buf[bytesread] = '\0';
			for(i=0;i<bytesread;i++) {
				ParseByte(buf[i]);
			}
		}
		else break;
	}
	printf("FIFO Closed\n");
	close(fd);
	return 0;
}

void ParseByte(unsigned char Val) {
	static int CurState=0;				//0 = waiting for bit .... 1 = mid-bit
	static int CurLen=0;				// length of current stream of symbols
	static int ParsedMostRecent=0;			// flag that end of transimission has been reached
	static char CurStr[MAX_BITSTREAM_LEN]="";		// current bitstream
	int CurBit,CurByte;

	if (CurState ==0) {
		
		if (Val > NOISE_FLOOR) {
			if (CurLen >= 1000/2 && ParsedMostRecent ==0) {
				int i=0;
				while (CurStr[i] =='_') i++;
				strcpy(CurStr,CurStr+i);			//Remove potentially many extraneous leading lows
				prepend(CurStr,"_");				//Add back a single leading low (sync pattern always starts with one)
				if (strlen(CurStr) == 127 || strlen(CurStr) == 191) {
					strcat(CurStr, "_");			//add back an ending low if needed
					if (Verbose) printf("len %x: %s\n",(int)strlen(CurStr),CurStr);
				}
				if (strlen(CurStr) >50 && strlen(CurStr) !=128) {
					if (Verbose) printf("String length %x: %s\n",(int)strlen(CurStr),CurStr);
				}
				else if(strlen(CurStr) ==128) {		
					uint64_t CurPacket=0;		
					for(CurByte=0;CurByte<8;CurByte++) {
						for(CurBit=0;CurBit<8;CurBit++) {
							CurPacket = CurPacket<<1;
							if (CurStr[CurByte*16+CurBit*2]=='_' && CurStr[CurByte*16+CurBit*2+1] == '-') CurPacket += 1;//(uint64_t) 1<<63;
						}
					}
					
					uint32_t DeviceID = (uint32_t) ((CurPacket & 0x0000FFFFFF000000)>>24);
					uint8_t Status = (uint8_t) ((CurPacket & 0x0000000000FF0000)>>16);
					uint16_t CRC = (uint16_t) CurPacket & 0xFFFF;
					if (Verbose) {
						printf("Packet Received: 0x%" PRIx64 "\n", CurPacket);					
						printf(" DeviceID: 0x%x\n",DeviceID);
						printf(" Status: 0x%x\n",Status);
						printf(" CRC: 0x%x\n",CRC);
					}
					uint8_t CRCBuffer[] = {CurPacket>>40 & 0xFF,CurPacket>>32 & 0xFF,CurPacket>>24 & 0xFF,CurPacket>>16 & 0xFF};
					int i,j;
					//for (i=sizeof(DeviceIDs)/sizeof(DeviceIDs[0])-1;i>=0;i--) if (DeviceIDs[i] == DeviceID) break;
					if(NumDevices > 0) {
						for (i=NumDevices;i>=0;i--) if (DeviceIDs[i] == DeviceID) break;
					}
					if (gen_crc16(CRCBuffer, 4) != (uint16_t) CurPacket & 0xFFFF) {
						if (Verbose) printf("CRC Fail: 0x%" PRIx64 "\n",CurPacket);
					}
					else if (i>=0 && NumDevices > 0) {
						printf("Device: %s ",DeviceNames[i]);
						for (j=0;j<8;j++) printf("%s",StatusList[(7-j)*2+((1<<j & Status )>0+TestBitAdjustment ? 1 : 0)]);
						printf("\n");
						char Topic[MAX_STRING_LENGTH] = "/security/";
						strcat(Topic,DeviceNames[i]);
						char Payload[7] = "CLOSED";
						if (Status & 1<<OpenCloseBit) strcpy(Payload,"OPEN");
						if (Verbose) printf("Mosquitto Sending: %s %s to %s:%d\n",Topic,Payload,MQTT_Host,MQTT_Port);
						mosquitto_publish_callback_set(MQ,MQ_Callback);
						mosquitto_publish(MQ,NULL,Topic,strlen(Payload),Payload,2,false);
					}
					else { 
						printf("Unknown Device: 0x%x ",DeviceID);
						for (i=0;i<8;i++) printf("%s",StatusList[(7-i)*2+((1<<i & Status )>0 ? 1 : 0)]);
						printf("\n");
					}
				}
				strcpy(CurStr,"");
				ParsedMostRecent=1;
				
			}
			else if ( CurLen>RUN_1_LEN) {
				strcat(CurStr,"__");
			}
			else if (CurLen>RUN_0_LEN) {
				strcat(CurStr,"_");
			}
			CurState = 1;
			CurLen = 1;
		}
		else {
			CurLen += 1;
		}
	}
	else {   						//CurState ==1
		if (Val> NOISE_CEILING) {
			CurLen += 1;
		}
		else {
			if (CurLen > RUN_1_LEN) {
				strcat(CurStr,"--");
				ParsedMostRecent=0;
			}
			else if (CurLen>RUN_0_LEN) {
				strcat(CurStr,"-");
				ParsedMostRecent=0;
			}
			else if (CurLen>RUN_ERR_LEN) {		//interpret as an error
				strcpy(CurStr,"");
			}
			CurLen=0;
			CurState=0;				//wait for next bit
		}	
	}
}

void MQ_Callback(struct mosquitto * MQ, void * obj, int MessageID) {
	//Not disconnecting b/w messages so nothing to do here
}

void prepend(char* s, const char* t)
{
    size_t len = strlen(t);
    size_t i;

    memmove(s + len, s, strlen(s) + 1);

    for (i = 0; i < len; ++i)
    {
        s[i] = t[i];
    }
}

#define CRC16 0x8005

uint16_t gen_crc16(const uint8_t *data, uint16_t size)
{
//Adapted from code by Michael Burr at stackoverflow.com/questions/10564491/function-to-calculate-a-crc16-checksum
    uint16_t out = 0;
    int bits_read = 7; //0;
	int bit_flag;

    /* Sanity check: */
    if(data == NULL)
        return 0;

    while(size > 0)
    {
        bit_flag = out >> 15;

        /* Get next bit: */
        out <<= 1;
        out |= (*data >> bits_read) & 1; 
        /* Increment bit counter: */
        bits_read--; //++;
        if(bits_read <0) //> 7)
        {
            bits_read = 7; //0;
            data++;
            size--;
        }

        /* Cycle check: */
        if(bit_flag)
            out ^= CRC16;

    }

    // item b) "push out" the last 16 bits
    int i;
    for (i = 0; i < 16; ++i) {
        bit_flag = out >> 15;
        out <<= 1;
        if(bit_flag)
            out ^= CRC16;
    }


    return out;
}
