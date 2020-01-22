#include <wiringPi.h>
#include <stdio.h>

#define BtnPin    0
#define LedPin1    1
#define LedPin2    2
#define LedPin3    3

int main(void)
{
	if(wiringPiSetup() == -1){ //when initialize wiring failed,print messageto screen
		printf("setup wiringPi failed !");
		return 1; 
	}

	pinMode(BtnPin, INPUT);
	pullUpDnControl(BtnPin, PUD_OFF);
	pinMode(LedPin1, OUTPUT);
	pinMode(LedPin2, OUTPUT);
	pinMode(LedPin3, OUTPUT);

	digitalWrite(LedPin1, 0);
	digitalWrite(LedPin2, 0);
	digitalWrite(LedPin3, 0);

	delay(2000);
	digitalWrite(LedPin1, 1);
	delay(2000);
	digitalWrite(LedPin1, 0);
	delay(2000);
	digitalWrite(LedPin2, 1);
	delay(2000);
	digitalWrite(LedPin2, 0);
	delay(2000);
	digitalWrite(LedPin3, 1);
	delay(2000);
	digitalWrite(LedPin3, 0);
	

	return 0;
}
