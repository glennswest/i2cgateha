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
          return(false);
          } 
     }
  
    //Serial.println(xpath);   

    AsyncWebServerResponse *response = request->beginChunkedResponse(contentType, [xpath](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {      
        return readbigfatsd(xpath,buffer,index,maxLen);
    });
    
    response->addHeader("Cache-Control", "no-cache");
    response->addHeader("Access-Control-Allow-Origin", "*");
    request->send(response);

    return true;
}


void webtask(void * parm0)
{
bool thestatus;
struct webreq_entry *webr;
AsyncWebServerRequest *request;
  
   server.onNotFound([](AsyncWebServerRequest *request) {
      if (handleStaticFile(request)) return; 
      //request->send(404);
      });
  server.begin();
  while(1){
    delay(1000);
  }
}
