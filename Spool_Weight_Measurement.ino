#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <WiFiClient.h>
#include <EEPROM.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include "HX711.h"  //You must have this library in your arduino library folder

MDNSResponder mdns;
 /////////////////////////
// Network Definitions //
/////////////////////////
const IPAddress AP_IP(192, 168, 1, 1);
 char* AP_SSID = "Weight Measurement";
boolean SETUP_MODE;
String SSID_LIST;
DNSServer DNS_SERVER;
ESP8266WebServer WEB_SERVER(80);

/////////////////////////
// Device Definitions //
/////////////////////////
String DEVICE_TITLE = "Weight Measurement";
boolean POWER_SAVE = false;

#define DOUT  12
#define CLK  14
 
HX711 scale(DOUT, CLK);
 
//Change this calibration factor as per your load cell once it is found you many need to vary it in thousands
float calibration_factor = -656650; //-106600 worked for my 40Kg max scale setup 
 
#define RTCMEMORYSTART 65
#define RTCMEMORYLEN 127
ADC_MODE(ADC_VCC); //vcc read-mode

#define SLEEP_TIME 3600*1*1000000
#define WIFI_CONNECT_TIMEOUT_S 20                   // max time for wifi connect to router, if exceeded restart
uint32_t time1, time2;
#define VCC_ADJ 1.096
extern "C" {
#include "user_interface.h" // this is for the RTC memory read/write functions
}

typedef struct {
  int count;
  int hours;
} rtcStore;

rtcStore rtcMem;

typedef struct {
  int weight1=13;
  int weight2=17;
} rtcStore1;

rtcStore1 rtcMem1;


bool tare = true;
float weight;
int i;
int buckets;
bool toggleFlag;
boolean scaleWeight = true;
String apiKey = "3GX4QWKXD1WKKZ0L";
float w;
float weightBeforeTaring;
float rawWeight;
WiFiClient client;

float t;
int setupButton = 5;
int setupButtonState;
String up_lim = "";
String low_lim = "";
String webpage;
String tare_weight;
String wifi_ssid = "";
String passwd = "";
void initHardware()
     {
       // Serial and EEPROM
       Serial.begin(115200);
       EEPROM.begin(512);
       delay(10);
       // LEDS

      }
/////////////////////////////
// AP Setup Mode Functions //
/////////////////////////////

// Load Saved Configuration from EEPROM
boolean loadSavedConfig() 
        {
          Serial.println("Reading Saved Config....");
          String ssid = "";
          String password = "";
          String upper_Limit = "";
          if (EEPROM.read(0) != 0)
             {
               for (int i = 0; i < 32; ++i) 
                   {
                     ssid += char(EEPROM.read(i));
                   }
               Serial.print("SSID: ");
               Serial.println(ssid);
              
               for (int i = 32; i < 96; ++i) 
                   {
                     password += char(EEPROM.read(i));
                   }
               Serial.print("Password: ");
               Serial.println(password);
              /*
               //temp limits
               for (int i = 96; i < 99; ++i) 
                   {
                     upper_Limit += char(EEPROM.read(i));
                   }
               Serial.print("upper limit: ");
               Serial.println(upper_Limit);
               up_lim = upper_Limit;

              */

              // WiFi.config(ip, gateway, subnet);
              WiFi.begin(ssid.c_str(), password.c_str());
              Serial.println(WiFi.macAddress());

              return true;
            }
          else 
            {
              Serial.println("Saved Configuration not found.");
              return false;
            }
        }

// Boolean function to check for a WiFi Connection
boolean checkWiFiConnection() 
        {
          int count = 0;
          Serial.print("Waiting to connect to the specified WiFi network");
          while ( count < 30 ) 
                {
                  if (WiFi.status() == WL_CONNECTED) 
                     {
                       Serial.println();
                       Serial.println("Connected!");
                       
                       return (true);
                      }
                  delay(500);
                  Serial.print(".");
                  count++;
               }
       Serial.println("Timed out.");
       // display.setFont(&FreeMono9pt7b);
      
       return false;
     }

// Start the web server and build out pages
void startWebServer()
     {
       if (SETUP_MODE) 
          {
            Serial.print("Starting Web Server at IP address: ");
            Serial.println(WiFi.softAPIP());
            // Settings Page
            WEB_SERVER.on("/settings", []() 
                 {
                   String s = "<h2>Wi-Fi Settings</h2>";
                   s += "<p>Please select the SSID of the network you wish to connect to and then enter the password and submit.</p>";
                   s += "<form method=\"get\" action=\"setap\"><label>SSID: </label><select name=\"ssid\">";
                   s += SSID_LIST;
                   s += "</select>";
                   s += "<br><br>Password: <input name=\"pass\" length=64 type=\"password\"><br><br>";
                   
                   // s +="<input type=\"submit\"></form>";
                 //  s += "<h3>Minimum Weight Notification Setting</h3>";
                  // s += "<p>Please select the Lower limit of weight for notification alert .</p>";
                  // s += "<br><br>Lower Limit For Alert: <input name=\"Upper_limit\" length=2 type=\"upper_Limit\"><br><br>";
                  // s += "<br><br>Lower Limit: <input name=\"Lower_limit\" length=2 type=\"lower_Limit\"><br><br>";
                   s += "<input type=\"submit\"></form>";
                   WEB_SERVER.send(200, "text/html", makePage("Wi-Fi Settings", s));
                });
              // setap Form Post
              WEB_SERVER.on("/setap", []() 
                  {
                   for (int i = 0; i < 103; ++i)
                       {
                         EEPROM.write(i, 0);
                       }
                   String ssid = urlDecode(WEB_SERVER.arg("ssid"));
                   Serial.print("SSID: ");
                   Serial.println(ssid);
    
                   String pass = urlDecode(WEB_SERVER.arg("pass"));
                   Serial.print("Password: ");
                   Serial.println(pass);
     
                   String Upper_Limit = urlDecode(WEB_SERVER.arg("Upper_limit"));
                   Serial.print("Upper Limit: ");
                   Serial.println(Upper_Limit);
     
                   //String Lower_Limit = urlDecode(WEB_SERVER.arg("Lower_limit"));
                   //Serial.print("Lower Limit: ");
                   //Serial.println(Lower_Limit);
      
                 
                   Serial.println("Writing SSID to EEPROM...");
                   for (int i = 0; i < ssid.length(); ++i)
                       {
                         EEPROM.write(i, ssid[i]);
                       }
                   
                    Serial.println("Writing Password to EEPROM...");
                    for (int i = 0; i < pass.length(); ++i) 
                        {
                          EEPROM.write(32 + i, pass[i]);
                        }
                   
                    //Serial.println("Writing Upper Limit to EEPROM...");
                    //for (int i = 0; i < Upper_Limit.length(); ++i) 
                    //    {
                    //      EEPROM.write(96 + i, Upper_Limit[i]);
                    //    }
                 
                  //Serial.println("Writing Lower Limit to EEPROM...");
                  //for (int i = 0; i < Lower_Limit.length(); ++i) 
                    //  {
                    //    EEPROM.write(98 + i, Lower_Limit[i]);
                     // }
 
                  
                 
                 EEPROM.commit();
                 Serial.println("Write EEPROM done!");
                 String s = "<h1>WiFi Setup complete.</h1>";
                 s += "<p>The device1 will be connected automatically to \"";
                 s += ssid;
                 s += "\" after the restart.";
                 WEB_SERVER.send(200, "text/html", makePage("Wi-Fi Settings", s));
                 ESP.restart();
             });
        // Show the configuration page if no path is specified
        WEB_SERVER.onNotFound([]() 
            {
              String s = "<h1>WiFi Configuration Mode</h1>";
              s += "<p><a href=\"/settings\">Wi-Fi Settings</a></p>";
              WEB_SERVER.send(200, "text/html", makePage("Access Point mode", s));
            });
       }
     else 
       {
         Serial.print("Starting Web Server at ");
         Serial.println(WiFi.localIP());
         WEB_SERVER.on("/", []() {
         IPAddress ip = WiFi.localIP();
         String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
         String s = "<h1>Filament Weight Measurement</h1>";
         s += "<h3>Network Details</h3>";
         s += "<p>Connected to: " + String(WiFi.SSID()) + "</p>";
         s += "<p>IP Address: " + ipStr + "</p>";
         s += "<p>MAC Address: " + String(WiFi.macAddress()) +  "</p>";
         s += "<h3>Weight(gm): " + String(w) + " Grams</h3>";
         s += "<h3>Click for tare</h3>";
         s += "<p><a href=\"tare_weight\"><button>Tare Weight</button></a>&nbsp";
        // s += "<h3>Temperature Details</h3>";
        // s += "<p>Minimum Weight for notification: " + up_lim + "Grams" + "</p>";
       //  s += "<p>Lower Limit: " + low_lim + "</p>";
         s += "<h3>Options</h3>";
         s += "<p><a href=\"/reset\">Clear Saved Wi-Fi Settings</a></p>";
         WEB_SERVER.send(200, "text/html", makePage("Station mode", s));
         if(mdns.begin("/",WiFi.localIP()))
          {
            Serial.println("MDNS responder started");
          }
        webpage =s;  
       WEB_SERVER.on("/tare_weight", [](){
             WEB_SERVER.send(200, "text/html",  makePage("Station mode", webpage));
             Serial.println("Tare Weight");
             scale.tare();             //Reset the scale to 0  
             delay(1000);
             Serial.println("done");
             tare = false;
          });
       });
    WEB_SERVER.on("/reset", []()
       {
         for (int i = 0; i < 96; ++i) 
             {
               EEPROM.write(i, 0);
             }
         EEPROM.commit();
         String s = "<h1>Wi-Fi settings was reset.</h1><p>Please reset device.</p>";
         WEB_SERVER.send(200, "text/html", makePage("Reset Wi-Fi Settings", s));
       });
     }
    WEB_SERVER.begin();
    //triggerButtonEvent(IFTTT_NOTIFICATION_EVENT);
   }

// Build the SSID list and setup a software access point for setup mode
void setupMode()
     {
       WiFi.mode(WIFI_STA);
       WiFi.disconnect();
       delay(100);
       int n = WiFi.scanNetworks();
       delay(100);
       Serial.println("");
       for (int i = 0; i < n; ++i) 
           {
             SSID_LIST += "<option value=\"";
             SSID_LIST += WiFi.SSID(i);
             SSID_LIST += "\">";
             SSID_LIST += WiFi.SSID(i);
             SSID_LIST += "</option>";
           }
       delay(100);
       WiFi.mode(WIFI_AP);
       // WiFi.config(ip, gateway, subnet);
       WiFi.softAPConfig(AP_IP, AP_IP, IPAddress(255, 255, 255, 0));
       WiFi.softAP(AP_SSID);
       DNS_SERVER.start(53, "*", AP_IP);
       startWebServer();
       Serial.print("Starting Access Point at \"");
       Serial.print(AP_SSID);
       Serial.println("\"");
     }


        
void setup() 
    {
      Serial.begin(115200);
      scale.set_scale(-656650);
      scaleWeight = true;
      pinMode(setupButton, INPUT_PULLUP);
      setupButtonState = digitalRead(setupButton);
      Serial.print("State: ");
      Serial.println(setupButtonState);
      EEPROM.begin(512);
     // int rtcPos1 = RTCMEMORYSTART + buckets;
     // system_rtc_mem_read(rtcPos1, &rtcMem1, sizeof(rtcMem1));
     // Serial.print("mem wght: ");Serial.println(rtcMem1.weight1);
     // if(rtcMem1.weight1 != 0)
     //   {
     //    rtcMem1.weight2 = rtcMem1.weight1;
     //    system_rtc_mem_write(rtcPos1, &rtcMem1, buckets * 4);
     //   } 
    //  Serial.print("mem wght2 first reading: ");Serial.println(rtcMem1.weight2);   
      if(setupButtonState == 1)
        {
          WiFi.forceSleepBegin();  // send wifi to sleep to reduce power consumption
          yield();
          // scale.power_down();
          Serial.begin(115200);
         
          // scale.set_scale(-681650);  //Calibration Factor obtained from first sketch
          delay(500);
          
          //float a = rtcMem1.weight1;
          //rtcMem1.weight1 = a;
          //system_rtc_mem_write(rtcPos1, &rtcMem1, sizeof(rtcMem1));
          Serial.println();
          // Serial.print("Weight: ");
          // Serial.print(scale.get_units()*1000);  //Up to 3 decimal points
          // Serial.println(" grams");
          // Serial.println("Start");
          buckets = (sizeof(rtcMem) / 4);
          //Serial.print("size of rtcmem: ");Serial.println(sizeof(rtcMem));
          if (buckets == 0) buckets = 1;
         
          // Serial.print("Buckets ");
          // Serial.println(buckets);
          // Serial.print(", hours: ");
          // Serial.println(rtcMem.hours);
          // system_rtc_mem_read(64, &toggleFlag, 4); 
          //  Serial.print("toggle Flag ");
          // Serial.println(toggleFlag);
          //if (toggleFlag) {
          //   Serial.println("Start Writing");
          // for (i = 0; i < RTCMEMORYLEN / buckets; i++) {
          int rtcPos = RTCMEMORYSTART;
          // rtcMem.battery = float((ESP.getVcc() * VCC_ADJ)/1000);
          // rtcMem.hours = i * 11;
          // if(rtcMem.hours == 0) rtcMem.hours = 1;
          system_rtc_mem_read(rtcPos, &rtcMem, sizeof(rtcMem));
       
          int rtcPos1 = RTCMEMORYSTART + buckets*2;
          system_rtc_mem_read(rtcPos1, &rtcMem1, sizeof(rtcMem1));
         // system_rtc_mem_read(rtcPos + buckets, &rtcMem1, sizeof(rtcMem1));
        
           //system_rtc_mem_write(rtcPos, &rtcMem, buckets * 4);
             
            Serial.print(",-->Hours: ");
            Serial.print(rtcMem.hours);
            Serial.print(",  Count: ");
            Serial.println(rtcMem.count);
           // Serial.print("-->mem wght2: ");Serial.println(rtcMem1.weight2);
           //Serial.print("-->mem wght1: ");Serial.println(rtcMem1.weight1);
          
          // rtcMem1.weight2;
         // if(rtcMem2.checkTare_weight == 1)
          //  {
          //    rtcMem1.weight2 = rtcMem2.tare_weight;
          //  }  
          if(rtcMem.hours == rtcMem.count)
            {
              rtcMem.hours +=1;
              rtcMem.count +=1;
             // rtcMem1.weight2+=7;
            //  rtcMem1.weight1+=5;
              // Serial.print("new value of hours: ");
              // Serial.println(rtcMem.hours);
            }
       
          //rtcMem1.weight2 = rtcMem1.weight2;
          //Serial.print("mem wght2 after add: ");Serial.println(rtcMem1.weight2);
          if((rtcMem.hours >=8))
            {
              rtcMem.hours = 0;
              rtcMem.count = 0;
              wifi_ssid = "";
              passwd = "";
              String savedWeight = "";
          
          if (EEPROM.read(0) != 0)
             {
               for (int i = 0; i < 32; ++i) 
                   {
                     wifi_ssid += char(EEPROM.read(i));
                   }
               Serial.print("SSID: ");
               Serial.println(wifi_ssid);
              
               for (int i = 32; i < 96; ++i) 
                   {
                     passwd += char(EEPROM.read(i));
                   }
               Serial.print("Password: ");
               Serial.println(passwd);    
        
                for (int i = 97; i < 103; ++i) 
                    {
                     savedWeight += char(EEPROM.read(i));
                    }
               
             }
             Serial.print("Tare weight: ");
             Serial.println(savedWeight);
             float b = savedWeight.toFloat();    
             Serial.print("Tare weight: ");
             Serial.println(b);
               
             // rtcMem1.weight2=99;
             // rtcMem1.weight1=88;
              //scaleWeight = true;
              // Serial.print("greater than 24: ");
              //Serial.println(rtcMem.hours);
             // scale.power_up();
              for(int i=0; i<3; i++)
                 {
                   weight = scale.get_units()*1000;
                   delay(500);
                 }
              // int rtcPos1 = RTCMEMORYSTART + buckets;
              // system_rtc_mem_read(rtcPos1, &rtcMem1, sizeof(rtcMem1));
              // Serial.print("mem wght: ");Serial.println(rtcMem1.weight1);
              //Serial.print("mem wght2 uploading: ");Serial.println(rtcMem1.weight2);
              weight = weight - b;
              Serial.print("Weight: ");
              Serial.print(weight);  //Up to 3 decimal points
              Serial.println(" grams");    
           
              WiFi.forceSleepWake();
              delay(500);
              wifi_set_sleep_type(MODEM_SLEEP_T);
              WiFi.mode(WIFI_STA);
              yield();

              time1 = system_get_time();

              // Connect to WiFi network
              Serial.println();
              Serial.println();
              Serial.print("Connecting to ");
              Serial.println(wifi_ssid);
              Serial.print("passwd ");
              Serial.println(passwd);
             // WiFi.begin("CuriosityGym-BCK", "#3Twinkle3#");
              WiFi.begin(wifi_ssid.c_str(), passwd.c_str());
              while (WiFi.status() != WL_CONNECTED) {
                   delay(500);
                   Serial.print(".");
                   time2 = system_get_time();
                   if (((time2 - time1) / 1000000) > WIFI_CONNECT_TIMEOUT_S)  // wifi connection lasts too ling, retry
                      { ESP.deepSleep(10, WAKE_RFCAL);
                        yield();
                      }
                 }
             Serial.println("");
             Serial.println("WiFi connected");
        
          
             const char* host = "api.thingspeak.com";
         
             Serial.print("Connecting to "); Serial.print(host);

             WiFiClient client;
             int retries = 5;
             while (!client.connect(host, 80) && (retries-- > 0)) {
                   Serial.print(".");
                  }
             Serial.println();
             if (!client.connected()) {
                 Serial.println("Failed to connect, going back to sleep");
                 //return false;
                }

             String value = String(float((ESP.getVcc() * VCC_ADJ)/1000));
  
             String postStr = apiKey;
             postStr += "&field1=";
             postStr += value;
             postStr += "&field2=";
             postStr += String(weight);
             postStr += "\r\n\r\n";

             client.print("POST /update HTTP/1.1\n");
             client.print("Host: api.thingspeak.com\n");
             client.print("Connection: close\n");
             client.print("X-THINGSPEAKAPIKEY: " + apiKey + "\n");
             client.print("Content-Type: application/x-www-form-urlencoded\n");
             client.print("Content-Length: ");
             client.print(postStr.length());
             client.print("\n\n");
             client.print(postStr);
 
             int timeout = 5 * 10; // 5 seconds
             while (!client.available() && (timeout-- > 0)) {
                   delay(100);
                  }

             if (!client.available()) {
                Serial.println("No response, going back to sleep");
                // return false;
               }
             Serial.println(F("disconnected"));
            // scale.power_down();
             //return true;
           } 
        
         //+ i * buckets;;
         if(rtcMem.hours != rtcMem.count)
           {
            Serial.print("different values: ");
            Serial.println(rtcMem.hours);
            rtcMem.hours = 0;
            rtcMem.count = 0;
           }
           
       
    
             system_rtc_mem_write(rtcPos, &rtcMem, buckets * 4);
               rtcPos1 = RTCMEMORYSTART + buckets*2;
          //system_rtc_mem_write(rtcPos1, &rtcMem1, sizeof(rtcMem1));
             //if(i==1)system_rtc_mem_write(rtcPos, &rtcMem1, buckets * 4);
             // system_rtc_mem_write(rtcPos +  buckets, &rtcMem1, buckets * 4);
            
          // Serial.print("mem wght2 after writing: ");Serial.println(rtcMem1.weight2); 
           
         //toggleFlag = false;
         //system_rtc_mem_write(64, &toggleFlag, 4);

         // Serial.print("i: ");
         // Serial.print(i);
         /*   Serial.print(" Position: ");
         Serial.print(rtcPos);
         Serial.print(", battery: ");
         Serial.print(rtcMem.battery);
         Serial.print(", hours: ");
         Serial.println(rtcMem.hours);
         Serial.print(" Position: ");
         Serial.print(rtcPos + buckets);*/
         //  Serial.print(", hours: ");
         //  Serial.print(rtcMem.hours);
         //  Serial.print(", count: ");
         //  Serial.println(rtcMem.count);
         yield();
         //}
         // Serial.println("Writing done");
   
         ESP.deepSleep(SLEEP_TIME, WAKE_RFCAL);
       }

    if(setupButtonState == 0)
      {
        initHardware();
       // Try and restore saved settings
       if (loadSavedConfig())
          {
           if (checkWiFiConnection())
              {
                SETUP_MODE = false;
                startWebServer();
                // Turn the status led Green when the WiFi has been connected
               // digitalWrite(LED_GREEN, HIGH);
                return;
              }
       }
       SETUP_MODE = true;
       //scaleWeight = true;
       setupMode();
      
          
         
      }
  }
 
void loop() {

  while(setupButtonState == 0)
    {
     // system_rtc_mem_read(64, &toggleFlag, 4);
     /* if (scaleWeight) 
         {
          for(int i=0; i<=3; i++)
             {
               w = scale.get_units()*1000; 
               rawWeight = w;
               Serial.print("Setup Raw Weight: ");
               Serial.print(rawWeight);  //Up to 3 decimal points
               Serial.println(" grams      ");
               scaleWeight = false;
               }
        }*/
      w = scale.get_units()*1000;
      
      if(tare == true)
        {
          weightBeforeTaring = w;
        }
        Serial.print("Raw Weight: ");
  Serial.print(rawWeight);  //Up to 3 decimal points
  Serial.println(" grams      ");
  Serial.print("Weight before taring: ");
  Serial.print(weightBeforeTaring);  //Up to 3 decimal points
  Serial.println(" grams      ");
      Serial.print("Weight: ");
  Serial.print(w);  //Up to 3 decimal points
  Serial.println(" grams      ");
      delay(500);
     
    
    tare_weight =  String(weightBeforeTaring);
    
      
     
       // Handle WiFi Setup and Webserver for reset page
       if (SETUP_MODE)
          {
            DNS_SERVER.processNextRequest();
          }
       WEB_SERVER.handleClient();
       
        
     setupButtonState = digitalRead(setupButton);
       Serial.print("State: ");
  Serial.println(setupButtonState);
  Serial.print("tare weight: ");
     Serial.println(tare_weight);
  if(setupButtonState == 1)
     {Serial.print("tare weight: ");
     Serial.println(tare_weight); 
      EEPROM.begin(512);
      for (int i = 0; i < tare_weight.length(); ++i) 
          {
            EEPROM.write(97 + i, tare_weight[i]);
          }
         
      EEPROM.commit();
       //int rtcPos = RTCMEMORYSTART;
       // system_rtc_mem_write(rtcPos, &rtcMem, buckets * 4);
       // int rtcPos1 = RTCMEMORYSTART + buckets;
      //system_rtc_mem_write(rtcPos1, &rtcMem2, sizeof(rtcMem2));
          
      yield();
      ESP.deepSleep(SLEEP_TIME, WAKE_RFCAL);
     }
    }
  }

    

String makePage(String title, String contents)
      {
        String s = "<!DOCTYPE html><html><head>";
        s += "<meta name=\"viewport\" content=\"width=device-width,user-scalable=0\">";
        s += "<style>";
        // Simple Reset CSS
        s += "*,*:before,*:after{-webkit-box-sizing:border-box;-moz-box-sizing:border-box;box-sizing:border-box}";
        s += "html{font-size:100%;-ms-text-size-adjust:100%;-webkit-text-size-adjust:100%}html,button,input";
        s += ",select,textarea{font-family:sans-serif}article,aside,details,figcaption,figure,footer,header,";
        s += "hgroup,main,nav,section,summary{display:block}body,form,fieldset,legend,input,select,textarea,";
        s += "button{margin:0}audio,canvas,progress,video{display:inline-block;vertical-align:baseline}";
        s += "audio:not([controls]){display:none;height:0}[hidden],template{display:none}img{border:0}";
        s += "svg:not(:root){overflow:hidden}body{font-family:sans-serif;font-size:16px;font-size:1rem;";
        s += "line-height:22px;line-height:1.375rem;color:#585858;font-weight:400;background:#fff}";
        s += "p{margin:0 0 1em 0}a{color:#cd5c5c;background:transparent;text-decoration:underline}";
        s += "a:active,a:hover{outline:0;text-decoration:none}strong{font-weight:700}em{font-style:italic}";
        // Basic CSS Styles
        s += "body{font-family:sans-serif;font-size:16px;font-size:1rem;line-height:22px;line-height:1.375rem;";
        s += "color:#585858;font-weight:400;background:#fff}p{margin:0 0 1em 0}";
        s += "a{color:#cd5c5c;background:transparent;text-decoration:underline}";
        s += "a:active,a:hover{outline:0;text-decoration:none}strong{font-weight:700}";
        s += "em{font-style:italic}h1{font-size:32px;font-size:2rem;line-height:38px;line-height:2.375rem;";
        s += "margin-top:0.7em;margin-bottom:0.5em;color:#343434;font-weight:400}fieldset,";
        s += "legend{border:0;margin:0;padding:0}legend{font-size:18px;font-size:1.125rem;line-height:24px;line-height:1.5rem;font-weight:700}";
        s += "label,button,input,optgroup,select,textarea{color:inherit;font:inherit;margin:0}input{line-height:normal}";
        s += ".input{width:100%}input[type='text'],input[type='email'],input[type='tel'],input[type='date']";
        s += "{height:36px;padding:0 0.4em}input[type='checkbox'],input[type='radio']{box-sizing:border-box;padding:0}";
        // Custom CSS
        s += "header{width:100%;background-color: #2c3e50;top: 0;min-height:60px;margin-bottom:21px;font-size:15px;color: #fff}.content-body{padding:0 1em 0 1em}header p{font-size: 1.25rem;float: left;position: relative;z-index: 1000;line-height: normal; margin: 15px 0 0 10px}";
        s += "</style>";
        s += "<title>";
        s += title;
        s += "</title></head><body>";
        s += "<header><p>" + DEVICE_TITLE + "</p></header>";
        s += "<div class=\"content-body\">";
        s += contents;
        s += "</div>";
        s += "</body></html>";
        return s;
      }

String urlDecode(String input)
      {
        String s = input;
        s.replace("%20", " ");
        s.replace("+", " ");
        s.replace("%21", "!");
        s.replace("%22", "\"");
        s.replace("%23", "#");
        s.replace("%24", "$");
        s.replace("%25", "%");
        s.replace("%26", "&");
        s.replace("%27", "\'");
        s.replace("%28", "(");
        s.replace("%29", ")");
        s.replace("%30", "*");
        s.replace("%31", "+");
        s.replace("%2C", ",");
        s.replace("%2E", ".");
        s.replace("%2F", "/");
        s.replace("%2C", ",");
        s.replace("%3A", ":");
        s.replace("%3A", ";");
        s.replace("%3C", "<");
        s.replace("%3D", "=");
        s.replace("%3E", ">");
        s.replace("%3F", "?");
        s.replace("%40", "@");
        s.replace("%5B", "[");
        s.replace("%5C", "\\");
        s.replace("%5D", "]");
        s.replace("%5E", "^");
        s.replace("%5F", "-");
        s.replace("%60", "`");
        return s;
      }

/////////////////////////
// Debugging Functions //
/////////////////////////

void wipeEEPROM()
     {
       EEPROM.begin(512);
       // write a 0 to all 512 bytes of the EEPROM
       for (int i = 0; i < 512; i++)
            EEPROM.write(i, 0);

       EEPROM.end();
      }
