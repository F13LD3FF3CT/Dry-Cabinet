//Need to specifically select Nano Every under "megaAVR Boards"
//Requires Arduino toolbox download

#include <Wire.h>
//#include <Serial.h>
#include <EEPROM.h>
#include "DFRobot_RGBLCD1602.h"
#include "DFRobot_SHT3x.h"

DFRobot_RGBLCD1602 lcd(16,2);  //16 characters and 2 lines of show
DFRobot_SHT3x sht3x;

#define DIS_THRESH 10
#define DEL_LOOP 50
#define DEL_BUT 200

//Pin Definitions
#define bu 13
#define wet 12
#define dry 11
#define heat 10
#define cool 9
#define be 8
#define br 7
#define fcir 6
#define bl 5
#define bd 4
#define fexh 3
#define temp1 A0
#define temp2 A1
#define espd A2
#define cspd A3

//Variables
int exhaust = 255; //Exhaust speed (255-0)
int circ = 255; //Circulation speed (255-0)

//Deadtime, it takes about 15s to recover from high hum situation, so I'd pulse exhaust 30s between bursts
float fhum;
float ftemp;
int hum;
int temp;

int state = 0;
int state_old = 14;
int valmod = 0;
//20ms loop time, values = time (s) *50
unsigned long exh_on = 250;
unsigned long exh_off = 3000;
//Exhaust count variables
unsigned long exh_time = 0;
unsigned long exh_dead = 0;
//Display update variables
int dis_time = 0;



unsigned int exh_h = 65;
unsigned int exh_p = 60;
unsigned int exh_r = 5;
unsigned int heat_1 = 40;
unsigned int heat_0 = 50;
unsigned int cool_1 = 70;
unsigned int cool_0 = 60;
unsigned int hum_1 = 45;
unsigned int hum_0 = 55;
unsigned int dry_1 = 65;
unsigned int dry_0 = 55;

int ht = 0; //Cycle H/T measurements
int mv = 0; //T and H measurements collected/valid
int heat_on = 0;
int cool_on = 0;
int wet_on = 0;
int dry_on = 0;

void save_set();
void recall();
void save_mem();
void reset_mem();

void disp_update();
void meas_update();


void setup() {
  //Configure MOSFET PWM (heat/cool to ~1kHz)
  pinMode(cool, OUTPUT);
  pinMode(heat, OUTPUT);
  pinMode(wet, OUTPUT);
  pinMode(dry, OUTPUT);
  pinMode(fcir, OUTPUT);
  pinMode(fexh, OUTPUT);
  //Configure Fan PWM (30kHz)
  TCB0_CTRLA = (TCB_CLKSEL_CLKDIV2_gc) | (TCB_ENABLE_bm);
  TCB1_CTRLA = (TCB_CLKSEL_CLKDIV2_gc) | (TCB_ENABLE_bm);
  //Set PWM 0
  analogWrite(fcir, 255);
  analogWrite(fexh, 255);
  //analogWrite(cool, 127);
  //analogWrite(heat, 0);
  digitalWrite(heat,LOW);
  digitalWrite(cool,LOW);
  digitalWrite(wet,LOW);
  digitalWrite(dry,LOW);
  //Configure analog in
  pinMode(espd, INPUT);
  pinMode(cspd, INPUT);
  //Configure buttons
  pinMode(br, INPUT);
  pinMode(bd, INPUT);
  pinMode(bu, INPUT);
  pinMode(bl, INPUT);
  pinMode(be, INPUT);
  lcd.init();
  // Print a message to the LCD.
  Wire.setClock(400000);
  lcd.setCursor(2, 0);
  lcd.print("Field");
  lcd.setCursor(8, 1);
  lcd.print("Effect");
  //Initialize the chip
  sht3x.begin();
  delay(1000);  
  if (!digitalRead(be)){
    reset_mem();
  }
  lcd.clear();
  recall();
  Serial.begin(9600);
}






void loop() {
  exh_off = (800/DEL_LOOP)*exh_p;
  exh_on = (800/DEL_LOOP)*exh_r;
  
  //Update at some fractional rate
  if (dis_time == DIS_THRESH){
    dis_time = 0;
    meas_update();
    disp_update();  
  }//If display end
  
  //Read button presses
  if (!digitalRead(bl)){
    if (state == 0){state = 10;}
    else {state = state - 1;}
    delay(DEL_BUT);
  }
  if (!digitalRead(br)){
    if (state == 10){state = 0;}
    else {state = state + 1;}
    delay(DEL_BUT);
  }
  if (!digitalRead(bu)){
    valmod = valmod + 1;
    delay(DEL_BUT);
  }
  if (!digitalRead(bd)){
    valmod = valmod - 1;
    delay(DEL_BUT);
  }
  if (!digitalRead(be)){
    delay(DEL_BUT);
    save_mem();
  }

  //Handle Heat-----------------------------------------------------------------------
  if (temp < heat_1 && heat_on == 0){
    heat_on=1;
    digitalWrite(heat,HIGH);
  } else if (temp > heat_0){
    heat_on=0;
    digitalWrite(heat,LOW);
  }

  //Handle Cool-----------------------------------------------------------------------
  if (temp > cool_1 && cool_on == 0){
    cool_on=1;
    digitalWrite(cool,HIGH);
  } else if (temp < cool_0){
    cool_on=0;
    digitalWrite(cool,LOW);
  }

  //Handle Humidify-----------------------------------------------------------------------
  if (hum < hum_1 && wet_on == 0){
    wet_on=1;
    digitalWrite(wet,HIGH);
  } else if (hum > hum_0){
    wet_on=0;
    digitalWrite(wet,LOW);
  }

  //Handle Dehumidify-----------------------------------------------------------------------
  if (hum > dry_1 && dry_on == 0){
    dry_on=1;
    digitalWrite(dry,HIGH);
  } else if (hum < dry_0){
    dry_on=0;
    digitalWrite(dry,LOW);
  }
   
  //Handle Exhaust PWM-----------------------------------------------------------------
  exhaust = 255-(analogRead(espd)/4);
  if (mv==1){
    if (hum > exh_h && exh_time < exh_on){
      analogWrite(fexh, exhaust);
      exh_time++;  
      exh_dead=0;
    }else{
      analogWrite(fexh, 256);      
      if (exh_dead < exh_off) exh_dead++;
      if (exh_dead == exh_off) exh_time = 0;
    }
  }

  //Update Circulation PWM-------------------------------------------------------------
  circ = 255-(analogRead(cspd)/4); //Circ fan always runs at speed set on pot
  analogWrite(fcir,circ);

  //Loop Maint-------------------------------------------------------------------------
  dis_time++; //Display count index   
  delay(DEL_LOOP); //Update delay
  
}





//------------------------------------

void meas_update(){
    if (ht == 0){
      ftemp = sht3x.getTemperatureF();
      temp = round(ftemp);
      lcd.setCursor(0, 0);
      lcd.print("T=");
      lcd.print(String(temp));
      lcd.print("F");
      ht = 1;
    }else if (ht==1){
      fhum = sht3x.getHumidityRH();
      hum = round(fhum);
      lcd.setCursor(0, 1);
      lcd.print("H=");
      lcd.print(String(hum));
      lcd.print("%");
      ht=0; 
      mv=1; //Measurement good, control enabled
    }
}


void disp_update(){
  switch (state){
      case 0: //Exhaust humidity adjust
        exh_h = exh_h + valmod;
        if (exh_h >= 100) exh_h = 99;
        if (state_old != state){
          lcd.setCursor(5, 0);
          lcd.print("   EXH H>");
          lcd.print(String(exh_h));
          lcd.print(" ");
          lcd.setCursor(7, 1);
          lcd.print(" ON   RH%"); 
        }else{
          lcd.setCursor(14, 0);
          lcd.print(String(exh_h));
          lcd.print(" ");
        }
        state_old = 0;   
        break;
        
      case 1: //Exhaust min off time
        exh_p = exh_p + valmod;
        if (exh_p >=100) exh_p=99;
        if (state_old != state){
          lcd.setCursor(5, 0);
          lcd.print(" EX WAIT=");
          lcd.print(String(exh_p));
          lcd.print(" ");
          lcd.setCursor(7, 1);
          lcd.print(" TIME sec");
        }else{
          lcd.setCursor(14, 0);
          lcd.print(String(exh_p));
          lcd.print(" ");
        }
        state_old = 1;
        break;
        
      case 2: //Exhaust on time
        exh_r = exh_r + valmod;
        if (exh_r >=100) exh_r=99;
        if (exh_r <=3) exh_r=3;
        if (state_old != state){
          lcd.setCursor(5, 0);
          lcd.print("   EX ON=");
          lcd.print(String(exh_r));
          lcd.print(" ");
          lcd.setCursor(7, 1);
          lcd.print(" TIME sec");
        }else{
          lcd.setCursor(14, 0);
          lcd.print(String(exh_r));
          lcd.print(" ");
        }
        state_old=2;
        break;
        
       case 3: //Heat on temp
        heat_1 = heat_1 + valmod;
        if (heat_1 >=100) heat_1=99;
        if (heat_1 <=32) heat_1 = 33;
        if (heat_1 >= heat_0){
          heat_1 = heat_0-1;
          lcd.setCursor(0, 0);
          lcd.print("ERR  ");
        }
        if (state_old != state){
          lcd.setCursor(5, 0);
          lcd.print(" HEAT ON<");
          lcd.print(String(heat_1));
          lcd.print(" ");
          lcd.setCursor(7, 1);
          lcd.print("  BELOW F");
        }else{
          lcd.setCursor(14, 0);
          lcd.print(String(heat_1));
          lcd.print(" ");
        }
        state_old=3;
        break;
        
      case 4: //Heat off temp
        heat_0 = heat_0 + valmod;
        if (heat_0 >=100) heat_0=99;
        if (heat_0 <=32) heat_0 = 33;
        if (heat_0 <= heat_1){
          heat_0 = heat_1+1;
          lcd.setCursor(0, 0);
          lcd.print("ERR  ");
        }
        if (state_old != state){
          lcd.setCursor(5, 0);
          lcd.print("HEAT OFF>");
          lcd.print(String(heat_0));
          lcd.print(" ");
          lcd.setCursor(7, 1);
          lcd.print("  ABOVE F");
        }else{
          lcd.setCursor(14, 0);
          lcd.print(String(heat_0));
          lcd.print(" ");
        }
        state_old=4;
        break;
        
      case 5: //Cool on temp
        cool_1 = cool_1 + valmod;
        if (cool_1 >= 100) cool_1=99;
        if (cool_1 <= 32) cool_1=33;
        if (cool_1 <= cool_0){
          cool_1 = cool_0+1;
          lcd.setCursor(0, 0);
          lcd.print("ERR  ");
        }      
        if (state_old != state){
          lcd.setCursor(5, 0);
          lcd.print(" COOL ON>");
          lcd.print(String(cool_1));
          lcd.print(" ");
          lcd.setCursor(7, 1);
          lcd.print("  ABOVE F");
        }else{
          lcd.setCursor(14, 0);
          lcd.print(String(cool_1));
          lcd.print(" ");
        }
        state_old=5;
        break;
      case 6: //Cool off temp
        cool_0 = cool_0 + valmod;
        if (cool_0 >= 100) cool_1=99;
        if (cool_0 <= 32) cool_1=33;
        if (cool_0 >= cool_1){
          cool_0 = cool_1-1;
          lcd.setCursor(0, 0);
          lcd.print("ERR  ");
        }      
        if (state_old != state){
          lcd.setCursor(5, 0);
          lcd.print("COOL OFF<");
          lcd.print(String(cool_0));
          lcd.print(" ");
          lcd.setCursor(7, 1);
          lcd.print("  BELOW F");
        }else{
          lcd.setCursor(14, 0);
          lcd.print(String(cool_0));
          lcd.print(" ");
        }
        state_old=6;
        break;
        
      case 7: //Hum on RH
        hum_1 = hum_1 + valmod;
        if (hum_1 >= 100) hum_1=99;
        if (hum_1 >= hum_0){
          hum_1 = hum_0-1;
          lcd.setCursor(0, 0);
          lcd.print("ERR  ");
        }      
        if (state_old != state){
          lcd.setCursor(5, 0);
          lcd.print("  HUM ON<");
          lcd.print(String(hum_1));
          lcd.print(" ");
          lcd.setCursor(7, 1);
          lcd.print("BELOW RH%");
        }else{
          lcd.setCursor(14, 0);
          lcd.print(String(hum_1));
          lcd.print(" ");
        }
        state_old=7;
        break;
        
      case 8: //Hum off RH
        hum_0 = hum_0 + valmod;
        if (hum_0 >= 100) hum_0=99;
        if (hum_0 <= hum_1){
          hum_0 = hum_1+1;
          lcd.setCursor(0, 0);
          lcd.print("ERR  ");
        }  
        if (state_old != state){
          lcd.setCursor(5, 0);
          lcd.print(" HUM OFF>");
          lcd.print(String(hum_0));
          lcd.print(" ");
          lcd.setCursor(7, 1);
          lcd.print("ABOVE RH%");
        }else{
          lcd.setCursor(14, 0);
          lcd.print(String(hum_0));
          lcd.print(" ");
        }
        state_old=8;
        break;

        
      case 9:
        dry_1 = dry_1 + valmod;
        if (dry_1 >= 100) dry_1=99;
        if (dry_1 <= dry_0){
          dry_1 = dry_0+1;
          lcd.setCursor(0, 0);
          lcd.print("ERR  ");
        }  
        if (state_old != state){
          lcd.setCursor(5, 0);
          lcd.print("  DRY ON>");
          lcd.print(String(dry_1));
          lcd.print(" ");
          lcd.setCursor(7, 1);
          lcd.print("ABOVE RH%");
        }else{
          lcd.setCursor(14, 0);
          lcd.print(String(dry_1));
          lcd.print(" ");
        }
        state_old=9;
        break;
        
      case 10:
        dry_0 = dry_0 + valmod;
        if (dry_0 >= 100) dry_0=99;
        if (dry_0 >= dry_1){
          dry_0 = dry_1-1;
          lcd.setCursor(0, 0);
          lcd.print("ERR  ");
        }  
        if (state_old != state){
          lcd.setCursor(5, 0);
          lcd.print(" DRY OFF<");
          lcd.print(String(dry_0));
          lcd.print(" ");
          lcd.setCursor(7, 1);
          lcd.print("BELOW RH%");
        }else{
          lcd.setCursor(14, 0);
          lcd.print(String(dry_0));
          lcd.print(" ");
        }
        state_old=10;
        break;


      default:
        break;  
        
    }   //End Switch case
    valmod=0;
}



void save_set(){
  EEPROM.put(0x00,exh_h);
  EEPROM.put(0x02,exh_p);
  EEPROM.put(0x04,exh_r);
  EEPROM.put(0x06,heat_1);
  EEPROM.put(0x08,heat_0);
  EEPROM.put(0x0A,cool_1);
  EEPROM.put(0x0C,cool_0);
  EEPROM.put(0x10,hum_1);
  EEPROM.put(0x12,hum_0);
  EEPROM.put(0x14,dry_1);
  EEPROM.put(0x16,dry_0);  
}

void recall(){
  EEPROM.get(0x00,exh_h);
  EEPROM.get(0x02,exh_p);
  EEPROM.get(0x04,exh_r);
  EEPROM.get(0x06,heat_1);
  EEPROM.get(0x08,heat_0);
  EEPROM.get(0x0A,cool_1);
  EEPROM.get(0x0C,cool_0);
  EEPROM.get(0x10,hum_1);
  EEPROM.get(0x12,hum_0);
  EEPROM.get(0x14,dry_1);
  EEPROM.get(0x16,dry_0);
}

void save_mem(){
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("  Hold to Save");
  lcd.setCursor(4, 1);
  lcd.print("Settings");
  delay(300);
  if (digitalRead(be)){lcd.clear(); return;}
  lcd.print(".");
  delay(300);
  if (digitalRead(be)){lcd.clear(); return;}
  lcd.print(".");
  delay(300);
  if (digitalRead(be)){lcd.clear(); return;}
  lcd.print(".");
  delay(300);
  if (digitalRead(be)){lcd.clear(); return;}
  lcd.print(".");  
  delay(300);
  save_set();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Settings");
  lcd.setCursor(10, 1);
  lcd.print("Saved!");
  delay(2000);
  state_old = 15;
  lcd.clear();
}

void reset_mem(){
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("  Hold to Reset");
  lcd.setCursor(4, 1);
  lcd.print("Settings");
  delay(300);
  if (digitalRead(be)){lcd.clear(); return;}
  lcd.print(".");
  delay(300);
  if (digitalRead(be)){lcd.clear(); return;}
  lcd.print(".");
  delay(300);
  if (digitalRead(be)){lcd.clear(); return;}
  lcd.print(".");
  delay(300);
  if (digitalRead(be)){lcd.clear(); return;}
  lcd.print(".");  
  delay(300);
  save_set();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Settings");
  lcd.setCursor(10, 1);
  lcd.print("Reset!");
  delay(2000);
  lcd.clear();
}
