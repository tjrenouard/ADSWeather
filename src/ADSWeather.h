/**********************************************************
** @file		ADSWeather.cpp
** @author		John Cape
** @copyright	Argent Data Systems, Inc. - All rights reserved
** Updated by Julia R
** Verion 1.2
**
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

*/

#ifndef ADSWeather_h
#define ADSWeather_h

#include "Arduino.h"

#define ADS_VERSION "1.2"

#define VANE_SAMPLE_BINS 50
#define WINDDIR_BINS 16
#define GUST_BINS 30


class ADSWeather
{
  public:
    ADSWeather(int rainPin, int windDirPin, int windSpdPin);
    
	float getRain(); // returns millimeters
	int getWindDirection(); // returns degrees
	float getWindSpeed(); // returns kmph
	float getWindGust(); // returns kmph
	
	
	void update();
	void resetWindGust(); // reset the wind gust value
	void resetRain(); //reset the rain value
	
	static void countRain();
	static void countAnemometer();
	static String getVersion(){return ADS_VERSION;};
	String debugMe();
	
  private:
	int _rainPin;
	int _windDirPin;
	int _windSpdPin;
	
	
	float _rain;
	int _windDir;
	float _windSpd;
	float _windSpdMax;
	
	unsigned long _nextCalc;
	unsigned long _timer;
  	
	unsigned int _vaneSample[VANE_SAMPLE_BINS]; //50 samples from the sensor for consensus averaging
	unsigned int _vaneSampleIdx;
	unsigned int _windDirBin[WINDDIR_BINS];
	
	unsigned int _gust[GUST_BINS]; //Array of 30 wind speed values to calculate maximum gust speed.
	unsigned int _gustIdx;
	
	float _readRainAmount();
    int _readWindDir(bool fDebug = false);
    float _readWindSpd();
	
	
	void _setBin(unsigned int windVane);
	static String _debugCounter(String counter_name, int counter);
	int windDirWeightedAverageCalc_new(int indexStart, int numBins, int max_samples);
	int windDirWeightedAverageCalc_original(int indexStart, int numBins, int max_samples);
	
};

//static void countRain();

#endif