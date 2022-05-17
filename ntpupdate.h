void setup_rtc(void)
{
rtc_time_t RTCtime;
rtc_date_t RTCdate;
char message[256];
tm localtm;
unsigned long thetime;
timeval tv;
timezone tz;
timezone tz_utc = {0,0};
  
  M5.RTC.getTime(&RTCtime);
  M5.RTC.getDate(&RTCdate);
  
  
  localtm.tm_sec  = RTCtime.sec;
  localtm.tm_min  = RTCtime.min;
  localtm.tm_hour = RTCtime.hour;
  localtm.tm_mday = RTCdate.day;
  localtm.tm_mon  = RTCdate.mon;
  localtm.tm_year = RTCdate.year;
  
  
  thetime = mktime(&localtm);  
  sprintf(message, "RTC Date: %02d/%02d/%02d Time: %02d:%02d:%02d (%i)",
            RTCdate.mon, RTCdate.day, RTCdate.year, 
            RTCtime.hour, RTCtime.min, RTCtime.sec,
            thetime);          
  tv.tv_sec = time_t(thetime);
  tz.tz_minuteswest = 0;
  tz.tz_dsttime = 0;
  settimeofday(&tv,&tz_utc);
  log(message);         
}

void set_rtc(struct tm *ttm)
{


rtc_time_t RTCtime;
rtc_date_t RTCdate;

 
  RTCtime.hour = ttm->tm_hour;
  RTCtime.min = ttm->tm_min;
  RTCtime.sec = ttm->tm_sec;
  M5.RTC.setTime(&RTCtime);

  RTCdate.year = ttm->tm_year + 1900;
  RTCdate.mon = ttm->tm_mon + 1;
  RTCdate.day = ttm->tm_mday;
  M5.RTC.setDate(&RTCdate);
}

//{"abbreviation":"CDT","client_ip":"174.80.45.95","datetime":"2022-05-10T21:18:24.928638-05:00","day_of_week":2,"day_of_year":130,"dst":true,
//"dst_from":"2022-03-13T08:00:00+00:00","dst_offset":3600,"dst_until":"2022-11-06T07:00:00+00:00","raw_offset":-21600,"timezone":"America/Chicago",
//"unixtime":1652235504,"utc_datetime":"2022-05-11T02:18:24.928638+00:00","utc_offset":"-05:00","week_number":19}
void process_time_update(void* optParm, AsyncHTTPSRequest* request, int readyState)
{
  time_t now;
  tm *localtm;
  (void) optParm;
  int tsize;
  char message[256];
  char *work;
  char *tptr;
  //char *fptr;
  char *eptr;
  String worldtime;
  StaticJsonDocument<1000> doc;
  long raw_offset;
  long dst_offset;
  timeval tv;
  timezone tz;
  timezone tz_utc = {0,0};

  if (readyState == readyStateDone)
  {
    //Serial.println(request->responseLongText());
    log("worldtimeapi responded");
    worldtime = request->responseText();
    tsize = worldtime.length();
    Serial.printf("Size = %d",tsize);
    if (tsize == 0){
       log("worldtimeapi empty");
       return;
       }
    if (tsize > 4096){
       log("worldtimeapi invalid response size");
       return;
       } 
    work = (char *)malloc(tsize+10);   
    worldtime.toCharArray(work,tsize);   
    tptr = strchr(work,'}');
    if (tptr == NULL){
      log("worldtimeapi: Missing close");
      strcat(work,"}");
      }
    Serial.print(work);
    DeserializationError error = deserializeJson(doc, work);
    if (error) {
       log("worldtimeapi bad json format"); 
       log(error.f_str());
       free(work);
       return;
       }
    linuxtime = doc["unixtime"];
    raw_offset = doc["raw_offset"];
    dst_offset = doc["dst_offset"];
    sprintf(message,"Linuxtime: %d Offset %d",linuxtime,raw_offset);
    log(message);
    linuxtime = linuxtime + raw_offset + dst_offset;
    now = (time_t)linuxtime;
    localtm = localtime(&now);
        
    sprintf(message,"Local Time: %s",asctime(localtm));
    log(message);       
    tv.tv_sec = time_t(linuxtime);
    tz.tz_minuteswest = 0;
    tz.tz_dsttime = 0;
    settimeofday(&tv,&tz_utc);
    set_rtc(localtm);    
    free(work);
    
   }
}

void start_time_update()
{
static char theurl[256];

    log("worldtimeapi update requested");
    strcpy(theurl,"https://worldtimeapi.org/api/ip.json");
    sendHttpRequest(theurl,process_time_update);
}


bool timerupdateneeded(void *ttimer)
{
    start_time_update();
    return(true);
}

void update_rtc(){
  log("Scheduluing Time Update");
  timer.in(1, timerupdateneeded);
}



        
