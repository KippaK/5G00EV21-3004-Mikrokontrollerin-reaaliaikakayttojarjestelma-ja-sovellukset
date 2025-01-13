
////////////// Timer basic example //////


#define LED_pin    12


TimerHandle_t tmr;

int id = 1;
int interval = 200;

volatile bool stat=0;


void Lblink( TimerHandle_t xTimer )
{
  //ESP_LOGI(TAG,"Change stat");
  
   digitalWrite(LED_pin,stat);

   stat ^= true;

  
 // Serial.print(stat);
}



void setup()
{

Serial.begin(9600);
Serial.println("\n\n");
Serial.println(" start ");
pinMode(LED_pin,OUTPUT);
   

    tmr = xTimerCreate
    (
      "MyTimer2", 
      pdMS_TO_TICKS(interval), 
      pdTRUE,
      ( void * )id, 
      &Lblink
     );

      
    if( xTimerStart(tmr, 10 ) != pdPASS ) 
    {
     Serial.println("Timer start error");
    }

    
}

void loop()
{}
