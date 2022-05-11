

static struct queue_struct logging_q;
struct log_entry_struct {
   struct qentry_struct qe;
   long   logtime;
   int    repeats;
   char   log_text[256];
};

void log_setup()
{
struct log_entry_struct *le;
int idx;

 logging_q.head = NULL;
 logging_q.tail = NULL;

 
 
 for(idx=0;idx < 100;idx++){
    le = (struct log_entry_struct *)ps_malloc(sizeof(struct log_entry_struct));
    if (le == NULL){
       Serial.println("Cannot allocat log entry");
       return;
       }
    le->logtime = 0;
    le->repeats = 0;
    le->log_text[0] = 0;   
    queue(&logging_q,&le->qe);
 }
}


void epdlog(char *message)
{
#define STARTING_POS 105
#define LINE_OFFSET  32
#define MAX_LINE     50 * LINE_OFFSET

  static int current_pos = STARTING_POS;

  canvas.drawString(message, 25, current_pos);
  canvas.pushCanvas(0, 0, UPDATE_MODE_DU4); //Update the screen.
  current_pos = current_pos + LINE_OFFSET;
  if (current_pos > MAX_LINE) {
    current_pos = STARTING_POS;
  }
}

bool epdupdate(void *ttimer)
{
#define STARTING_POS 105
#define LINE_OFFSET  32
#define MAX_LINE     29 * LINE_OFFSET   
int cpos;
int idx;
struct log_entry_struct *le;   
char thetime[64];
String myrtctime;
time_t curtime;
struct tm *loc_time;
char buf[150];

  M5.EPD.Clear(false);
  canvas.setTextSize(4); //Set the text size.
  canvas.fillCanvas(0x0000);  
  canvas.drawString("I2CGateHa", 25, 20);
  canvas.drawString("HomeAssistantGateway", 25, 52);  //Draw a string.

  canvas.setTextSize(3); //Set the text size.
  cpos = STARTING_POS;
  le = (struct log_entry_struct *)logging_q.tail;
  for(idx=0;idx<25;idx++){
     if (strlen(le->log_text) > 0){
        canvas.drawString(le->log_text, 25, cpos);
        cpos = cpos + LINE_OFFSET;
        }
     le = (struct log_entry_struct *)le->qe.prev;
     }  
  //myrtctime = rtc.getTime();
  //myrtctime.toCharArray(thetime,50);

  time (&curtime);
  loc_time = localtime (&curtime);
  canvas.drawString(asctime (loc_time), 25, MAX_LINE);   
  canvas.pushCanvas(0, 0, UPDATE_MODE_DU); //Update the screen.
  return(true); 
}

void logq(char *message)
{
struct log_entry_struct *hle;
struct log_entry_struct *le;

  Serial.println(message);
  hle = (struct log_entry_struct *)logging_q.tail;
  if (hle == NULL){
     Serial.println("Invalid Logging Q State");
     return;
     }
     
  if (strcmp(hle->log_text,message) == 0){
     hle->repeats++;
     hle->logtime = rtc.getEpoch();
     Serial.println("Repeating message\n");
     return;
     }
  le = (struct log_entry_struct *)unqueue(&logging_q);
  le->repeats = 1;
  le->logtime = rtc.getEpoch();
  strcpy(le->log_text,message);
  queue(&logging_q,&le->qe);  
}

void log(String themessage)
{
  char Buf[250];

  themessage.toCharArray(Buf, 250);
  log(Buf);
}

void log(char *message)
{
  //epdlog(message);
  logq(message);
   
}
