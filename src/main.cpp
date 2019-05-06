#include <Arduino.h>
#include <RCSwitch.h>

RCSwitch myTransmitter = RCSwitch();

int hum_sensor = 1;               //Pin of humidty sensor
int led_fail = 4;                 //Pin of LED to show Errorcode
int pump = 12;                    //Pin of pump
int ignore_button = 6;            //Pin of button to ignore errorcode
int pump_button = 2;              //interrupt pin 2 for pumping manually
int transmitter = 10;             //D10 is transmitter pin
unsigned long id = 387600000;     //id of this device (for raspi, to know who talks to him)

int hum = 0;                      //value of humidity
int hum_old = 0;                  //for comparison
int count = 0;                    //counts pumping routines in a row
int count2 = 0;                   //counts runs through loops for transmission

int dry = 400 ;                   //pump is on for hum_sensor >dry
int dt = 10;                      //time between two measurements in seconds
int t_pump = 12;                  //time how long pump is on
int transmit_repeat = 5;          //time between transmission of data in minutes
double transmit_count = round(transmit_repeat*60/dt);   //counter for runs through loops till transmit
unsigned long transmission = 0;   //code which will be transmitted

boolean sensors_enabled = true;   //enables or disables routine without sensors
boolean cap_booted = false;

void show_errorcode(int code);
void errorcode_string(int errorcode);
void pump_manually();
void error_routine(int errorcode);
void pumping(int t);


void setup() {
  pinMode(pump, OUTPUT);
  digitalWrite(pump, HIGH);

  pinMode(led_fail, OUTPUT);
  digitalWrite(led_fail, LOW);

  pinMode(hum_sensor, INPUT);
  pinMode(ignore_button, INPUT);

  attachInterrupt(digitalPinToInterrupt(2), pump_manually, RISING);

  myTransmitter.enableTransmit(transmitter);
  myTransmitter.setProtocol(1);    //standard is 1, just a reminder
  myTransmitter.send(id+5000, 32);     //login in bridge

  Serial.begin(9600);

  delay(1000);
}

void loop() {

  if(!cap_booted){
    digitalWrite(led_fail,HIGH);
    Serial.println("boot-routine started");
    int i = 0;
    while(i<8){
      Serial.print("Humidity: ");
      Serial.println(analogRead(hum_sensor));
      i++;
      delay(10000);

    }
    Serial.println("boot-routine finished");
    cap_booted = true;
    digitalWrite(led_fail,LOW);
  }

//disables sensors and pumps "i" times a day
  while(!sensors_enabled){
    int i = 1;                    //"i" times a day
    long d = 86400000/i;          //ms per day divided by times a day
    Serial.print("Delay of: ");
    Serial.print(d);
    Serial.println("ms");
    delay(d);
    pumping(6);
  }

  //read sensors
  hum_old = hum;
  hum = analogRead(hum_sensor);



  //status output
  Serial.println("-------------");
  Serial.print("Humidity: ");
  Serial.println(hum);

  //#pumps output
  if (count != 0){
    Serial.print("pumped before: ");
    Serial.print(count);
    Serial.println("x");
  }

  //checking whether Soil Sensor is in soil
  if (hum>1000)
  {
   error_routine(1);
  }

  //checking for sensor failure
  if ((abs(hum - hum_old)>150) && !(hum_old==0) & (count==0)){
    error_routine(3);
    }

  if (count>=2){
    error_routine(4);
    count=0;
  }



  //checking, whether soil is dry
  if((hum>dry)&&(hum_old>dry))
  {
    count++;
    pumping(t_pump);
    delay(1000);
    transmission = id + 80000 + analogRead(hum_sensor);    //'8' on position 4 indicates automatic pump
    myTransmitter.send(transmission, 32);
    count2 = 0;                   //causes a transmission of hum values
    delay(1000);

  }
  else{
    count=0;
  }

  if ((count2 == transmit_count+1) || (count2 == 0))  {
    transmission = id;
    transmission = transmission + hum;
    myTransmitter.send(transmission, 32);
    Serial.print("transmitted: ");
    Serial.println(transmission);
    count2 = 1;
  }
  else {
    count2++;
  }

  //delay till next measurment.
  delay(dt*1000);
}

void show_errorcode(int code){
  int i = code;
    while (i--)
    {
      digitalWrite(led_fail, HIGH);
      delay(30);
      digitalWrite(led_fail, LOW);
      delay(200);
    }
}

void errorcode_string(int errorcode){       //'string' is now wrong: transmission added, but it's ok
  switch(errorcode){
    case 1: Serial.println("Soil sensor isn't in soil!");
            break;
    case 2: Serial.println("Water level to low!");
            break;
    case 3: Serial.println("Soil sensor failure. Output changed to fast!");
            break;
    case 4: Serial.println("Unknown reason for high pumping ratio!");
            break;
  }
  transmission = id + errorcode*10000;
  myTransmitter.send(transmission,32);
}

void pump_manually(){
  Serial.println("manual pumping started");
  while (digitalRead(pump_button)==HIGH){
    digitalWrite(pump, LOW);
  }
  digitalWrite(pump, HIGH);
  Serial.println("manual pumping stopped");
  delay(1000);
  hum = analogRead(hum_sensor);
  transmission = id + 90000 + hum;       //'9' on position 4 indicates manual pump
  myTransmitter.send(transmission, 32);
}

//stops program and waits for user input
void error_routine(int errorcode){
    bool ignored = false;
    Serial.print("Errorcode #");
    Serial.print(errorcode);
    Serial.print(" - ");
    errorcode_string(errorcode);
    while (!ignored){
      show_errorcode(errorcode);
      ignored = digitalRead(ignore_button) == HIGH;
      delay(500);
    }
    Serial.println("continueing manually");
    hum = analogRead(hum_sensor);
    transmission = id + hum;
    myTransmitter.send(transmission, 32);
 }

 //starts pumping and stops after "t" seconds
 void pumping(int t){
   digitalWrite(pump, LOW);
   Serial.println("pump on");
   delay(t*1000);
   digitalWrite(pump, HIGH);
   Serial.println("pump off");
 }
