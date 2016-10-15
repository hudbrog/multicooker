#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <time.h>
extern "C" {
#include "user_interface.h"
#include "sntp.h"
}
#include "ringbuffer.hpp"

#define BUFFER_SIZE 1024
#define TEMPERATURE_BUFFER_SIZE 1024
#define PACKET_SIZE 8
#define PACKET_BUFFER_SIZE 16

ESP8266WebServer server ( 80 );

const char* ssid = "hudbrogwifi";
const char* password = "wifipass";

ringbuffer<char, BUFFER_SIZE> buffer_debug = ringbuffer<char, BUFFER_SIZE>();
char buffer_serial[PACKET_BUFFER_SIZE];

int last_temp_lower;
int last_temp_upper;
unsigned long last_millis;
unsigned long log_heartbeat;

struct packet{
  char pad_00;
  char temp_lower;
  char temp_upper;
  char pad_1a;
  char pad_a4;
  char checksum;
  char pad_aa;
  char pad_ff;
};

struct temp_struc{
  char temp_lower;
  char temp_upper;
  time_t timestamp;
};

ringbuffer<temp_struc, TEMPERATURE_BUFFER_SIZE> temp_buffer = ringbuffer<temp_struc, TEMPERATURE_BUFFER_SIZE>();

void buffer_put_str(ringbuffer<char, BUFFER_SIZE> *buffer, String str){
  for (int i=0; i<str.length();i++)buffer->push(str.charAt(i));
}

void debug_log(String str){
  char dt[2+1+2+1+2+2];
  sprintf(dt, "%02d:%02d:%02d \0", hour(), minute(), second());
  buffer_put_str(&buffer_debug, String(dt) + str+"\n");
}

char buffer_get_char(ringbuffer<char, BUFFER_SIZE> buffer, int pos){
  return buffer.get(pos);
}

bool check_crc(char temp1, char temp2, char crc){
    return (0x41 - (temp1+temp2)) == crc;
}

void handleRoot() {
  char temp[2400];
	sprintf ( temp,
"<html>\
  <head>\
    <title>ESP8266 Demo</title>\
  </head>\
  <body>\
    <h1>Hello from ESP8266!</h1>\0"
	);
  int pos=strlen(temp);
  char ch;
  int skip=0;

  for(int i=0; i<buffer_debug.size(); i++){
    ch = buffer_debug.get(i);
    if(isPrintable(ch) || isControl(ch)){
      temp[pos+i+skip] = ch;
    }else{
      sprintf(temp+pos+i+skip, "%02X:", ch);
      skip+=2;
    }
  }
	server.send ( 200, "text/html", temp );
}

void handleTemp() {
  char temp[500];
  int pos;
  temp_struc temperature;
  int skip=0;
  String out = "";
  // time_t now = time(nullptr);
  sprintf(temp,
    "<html>\
      <head>\
        <title>ESP8266 Demo</title>\
        <script type='text/javascript' src='https://cdnjs.cloudflare.com/ajax/libs/canvasjs/1.7.0/canvasjs.js'></script>\
      </head>\
      <body>\
        Upper temp: %d, lower temp: %d, time: %d\
        <script type='text/javascript'>\
          var mcData = [\0",
    last_temp_upper, last_temp_lower, now()
  );

  out += temp;
  char array[11];
  for(int i=0; i<temp_buffer.size(); i++){
    temperature = temp_buffer.get(i);
    sprintf(array, "[%d,%d,%lu],", temperature.temp_upper, temperature.temp_lower, temperature.timestamp);
    out += array;
  }

  sprintf(temp,
    "];\
    </script>\
    <div id=\"chartContainer\" style=\"height: 300px; width: 100\%;\">\
	  </div>\
    <script type='text/javascript' src='http://dtp.gcode.ws/js/lib.js'></script>\
    </body>\
    </html>\
    \0");
  out += temp;

  server.send(200, "text/html", out);
}

void process_packet(){
  packet *pck = (packet *)&buffer_serial;
  if (check_crc(pck->temp_lower, pck->temp_upper, pck->checksum)){
    last_temp_lower = (int)pck->temp_lower;
    last_temp_upper = (int)pck->temp_upper;
    if (millis() - last_millis > 10000 ){
      char msg[32];
      sprintf(msg, "Temp: %d, %d\0", pck->temp_lower, pck->temp_upper);
      debug_log(msg);
      temp_struc cur_temp;
      cur_temp.temp_lower = pck->temp_lower;
      cur_temp.temp_upper = pck->temp_upper;
      cur_temp.timestamp = now();
      temp_buffer.push(cur_temp);
      last_millis = millis();
    }
  } else {
    char msg[PACKET_BUFFER_SIZE*3+1];
    sprintf(msg, "Invalid CRC: %02x, %02x != %02x\0", pck->temp_lower, pck->temp_upper, pck->checksum);
    debug_log(msg);
    char msg_pos=0;
    for(int i=0;i<PACKET_BUFFER_SIZE; i++){
      sprintf(msg+msg_pos, "%02X:", buffer_serial[i]);
      msg_pos+=3;
    }
    debug_log(msg);
  }
}


void setup() {
  Serial.begin(9600);
  debug_log("Booting\n");
  WiFi.mode(WIFI_AP_STA );
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    delay(5000);
    ESP.restart();
  }
  configTime(3 * 3600, 0, "2.pool.ntp.org", "0.pool.ntp.org", "1.pool.ntp.org");
  setSyncProvider((getExternalTime)sntp_get_current_timestamp);
  setSyncInterval(300);
  setTime(sntp_get_current_timestamp()); // otherwise it will wait for 600 seconds before sync
  last_millis = millis();

  server.on ( "/", handleRoot );
  server.on ( "/temp", handleTemp );
  server.begin();

  ArduinoOTA.begin();
  debug_log("Ready\n");
  debug_log("IP address: ");
  debug_log(WiFi.localIP().toString());

}

void loop() {
  char ch=42;
  static int packet_pos=0;
  static char prev_ch=42; // anything but 0 and 255
  if(now() < 100){
    setTime(sntp_get_current_timestamp()); // otherwise it will wait for 600 seconds before sync
  }

  ArduinoOTA.handle();
  server.handleClient();

  while(Serial.available()>0){
    ch = Serial.read();
    buffer_serial[packet_pos]=ch;
    packet_pos++;
    if(packet_pos!=0 && prev_ch==0xAA && ch==0xFF){ // packet_ready
      process_packet();
      packet_pos = 0;
    }else if (packet_pos >= PACKET_BUFFER_SIZE ){
      debug_log("Error, haven't seen packet end signature for some time");
      char msg[PACKET_BUFFER_SIZE*3+1];
      char msg_pos=0;
      for(int i=0;i<PACKET_BUFFER_SIZE; i++){
        sprintf(msg+msg_pos, "%02X:", buffer_serial[i]);
        msg_pos+=3;
      }
      debug_log(msg);
      packet_pos = 0;
    }
    prev_ch=ch;
  }
  if(millis() - log_heartbeat > 10000){
    debug_log("x");
    log_heartbeat = millis();
  }
}
