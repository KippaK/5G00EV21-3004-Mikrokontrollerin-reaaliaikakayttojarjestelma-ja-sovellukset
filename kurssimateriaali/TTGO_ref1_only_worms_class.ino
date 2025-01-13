

//    Edited KN 5.1.2024
//
//    only worm class
//

#include <TFT_eSPI.h>
#include <SPI.h>

static int app_cpu = 1; // Updated by setup()


//////////////////////////////////////////// Display Class //////////////////////////////////////////

class Display : public TFT_eSPI
{
  int w, h; // Width, height
  
  public:  
      Display( int width=135, int height=240 );
      void clear();
      void init();
};


Display::Display( int width, int height ) : TFT_eSPI(),  w(width), h(height) // constructor
{ }


void Display::init()                                                          // Init
{
  
  TFT_eSPI::init();
  clear();

}

void Display::clear()                                                         // Clear
{

  fillRect(0,0,w,h, TFT_BLUE);

}


///////////////////////////////////////////////// InchWorm class  /////////////////////////////////

class InchWorm 
{
  static const int segw = 9, segsw = 4, segh = 3;

  volatile int nro;
  
  Display& disp;
  
  int worm;
  int x, y;             // Coordinates of worm (left)
  int wormw=30;
  int wormh=10;
  int dir=1;            // Direction
  int state=0;

public: 

  InchWorm(Display& disp,int worm_nro);
  
  void draw();

};


InchWorm::InchWorm(Display& disp_obj,int worm_nro) : disp(disp_obj)  ////////// constructor ////////////
{
  nro=worm_nro;                         /// set worm y position, number of worm defines y position
}


void InchWorm::draw()         ///////////////// draw worm /////////////////////////
{
  int py = 7 + (worm-1) * 20;
  py  =nro * 20 - 15;

  int px = 2 + x;
 
  py += wormh - 3;

  disp.fillRect(px,py-2*segh,3*segw,3*segh, TFT_WHITE);
    
  switch ( state ) 
  {
    case 0: // _-_
      disp.fillRect(px,py,segw,segh, TFT_BLACK);
      disp.fillRect(px+segw,py-segh,segsw,segh, TFT_BLACK);
      disp.fillRect(px+segw+segsw,py,segw,segh, TFT_BLACK);
      break;
    case 1: // _^_ (high hump)
      disp.fillRect(px,py,segw,segh, TFT_BLACK);
      disp.fillRect(px+segw,py-2*segh,segsw,segh, TFT_BLACK);
      disp.fillRect(px+segw+segsw,py,segw,segh, TFT_BLACK);
      disp.drawLine(px+segw,py,px+segw,py-2*segh, TFT_BLACK);
      disp.drawLine(px+segw+segsw,py,px+segw+segsw,py-2*segh, TFT_BLACK);
      break;
    case 2: // _^^_ (high hump, stretched)
      if ( dir < 0 )
        px -= segsw;
      disp.fillRect(px,py,segw,segh, TFT_BLACK);
      disp.fillRect(px+segw,py-2*segh,segw,segh, TFT_BLACK);
      disp.fillRect(px+2*segw,py,segw,segh,TFT_BLACK );
      disp.drawLine(px+segw,py,px+segw,py-2*segh, TFT_BLACK );
      disp.drawLine(px+2*segw,py,px+2*segw,py-2*segh,TFT_BLACK );
      break;
    case 3: // _-_ (moved)
      if ( dir < 0 )
        px -= segsw;
      else
        px += segsw;
      disp.fillRect(px,py,segw,segh, TFT_BLACK);
      disp.fillRect(px+segw,py-segh,segsw,segh, TFT_BLACK);
      disp.fillRect(px+segw+segsw,py,segw,segh, TFT_BLACK);
      break;
    default:
      ;
  }

  state = (state+1) % 4;
  if ( !state ) {
    x += dir*segsw;
    if ( dir > 0 ) {
      if ( x + 3*segw+segsw >= disp.width() )
        dir = -1;
    } else if ( x <= 2 )
      dir = +1;
  }
  
}




static Display oled;

static InchWorm worm_1(oled,1);                         // create worm object 1
static InchWorm worm_2(oled,2);                         // create worm object 2



///////////////////////////////////////////////////////// SETUP //////////////////////////////////////////


void setup() 
{

  Serial.begin(115200);

  Serial.println("\n\n Starting priority demo ");
  

  oled.init();


  // Draw at least one worm each:
  worm_1.draw();
  worm_2.draw();


  ///////////////////////////////////////////////////////// create tasks //////////////////////////////////




  printf("\n Running on CPU %d\n",app_cpu);
}



/////////////////////////////////////////////////////// LOOP ////////////////////////////////


void loop() 
{

    worm_1.draw();
 
    delay(30);


}
