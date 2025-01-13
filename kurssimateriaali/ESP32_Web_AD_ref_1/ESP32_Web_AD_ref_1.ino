/*********
  AD web gauge 19.3.2021 KN
  
*********/

// Import required libraries
#include "WiFi.h"
#include "ESPAsyncWebServer.h"
#include "SPIFFS.h"
#include <SPI.h>


// I2C address of SSD1306 display
#define CFG_OLED_ADDRESS    0x3C

// GPIO for display I2C SDA
#define CFG_OLED_SDA        5

// GPIO for display I2C SCL
#define CFG_OLED_SCL        4

// Pixel width of display
#define CFG_OLED_WIDTH      128

// Pixel height of display
#define CFG_OLED_HEIGHT     64

// GPIO for ADC input
#define CFG_ADC_GPIO        36

// GPIO for PWM LED
#define CFG_LED_GPIO        13


#include "SSD1306.h"

SSD1306 display(CFG_OLED_ADDRESS,CFG_OLED_SDA,CFG_OLED_SCL);

static const int disp_width = CFG_OLED_WIDTH;
static const int disp_height = CFG_OLED_HEIGHT;



// Replace with your network credentials
//const char* ssid = "REPLACE_WITH_YOUR_SSID";
//const char* password = "REPLACE_WITH_YOUR_PASSWORD";


const char *ssid = "KN-ESP32";
const char *passphrase = "12345678";

IPAddress local_IP(192,168,4,22);
IPAddress gateway(192,168,4,9);
IPAddress subnet(255,255,255,0);

WiFiClient espClient;

float meas=1;

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);



//////////////////////////////////  SETUP ///////////////////////////////

 
void setup()
{
  
  // Serial port for debugging purposes
  Serial.begin(115200);

  Serial.println(" ---- booting 19.3.2021 ----------------");

  // Initialize SPIFFS
  if(!SPIFFS.begin(true))
  {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }


  // Connect to Wi-Fi
  Serial.print("Setting soft-AP configuration ... ");
  Serial.println(WiFi.softAPConfig(local_IP, gateway, subnet) ? "Ready" : "Failed!");

  Serial.print("Setting soft-AP ... ");
  Serial.println(WiFi.softAP(ssid,passphrase) ? "Ready" : "Failed!");
  //WiFi.softAP(ssid);
  //WiFi.softAP(ssid, passphrase, channel, ssdi_hidden, max_connection)
  
  Serial.print("Soft-AP IP address = ");
  Serial.println(WiFi.softAPIP());


  Serial.println("Start web server ");

  start_web_serv();

  draw_border();

} ///////////////////////// SETUP ENDS /////////////////////////////////


//////////////////////////////// functions start ///////////////////////////////

void start_web_serv()
{  

  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/index.htm", String(), false, processor);

              Serial.println(" HTML GET ");

                int paramsNr = request->params();

              Serial.println(" ------ ");
              Serial.print("Param qty: ");
              Serial.println(paramsNr);

                for (byte i=0;i<paramsNr;i++)
                    {

                        AsyncWebParameter* p = request->getParam(0);
 
                      Serial.print("Param name: ");
                      Serial.println(p->name());
      
                      Serial.print("Param value: ");
                      Serial.println(p->value());
                    };  //    end of for loo
        
  });

      // Route to load style.css file
  server.on("/index.htm", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/index.htm", "text/html");
  });


      // Route to load style.css file
  server.on("/gauge.js", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/gauge.js", "application/javascript");
  });

      // Route to load style.css file
  server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/favicon.ico", "image/x-icon");
  });

  
  // Route to load style.css file
  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/style.css", "text/css");
  });

    // Route to load style.css file
  server.on("/jquery-1.11.3.min.js", HTTP_GET, [](AsyncWebServerRequest *request)
  {
    request->send(SPIFFS, "/jquery-1.11.3.min.js", "text/javascript");
    Serial.println(" JS jquery GET ");
      
  });


   //      distance query (Lidar) from web page tag is /Lid_dist to send Lidar value
  server.on("/AD1", HTTP_GET, [](AsyncWebServerRequest *request)
    {
          Serial.println(" GET AD1");
      request->send_P(200, "text/plain", get_AD().c_str()); // return value to web page JS
      
    }); 


  // Start server
  server.begin();
}


// Replaces placeholder own func
String processor(const String& var)
{
  return "OK";
}


String get_AD()
{
  char ret[10];

  dtostrf(meas,1,2,ret);

  meas=meas + 0.1;

  if (meas>8){ meas=1.5; };
  
  return ret;
}


  
void draw_border( ) 
{
  
  display.init();
  display.clear();

  display.setColor(WHITE);

  display.drawRect(0,0,disp_width-1,disp_height-1);

  display.display();
}



 
void loop()

{

  
}
