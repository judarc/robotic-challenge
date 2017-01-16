#define FRAMELENGTH 324
#define SCLK 6 // portd.6
#define SDIO 7 // portd.7

byte frame[FRAMELENGTH];
byte flop;

/*
  Serial driver for ADNS2010, by Conor Peterson (robotrobot@gmail.com)
  Serial I/O routines adapted from Martjin The and Beno?t Rosseau's work.
  Delay timings verified against ADNS2061 datasheet.

  The serial I/O routines are apparently the same across several Avago chips.
  It would be a good idea to reimplement this code in C++. The primary difference
  between, say, the ADNS2610 and the ADNS2051 are the schemes they use to dump the data
  (the ADNS2610 has an 18×18 framebuffer which can't be directly addressed).

  This code assumes SCLK is defined elsewhere to point to the ADNS's serial clock,
  with SDIO pointing to the data pin.

  Adapted for A2620 on AVRDBM328 by Luke.
*/

const byte regConfig    = 0x40; //a2620 . A2610 =0x00
const byte regStatus    = 0x41; //a2620 . A2610 =0x01
const byte regPixelData = 0x48; //a2620 . A2610 =0x08
const byte maskNoSleep  = 0x01; // unchanged for a2620
const byte maskPID      = 0xE0; // idem

const byte regYmov = 0x42; // a2620
const byte regXmov = 0x43;  //a2620

void mouseInit(void)
{
  digitalWrite(SCLK, HIGH);
  delayMicroseconds(5);
  digitalWrite(SCLK, LOW);
  delayMicroseconds(1);
  digitalWrite(SCLK, HIGH);
  delay(1025);
  writeRegister(regConfig, maskNoSleep); //Force the mouse to be always on.
}

void dumpDiag(void)
{
  unsigned int val;

  val = readRegister(regStatus);

  Serial.print("Product ID: ");
  Serial.println( (unsigned int)((val & maskPID) >> 5));
  Serial.println("Ready.");
  Serial.flush();
}

void writeRegister(byte addr, byte data)
{
  byte i;

  addr |= 0x80; //Setting MSB high indicates a write operation.

  //Write the address
  pinMode (SDIO, OUTPUT);
  for (i = 8; i != 0; i--)
  {
    digitalWrite (SCLK, LOW);
    digitalWrite (SDIO, addr & (1 << (i - 1) ));
    digitalWrite (SCLK, HIGH);
  }

  //Write the data
  for (i = 8; i != 0; i--)
  {
    digitalWrite (SCLK, LOW);
    digitalWrite (SDIO, data & (1 << (i - 1) ));
    digitalWrite (SCLK, HIGH);
  }
}

byte readRegister(byte addr)
{
  byte i;
  byte r = 0;

  //Write the address
  pinMode (SDIO, OUTPUT);
  for (i = 8; i != 0; i--)
  {
    digitalWrite (SCLK, LOW);
    digitalWrite (SDIO, addr & (1 << (i - 1) ));
    digitalWrite (SCLK, HIGH);
  }

  pinMode (SDIO, INPUT);  //Switch the dataline from output to input
  delayMicroseconds(110);  //Wait (per the datasheet, the chip needs a minimum of 100 µsec to prepare the data)

  //Clock the data back in
  for (i = 8; i != 0; i--)
  {
    digitalWrite (SCLK, LOW);
    digitalWrite (SCLK, HIGH);
    r |= (digitalRead (SDIO) << (i - 1) );
  }

  delayMicroseconds(110);  //Tailing delay guarantees >100 µsec before next transaction

  return r;
}

//ADNS2610 dumps a 324-byte array, so this function assumes arr points to a buffer of at least 324 bytes. (A2620: unchanged)
void readFrame(byte *arr)
{
  byte *pos;
  byte *uBound;
  unsigned long timeout;
  byte val;

  //Ask for a frame dump
  writeRegister(regPixelData, 0x2A); // wite anything to pixeldatareg to start at first pixel (A2620)

  val = 0;
  pos = arr;
  uBound = arr + 325;

  timeout = millis() + 1000;

  //There are three terminating conditions from the following loop:
  //1. Receive the start-of-field indicator after reading in some data (Success!)
  //2. Pos overflows the upper bound of the array (Bad! Might happen if we miss the start-of-field marker for some reason.)
  //3. The loop runs for more than one second (Really bad! We're not talking to the chip properly.)
  while ( millis() < timeout && pos < uBound)
  {
    val = readRegister(regPixelData);

    //Only bother with the next bit if the pixel data is valid.
    if ( !(val & 64) ) {
      //Serial.println("Invalid data.");
      continue;
    }

    //If we encounter a start-of-field indicator, and the cursor isn't at the first pixel,
    //then stop. ('Cause the last pixel was the end of the frame.)
    if ( ( val & 128 ) &&  ( pos != arr) ) {
      //      Serial.println("last pixel read.");
      break;
    }

    *pos = val & 63;
    pos++;
  }

}

void setup()
{
  pinMode(SCLK, OUTPUT);
  pinMode(SDIO, OUTPUT);

  Serial.begin(38400);
  Serial.println("Serial established.");
  Serial.flush();

  mouseInit();
  dumpDiag();

}

void loop()
{
  int input;
  byte buff;

  //readFrame(frame);

  if ( Serial.available() )
  {
    input = Serial.read();
    switch ( input )
    {
      case 'f':      // capture frame
        Serial.println("Frame capture.");
        readFrame(frame);
        Serial.println("Done.");
        break;
      case 'd': // dump frame, raw data
        for ( input = 0; input < FRAMELENGTH; input++ ) //Reusing 'input' here
          Serial.write( (byte) frame[input] ); // use serial.write so it does not convert to ascii, --Luke
        Serial.write( (byte) 127 );
        break;
      case 'p': // Powerup sequence
        mouseInit();
        dumpDiag();
        break;
      case 'x':  // read X movement register
        buff = readRegister(regXmov);
        Serial.print((byte) buff);
        Serial.print(" ");
        break;
      case 'y':  // read Y movement register
        buff = readRegister(regYmov);
        Serial.print((byte) buff);
        Serial.print(" ");
        break;
      case 's':  // Shutdown
        writeRegister(regConfig, 0xE0);
        Serial.print("Shutdown");
        break;
      case 'i': // Dump frame values seperated and human readable

        for ( input = 0; input < FRAMELENGTH; input++ ) { //Reusing 'input' here
          Serial.print( (byte) frame[input]);
          Serial.print( " ");
          // maybe find something to print LF each 18th datapoint?
        }
        Serial.print( (byte)127 );
        break;
      case 't': // test image

        for ( input = 0; input < FRAMELENGTH; input++ ) { //Reusing 'input' here
          Serial.write( (byte) input % 64);
        }
        Serial.write( (byte) 127 );
        break;

    }
    Serial.flush();
  }
}
