void download_done(void *optParm, AsyncHTTPSRequest *request, int readyState);
void download_data(void *xo, AsyncHTTPSRequest *request, int available);
void sendDownloadReq(char *theURL,void (*stateCB)(void* optParm, AsyncHTTPSRequest* request, int readyState),void (*dataCB)(void* optParm, AsyncHTTPSRequest* request, int readyState));

int getlocalversion()
{
char versionstr[32];
int theversion;
FsFile vfile;

	vfile.open(".version",O_RDWR);
  if (vfile){
     vfile.read((uint8_t * )versionstr,31);
     vfile.close();
     //log(versionstr);
     } else {
     strcpy(versionstr,"-1");
     }
  theversion = atoi(versionstr);
  return(theversion);

}

void sendHttpRequest(char *theURL,void (*pFunc)(void* optParm, AsyncHTTPSRequest* request, int readyState))
{
  static bool requestOpenResult;
  if (request.readyState() == readyStateUnsent || request.readyState() == readyStateDone)
  {
    request.setTimeout(100000);
    request.onReadyStateChange(pFunc);
    request.setDebug(true);
    requestOpenResult = request.open("GET", theURL);
    
    if (requestOpenResult)
    {
      // Only send() if open() returns true, or crash
      //Serial.println("Sending request");
      //Serial.println(theURL);
      request.send();
    }
    else
    {
      log("Can't send bad request");
      log(theURL);
    }
  }
  else
  {
    log("Can't send request");
    log(theURL);
  }
}

void requestCB(void* optParm, AsyncHTTPSRequest* request, int readyState)
{
  (void) optParm;
  
  if (readyState == readyStateDone)
  {
    Serial.println("\n**************************************");
    Serial.println(request->responseLongText());
    Serial.println("**************************************");

    request->setDebug(false);
  }
}

void add_download(char *filename)
{
struct content_entry *dl;

    if (strlen(filename) == 0){
       log("Ignoring empty filename");
       return;
       }
    dl = (struct content_entry *)malloc(sizeof(struct content_entry));
    if (dl == NULL){
         log("Malloc Failed for add_download");
         return;
         }
    strcpy(dl->filename,filename);
    dl->state = DL_STATE_DOWNLOAD_NEEDED;
    queue(&dl_q,&dl->qe);
}

void start_downloading()
{
char theurl[256];
char dirpath[256];

char message[256];

    
    cur_dl = (struct content_entry *)dl_q.tail;
    if (cur_dl == NULL){
       log("No Downloads");
       return;
       }
    if (cur_dl->state != DL_STATE_DOWNLOAD_NEEDED){
       log("Wrong Download State to start");
       return;
       }
    sprintf(message,"Starting download for: %s\n",cur_dl->filename);
    log(message);   
    strcpy(theurl,"https://raw.githubusercontent.com/glennswest/i2cgateha/main/contents/");
    strcat(theurl,cur_dl->filename);
    //strcpy(dirpath,cur_dl->filename);
    
     
    if (sd.exists(cur_dl->filename)){
        sprintf(message,"Removing %s",cur_dl->filename);
        log(message); 
        sd.remove(cur_dl->filename);
        log("Remove done");
        }
   
    log("Opening file");
    
    if (dfile.open(cur_dl->filename, O_WRONLY | O_CREAT)){
       log("File Open");
       } else {
       sprintf(message,"Open Failed: %s(%d)",cur_dl->filename,strlen(cur_dl->filename));
       log(message);
       return;
       }
 
    log("Send Download Request");
    sendDownloadReq(theurl,download_done,download_data);
}

void download_done(void *optParm, AsyncHTTPSRequest *request, int readyState)
{
char message[256];
int remain_length;

   sprintf(message,"Download done callback %s - state %d\n",cur_dl->filename,readyState);
   Serial.print(message);
   if (readyState == readyStateDone){
      remain_length = request->available();
      sprintf(message,"Download complete: %s - Remainer: %d\n", cur_dl->filename,remain_length);
      Serial.print(message);
      
      dfile.write((uint8_t *)request->responseLongText(),remain_length);
      dfile.close();
      if (cur_dl == (struct content_entry *)dl_q.tail){
         unqueue(&dl_q); 
         }
      start_downloading();
      }
}

void download_data(void *xo, AsyncHTTPSRequest *request, int available)
{
char message[128];

    //Serial.print("download_data\n");
    if (available > 0){
       //sprintf(message,"Downloading %s - %d bytes",cur_dl->filename,available);
       //Serial.print(message);
       dfile.write((uint8_t *)request->responseLongText(),available);
       }

}


void get_content_list(void* optParm, AsyncHTTPSRequest* request, int readyState)
{
  (void) optParm;
  int tsize;
  char filename[64];
  char work[2048];
  char *fptr;
  char *eptr;

  
  if (readyState == readyStateDone)
  {
    Serial.print("Got content list\n");
    strcpy(work,request->responseLongText());
    
    tsize = strlen(work);
    Serial.printf("Size = %d\n\r",tsize);
    fptr = work;
    eptr = strchr(fptr,0x0A);
    if (eptr == NULL){
       log("No Data");
       return;
       }
    while(eptr != NULL){
      *eptr = 0;
      eptr++;
      strcpy(filename,fptr);
      Serial.printf("Filename: %s\n\r",filename);
      add_download(filename);
      fptr = eptr;
      eptr = strchr(fptr,0x0A);
      }
   strcpy(filename,fptr);
   Serial.printf("Filename: %s\n\r",filename);
   add_download(filename);
   start_downloading();
   }
}


void start_content_update()
{
    log("Getting List of Updated Content");
    sendHttpRequest("https://raw.githubusercontent.com/glennswest/i2cgateha/main/contents/.content",get_content_list);
    //sendHttpRequest("https://raw.githubusercontent.com/glennswest/i2cgateha/main/contents/.content",requestCB);
}

void remote_version_check(void* optParm, AsyncHTTPSRequest* request, int readyState)
{
  (void) optParm;
  char message[256];

  
  if (readyState == readyStateDone)
  {
    remote_version = request->responseText().toInt();
    
    sprintf(message,"Remote Content Version: %d",remote_version);
    log(message);
    request->setDebug(false);
    if (local_version < remote_version){
       log("Content Update Needed - Starting");
       start_content_update();
       }
    }
}

// Start the content update process
void content_check()
{
char message[256];
    
    log("Getting Remote Version");
    sendHttpRequest("https://raw.githubusercontent.com/glennswest/i2cgateha/main/contents/.version",remote_version_check);
}









void sendDownloadReq(char *theURL,void (*stateCB)(void* optParm, AsyncHTTPSRequest* request, int readyState),void (*dataCB)(void* optParm, AsyncHTTPSRequest* request, int readyState))
{
  static bool requestOpenResult;
  Serial.print("sendDownloadReq\n");
  if (request.readyState() == readyStateUnsent || request.readyState() == readyStateDone)
  {
    request.setTimeout(5);
    request.onReadyStateChange(stateCB);
    request.onData(dataCB);
    //request.setDebug(true);
    requestOpenResult = request.open("GET", theURL);

    if (requestOpenResult)
    {
      // Only send() if open() returns true, or crash
      Serial.println("Sending request");
      Serial.println(theURL);
      request.send();
    }
    else
    {
      Serial.println("Can't send bad request");
    }
  }
  else
  {
    Serial.println("Can't send request");
  }
}
