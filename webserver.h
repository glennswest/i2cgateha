struct webreq_entry {
       struct qentry_struct qe;
       AsyncWebServerRequest *request;
       };

struct queue_struct webreq_free;
struct queue_struct webreq_busy;

void setup_webreq_queue()
{
int idx;
struct webreq_entry *webr;

      queue_init(&webreq_free);
      queue_init(&webreq_busy);
      for(idx=0;idx < 20;idx++){
             webr = (struct webreq_entry *) malloc(sizeof(struct webreq_entry));
             queue(&webreq_free,&webr->qe);        
             }

}


int  readbigfatsd(String thepath,uint8_t *buffer, size_t index,int maxlen)
{
FsFile vfile;
//char message[256];
volatile int thesize = 0;
volatile bool seekok;
 
  //sprintf(message,"%s: Index: %d MaxLen: %d\n",thepath,index,maxlen);
  //Serial.print(index);
  //Serial.print(" - ");
  //Serial.println(thepath);
  if (maxlen > 8192){
     maxlen = 8192;
     }
   
  vfile.open((char *)thepath.c_str(),O_RDWR);
  if (vfile){
     seekok = vfile.seekSet(index);
     if (seekok == false){
        //Serial.print(thepath);
        //Serial.println(F(" - EOF"));
        vfile.close();
        vfile_busy = 0; 
        return(0);
        }
     if (vfile.available() == false){
        //Serial.print(thepath);
        //Serial.println(F(" - EOF1"));
        vfile.close();
        vfile_busy = 0;
        return(0);
        }     
     thesize = vfile.read(buffer,maxlen);
     if (thesize == 0){
        vfile_busy = 0;
        //Serial.print(thepath);
        //Serial.println(F(" - EOF2"));
        }
     vfile.close();
     } else {
      thesize = 0;
     }
  return(thesize);
}

bool handleStaticFile(AsyncWebServerRequest *request) {
struct webreq_entry *webr;

  if (vfile_busy == 1){
     webr = (struct webreq_entry *)unqueue(&webreq_free);
     webr->request = request;
     queue(&webreq_busy,&webr->qe);     
     Serial.println("Web Request Deferred");
     return(false);
     }      
  vfile_busy = 1;
  String xpath = request->url();
  Serial.println(xpath); 
  
  if (xpath.equals("/")){
     xpath = F("index.html");
     }
  String contentType = request->contentType();
  

  if (sd.exists(xpath)){
    //Serial.println("Path Exists");
    } else {
      xpath = xpath + F(".gz");
      if (sd.exists(xpath) == false){
          request->send(404);
          vfile_busy = 0;
          return(false);
          } 
     }
  
    //Serial.println(xpath);   
    AsyncWebServerResponse *response = request->beginChunkedResponse(contentType, [xpath](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {      
        return readbigfatsd(xpath,buffer,index,maxLen);
    });
    //response->addHeader("Server","ESP Async Web Server");
    //response->addHeader("Content-Disposition", "attachment; filename=" + xpath);
    response->addHeader("Cache-Control", "no-cache");
    response->addHeader("Access-Control-Allow-Origin", "*");
    request->send(response);
    return true;
  
  return false;
}


void webtask(void * parm0)
{
bool thestatus;
struct webreq_entry *webr;
AsyncWebServerRequest *request;
  
   setup_webreq_queue();
   server.onNotFound([](AsyncWebServerRequest *request) {
      if (handleStaticFile(request)) return; 
      //request->send(404);
      });
  server.begin();
  while(1){
    if (webreq_busy.head != NULL){
       if (vfile_busy == 0){
           webr = (struct webreq_entry *)unqueue(&webreq_busy);
           request = webr->request;
           webr->request = NULL;
           queue(&webreq_free,&webr->qe);  
           thestatus = handleStaticFile(request); 
       }
    }
    delay(100);
  }
}
