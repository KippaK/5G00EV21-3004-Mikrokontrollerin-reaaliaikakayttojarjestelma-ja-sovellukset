/*********
  AD web gauge 8.3.2022 KN

  reference code
  
*********/

// Import required libraries
#include "WiFi.h"
#include "ESPAsyncWebServer.h"
#include "SPIFFS.h"
#include <SPI.h>
#include "ArduinoJson.h"


// Set to zero if NOT using SSD1306

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


#include "SSD1306.h"

SSD1306 display(CFG_OLED_ADDRESS,CFG_OLED_SDA,CFG_OLED_SCL);

static const int disp_width = CFG_OLED_WIDTH;
static const int disp_height = CFG_OLED_HEIGHT;



// Replace with your network credentials
//const char* ssid = "REPLACE_WITH_YOUR_SSID";
//const char* password = "REPLACE_WITH_YOUR_PASSWORD";

#define ssid  "mokkula_925936"
#define password  "12345678"

//#define ssid  "kariwlan"
//#define password "12345678"

WiFiClient espClient;

DynamicJsonDocument doc(100);


// Create AsyncWebServer object on port 80
AsyncWebServer server(80);



// Replaces placeholder with LED state value
String processor(const String& var){
 
  return String();
}

//////////////////////////////////  SETUP ///////////////////////////////

 
void setup()
{
  
  // Serial port for debugging purposes
  Serial.begin(115200);


  
  Serial.println(" ---- booting 8.3.2022 ----------------");

  // Initialize SPIFFS
  if(!SPIFFS.begin(true))
  {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) 
  {
    delay(1000);
    Serial.println("Connecting to WiFi..");
  }

  // Print ESP32 Local IP Address
  Serial.println(WiFi.localIP());

  Serial.println("Start web server ");

  start_web_serv();

 

} ///////////////////////// SETUP ENDS /////////////////////////////////




//////////////////////////////// functions start ///////////////////////////////

void start_web_serv()
{  

  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/index.html", String(), false, processor);

              Serial.println(" HTML GET ");
                
              Serial.println(" -------------------------------- ");
   
  });

      // Route to load style.css file
  server.on("/index.html", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/index.html", "text/html");
  });

    // Route to load style.css file
  server.on("/Chart.bundle.js", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/Chart.bundle.js", "application/javascript");
  });

  server.on("/Chart.bundle.min.js", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/Chart.bundle.min.js", "application/javascript");
  });


  server.on("/Chart.js", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/Chart.js", "application/javascript");
  });


  server.on("/Chart.min.js", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/Chart.min.js", "application/javascript");
  });

  
  // Route to load style.css file
  server.on("/Chart.css", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/Chart.css", "text/css");
  });

    // Route to load style.css file
  server.on("/Chart.min.css", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/Chart.min.css", "text/css");
  });

  
  //      scope data query from web page tag is /DATA to send scope data
  server.on("/DATA", HTTP_GET, [](AsyncWebServerRequest *request)
    {
         // Serial.println(" DATA");
          
      request->send_P(200, "text/plain", scope_data().c_str()); // return value to web page JS
      
    }); 


  server.on( "/C_But/",  HTTP_POST,                     // web client command buttons post
    [](AsyncWebServerRequest * request){}, NULL,
    [](AsyncWebServerRequest * request, uint8_t *data, size_t len, size_t index, size_t total) {

    size_t i;

    parse_msg(data);                                    // call parse message
    
  });


  // Start server
  server.begin();
}



String scope_data() 
{

        //             data to web page    {"ch1":[48,1,2,3,4,5,6,7,8]} 

 String ret="{\"ch1\":[8,1,2,3,4,5,6,7,8,9]}";                          // start creating json string
      
    return ret;
}




void parse_msg(uint8_t *data)                          // parse message
{
    Serial.println("Parsing start: ");
    DeserializationError error = deserializeJson(doc, data);
                                              // Test if parsing succeeds.
    if (error) {
                  Serial.print(F("deserializeJson() failed: "));
                  Serial.println(error.f_str());
                  return;
                }
    JsonObject root = doc.as<JsonObject>();

    String com;                                 // command
    double val;                                 // value

    for (JsonPair kv : root) 
      {
             com=(kv.key().c_str());
             val=kv.value().as<double>();
       }
    Serial.println();
    if (com=="trig")                            // command == "trig"
      {
         Serial.print("Trig value ="); Serial.println(val);
      }

    if (com=="time")                            // commad == "time"
      {
         Serial.print("Time value ="); Serial.println(val);
      }
} // end parse message

 
void loop()

{

  
}
