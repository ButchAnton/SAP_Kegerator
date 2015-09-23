/****************************************************************
Kegerator.ino
IoT enabled Kegerator.  Provides the following data:
- Temperature
- Door state
- Volume of pour
Butch Anton @ SAP Labs, LLC.
15-May-2015
Based on a bunch of code from
Shawn Hymel @ SparkFun Electronics
March 1, 2014
https://github.com/sparkfun/SFE_CC3000_Library

The below refers to the Wi-Fi portion of the code.

The security mode is defined by one of the following:
WLAN_SEC_UNSEC, WLAN_SEC_WEP, WLAN_SEC_WPA, WLAN_SEC_WPA2

Hardware Connections for Wi-Fi shield:

 Uno Pin    CC3000 Board    Function

 +5V        VCC or +5V      5V
 GND        GND             GND
 2          INT             Interrupt
 7          EN              WiFi Enable
 10         CS              SPI Chip Select
 11         MOSI            SPI MOSI
 12         MISO            SPI MISO
 13         SCK             SPI Clock

Resources:
Include SPI.h, SFE_CC3000.h, and SFE_CC3000_Client.h

Distributed as-is; no warranty is given.
****************************************************************/

#include <SPI.h>
#include <SFE_CC3000.h>
#include <SFE_CC3000_Client.h>

// Pins
#define CC3000_INT      2   // Needs to be an interrupt pin (D2/D3)
#define CC3000_EN       7   // Can be any digital pin
#define CC3000_CS       10  // Preferred is pin 10 on Uno

// Connection info data lengths
#define IP_ADDR_LEN     4   // Length of IP address in bytes

// Constants
// char ap_ssid[] = "DevRel2Go";                  // SSID of network
char ap_ssid[] = "SAP-Guest";                  // SSID of network
// char ap_password[] = "21119709";          // Password of network
char ap_password[] = "";          // Password of network
// unsigned int ap_security = WLAN_SEC_WPA2; // Security of network
unsigned int ap_security = WLAN_SEC_UNSEC; // Security of network
unsigned int timeout = 30000;             // Milliseconds
char *server = "ec2-52-7-151-201.compute-1.amazonaws.com";
// IPAddress server(10,0,1,102);
int port = 8000;

// Global Variables
SFE_CC3000 wifi = SFE_CC3000(CC3000_INT, CC3000_EN, CC3000_CS);
SFE_CC3000_Client client = SFE_CC3000_Client(wifi);
#define MAX_TCP_CONNECT_RETRIES 10
int tcpConnectFails = 0;

// Pin for temperature sensor

#define aref_voltage 5.0
int temperatureSensorPin = 0;   // Analog pin 0

// Pin for door switch

int doorSwitchPin = 8;
int doorIsOpen = 0;   // If the door is open, the value is 1.
int previousDoorIsOpen = 0;   // If the door is open, the value is 1.

// Service endpoints

char *baseService = "/sap/devs/demo/iot/services/iot_input.xsodata/";
char *tempService = "temp";
char *doorService = "door";
char *pourService = "flow";

String authorization = "Basic U1lTVEVNOmFEajM3c1Fr";
String content_type = "application/json";

// Flow sensor setup.

// Digital pin to use for reading the sensor.
#define FLOWSENSORPIN 4

// Have we finished the pour?
#define meterLatency (100)
#define POUR_MINIMUM (.40)
int noPulseCount = 0;
// Number of pulses
volatile uint16_t pulses = 0;
// State of the flow pin
volatile uint8_t lastflowpinstate;
// Time of the last pulse
volatile uint32_t lastflowratetimer = 0;
// Rate of flow
volatile float flowrate;
// Interrupt is called once a millisecond.  Checks to see if there
// is a pulse.  If so, update the time and increment the pulse count.
SIGNAL(TIMER0_COMPA_vect) {
  uint8_t x = digitalRead(FLOWSENSORPIN);

  if (x == lastflowpinstate) {
    // Nothing changed
    noPulseCount++;
    lastflowratetimer++;
    return;
  }

  if (x == HIGH) {
    // Low to high transition
    noPulseCount = 0;
    pulses++;
  }

  lastflowpinstate = x;
  flowrate = 1000.0;
  flowrate /= lastflowratetimer;  // in hertz
  lastflowratetimer = 0;
}

void useInterrupt(boolean v) {
  if (v) {
    // Timer0 is already used for millis() - we'll just interrupt somewhere
    // in the middle and call the "Compare A" function above
    OCR0A = 0xAF;
    TIMSK0 |= _BV(OCIE0A);
  } else {
    // do not call the interrupt function COMPA anymore
    TIMSK0 &= ~_BV(OCIE0A);
  }
}

void setup() {

  ConnectionInfo connection_info;

  // Initialize Serial port
  Serial.begin(115200);
  Serial.println();
  Serial.println("---------------------------");
  Serial.println("SparkFun CC3000 - WebClient");
  Serial.println("---------------------------");

  // Initialize CC3000 (configure SPI communications)
  if ( wifi.init() ) {
    Serial.println("CC3000 initialization complete");
  } else {
    Serial.println("Something went wrong during CC3000 init!  Halting.");
    while (1);  // Don't go any further
  }

  // Connect using DHCP
  Serial.print("Connecting to SSID: ");
  Serial.println(ap_ssid);
  if(!wifi.connect(ap_ssid, ap_security, ap_password, timeout)) {
    Serial.println("Error: Could not connect to AP.  Halting.");
    while (1);  // Don't go any further
  }

  // Gather connection details and print IP address
  if ( !wifi.getConnectionInfo(connection_info) ) {
    Serial.println("Error: Could not obtain connection details.  Halting.");
    while (1);  // Don't go any further
  } else {
    Serial.print("IP Address: ");
    for (int i = 0; i < IP_ADDR_LEN; i++) {
      Serial.print(connection_info.ip_address[i]);
      if ( i < IP_ADDR_LEN - 1 ) {
        Serial.print(".");
      }
    }
    Serial.println();
  }

  // Set up the door sensor.

  pinMode(doorSwitchPin, INPUT);

  // Set up the flow sensor.

  pinMode(FLOWSENSORPIN, INPUT);
  digitalWrite(FLOWSENSORPIN, HIGH);
  lastflowpinstate = digitalRead(FLOWSENSORPIN);
  useInterrupt(true);

}

void PostToService(char *service, String body) {

  // Make a TCP connection to remote host
  Serial.print("Performing HTTP POST to ");
  Serial.print(server);
  Serial.print (" on port ");
  Serial.println(port);

  if ( !client.connect(server, port) ) {
    Serial.println("Error: Could not make a TCP connection");
    tcpConnectFails++;
    if (tcpConnectFails > MAX_TCP_CONNECT_RETRIES) {
      Serial.println("Halting.");
      while (1);  // Don't go any further
    }
    return;
  } else {
    tcpConnectFails = 0;
    Serial.println("Sending POST request ...");
    client.print("POST ");
    Serial.print("POST ");
    client.print(baseService);
    Serial.print(baseService);
    client.print(service);
    Serial.print(service);
    client.println(" HTTP/1.1");
    Serial.println(" HTTP/1.1");
    client.print("Host: ");
    Serial.print("Host: ");
    client.println(server);
    Serial.println(server);
    client.print("Authorization: ");
    Serial.print("Authorization: ");
    client.println(authorization);
    Serial.println(authorization);
    client.print("Content-Type: ");
    Serial.print("Content-Type: ");
    client.println(content_type);
    Serial.println(content_type);
    client.print("Content-Length: ");
    Serial.print("Content-Length: ");
    client.println(String(body.length()));
    Serial.println(String(body.length()));
    client.print("\r\n");
    Serial.print("\r\n");
    client.println(body);
    Serial.println(body);
    delay(500);

    while ( client.available() ) {
      char c = client.read();
      Serial.print(c);
    }
    delay(500);   // Don't know why, but if we don't, things wedge.  I hate timing problems.
    Serial.println("\n************************* End of transaction *************************");
  }
}

void PostToTemp(float temp) { // temp is in degrees C
  String body = "{ \"MeasureID\" : \"-1\", \"Temp\" : \"" + String(temp) + "\", \"Unit\" : \"C\" }";
  // PostToService(tempService, body);
}

void PostToPour(float volume) { // volume is in ounces
  String body = "{ \"PourID\" : \"-1\", \"Pour\" : \"" + String(volume) + "\", \"Unit\" : \"oz\" }";
  PostToService(pourService, body);
}

void PostToDoor(int doorOpen) { // 1 == true, the door is open
  String body = "{ \"ActivityID\" : \"-1\", \"Open\" : " + String(doorOpen) + " }";
  // PostToService(doorService, body);
}

// Sensor functions

// Detemine if the door is open.
// Returns 1 if the door is open, 0 otherwise.

int GetDoorState() {
  int state = digitalRead(doorSwitchPin);
  return(state);
}

// Get the temperature in degrees C.

float GetTemperature() {

  int tempReading = analogRead(temperatureSensorPin);

  // Convert that reading to a voltage, which is based off the reference voltage.

  float voltage = tempReading * aref_voltage;
  voltage /= 1024.0;

  // Print out the temperature.

  float temperatureC = (voltage - 0.5) * 100 ;  // Converting from 10 mv per degree with 500 mV offset

  return(temperatureC);
}

void loop() {

  // If there is a door status change, post it.

  doorIsOpen = GetDoorState();
  if (doorIsOpen != previousDoorIsOpen) {
    PostToDoor(doorIsOpen);
    previousDoorIsOpen = doorIsOpen;
  }

  // If there is a pour event, post it.

  if (noPulseCount >= meterLatency && pulses != 0) {
    float gallons = pulses / 10200.0;
    float ounces = gallons * 128.0;
    // Ignore pours less than some number of ounces
    if (ounces > POUR_MINIMUM) {
      PostToPour(ounces);
    }
    pulses = 0;
  }

  // Post the temperature all the time.

  PostToTemp(GetTemperature());

  // Close the socket -- we repoen it on the next request.

  if ( !client.close() ) {
    Serial.println("Error: Could not close socket");
  }

  // Sleep 1 second before sending the next reading.

  // delay(10000);
  delay(1000);

#if 0
  if ( !client.connected() ) {
    Serial.println();


    // Disconnect WiFi
    if ( !wifi.disconnect() ) {
      Serial.println("Error: Could not disconnect from network");
    }

    // Do nothing
    Serial.println("Finished WebClient test");
    while(true){
      delay(1000);
    }
  }
#endif // 0
}
