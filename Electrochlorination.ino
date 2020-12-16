/*
 * Tim Erwin
 * Automatic Electrochlorination
 * 
 * This device produces sodium hypchlorite by electrolysis of salt water
 * Time to run is set by user through the serial monitor
 * Supply Voltage, Voltage drop across the carbon electrodes, and current is displayed on serial monitor
 * Electrolysis is switched on/off by nMOS transistor
 * Electrolysis can be paused with the power button on the IR remote. While paused, the pH sensor reads values and 
 *    pH value can be saved with the EQ button on IR remote.
 */


#include <IRremote.h> //library for IR RECEIVER
#include <Wire.h>     //libraries used for real time clock module
#include <DS3231.h>   //library for real time clock module

//pH meter
#define Offset -.01     //deviation compensate
#define samplingInterval 20 //interval for pH measurement update
#define printInterval 800   //interval for pH value display
#define ArrayLenth  40      //times of collection
int pHArray[ArrayLenth];    //Store the average value of the sensor feedback
int pHArrayIndex=0;

DS3231 clock;   //object for clock module
RTCDateTime t;  //time variable t

const float FACTOR = (1.08/1023); //convert to voltage value
const int GATE = A0;            //pin for gate of MOSFET. Start/stops electrolysis 
const int SHUNT = A1;           //pin for measuring voltage across shunt resistor
const int SUPPLY = A2;          //pin for measuring supply voltage
const int ELECTRODE = A3;       //pin for measuring voltage after electrodes

const int PHSENSOR = A4;        //pH meter Analog output pin

const int LED = 2;              //indicator LED - pulses when electrolysis is paused - flashes when electrolysis is stopped
const int RECEIVER = 19;        //pin for IR receiver output

//start IR receiver
IRrecv irrecv(RECEIVER);
decode_results results;

//-------------------global variables-------------------//

volatile int paused=LOW;        //variable modified by hardware interrupt. used to check if process is paused
volatile float stored_pH = 0;   //pH value that is displayed on serial monitor
float pHValue;                  //PH value measured by pH sensor

//electrode voltage and current
float Vshunt;        //voltage across current sense resistor
float Vsupply;       //voltage at supply
float Velectrode;    //voltage after electrodes
float Vdrop;         //Vsupply-Velectrode

//time
long timeToRun = 0;   //user-specified time
int pausedMin = 0;    //minutes paused
int oldpausedMin;
int startPauseMin;    //time at pause start
int minutesRemaining; //how many minutes left in process
int secondsRemaining; //how many seconds left in process

void setup() {
  irrecv.enableIRIn();          //Start the IR Receiver 
  clock.begin();                //Starts real time module
  
  Serial.begin(9600);
  
  pinMode(SHUNT,INPUT);     
  pinMode(GATE,OUTPUT);
  pinMode(SUPPLY,INPUT);
  pinMode(ELECTRODE,INPUT);
  pinMode(LED, OUTPUT);

  attachInterrupt(digitalPinToInterrupt(RECEIVER),translateIR, FALLING); /*hardware interrupt triggered by IR receiver. calls the translateIR() function.
                                                                           translateIR() only does something if power or EQ button was pressed */
}

void loop() {
  analogReference(INTERNAL1V1); //sets voltage reference to 1.1V
  //Prompts user for run time at startup or when previouse time expired
  if(timeToRun == 0){
    startupText(); //displays name of device
    Serial.println("Enter Minutes To Run: ");
  }
  //while waiting for user to input run time
  while (timeToRun == 0){
    //pulses indicator LED
    analogWrite(LED,5);
    delay(250);
    digitalWrite(LED,LOW);
    delay(250);
    
    if (Serial.available()>0){
      //saves user input as integer in timeToRun
      timeToRun = Serial.parseFloat();
      clock.setDateTime(0000, 00, 00, 00, 00, 00); //sets clock to start counting at 0
      //resets time remaining countdown
      }
    }
    
  t = clock.getDateTime(); //saves current time
  
  //if process is not paused and there is still time remaining
  if(paused == LOW && minutesRemaining>=0){ 
    digitalWrite(GATE,HIGH);  //turns on nMOS 
    digitalWrite(LED,LOW);    //turn off indicator LED
    readAllVoltages();        //reads voltages at source,shunt,and electrodes
    displayInfo();            //displays time remaining, pH, current and voltages
  }

  //if no time left
  if(minutesRemaining==0 && secondsRemaining<=1){
    Serial.println("DONE");
    digitalWrite(GATE, LOW);  //turn off nMOS
    stored_pH=0;              //reset pH value 
    clock.setDateTime(0000, 00, 00, 00, 00, 00);//start clock at 0
    timeToRun =0;             //reset timeToRun
  }

  //if paused
  if(paused == HIGH){
    digitalWrite(GATE, LOW);  //turn off nMOS
    analogReference(DEFAULT);
    Serial.println("PAUSED"); 
  //pulse LED and run pH sensor
    while(paused == HIGH){
    for(int i=0; i< 255; i++){
      analogWrite(LED,i);
      delay(5);
      readpH(); 
      }
    }
  }
  delay(1000);
}
//prints all info to serial monitor
//comment out if using the displayInfo() below to stream data to Excel
void displayInfo(){
  //calculate time left
  minutesRemaining = (timeToRun-1) - t.minute;
  secondsRemaining = 59 - (t.second);
  
  Serial.print("Time Remaining: "); if(timeToRun - t.minute<=0) minutesRemaining = 0; 
  Serial.print(minutesRemaining); Serial.print(":"); if(secondsRemaining <10) Serial.print("0"); Serial.println(secondsRemaining); 
  Serial.println("-----------------------");
  Serial.print("Supply Voltage: "); Serial.println(Vsupply);
  Serial.print("Voltage Across Electrodes: ");   Serial.println(Vdrop);
  //Serial.print("Final Voltage: ");  Serial.println(Velectrode);
  Serial.print("Current: ");        Serial.println((Vshunt/.1),3);
  Serial.print("Last Saved pH: ");  Serial.println(stored_pH);
  Serial.println("");Serial.println("");Serial.println("");Serial.println("");
}
/*
//display info to stream to Excel
void displayInfo(){
  //calculate time left
  minutesRemaining = (timeToRun-1) - t.minute;
  secondsRemaining = 59 - (t.second);
  
  Serial.print(t.minute); Serial.print(","); Serial.print(t.second); Serial.print(",");
  Serial.print(Vsupply);  Serial.print(",");
  Serial.print(Vdrop);  Serial.print(",");
  Serial.print((Vshunt/.1),3);
  Serial.println();
}
*/
//Reads specified voltage 25 times and returns average
float ReadAverageVoltage(int pin){
  float averageV = 0.000;
  for(int i=1;i<=25;i++){
    averageV = averageV + analogRead(pin);
    //Serial.println(averageV);
  }
  averageV = averageV/25;
  float voltage = averageV * FACTOR;
  return voltage;
}
//calculates shunt, supply, and electrode voltages
void readAllVoltages(){
  Vshunt = ReadAverageVoltage(SHUNT);                       //voltage across current sense resistor
  Vsupply = .3+(ReadAverageVoltage(SUPPLY)/.11111);         //voltage at supply
  Velectrode = .3+(ReadAverageVoltage(ELECTRODE)/.11111);   //voltage after electrode
  Vdrop = Vsupply - Velectrode;                             //voltage across electrodes
}
//function for reading remote controll input
void translateIR(){
  //if an IR signal is received
  if (irrecv.decode(&results)) 
  {
  switch(results.value){
  case 0xFFA25D: paused=!paused; break; //power button signal - sets paused to true/false
  case 0xFF9867: savepH(); break;       //EQ button signal - calls function to save the pH value
  }
    irrecv.resume(); // receive the next value
  }  
  
}
//function for storing and displaying saved pH value
void savepH(){
  stored_pH=pHValue; //saves pH value to screen
  Serial.print("pH: "); Serial.print(stored_pH); Serial.print(" "); Serial.println("saved"); //displays saved pH value
}
//text displayed at startup or when timer expires
void startupText(){
  Serial.println("----------------------------");
  Serial.println("Automatic Electrochlorinator");
  Serial.println("By Tim Erwin");
  Serial.println("----------------------------");
  Serial.println("");
  Serial.println("");
}

//functions for analog pH sensor adapted from example code
void readpH(){ //runs pH sensor and converts voltage to pH value
  static unsigned long samplingTime = millis();
  static unsigned long printTime = millis();
  static float voltage;
  if(millis()-samplingTime > samplingInterval)
  {
      pHArray[pHArrayIndex++]=analogRead(PHSENSOR);
      if(pHArrayIndex==ArrayLenth)pHArrayIndex=0;
      voltage = avergearray(pHArray, ArrayLenth)*5.0/1024;
      pHValue = 3.5*voltage+Offset;
      samplingTime=millis();
  }
  /*if(millis() - printTime > printInterval)   //Every 800 milliseconds, print a numerical, convert the state of the LED indicator
  {
    Serial.println(analogRead(PHSENSOR));
    Serial.print("Voltage:");
        Serial.print(voltage,2);
        Serial.print("    pH value: ");
    Serial.println(pHValue,2);
        //digitalWrite(LED,digitalRead(LED)^1);
        printTime=millis();
  }
  */
}
double avergearray(int* arr, int number){
  int i;
  int max,min;
  double avg;
  long amount=0;
  if(number<=0){
    Serial.println("Error number for the array to avraging!/n");
    return 0;
  }
  if(number<5){   //less than 5, calculated directly statistics
    for(i=0;i<number;i++){
      amount+=arr[i];
    }
    avg = amount/number;
    return avg;
  }else{
    if(arr[0]<arr[1]){
      min = arr[0];max=arr[1];
    }
    else{
      min=arr[1];max=arr[0];
    }
    for(i=2;i<number;i++){
      if(arr[i]<min){
        amount+=min;        //arr<min
        min=arr[i];
      }else {
        if(arr[i]>max){
          amount+=max;    //arr>max
          max=arr[i];
        }else{
          amount+=arr[i]; //min<=arr<=max
        }
      }//if
    }//for
    avg = (double)amount/(number-2);
  }//if
  return avg;
}
