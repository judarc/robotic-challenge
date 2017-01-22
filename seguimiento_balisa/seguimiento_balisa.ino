/**************************************programa para seguir una señal de un mando a distancia***********************************************/
//un servo sin modificar conectado al pin 9 del aduino
//un reptor infrarojo conectado al pin 11 y otro al pin 12
//segun el sensor que pueda leer codigo desde el mando a distancia el servo girara hasta que los 2 o ningun sensor pueda leer el codigo

#include <IRremote.h>
#include <Servo.h>


int RECV_PIN1 = 11;
int RECV_PIN2 = 12;

Servo myservo;
IRrecv irrecv1(RECV_PIN1);
IRrecv irrecv2(RECV_PIN2);

decode_results results;
int pos = 0;

void setup()
{
  Serial.begin(9600);
  myservo.attach(9);
  irrecv1.enableIRIn(); // Start the receiver
}

void loop() {
  if (irrecv1.decode(&results)&&pos<180) {
    pos++;
  }
    if (irrecv2.decode(&results)&&pos>0) {
    pos--;
  }
  myservo.write(pos);
  delay(100);
}
