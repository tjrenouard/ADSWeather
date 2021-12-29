/**********************************************************
** @file		ADSWeather.cpp
** @author		John Cape
** @copyright	Argent Data Systems, Inc. - All rights reserved
** @maintainer  tjrenouard
**

 * Forked from ADSWeather library v0.1.1 - https://github.com/rahife/ADSWeather
** Argent Data Systems weather station Arduino library.
** This library provides a set of functions for interfacing
** with the Argent Data Systesm weather station sensor package
** These sensors consist of a rain gauge, a wind vane and an
** anemometer. The anemometer and the rain gauge should be 
** connected to digital input pins on one side and ground on
** the other. The weather vane is a variable resistor. 
** It should be connected to an analog input pin on one side
** and ground on the other. The analog input pin needs to be 
** connected to 5V from the Arduion through a 10K Ohm resistor.
** 
**
**
 * Specs from data sheet:
 * Rain Bucket -  RJ11 -  bucket tips each 0.011" or 0.2794mm  - Connector is 2 center conductors
 * 
 * Wind Sensor:  RJ11
 * Anemometer - Switch closure 1/sec = 1.492mph or 2.4 km/h - inner 2 conductors of the RJ11 (pins 2 & 3)
 * Wind Vane -  Varies voltage depending on direction. 
 * The anemometer and the rain gauge each require a digital input pin that can be used as an interrupt
 * The wind vane requires an analog input pin.
 * 
 * Changes:
 * 		Changing wind speed & rain to metric 
 *		Changing wind speed to float for better precision
 * 		Create a getVersion 

*/

#include "Arduino.h"
#include "ADSWeather.h"

#define DEBOUNCE_TIME 15
#define CALC_INTERVAL 1000

volatile int _anemometerCounter;
volatile int _rainCounter;
volatile unsigned long last_micros_rg;
volatile unsigned long last_micros_an;

const float RainMultiplier_mm = 0.2794; // .2794mm per tip (see specs)
const float AnemometerMultiplier_kmph = 2.4; // 1 count per sec = 2.4 kmph


String WindDirDebugString;



//Initialization routine. This functrion sets up the pins on the Arduino and initializes variables.
ADSWeather::ADSWeather(int rainPin, int windDirPin, int windSpdPin)
{
	
	
  //Initialization routine
  _anemometerCounter = 0;
  _rainCounter = 0;
  _gustIdx = 0;
  _vaneSampleIdx = 0;
  
  _rainPin = rainPin;
  _windDirPin = windDirPin;
  _windSpdPin = windSpdPin;
  

  pinMode(_rainPin, INPUT);
  digitalWrite(_rainPin, HIGH);
  pinMode(_windSpdPin, INPUT);
  digitalWrite(_windSpdPin, HIGH);

  pinMode(_windDirPin, INPUT);
  }

//The update function updates the values of all of the sensor variables. This should be run as frequently as possible
//in the main loop for maximum precision.
void ADSWeather::update()
{
	_timer = millis();
	_vaneSample[_vaneSampleIdx] = analogRead(_windDirPin);
	_vaneSampleIdx++;
	if(_vaneSampleIdx >= VANE_SAMPLE_BINS)
	{
		_vaneSampleIdx=0;
	}
	if(_timer > _nextCalc)
	{
		_nextCalc = _timer + CALC_INTERVAL;
 
		//UPDATE ALL VALUES
		_rain += _readRainAmount();
		_windSpd = _readWindSpd();
		
		_windDir = _readWindDir(_fDebug);

	}
}

//Returns the ammount of rain since the last time the getRain function was called.
float ADSWeather::getRain()
{
	return _rain;
}

// Resets the rain amount
void ADSWeather::resetRain()
{
	_rain = 0;
}

//Returns the direction of the wind in degrees.
int ADSWeather::getWindDirection()
{
	return _windDir;
}

//Returns the wind speed.
float ADSWeather::getWindSpeed()
{
	return _windSpd;
}

//Returns the maximum wind gust speed. 
float ADSWeather::getWindGust()
{
	return _windSpdMax;
}

// Resets the max wind gust speed to 0
void ADSWeather::resetWindGust()
{
	_windSpdMax = 0;
}
 
//Updates the rain ammmount internal state.
float ADSWeather::_readRainAmount()
{
	float rain = 0;
	rain = RainMultiplier_mm * _rainCounter;
	_rainCounter = 0;
	return rain;
} 

// Calculate the weighted average of wind vane samples and return degrees for wind direction
// original code pulled out to experiment with different formulas
int ADSWeather::windDirWeightedAverageCalc(int indexStart, int numBins, int max_samples)
{
	int sum = 0;
	int i;
 	for(i=1;i<numBins;i++)
	{
		sum += (_windDirBin[(indexStart + i) & 0x0F] * i);
	}
	sum = ((indexStart * 45) + ((sum * 45) / max_samples) >> 1) % 360; //Convert into degrees
	return sum;
}


//Updates the wind direction internal state.
int ADSWeather::_readWindDir(bool fDebug)
{
	unsigned int max_samples, sum;
	unsigned char i, j, max_i;
	
	//Clear wind vane averaging bins
	for(i=0;i<WINDDIR_BINS;i++)
	{
		_windDirBin[i] = 0;
	}
	
	//Read all samples into bins - each bin has the number of samples for that directional reading
	for(i=0;i<VANE_SAMPLE_BINS;i++)
	{
		_setBin(_vaneSample[i]);
	}
	
	
	if (fDebug)
	{
		WindDirDebugString = String("_readWindDir() debug\n");
	}
	else
	{
		WindDirDebugString = String("");
	}
	
	
	//Calculate the weighted average
	//Find the block of 5 bins with the highest number of samples
	max_samples = 0;
	// Bug Fix - Changing to be WINDDIR_BINS - I believe this was the intent.
	// 'i' will be the starting point.  The bins are organized where "N" is 0 and then increment by 45 degrees.
	for(i=0;i<WINDDIR_BINS;i++)
	{
		//get the sum of the next 5 bins
		sum = 0;
		for(j=0;j<5;j++)
		{
			// & 0x0F allows this to wrap
			sum += _windDirBin[(i+j) & 0x0F];
		}
		if(sum > max_samples)
		{
			// Figure out the set of 5 directions with the most samples
			max_samples = sum;
			max_i = i;
		}
	}
	if (fDebug)
	{
		WindDirDebugString.concat("Found largest sample set at index = ");
		WindDirDebugString.concat(max_i);
		WindDirDebugString.concat(", number of samples = ");
		WindDirDebugString.concat(max_samples);
		WindDirDebugString.concat("\n   All Sample dump: \n\t bin: ");
		for(j=0;j<WINDDIR_BINS;j++)
		{
			if (j < 10)
			{
				WindDirDebugString += '0';
			}
			WindDirDebugString += j;
			WindDirDebugString += ", ";
		}
		WindDirDebugString.concat("\n\t val: ");
		for(j=0;j<WINDDIR_BINS;j++)
		{
			if (_windDirBin[j] < 10)
			{
				WindDirDebugString += '0';
			}
			WindDirDebugString.concat((int)_windDirBin[j]);
			WindDirDebugString.concat(", ");
		}
	}


	sum = windDirWeightedAverageCalc(max_i, 5, max_samples);
	
	if (fDebug)
	{
		WindDirDebugString.concat("\n   Wind Direction calc = ");
		WindDirDebugString.concat(sum);
	}


	
	return sum;
}



// returns the wind speed since the last calcInterval.
// Assumes calc interval is 1 second
// _windSpdMax is persistent - recommend calling function reset this periodically at whatever interval you want.

float ADSWeather::_readWindSpd()
{
	unsigned char i;
	
	float spd = _anemometerCounter*AnemometerMultiplier_kmph;
	_anemometerCounter = 0;
	if(_gustIdx > 29)
	{
		_gustIdx = 0;
	}
	_gust[_gustIdx++] = (int) spd;

	for (i = 0; i < 30; i++)
	{
		if (_gust[i] > _windSpdMax) _windSpdMax = _gust[i];	
	}
	return spd;
}


//Internal function for calculating the wind direction using consensus averaging.  
//The windVane analog values are calculated based on max value of 1024 representing 5v.  
// See Weather Sensor PDF for spec'd voltage - (voltage/5v)*1024 is expected value
/* TODO - create mapping to ordinal direction
index, 	deg, 	direction
0, 		0, 		"N"
1, 		22, 	"NNE"
2, 		45, 	"NE"
3,			
4, 		90, 	"E"
5,		
6, 		135, 	"SE"
7, 			
8, 		180, 	"S"
9,			
10,		225, 	"SW"
11,		 
12,		270, 	"W"
13,		 
14,		315, 	"NW"
15,
 */
void ADSWeather::_setBin(unsigned int windVane)
{
	//Read wind directions into bins
	unsigned char bin;
	if( windVane > 940) bin = 12;     //W
	else if(windVane > 890) bin = 14; //NW
	else if(windVane > 820) bin = 13; //WNW
	else if(windVane > 785) bin = 0;  //N
	else if(windVane > 690) bin = 15; //NNW
	else if(windVane > 630) bin = 10; //SW
	else if(windVane > 590) bin = 11; //WSW
	else if(windVane > 455) bin = 2;  //NE
	else if(windVane > 400) bin = 1;  //NNE
	else if(windVane > 285) bin = 8;  //S
	else if(windVane > 240) bin = 9;  //SSW
	else if(windVane > 180) bin = 6;  //SE
	else if(windVane > 125) bin = 7;  //SSE
	else if(windVane > 90)  bin = 4;  //E
	else if(windVane > 80)  bin = 3;  //ENE 
	else bin = 5; //ESE
	_windDirBin[bin]++;
}






//ISR for rain gauge.
void ADSWeather::countRain()
{

	if((long)(micros() - last_micros_rg) >= DEBOUNCE_TIME * 1000)
	{
		
		_rainCounter++;
		last_micros_rg = micros();
	}
	
}

//ISR for anemometer.
void ADSWeather::countAnemometer()
{
	if((long)(micros() - last_micros_an) >= DEBOUNCE_TIME * 1000)
	{
		_anemometerCounter++;
		last_micros_an = micros();
	}
}



String ADSWeather::debugCounters()
{
	String debugString;
	if (_fDebug)
	{
		debugString = _debugCounter("_anemometerCounter", _anemometerCounter);
		debugString.concat("\n");
		debugString.concat(_debugCounter("_rainCounter", _rainCounter));
		debugString.concat("\n");
		debugString.concat(_debugCounter("_gustIdx", _gustIdx));
		debugString.concat("\n");
	}
	else
	{
		debugString = "";
	}
	
  return debugString;
}

String ADSWeather::debugWindVane()
{
	String debugString = String(WindDirDebugString);
	return debugString;
}



String ADSWeather::_debugCounter(String counter_name, int counter)
{
	String debugString = "ADSWeather:: " + counter_name + "= ";
	debugString = String(debugString + String(counter));
	return debugString;
}



