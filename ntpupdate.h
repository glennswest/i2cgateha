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
    rtc.setTime(linuxtime);
    free(work);
    
   }
}

void start_time_update()
{
static char theurl[256];

    log("worldtimeapi update requested");
    strcpy(theurl,"https://worldtimeapi.org/api/ip.json");
    sendHttpRequest(theurl,process_time_update);
    //sendHttpRequest(theurl,requestCB);
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



        
