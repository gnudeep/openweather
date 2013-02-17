//This Arduino agent read temperature and the humidity from DHT11 device and publish to Cosm
////////////////

#include <SPI.h>
#include <Ethernet.h>
#include <HttpClient.h>
#include <Cosm.h>

#define API_KEY "OhmmH6kjeO5FiW8gWZUqlH7c0RKSAKxHTjBtcm1KSm5xxxxx" // your Cosm API key
#define FEED_ID 103776 // your Cosm feed ID
#define dht_dpin 14 //no ; here. Set equal to channel sensor is on,
//where if dht_dpin is 14, sensor is on digital line 14, aka analog 0


// MAC address for your Ethernet shield
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };

// Analog pin which we're monitoring (0 and 1 are used by the Ethernet shield)
int sensorPin = 2;

unsigned long lastConnectionTime = 0;                // last time we connected to Cosm
const unsigned long connectionInterval = 15000;      // delay between connecting to Cosm in milliseconds

// Initialize the Cosm library

// Define the string for our datastream ID
char sensorId[] = "sensor_reading";

CosmDatastream datastreams[] = {
  CosmDatastream(sensorId, strlen(sensorId), DATASTREAM_FLOAT),
};

// Wrap the datastream into a feed
CosmFeed feed(FEED_ID, datastreams, 1 /* number of datastreams */);

EthernetClient client;
CosmClient cosmclient(client);

int humidity = 0;
int temperature = 0;

byte bGlobalErr;//for passing error code back from complex functions.
byte dht_dat[4];//Array to hold the bytes sent from sensor.

void setup(){
  //Network init
   Serial.println("Initializing network");
  while (Ethernet.begin(mac) != 1) {
    Serial.println("Error getting IP address via DHCP, trying again...");
    delay(15000);
  }

  Serial.println("Network initialized");
  Serial.println();

  InitDHT();//Do what's necessary to prepare for reading DHT
  Serial.begin(9600);
  delay(300);//Let system settle
  Serial.println("Humidity and temperature\n\n");
  delay(700);//Wait rest of 1000ms recommended delay before
  //accessing sensor
}//end "setup()"

//Main Loop
void loop(){
  ReadDHT();//This is the "heart" of the program.
  //Fills global array dht_dpin[], and bGlobalErr, which
  //will hold zero if ReadDHT went okay.
  //Must call InitDHT once (in "setup()" is usual) before
  //calling ReadDHT.
  //Following: Display what was seen...

  switch (bGlobalErr){
  case 0:
    Serial.print("Current humdity = ");
    Serial.print(dht_dat[0], DEC);
    Serial.print(".");
    Serial.print(dht_dat[1], DEC);
    Serial.print("%  ");
    Serial.print("temperature = ");
    Serial.print(dht_dat[2], DEC);
    Serial.print(".");
    Serial.print(dht_dat[3], DEC);
    Serial.println("C  ");

    humidity = int(dht_dat[0]);
    temperature = int(dht_dat[2]);

    // send it to Cosm
    //sendData(humidity);
    sendData(temperature);
    
    // read the datastream back from Cosm
    getData();

    break;
  case 1:
    Serial.println("Error 1: DHT start condition 1 not met.");
    break;
  case 2:
    Serial.println("Error 2: DHT start condition 2 not met.");
    break;
  case 3:
    Serial.println("Error 3: DHT checksum error.");
    break;
  default:
    Serial.println("Error: Unrecognized code encountered.");
    break;
  }//end "switch"
  delay(800);//Don't try to access too frequently... in theory
  //should be once per two seconds, fastest,
  //but seems to work after 0.8 second.
}// end loop()

/*Below here: Only "black box" elements which can just be plugged unchanged
 unchanged into programs. Provide InitDHT() and ReadDHT(), and a function
 one of them uses.*/

void InitDHT(){
  //DDRC |= _BV(dht_PIN);//set data pin... for now... as output
  //DDRC is data direction register for pins A0-5 are on
  //PORTC |= _BV(dht_PIN);//Set line high
  //PORTC relates to the pins A0-5 are on.
  //Alternative code...
  //        if (dht_dpin-14 != dht_PIN){Serial.println("ERROR- dht_dpin must be 14 more than dht_PIN");};//end InitDHT
  pinMode(dht_dpin,OUTPUT);// replaces DDRC... as long as dht_dpin=14->19
  digitalWrite(dht_dpin,HIGH);//Replaces PORTC |= if dht_pin=14->19
}//end InitDHT

void ReadDHT(){
  /*Uses global variables dht_dat[0-4], and bGlobalErr to pass
   "answer" back. bGlobalErr=0 if read went okay.
   Depends on global dht_PIN for where to look for sensor.*/
  bGlobalErr=0;
  byte dht_in;
  byte i;
  // Send "start read and report" command to sensor....
  // First: pull-down i/o pin for 18ms
  digitalWrite(dht_dpin,LOW);//Was: PORTC &= ~_BV(dht_PIN);
  delay(18);
  delay(5);//TKB, frm Quine at Arduino forum
  /*aosong.com datasheet for DHT22 says pin should be low at least
   500us. I infer it can be low longer without any]
   penalty apart from making "read sensor" process take
   longer. */
  //Next line: Brings line high again,
  //   second step in giving "start read..." command
  digitalWrite(dht_dpin,HIGH);//Was: PORTC |= _BV(dht_PIN);
  delayMicroseconds(40);//DHT22 datasheet says host should
  //keep line high 20-40us, then watch for sensor taking line
  //low. That low should last 80us. Acknowledges "start read
  //and report" command.

  //Next: Change Arduino pin to an input, to
  //watch for the 80us low explained a moment ago.
  pinMode(dht_dpin,INPUT);//Was: DDRC &= ~_BV(dht_PIN);
  delayMicroseconds(40);

  dht_in=digitalRead(dht_dpin);//Was: dht_in = PINC & _BV(dht_PIN);

  if(dht_in){
    bGlobalErr=1;//Was: Serial.println("dht11 start condition 1 not met");
    return;
  }//end "if..."
  delayMicroseconds(80);

  dht_in=digitalRead(dht_dpin);//Was: dht_in = PINC & _BV(dht_PIN);

  if(!dht_in){
    bGlobalErr=2;//Was: Serial.println("dht11 start condition 2 not met");
    return;
  }//end "if..."

  /*After 80us low, the line should be taken high for 80us by the
   sensor. The low following that high is the start of the first
   bit of the forty to come. The routine "read_dht_dat()"
   expects to be called with the system already into this low.*/
  delayMicroseconds(80);
  //now ready for data reception... pick up the 5 bytes coming from
  //   the sensor
  for (i=0; i<5; i++)
    dht_dat[i] = read_dht_dat();

  //Next: restore pin to output duties
  pinMode(dht_dpin,OUTPUT);//Was: DDRC |= _BV(dht_PIN);
  //N.B.: Using DDRC put restrictions on value of dht_pin

  //Next: Make data line high again, as output from Arduino
  digitalWrite(dht_dpin,HIGH);//Was: PORTC |= _BV(dht_PIN);
  //N.B.: Using PORTC put restrictions on value of dht_pin

  //Next see if data received consistent with checksum received
  byte dht_check_sum =
    dht_dat[0]+dht_dat[1]+dht_dat[2]+dht_dat[3];
  /*Condition in following "if" says "if fifth byte from sensor
   not the same as the sum of the first four..."*/
  if(dht_dat[4]!= dht_check_sum)
  {
    bGlobalErr=3;
  }//Was: Serial.println("DHT11 checksum error");
};//end ReadDHT()

byte read_dht_dat(){
  //Collect 8 bits from datastream, return them interpreted
  //as a byte. I.e. if 0000.0101 is sent, return decimal 5.

  //Code expects the system to have recently entered the
  //dataline low condition at the start of every data bit's
  //transmission BEFORE this function is called.

  byte i = 0;
  byte result=0;
  for(i=0; i< 8; i++){
    //We enter this during the first start bit (low for 50uS) of the byte
    //Next: wait until pin goes high
    while(digitalRead(dht_dpin)==LOW);//Was: while(!(PINC & _BV(dht_PIN)));
    //signalling end of start of bit's transmission.

    //Dataline will now stay high for 27 or 70 uS, depending on
    //whether a 0 or a 1 is being sent, respectively.
    delayMicroseconds(30);//AFTER pin is high, wait further period, to be
    //into the part of the timing diagram where a 0 or a 1 denotes
    //the datum being send. The "further period" was 30uS in the software
    //that this has been created from. I believe that a higher number
    //(45?) would be more appropriate.

    //Next: Wait while pin still high
    if (digitalRead(dht_dpin)==HIGH)//Was: if(PINC & _BV(dht_PIN))
      result |=(1<<(7-i));// "add" (not just addition) the 1
    //to the growing byte
    //Next wait until pin goes low again, which signals the START
    //of the NEXT bit's transmission.
    while (digitalRead(dht_dpin)==HIGH);//Was: while((PINC & _BV(dht_PIN)));
  }//end of "for.."
  return result;
}//end of "read_dht_dat()"

// send the supplied value to Cosm, printing some debug information as we go
void sendData(int sensorValue) {
  datastreams[0].setFloat(sensorValue);

  Serial.print("Read sensor value ");
  Serial.println(datastreams[0].getFloat());

  Serial.println("Uploading to Cosm");
  int ret = cosmclient.put(feed, API_KEY);
  Serial.print("PUT return code: ");
  Serial.println(ret);

  Serial.println();
}

// get the value of the datastream from Cosm, printing out the value we received
void getData() {
  Serial.println("Reading data from Cosm");

  int ret = cosmclient.get(feed, API_KEY);
  Serial.print("GET return code: ");
  Serial.println(ret);

  if (ret > 0) {
    Serial.print("Datastream is: ");
    Serial.println(feed[0]);

    Serial.print("Sensor value is: ");
    Serial.println(feed[0].getFloat());
  }

  Serial.println();
}

    
