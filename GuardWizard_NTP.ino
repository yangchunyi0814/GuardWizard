//
// Copyright (c) 2019/9/27 
// Author: Eric Yang
//
// Description :
//   Dust sensor - PMS5003T BH1750 PIR
//   Environment:
//     DSI5168 1.0
//     Arduino 1.8.9
//
// Connections :
//   PMS5003T => Serial Port
//
// Required Library :
//   DFRobot_LCD BH1750 Conplug_PMS5003T TimeLib
//   TimeLib              https://github.com/PaulStoffregen/Time
//   DFRobot              https://github.com/bearwaterfall/DFRobot_LCD-master
//   Conplug_PMS5003T     https://github.com/Conplug/Conplug_PMS5003T
//   BH1750               https://github.com/claws/BH1750
//
#include <WiFi.h>
#include <WiFiUdp.h>
//#include "wifi_data.h"
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "DFRobot_LCD.h"
#include "Conplug_PMS5003T.h"
#include "SoftwareSerial.h"
#include <BH1750.h>
#include <TimeLib.h>
#include <FlashMemory.h>


int connectTimeout = 20000;
int status = WL_IDLE_STATUS;
char ssid[] = "Pos";  //  your network SSID (name)
char pass[] = "0222435183";       // your network password
//char ssid[] = "EricYang";
//char pass[] = "66666666";

int keyIndex = 0;            // your network key Index number (needed only for WEP)
int bootCounter = 0;         // 統計 reset 次數
// NTP Servers:
char timeServer[] = "time.stdtime.gov.tw";
const int timeZone = 8;     // Beijing Time, Taipei Time

// A UDP instance to let us send and receive packets over UDP
WiFiUDP Udp;
unsigned int localPort = 2390;  // local port to listen for UDP packets

time_t getNtpTime();
void sendNTPpacket(IPAddress &address);

BH1750 lightMeter;  // 照度偵測器 宣告
LiquidCrystal_I2C lcd2(0x27, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE); //第二個LCD
DFRobot_LCD lcd(16,2);                                          //第一個LCD
SoftwareSerial SerialSensor(0, 1); // RX, TX
Conplug_PMS5003T Pms(&SerialSensor); //PMS 宣告

const int Set_BUTTON = 12;        //D12 Pin
const int PIR_PIN = 2;            //PIR Pin
const int Konb_PIN = A2;          //旋鈕 Pin

unsigned long startTime,nowTime;  // 程式起始時間與目前時間
unsigned long WifiConnectingTimeout; //連線逾時時間
float luxValue;                   // 照度
int unlockKey = 0;                // 按鈕按下狀態設定
int cut_apart = 4;                // LCD螢幕頁數設定
int displayValue=0;               // 目前顯示 LCD的頁數

int luxValueAlarm = 30;           //設定光線警告值
int pm25ValueAlarm = 50;          //設定PM25警告值
int tempValueAlarm = 30;          //設定溫度警告值



const int buzzer = 10;


void setup() {
      
   Serial.begin(9600); 
   SerialSensor.begin(9600); 
   
   pinMode(PIR_PIN,INPUT);
   //pinMode(Set_BUTTON,INPUT);
   
   pinMode(Set_BUTTON, INPUT_IRQ_RISE);
   digitalSetIrqHandler(Set_BUTTON, keyPress);
   
   pinMode(Konb_PIN,INPUT);            //旋鈕

   Serial.println("Connected to wifi");
 
   Wire.begin();

  //
  // Sensors must be initialized later.
  //
  
  Pms.begin();
  lightMeter.begin();    // 照度感測器啟用 

  lcd.init();            // DFROBOT 16 *2 LCD 初始化
  lcd.clear();           //清除LCD畫面

  lcd2.begin(16,2);               // 第二個LCD 初始化
  lcd2.backlight();

  lcd2.setCursor ( 0, 0 );       
  lcd2.print("Hello World!");  
  lcd2.setCursor ( 0, 1 );        
  lcd2.print ("DHI5168 Ver 0.2b");  

   if (WiFi.status() == WL_NO_SHIELD) {
    Serial.println("WiFi shield not present");
    // don't continue:
    while (true);
  }

  String fv = WiFi.firmwareVersion();
  if (fv != "1.1.0") {
    Serial.println("Please upgrade the firmware");
  }

  // attempt to connect to Wifi network:
  
  WifiConnectingTimeout = millis();
  while (status != WL_CONNECTED) {
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);
                                                                // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    status = WiFi.begin(ssid, pass);
    
    lcd.setCursor ( 0, 0 );        
    lcd.print("Connect ...");
    if((millis() - WifiConnectingTimeout) > 10000) //等待 10秒
        break; 
    lcd.setCursor(14, 0); 
    lcd.print((int)((millis() - WifiConnectingTimeout) / 1000));   
                                                                  // wait 0.5 seconds for connection:
    delay(500);
  }
 
  
  lcd.setCursor ( 0, 0 ); 
  if ( status == WL_CONNECTED ){
       lcd.print("waiting for sync");
       Udp.begin(localPort);
  }
  else
     lcd.print("no network !!   ");
     
  setSyncProvider(getNtpTime);
  setSyncInterval(300);
  delay(1000);
  startTime =  millis();          //設定程式開始執行的時間
  lcd2.clear();

  bootCount();                    //從 flash 寫入並 讀出 開機次數
}
void loop() {

  int posValue =0;         

  nowTime =  millis() - startTime ;  // 到目前的執行時間
  posValue = analogRead(Konb_PIN);         //讀取旋鈕位置
  
  displayValue = map(posValue, 0, 1023, 1, cut_apart); // 從0-1024中旋鈕位置值,來對映幾等分的位置
   
  switch(displayValue)               //依據旋鈕位置決定顯示的資訊
  {
    case 1 :
            displayPage1();          //顯示PM2.5 與溫溼度
            break;   
    case 2 :
            displayPage2();          //顯示照度 與 PIR偵測
            break;
    case 3 :
            displayPage3();          //顯示連網資訊
            break;
    case 4 :
            displayPage4();          //顯示晶片資訊
            break;

   }

}

void displayPage1()   //---------- Running readPms before running pm2_5, temp, humi and readDeviceType----------
{

  char pm25Str[5],runTime[7],realTimeStr[9];
  
  PMS5003T_DATA* pd = 0;
  if(pd = Pms.readPms()) {
    if(Pms.readDeviceType() == Conplug_PMS5003T::PMS5003T) {
      Serial.println("PMS5003T is detected.");

      /*lcd.setCursor ( 0, 0 );          //讀取目前程式執行的時間 呈現於 LCD
      lcd.print("Run:");  
      lcd.setCursor ( 4, 0 ); 
      sprintf(runTime,"%-6d",long(nowTime/1000));   //定義格式 
      lcd.print (runTime); */
      
      lcd.setCursor ( 0, 1 );          //讀取 PM2.5 呈現於 LCD
      lcd.print("PM2.5: ");  
      lcd.setCursor ( 6, 1 ); 
      sprintf(pm25Str,"%-4d",Pms.pm2_5());         //定義格式    
      lcd.print (pm25Str);
      
      lcd.setCursor ( 10, 1 );         //讀取溫溼度 呈現於 LCD
      lcd.print("T:");  
      lcd.setCursor ( 12, 1 );
      lcd.print (Pms.temp());

      lcd.setCursor ( 10, 0 );        
      lcd.print("H:");  
      lcd.setCursor ( 12, 0 );
      lcd.print (Pms.humi());
       
    }
    else {
      //Serial.println("PMS3003 is detected.");

      PMS3003_DATA* pd3003 = (PMS3003_DATA*)pd;

    }
  }
  else {
    Serial.println("PMS data format is wrong.");
  }
  if( Pms.pm2_5() > pm25ValueAlarm) {  //造成閃爍狀況
    lcd.setCursor ( 6, 1 );
    lcd.print("    "); 
  }
  if( Pms.temp() > tempValueAlarm) {   //造成閃爍狀況
    lcd.setCursor ( 12, 1 );
    lcd.print("    "); 
  }
  lcd.setCursor ( 0, 0 );
  sprintf(realTimeStr,"%02d:%02d:%02d  ",hour(),minute(),second());
  lcd.print(realTimeStr);
  
  delay(300); 
  
}

void displayPage2()  //----------PIR 偵測與警示按鈕設定--------------------
{
  
  char luxStr[5];
  int PIR_Value;  

//------------------Read Light Lux------------------------------------
  luxValue = lightMeter.readLightLevel();    //讀取 光度 呈現於 LCD
  lcd.setCursor ( 0, 0 );
  lcd.print("Lux: ");
  lcd.setCursor ( 5, 0 );
  sprintf(luxStr,"%-4d",(int)luxValue);
  lcd.print(luxStr);
  
//------------------Read LockButton------------------------------------
    
  if( unlockKey )
  {
     lcd.setCursor ( 8, 0 );
     lcd.print("     Set");   
  }
  else
  {
     lcd.setCursor ( 8, 0 );
     lcd.print("   Unset");       
  }
 
 //----------------- Raed PIR Status--------------------------------------
      
  PIR_Value = digitalRead(PIR_PIN);
  
  Serial.print("PIR: ");                     
  Serial.println(PIR_Value); 
  
  lcd.setCursor ( 0, 1 );
  lcd.print("PIR: ");
  lcd.setCursor ( 5, 1 );                       //偵測到人體感應時 LCD呈現 Alarm
  if( PIR_Value )
     lcd.print("Alarm      ");
  else
     lcd.print("            "); 
     
  if(PIR_Value  && unlockKey ) {                //偵測到人體且按鈕設定時,蜂鳴器會產生叫聲
    
    lcd.setCursor ( 5, 1 );     //造成閃爍狀況
    lcd.print("            ");
    alarmBeep(1);
  }   
  else   
      alarmBeep(0);

  if(luxValue < luxValueAlarm) {  //造成閃爍狀況
    lcd.setCursor ( 5, 0 );
    lcd.print("    ");
  }
    
}

void displayPage3()  //-----------顯示網路資訊-----------------
{
  char SSID_Str[11];
   
  lcd.setCursor ( 0, 0 );        
  lcd.print("SSID:");  
  lcd.setCursor ( 0, 1 );        
  lcd.print("IP:");          
  
  if(status == WL_CONNECTED ){
    lcd.setCursor ( 5, 0 );  
    sprintf(SSID_Str,"%-11s",WiFi.SSID());
    lcd.print (SSID_Str); 
    lcd.setCursor ( 3, 1 );        // go to the next line
    lcd.print (WiFi.localIP()); 
  }
  else {
    lcd.setCursor ( 5, 0 );  
    lcd.print ("no network "); 
    lcd.setCursor ( 3, 1 );        // go to the next line
    lcd.print ("X .X .X .X     "); 
  }

}

void displayPage4()  //-----------顯示日期與晶片代號-----------------
{
  char today_Str[11];
  char bootCounter_Str[4];
  char *weekdayStr[7] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"}; 
  lcd.setCursor ( 0, 0 );
  sprintf(today_Str,"%4d/%02d/%02d",year(),month(),day());
  lcd.print(today_Str);
  lcd.setCursor(13,0);
  lcd.print(*(weekdayStr+weekday()-1));    // Sun 為 weekday = 1 

  lcd.setCursor ( 0, 1 );                  //顯示重新啟動的次數
  lcd.print("Rst counter: ");
  lcd.setCursor ( 13, 1 );
  sprintf(bootCounter_Str,"%3d",bootCounter);   
  lcd.print(bootCounter_Str); 
  
}

void alarmBeep(int Status)                //蜂鳴器發聲
{
  if(Status)
    for ( int ii=0; ii<10; ii++ ) {
        tone(buzzer,5000);
        delay(50);
        tone(buzzer,2500);
        delay(50);
        
    }
   else
      noTone(buzzer);
}

void keyPress(uint32_t id, uint32_t event)
{
  if( digitalRead(Set_BUTTON))              //偵測若有按鈕,就於LCD上 呈現"Unset" 啟動 
     unlockKey = !unlockKey;
  Serial.println("SetButton Press...");
  
}
void lcdClear() //填入空白清除螢幕 用 LcdClear() 會造螢幕閃爍
{

     lcd.setCursor(0, 0);
     lcd.print("              ");
     lcd.setCursor(0, 1);
     lcd.print("              ");
}

/*-------- NTP code ----------*/

const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

time_t getNtpTime()
{
  Serial.println("Transmit NTP Request");
  Udp.setRecvTimeout(1500);
  sendNTPpacket();
  if ( Udp.read(packetBuffer, NTP_PACKET_SIZE) > 0 ) {
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
  } else {
    Serial.println("No NTP Response :-(");
    return 0; // return 0 if unable to get the time
  }
}

// send an NTP request to the time server at the given address
void sendNTPpacket()
{
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
  Udp.beginPacket(timeServer, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}

void bootCount()
{
  FlashMemory.read();
  if (FlashMemory.buf[0] == 0xFF) {
    FlashMemory.buf[0] = 0x00;
    FlashMemory.update();
    Serial.println("write count to 0");
  } 
  else {
    FlashMemory.buf[0]++;
    FlashMemory.update();
    bootCounter = FlashMemory.buf[0];
  }  
}
