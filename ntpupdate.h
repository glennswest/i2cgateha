const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = -21600;
const int   daylightOffset_sec = 3600;

static int time_data_needed = 0;

AsyncClient *client_tcp = new AsyncClient;

#define CST -21600

#define TO_UNIX_EPOCH 1062763952UL 
#define SECONDS_PER_YEAR  31556926

void onTimePacket(void *arg, AsyncClient *client, struct pbuf *pb)
{
unsigned int nist_network_time;
unsigned int nist_local_time;
unsigned char *timeout;
unsigned char *timein;

unsigned int ntptime;
char stime[64];
char message[256];
unsigned long nist_to_linux_time;
  
  nist_to_linux_time =  SECONDS_PER_YEAR * 70; 
  log("time.nist.gov responded");
  memcpy(&nist_network_time,(char *)&pb->payload,4);
  timein = (unsigned char *)&nist_network_time;
  timeout = (unsigned char *)&nist_local_time;

  *(timeout + 0) = *(timein+3);
  *(timeout + 1) = *(timein+2);
  *(timeout + 2) = *(timein+1);
  *(timeout + 3) = *(timein+0);

  //sprintf(message,"Nist Network Time: %lu(%4.4x)",nist_network_time,nist_network_time);
  //log(message);
  sprintf(message,"Nist Local Time: %lu(%4.4x)",nist_local_time,nist_local_time);
  log(message);

  ntptime = nist_local_time;  
 
  // Convert to Unix/Linux EPOCH
  ntptime = ntptime - nist_to_linux_time;
  sprintf(message,"Linux Epoch Time: %lu(%4.4x)",ntptime,ntptime);
  log(message);
   
  //ntptime = ntptime + CST;
  rtc.setTime(ntptime);
  
 
 rtc.getDateTime().toCharArray(stime,64);
 sprintf(message,"NTP Time: %s", stime);
 log(message);
 time_data_needed = 0;
}

void handleTimeData(void *arg, AsyncClient *client, void *data, size_t len)
{
unsigned int ntptime;
char stime[64];
char message[256];
  
  log("time.nist.gov responded");
  memcpy((char *)&ntptime,data,4);
 
  //sprintf(message,"NTP Epoch Time: %lu(%4.4x)",ntptime,ntptime);
  //log(message);
  // Convert to Unix/Linux EPOCH
  ntptime = ntptime + TO_UNIX_EPOCH;
  sprintf(message,"Linux Epoch Time: %lu(%4.4x)",ntptime,ntptime);
  log(message);
   
  //ntptime = ntptime + CST;
  rtc.setTime(ntptime);
  
 
 rtc.getDateTime().toCharArray(stime,64);
 sprintf(message,"NTP Time: %s", stime);
 log(message);
 time_data_needed = 0;
}

void onTimeConnect(void *arg, AsyncClient *client)
{
  log("time.nist.gov connected");
 
}


void onTimeDisconnect(void *arg, AsyncClient *client)
{
 int idx;
  char work[256];
  char message[256];
  unsigned long ntptime;
  char stime[64];
    
   log("time.nist.gov disconnected");
 
   //client_tcp->close();
   //if (time_data_needed == 1){
   //   log("time.nist.gov failed");
   //    client->connect("time.nist.gov", 37);
   //    }
}


void update_rtc(){
  time_data_needed = 1;
  //client_tcp->onData(handleTimeData, client_tcp);
  client_tcp->onPacket(onTimePacket, client_tcp);
  client_tcp->onConnect(onTimeConnect, client_tcp);
  client_tcp->onDisconnect(onTimeDisconnect, client_tcp);
  
  //client_tcp->connect("time.nist.gov", 13);
  client_tcp->connect("time.nist.gov", 37);
  client_tcp->close();
}

        
