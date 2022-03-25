void epdlog(char *message)
{
#define STARTING_POS 105
#define LINE_OFFSET  20
#define MAX_LINE     50 * LINE_OFFSET

  static int current_pos = STARTING_POS;

  canvas.drawString(message, 25, current_pos);
  canvas.pushCanvas(0, 0, UPDATE_MODE_DU4); //Update the screen.
  current_pos = current_pos + LINE_OFFSET;
  if (current_pos > MAX_LINE) {
    current_pos = STARTING_POS;
  }
}

void log(String themessage)
{
  char Buf[250];

  themessage.toCharArray(Buf, 250);
  log(Buf);
}

void log(char *message)
{
  Serial.println(message);
  epdlog(message);

}


