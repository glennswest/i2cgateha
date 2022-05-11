
void process_time_update(void* optParm, AsyncHTTPSRequest* request, int readyState)
{
  (void) optParm;
  int tsize;
  //char filename[64];
  char *work;
  char *tptr;
  //char *fptr;
  char *eptr;
  String worldtime;
  
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
    
    Serial.print(work);
    //request->responseText().toCharArray(work,tsize);
    //work[tsize] = 0; // Force Termination
    log("Hummmm");
    tptr = strstr(work,"unixtime");
    if (tptr == NULL){
       log("worldtimeapi invalid time response");
       free(work);
       return;
       }
    tptr = tptr + 10;
    eptr = strchr(tptr,'\n');
    if (eptr == NULL){
       log("worldtimeapi end not found");
       free(work);
       return;
       }     
    *eptr = 0;
   
    linuxtime = atoi(tptr);
    Serial.println(linuxtime);
    rtc_set_needed = 1;
    free(work);
    
   }
}

void start_time_update()
{
static char theurl[256];

    log("worldtimeapi update requested");
    strcpy(theurl,"https://worldtimeapi.org/api/ip.txt");
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
  timer.in(7000, timerupdateneeded);
}



        
