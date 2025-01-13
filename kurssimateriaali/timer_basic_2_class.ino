
#define LED1  12
#define LED2  13


class AlertLED
{
TimerHandle_t tmr = nullptr;

int             Led_pin;

unsigned        period_ms;

volatile bool   stat=0;


public:

AlertLED(int Led_pin, unsigned period_ms=1000 );

void Tstart();

static void callback(TimerHandle_t tmr);
  
};


AlertLED::AlertLED(int Led_pin, unsigned period_ms )
{ 
  this ->Led_pin=Led_pin;
  this ->period_ms =period_ms;

  pinMode(this->Led_pin,OUTPUT);
  digitalWrite(this->Led_pin,LOW);
}

void AlertLED::Tstart()
{

    tmr = xTimerCreate
    (
      "MyTimer", 
      pdMS_TO_TICKS(period_ms), 
      pdTRUE,
      this, 
      AlertLED::callback
     );
           
    if( xTimerStart(tmr, 10 ) != pdPASS ) 
    {
     Serial.println("Timer start error");
    }
  
}

void AlertLED::callback(TimerHandle_t tmr)
{

  AlertLED *obj = (AlertLED*) pvTimerGetTimerID(tmr);
    digitalWrite(obj->Led_pin,obj->stat);

    obj-> stat ^= true; 
Serial.print(obj->Led_pin);
    Serial.println("Tick ");
}



static AlertLED alert1(LED1, 300);
static AlertLED alert2(LED2, 80);


void setup()
{

Serial.begin(9600);
Serial.println("\n\n");
Serial.println(" start ");

alert1.Tstart();
alert2.Tstart();   
    
}


void loop()
{}
