#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
// #include <TimeLib.h>
// #include <NtpClientLib.h>
#include <time.h>
extern "C" {
#include "user_interface.h"
#include "sntp.h"
}
// #include <timelib.h>
#include "ringbuffer.hpp"

#define BUFFER_SIZE 1024
#define PACKET_BUFFER_SIZE 16

ESP8266WebServer server ( 80 );

const char* ssid = "hudbrogwifi";
const char* password = "wifipass";

// char buffer_debug[BUFFER_SIZE+1];
ringbuffer<char, BUFFER_SIZE> buffer_debug = ringbuffer<char, BUFFER_SIZE>();
// ringbuffer<char, BUFFER_SIZE> buffer_serial = ringbuffer<char, BUFFER_SIZE>();
char buffer_serial[PACKET_BUFFER_SIZE];
// int buffer_debug_pos=0;

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

#define PACKET_SIZE 8

// int buffer_put_char(char* buffer, int *pos, char ch){
//     buffer[*pos] = ch;
//     (*pos)++;
//     if(*pos == BUFFER_SIZE) *pos=0;
//     return *pos;
// }
//

// void buffer_put_array(const char *str, int len){
//   for (int i=0; i<len;i++)buffer_put_char(str[i]);
// }

void buffer_put_str(ringbuffer<char, BUFFER_SIZE> *buffer, String str){
  // for (int i=0; i<str.length();i++)buffer_put_char(buffer, pos, str.charAt(i));
  for (int i=0; i<str.length();i++)buffer->push(str.charAt(i));
}

void debug_log(String str){
  char dt[2+1+2+1+2+2];
  sprintf(dt, "%02d:%02d:%02d \0", hour(), minute(), second());
  buffer_put_str(&buffer_debug, String(dt) + str+"\n");
}

// char buffer_get_char(ringbuffer<char, BUFFER_SIZE> buffer, int buffer_pos, int pos){
//   if (buffer_pos+pos >= BUFFER_SIZE){
//     return buffer[(buffer_pos+pos)%BUFFER_SIZE];
//   }
//   return buffer[buffer_pos+pos];
// }

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
  char temp[1000];
  // time_t now = time(nullptr);
  sprintf(temp,
    "<html>\
      <head>\
        <title>ESP8266 Demo</title>\
      </head>\
      <body>\
        Upper temp: %d, lower temp: %d, time: %d\0",
    last_temp_upper, last_temp_lower, now()
  );
  server.send(200, "text/html", temp);
}

void process_packet(){
  // return;
  packet *pck = (packet *)&buffer_serial;
  // pck = (packet *)&buffer_serial[start_pos];
  if (check_crc(pck->temp_lower, pck->temp_upper, pck->checksum)){
    last_temp_lower = (int)pck->temp_lower;
    last_temp_upper = (int)pck->temp_upper;
    if (millis() - last_millis > 10000 ){
      char msg[32];
      sprintf(msg, "Temp: %d, %d\0", pck->temp_lower, pck->temp_upper);
      debug_log(msg);
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
  // for(int i=0;i<BUFFER_SIZE;i++)buffer_serial[i] = '*';
  // for(int i=0;i<BUFFER_SIZE;i++)buffer_debug[i] = '*';
  // buffer_debug[BUFFER_SIZE] = 0;
  // buffer_serial[BUFFER_SIZE] = 0;
  debug_log("Booting\n");
  WiFi.mode(WIFI_AP_STA );
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    delay(5000);
    ESP.restart();
  }
  // NTP.onNTPSyncEvent([](NTPSyncEvent_t error) {
  //   if (error) {
  //       debug_log("Time Sync error: ");
  //       if (error == noResponse)
  //           debug_log("NTP server not reachable");
  //       else if (error == invalidAddress)
  //           debug_log("Invalid NTP server address");
  //   }
  //   else {
  //       debug_log("Got NTP time: ");
  //       debug_log(NTP.getTimeDateString(NTP.getLastNTPSync()));
  //   }
  // });
  // NTP.begin("176.31.251.139", 1, true);
  // NTP.setInterval(30);
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
    // if(ch == 0)packet_start=buffer_serial_pos;
    // if(ch == 0)packet_start=buffer_serial.;
    // buffer_put_char(buffer_serial, &buffer_serial_pos, ch);
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
