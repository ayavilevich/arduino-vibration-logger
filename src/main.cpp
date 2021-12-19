#include <Arduino.h>
#include <SPI.h>
#include <SD.h> // https://www.arduino.cc/en/reference/SD

#include "SparkFun_ADXL345.h" // https://github.com/sparkfun/SparkFun_ADXL345_Arduino_Library/blob/master/examples/SparkFun_ADXL345_Example/SparkFun_ADXL345_Example.ino
#include "RTClib.h" // https://github.com/adafruit/RTClib/blob/master/examples/ds1307/ds1307.ino

/*********** COMMUNICATION SELECTION ***********/
/*    Comment Out The One You Are Not Using    */
// ADXL345 adxl = ADXL345(10);           // USE FOR SPI COMMUNICATION, ADXL345(CS_PIN);
ADXL345 adxl = ADXL345(); // USE FOR I2C COMMUNICATION
const int VIBRATION_SAMPLES = 10;

RTC_DS1307 rtc;

// set up variables using the SD utility library functions:
Sd2Card card;
SdVolume volume;
SdFile root;
const int SD_CS_PIN = 10;

// functions

void printSdCardInfo()
{
	// print the type of card
	Serial.println();
	Serial.print("Card type:         ");
	switch (card.type())
	{
	case SD_CARD_TYPE_SD1:
		Serial.println("SD1");
		break;
	case SD_CARD_TYPE_SD2:
		Serial.println("SD2");
		break;
	case SD_CARD_TYPE_SDHC:
		Serial.println("SDHC");
		break;
	default:
		Serial.println("Unknown");
	}
	// Now we will try to open the 'volume'/'partition' - it should be FAT16 or FAT32
	if (!volume.init(card))
	{
		Serial.println("Could not find FAT16/FAT32 partition.\nMake sure you've formatted the card");
		while (1)
			;
	}
	Serial.print("Clusters:          ");
	Serial.println(volume.clusterCount());
	Serial.print("Blocks x Cluster:  ");
	Serial.println(volume.blocksPerCluster());
	Serial.print("Total Blocks:      ");
	Serial.println(volume.blocksPerCluster() * volume.clusterCount());
	Serial.println();
	// print the type and size of the first FAT-type volume
	uint32_t volumesize;
	Serial.print("Volume type is:    FAT");
	Serial.println(volume.fatType(), DEC);
	volumesize = volume.blocksPerCluster(); // clusters are collections of blocks
	volumesize *= volume.clusterCount();	// we'll have a lot of clusters
	volumesize /= 2;						// SD card blocks are always 512 bytes (2 blocks are 1KB)
	Serial.print("Volume size (Kb):  ");
	Serial.println(volumesize);
	Serial.print("Volume size (Mb):  ");
	volumesize /= 1024;
	Serial.println(volumesize);
	Serial.print("Volume size (Gb):  ");
	Serial.println((float)volumesize / 1024.0);
	Serial.println("\nFiles found on the card (name, date and size in bytes): ");
	root.openRoot(volume);
	// list all files in the card with date and size
	root.ls(LS_R | LS_DATE | LS_SIZE);
	root.close();
}

void setup()
{

	Serial.begin(115200); // Start the serial terminal
	Serial.println("SparkFun ADXL345 Accelerometer Hook Up Guide Example");

	if (!rtc.begin())
	{
		Serial.println("Couldn't find RTC");
		Serial.flush();
		while (1)
			delay(10);
	}

	if (!rtc.isrunning())
	{
		Serial.println("RTC is NOT running, let's set the time!");
		// When time needs to be set on a new device, or after a power loss, the
		// following line sets the RTC to the date & time this sketch was compiled
		rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
		// This line sets the RTC with an explicit date & time, for example to set
		// January 21, 2014 at 3am you would call:
		// rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
	}

	Serial.print("Initializing SD card...");
	// we'll use the initialization code from the utility libraries
	// since we're just testing if the card is working!
	if (!card.init(SPI_HALF_SPEED, SD_CS_PIN))
	{
		Serial.println("initialization failed. Things to check:");
		Serial.println("* is a card inserted?");
		Serial.println("* is your wiring correct?");
		Serial.println("* did you change the chipSelect pin to match your shield or module?");
		while (1)
			;
	}
	else
	{
		Serial.println("Wiring is correct and a card is present.");
	}
	printSdCardInfo();

	// accelerometer init
	adxl.powerOn(); // Power on the ADXL345

	adxl.setRangeSetting(2); // Give the range settings
							 // Accepted values are 2g, 4g, 8g or 16g
							 // Higher Values = Wider Measurement Range
							 // Lower Values = Greater Sensitivity

	adxl.setSpiBit(0); // Configure the device to be in 4 wire SPI mode when set to '0' or 3 wire SPI mode when set to 1
					   // Default: Set to 1
					   // SPI pins on the ATMega328: 11, 12 and 13 as reference in SPI Library

	/*
	adxl.setActivityXYZ(1, 0, 0);  // Set to activate movement detection in the axes "adxl.setActivityXYZ(X, Y, Z);" (1 == ON, 0 == OFF)
	adxl.setActivityThreshold(75); // 62.5mg per increment   // Set activity   // Inactivity thresholds (0-255)

	adxl.setInactivityXYZ(1, 0, 0);	 // Set to detect inactivity in all the axes "adxl.setInactivityXYZ(X, Y, Z);" (1 == ON, 0 == OFF)
	adxl.setInactivityThreshold(75); // 62.5mg per increment   // Set inactivity // Inactivity thresholds (0-255)
	adxl.setTimeInactivity(10);		 // How many seconds of no activity is inactive?

	adxl.setTapDetectionOnXYZ(0, 0, 1); // Detect taps in the directions turned ON "adxl.setTapDetectionOnX(X, Y, Z);" (1 == ON, 0 == OFF)

	// Set values for what is considered a TAP and what is a DOUBLE TAP (0-255)
	adxl.setTapThreshold(50);	  // 62.5 mg per increment
	adxl.setTapDuration(15);	  // 625 μs per increment
	adxl.setDoubleTapLatency(80); // 1.25 ms per increment
	adxl.setDoubleTapWindow(200); // 1.25 ms per increment

	// Set values for what is considered FREE FALL (0-255)
	adxl.setFreeFallThreshold(7); // (5 - 9) recommended - 62.5mg per increment
	adxl.setFreeFallDuration(30); // (20 - 70) recommended - 5ms per increment

	// Setting all interupts to take place on INT1 pin
	//adxl.setImportantInterruptMapping(1, 1, 1, 1, 1);     // Sets "adxl.setEveryInterruptMapping(single tap, double tap, free fall, activity, inactivity);"
	// Accepts only 1 or 2 values for pins INT1 and INT2. This chooses the pin on the ADXL345 to use for Interrupts.
	// This library may have a problem using INT2 pin. Default to INT1 pin.

	// Turn on Interrupts for each mode (1 == ON, 0 == OFF)
	adxl.InactivityINT(1);
	adxl.ActivityINT(1);
	adxl.FreeFallINT(1);
	adxl.doubleTapINT(1);
	adxl.singleTapINT(1);
	*/

	//attachInterrupt(digitalPinToInterrupt(interruptPin), ADXL_ISR, RISING);   // Attach Interrupt
}

int calculateVibrationLevel()
{
	// Accelerometer Readings
	int x, y, z;
	int px, py, pz; // previous measurement
	int intensity = 0;
	adxl.readAccel(&px, &py, &pz);
	for(int i=0;i<VIBRATION_SAMPLES;i++)
	{
		adxl.readAccel(&x, &y, &z);
		intensity += abs(x - px);
		intensity += abs(y - py);
		intensity += abs(z - pz);
		px = x;
		py = y;
		pz = z;
	}
	return intensity;
}

void loop()
{
	DateTime now = rtc.now();

	Serial.print(now.year(), DEC);
	Serial.print('/');
	Serial.print(now.month(), DEC);
	Serial.print('/');
	Serial.print(now.day(), DEC);
	Serial.print(" ");
	Serial.print(now.hour(), DEC);
	Serial.print(':');
	Serial.print(now.minute(), DEC);
	Serial.print(':');
	Serial.print(now.second(), DEC);
	Serial.print(' ');
	Serial.println(calculateVibrationLevel());

	// Accelerometer Readings
	// int x, y, z;
	// adxl.readAccel(&x, &y, &z); // Read the accelerometer values and store them in variables declared above x,y,z

	// Output Results to Serial
	/* UNCOMMENT TO VIEW X Y Z ACCELEROMETER VALUES */
	// Serial.print(x);
	// Serial.print(", ");
	// Serial.print(y);
	// Serial.print(", ");
	// Serial.println(z);

	// ADXL_ISR();
	// You may also choose to avoid using interrupts and simply run the functions within ADXL_ISR();
	//  and place it within the loop instead.
	// This may come in handy when it doesn't matter when the action occurs.

	delay(100);
}
