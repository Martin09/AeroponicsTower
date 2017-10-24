// #include <Time.h>
#include <TimeLib.h>
#include <TaskScheduler.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

#define _TASK_SLEEP_ON_IDLE_RUN // Allow processor to sleep when no tasks running

bool USE_WIFI = false;

// Wifi Variables     -----------------------------------------------
char ssid[] = "XXX";  //  your network SSID (name)
char pass[] = "XXX";;       // your network password

unsigned int localPort = 2390;      // local port to listen for UDP packets

IPAddress timeServerIP; // pool.ntp.org NTP server address
const char* ntpServerName = "pool.ntp.org";

const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message

byte packetBuffer[ NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets

// A UDP instance to let us send and receive packets over UDP
WiFiUDP udp;
// End Wifi Variables   -----------------------------------------------

const bool DEBUG = true;

const int watering_period = 20*TASK_SECOND;//2*TASK_MINUTE; // Water every 2 minutes
const int watering_time = 2*TASK_SECOND;//10*TASK_SECOND; // For 10 seconds
const int meas_temps_period = 5*TASK_SECOND; // Meas temps every minute
const int meas_nutes_period = 4*TASK_SECOND; // Meas nutrient level every minute
const int logging_period = 6*TASK_SECOND; // Write to logfile every 5 seconds (increase to ever 1 second?)
const int upload_period = 20*TASK_SECOND; // Upload logfile contents every 30 seconds
const int time_sync_period = 1*TASK_HOUR; // Sync time with NTP servers over wifi once per hour
const int wifi_check_period = 10*TASK_SECOND; // If not connected, check wifi every ten seconds

const int WATERING_PIN = D4; // Pump to water plants
const int UV_LAMP_PIN = D5; // Anti-bacterial UV lamp in reservoir
const int BUBBLES_PIN = D6; // Bubbler in nutrient reservoir
const int LED_PIN = D7; // NodeMCUs Onboard LED
const int ONE_WIRE_BUS = D3; // OneWire Sensor Pin

// Setup a oneWire instance to communicate with any OneWire devices  
OneWire oneWire(ONE_WIRE_BUS); 

// Pass our oneWire reference to Dallas Temperature. 
DallasTemperature sensors(&oneWire);

Scheduler ts;

// Callback methods prototypes
void watering_on_Callback();
void watering_off_Callback();
void meas_temps_Callback();
void meas_nutes_Callback();
void logging_Callback();
void upload_Callback();
void sync_time_Callback();
void connect_wifi_Callback();
void check_wifi_Callback();

Task connect_wifi(wifi_check_period, TASK_ONCE, &connect_wifi_Callback, &ts, USE_WIFI);
Task check_wifi(wifi_check_period, TASK_ONCE, &check_wifi_Callback, &ts, false);
Task sync_time(time_sync_period, TASK_FOREVER, &sync_time_Callback, &ts, false);
Task watering_on(watering_period, TASK_FOREVER, &watering_on_Callback, &ts, false);
Task watering_off(watering_time, TASK_ONCE, &watering_off_Callback, &ts, false);  // Run only once after enabling it
Task meas_temps(meas_temps_period, TASK_FOREVER, &meas_temps_Callback, &ts, true);
Task meas_nutes(meas_nutes_period, TASK_FOREVER, &meas_nutes_Callback, &ts, true);
Task logging(logging_period, TASK_FOREVER, &logging_Callback, &ts, false);
Task upload(upload_period, TASK_FOREVER, &upload_Callback, &ts, false);

double temp_reservoir = 0;
double temp_outside = 0;
double nutrient_lvl = 0;

// TODO: Initiate these to the last known values?
double temp_root_chamber_bot = 0;
double temp_root_chamber_top = 0;

// For writing log information
String log_packet = "";

void watering_on_Callback() {
  db_print("Turning Watering On");
  digitalWrite(WATERING_PIN, HIGH);
//  ts.addTask(watering_off);
//  watering_off.enable();
  watering_off.restartDelayed();
}

void watering_off_Callback() {
  db_print("Turning Watering Off");
  digitalWrite(WATERING_PIN, LOW);
//  watering_off.disable();
//  ts.deleteTask(watering_off); //Not sure if its necessary to delete it as well, check this?
}

void meas_temps_Callback() {
  db_print("Measuring Temps");
  // Read the one wire temperature sensors
  //sensors.requestTemperatures();
  //temp_root_chamber_bot = sensors.getTempCByIndex(0);
  //temp_root_chamber_top = sensors.getTempCByIndex(1);
  //temp_reservoir = sensors.getTempCByIndex(2);
  //temp_outside = sensors.getTempCByIndex(3);
  //TODO: Also get the humidity sensor values like this?
  temp_root_chamber_bot = random(11)+20;
  temp_root_chamber_top = random(11)+20;
  temp_reservoir = random(11)+20;
  temp_outside = random(11)+20;
}

void meas_nutes_Callback() {
  db_print("Measuring Nutes");  
  //TODO: implement nutrient level measurement
  nutrient_lvl = random(101);
}

void logging_Callback() {
  db_print("Logging");
  log_packet += String(millis()) + ",";
  log_packet += String(temp_root_chamber_bot) + ",";
  log_packet += String(temp_root_chamber_top) + ",";
  log_packet += String(temp_reservoir) + ",";
  log_packet += String(temp_outside) + ",";
  log_packet += String(nutrient_lvl) + ";";
  //TODO: Add serial pin values: watering motor, etc.
}

void upload_Callback() {
  db_print("Uploading");
  //TODO: Send all logs as a single packet over wifi network with handshake. MQTT?
  Serial.println(log_packet);
  log_packet = "";
}

// For outputting time-stamped messages
void db_print(String msg){
  if (DEBUG) {
    time_t t = now(); // Store the current time in time 
    Serial.print(year(t));
    Serial.print("/");    
    Serial.print(month(t));
    Serial.print("/");    
    Serial.print(day(t));
    Serial.print(" ");
    Serial.print(hour(t));
    Serial.print(":");
    Serial.print(minute(t));
    Serial.print(":");
    Serial.print(second(t));
    Serial.print(": ");
    Serial.println(msg);
  }
}

void connect_wifi_Callback(){
  db_print("Connecting to Wifi");  
  WiFi.persistent(false);

  // We start by connecting to a WiFi network
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, pass);
  
  check_wifi.enableDelayed();
}

void check_wifi_Callback(){
  db_print("Checking Wifi Connection");    
  if (WiFi.status() != WL_CONNECTED) {
    db_print("Wifi Not Connected");  
    return;
  }
  Serial.println("");
  
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  Serial.println("Starting UDP");
  udp.begin(localPort);
  Serial.print("Local port: ");
  Serial.println(udp.localPort());
  check_wifi.disable();
  sync_time.enable();
}

void sync_time_Callback(){
  db_print("Syncing with NPT Server");
  if (WiFi.status() != WL_CONNECTED) {  // Only try to sync if we have a wifi connection
    db_print("Can't sync, no wifi connection...");
    sync_time.disable();
    check_wifi.enable();
    return;
  }
  
  //get a random server from the pool
  WiFi.hostByName(ntpServerName, timeServerIP); 

  sendNTPpacket(timeServerIP); // send an NTP packet to a time server
  // wait to see if a reply is available
  delay(1000);
  
  int cb = udp.parsePacket();
  if (!cb) {
    Serial.println("no packet yet");
  }
  else {
    Serial.print("packet received, length=");
    Serial.println(cb);
    // We've received a packet, read the data from it
    udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer

    //the timestamp starts at byte 40 of the received packet and is four bytes,
    // or two words, long. First, esxtract the two words:

    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
    // combine the four bytes (two words) into a long integer
    // this is NTP time (seconds since Jan 1 1900):
    unsigned long secsSince1900 = highWord << 16 | lowWord;
    Serial.print("Seconds since Jan 1 1900 = " );
    Serial.println(secsSince1900);

    // now convert NTP time into everyday time:
    Serial.print("Unix time = ");
    // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
    const unsigned long seventyYears = 2208988800UL;
    // subtract seventy years:
    unsigned long epoch = secsSince1900 - seventyYears;
    // print Unix time:
    Serial.println(epoch);

    if (year(epoch) > 2000) { // Only sync time if got reasonable value from the server
        setTime(epoch); // Set current arduino time
    }
  }
}

// send an NTP request to the time server at the given address
unsigned long sendNTPpacket(IPAddress& address)
{
  Serial.println("sending NTP packet...");
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;

  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  udp.beginPacket(address, 123); //NTP requests are to port 123
  udp.write(packetBuffer, NTP_PACKET_SIZE);
  udp.endPacket();
}

void setup() {
  // Setup hardware
  Serial.begin(115200);
  Serial.println(""); //Clear buffer/gibberish at startup
  setTime(0);
  db_print("Initializing hardware.");
  pinMode(WATERING_PIN, OUTPUT);
  pinMode(UV_LAMP_PIN, OUTPUT);
  pinMode(BUBBLES_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  
//  digitalWrite(WATERING_PIN, LOW);
//  digitalWrite(UV_LAMP_PIN, LOW);
//  digitalWrite(BUBBLES_PIN, LOW);
//  digitalWrite(LED_PIN, LOW);  
//  sensors.begin(); // Setup the onewire temperature sensors
  ts.startNow();  // set point-in-time for scheduling start
  watering_on.enableDelayed();
  logging.enableDelayed();
  upload.enableDelayed();  
}

void loop() {
  ts.execute();
}
