#include <SoftwareSerial.h>
#define DEBUG true
SoftwareSerial esp8266(9,10); //Rx, Tx -> For the Wi-Fi module
SoftwareSerial sim(11, 12); //Rx, Tx -> For GSM module
#include <LiquidCrystal.h>
#include <stdlib.h>
LiquidCrystal lcd(2,3,4,5,6,7); //RS, E, D4, D5, D6, D7
 
#define SSID "Mi-A3" // "SSID-WiFiname"
#define PASS "nopasscode" // "password"
#define IP "184.106.153.149"// thingspeak.com ip
String msg = "GET /update?key=EWM1RDNWEK8RLINJ"; //change it with your api key
String number = "+2547......";
 
//Variables
float temp;
int hum;
String tempC;
int error;
int pulsePin = A1; // Pulse Sensor connected to analog pin
int buzzer = 13; // pin to send sound to buzzer
int fadePin = 5;
int faderate = 0;
bool alert = false;
 
// Volatile Variables, used in the interrupt service routine!
volatile int BPM; // int that holds raw Analog in 0. updated every 2mS
volatile int Signal; // holds the incoming raw data
volatile int IBI = 600; // int that holds the time interval between beats! Must be seeded!
volatile boolean Pulse = false; // "True" when heartbeat is detected. "False" when not a "live beat".
volatile boolean QS = false; // becomes true when Arduino finds a beat.
 
// Regards Serial OutPut -- Set This Up to your needs
static boolean serialVisual = true; // Set to 'false' by Default.
volatile int rate[10]; // array to hold last ten IBI values
volatile unsigned long sampleCounter = 0; // used to determine pulse timing
volatile unsigned long lastBeatTime = 0; // used to find IBI
volatile int P =512; // used to find peak in pulse wave
volatile int T = 512; // used to find trough in pulse wave
volatile int thresh = 525; // used to find instant moment of heart beat
volatile int amp = 100; // used to hold amplitude of pulse waveform
volatile boolean firstBeat = true; // used to seed rate array
volatile boolean secondBeat = false; // used to seed rate array

 
void setup()
{      
pinMode(fadePin,OUTPUT);                     
interruptSetup();
lcd.begin(16, 2);
lcd.print("Connecting...");
Serial.begin(9600);
pinMode(buzzer, OUTPUT);
esp8266.begin(115200);
Serial.println("AT");
esp8266.println("AT");
delay(5000);
if(esp8266.find("OK")){
connectWiFi();
}
interruptSetup();
}

 
void loop(){
  if(QS){
lcd.clear();
start:
error=0;
lcd.setCursor(0, 0);
lcd.print("BPM = ");
lcd.print(BPM);
lcd.setCursor(0, 1);
lcd.print("Normal BPM");
Serial.print("BPM = ");
Serial.println(BPM);
delay(1000);

if(alert == false){
   if(BPM>100){
      digitalWrite(buzzer, HIGH);
      lcd.clear();
      lcd.print("BPM = ");
      lcd.print(BPM);
      lcd.setCursor(0,1);
      lcd.print("BPM Too High");
      bpmTooHigh();
      alert = true;
}
if(BPM<35){
  digitalWrite(buzzer, HIGH);
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("BPM = ");
  lcd.print(BPM);
  lcd.setCursor(0,1);
  lcd.print("BPM too Low");
  bpmTooLow();
  alert = true;
  
}
}
if(alert){
  if(BPM<100 && BPM >35){
  digitalWrite(buzzer, LOW);
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("BPM = ");
  lcd.print(BPM);
  lcd.setCursor(0,1);
  lcd.print("Normal BPM");
  alert = false;
  }
}

updatebeat();
if (error==1){
goto start; //go to label "start"
}
QS = false;
  }
  else{
    digitalWrite(buzzer, LOW);
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("No heartbeat:");
    lcd.setCursor(0,1);
    lcd.print("Finger removed");
    Serial.println("Sensor disconnected ");
   

  }

delay(100);
}
 
void updatebeat(){
String cmd = "AT+CIPSTART=\"TCP\",\"";
cmd += IP;
cmd += "\",80";
Serial.println(cmd);
esp8266.println(cmd);
delay(2000);
if(esp8266.find("Error")){
return;
}
cmd = msg ;
cmd += "&field1=";
cmd += BPM;
cmd += "\r\n";
Serial.print("AT+CIPSEND=");
esp8266.print("AT+CIPSEND=");
Serial.println(cmd.length());
esp8266.println(cmd.length());
if(esp8266.find(">")){
Serial.print(cmd);
esp8266.print(cmd);
}
else{
Serial.println("AT+CIPCLOSE");
esp8266.println("AT+CIPCLOSE");
error=1;
}
}
 
boolean connectWiFi(){
Serial.println("AT+CWMODE=1");
esp8266.println("AT+CWMODE=1");
delay(2000);
String cmd="AT+CWJAP=\"";
cmd+=SSID;
cmd+="\",\"";
cmd+=PASS;
cmd+="\"";
Serial.println(cmd);
esp8266.println(cmd);
delay(5000);
if(esp8266.find("OK")){
Serial.println("OK");
return true;
}else{
return false;
}
}
 
void interruptSetup(){
TCCR2A = 0x02; // DISABLE PWM ON DIGITAL PINS 3 AND 11, AND GO INTO CTC MODE
TCCR2B = 0x06; // DON'T FORCE COMPARE, 256 PRESCALER
OCR2A = 0X7C; // SET THE TOP OF THE COUNT TO 124 FOR 500Hz SAMPLE RATE
TIMSK2 = 0x02; // ENABLE INTERRUPT ON MATCH BETWEEN TIMER2 AND OCR2A
sei(); // MAKE SURE GLOBAL INTERRUPTS ARE ENABLED
}
 
ISR(TIMER2_COMPA_vect){ // triggered when Timer2 counts to 124
cli(); // disable interrupts while we do this
Signal = analogRead(pulsePin); // read the Pulse Sensor
sampleCounter += 2; // keep track of the time in mS
int N = sampleCounter - lastBeatTime; // monitor the time since the last beat to avoid noise
 
// find the peak and trough of the pulse wave
if(Signal < thresh && N > (IBI/5)*3){ // avoid dichrotic noise by waiting 3/5 of last IBI
if (Signal < T){ // T is the trough
T = Signal; // keep track of lowest point in pulse wave
}
}
 
if(Signal > thresh && Signal > P){ // thresh condition helps avoid noise
P = Signal; // P is the peak
} // keep track of highest point in pulse wave
 
if (N > 250){ // avoid high frequency noise
if ( (Signal > thresh) && (Pulse == false) && (N > (IBI/5)*3) ){
Pulse = true; // set the Pulse flag when there is a pulse

IBI = sampleCounter - lastBeatTime; // time between beats in mS
lastBeatTime = sampleCounter; // keep track of time for next pulse 
if(secondBeat){ // if this is the second beat
secondBeat = false; // clear secondBeat flag
for(int i=0; i<=9; i++){ // seed the running total to get a realistic BPM at startup
rate[i] = IBI;
}
}
 
if(firstBeat){ // if it's the first time beat is found
firstBeat = false; // clear firstBeat flag
secondBeat = true; // set the second beat flag
sei(); // enable interrupts again
return; // IBI value is unreliable so discard it
}
word runningTotal = 0; // clear the runningTotal variable
 
for(int i=0; i<=8; i++){ // shift data in the rate array
rate[i] = rate[i+1]; // and drop the oldest IBI value
runningTotal += rate[i]; // add up the 9 oldest IBI values
}
 
rate[9] = IBI; // add the latest IBI to the rate array
runningTotal += rate[9]; // add the latest IBI to runningTotal
runningTotal /= 10; // average the last 10 IBI values
BPM = 60000/runningTotal; // how many beats can fit into a minute? that's BPM!
QS = true; // set Quantified Self flag
}
}
 
if (Signal < thresh && Pulse == true){ // when the values are going down, the beat is over
Pulse = false; // reset the Pulse flag so we can do it again
amp = P - T; // get amplitude of the pulse wave
thresh = amp/2 + T; // set thresh at 50% of the amplitude
P = thresh; // reset these for next time
T = thresh;
}
 
if (N > 2500){ // if 2.5 seconds go by without a beat
thresh = 512; // set thresh default
P = 512; // set P default
T = 512; // set T default
lastBeatTime = sampleCounter; // bring the lastBeatTime up to date
firstBeat = true; // set these to avoid noise
secondBeat = false; // when we get the heartbeat back
}
sei();
}
   void bpmTooHigh()
    {
      Serial.println ("Sending Message........\n");
      sim.println("AT+CMGF=1");    //Sets the GSM Module in Text Mode
      delay(1000);
      //Serial.println ("Set SMS Number"); nb
      sim.println("AT+CMGS=\"" + number + "\"\r"); //Mobile phone number to send message
      delay(1000);
      //char value[5];
      //itoa(h,value,10); //convert integer to char array      
      String SMS = "Warning! BPM too high!";
      sim.println(SMS);
      Serial.println(SMS);
      delay(100);
      sim.println((char)26);// ASCII code of CTRL+Z
      delay(1000);
      //_buffer = _readSerial();
    }


   
    void bpmTooLow()
    {
      Serial.println ("Sending Message..........\n");
      sim.println("AT+CMGF=1");    //Sets the GSM Module in Text Mode
      delay(1000);
      //Serial.println ("Set SMS Number");
      sim.println("AT+CMGS=\"" + number + "\"\r"); //Mobile phone number to send message
      delay(1000);
      //char value[5];
      //itoa(h,value,10); //convert integer to char array      
      String SMS = "Warning! BPM too low!";
      sim.println(SMS);
      Serial.println(SMS);
      delay(100);
      sim.println((char)26);// ASCII code of CTRL+Z
      delay(1000);
      //_buffer = _readSerial();
    }
