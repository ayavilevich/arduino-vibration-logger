#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
// #define BASIC_SD
#ifdef BASIC_SD
#include <SD.h> // https://www.arduino.cc/en/reference/SD
#else
#include "SdFat.h" // https://github.com/greiman/SdFat
#endif
#include <MemoryFree.h>

#include "SparkFun_ADXL345.h" // https://github.com/sparkfun/SparkFun_ADXL345_Arduino_Library/blob/master/examples/SparkFun_ADXL345_Example/SparkFun_ADXL345_Example.ino
#include "RTClib.h"			  // https://github.com/adafruit/RTClib/blob/master/examples/ds1307/ds1307.ino

// ADXL345 adxl = ADXL345(10);           // USE FOR SPI COMMUNICATION, ADXL345(CS_PIN);
ADXL345 adxl = ADXL345(); // USE FOR I2C COMMUNICATION
const int VIBRATION_SAMPLES = 10;

RTC_DS1307 rtc;

const int SD_CS_PIN = 10;
#ifdef BASIC_SD
Sd2Card card;
#else
// Test with reduced SPI speed for breadboards.  SD_SCK_MHZ(4) will select
// the highest speed supported by the board that is not over 4 MHz.
// Change SPI_SPEED to SD_SCK_MHZ(50) for best performance.
#define SPI_SPEED SD_SCK_MHZ(4)

// Note: Uno will not support SD_FAT_TYPE = 3.
// SD_FAT_TYPE = 0 for SdFat/File as defined in SdFatConfig.h,
// 1 for FAT16/FAT32, 2 for exFAT, 3 for FAT16/FAT32 and exFAT.
#define SD_FAT_TYPE 1 // 1 for the 128MB card

#if SD_FAT_TYPE == 0
SdFat sd;
typedef File file_t;
#elif SD_FAT_TYPE == 1
SdFat32 sd;
typedef File32 file_t;
#elif SD_FAT_TYPE == 2
SdExFat sd;
typedef ExFile file_t;
#elif SD_FAT_TYPE == 3
SdFs sd;
typedef FsFile file_t;
#else // SD_FAT_TYPE
#error Invalid SD_FAT_TYPE
#endif // SD_FAT_TYPE
#endif // BASIC_SD

// functions

void printSdCardInfo()
{
#ifdef BASIC_SD
	SdVolume volume;
	SdFile root;
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
#else
	uint32_t size = sd.card()->sectorCount();
	if (size == 0)
	{
		Serial.println(F("Can't determine the card size."));
		// cardOrSpeed();
		return;
	}
	uint32_t sizeMB = 0.000512 * size + 0.5;
	Serial.print(F("Card size: "));
	Serial.println(sizeMB);
	Serial.println(F(" MB (MB = 1,000,000 bytes)"));
	Serial.print(F("Volume is FAT"));
	Serial.println((int(sd.vol()->fatType())));
	Serial.print(F("Cluster size (bytes): "));
	Serial.println(sd.vol()->bytesPerCluster());

	Serial.println(F("Files found (date time size name):"));
	sd.ls(LS_R | LS_DATE | LS_SIZE);

	if ((sizeMB > 1100 && sd.vol()->sectorsPerCluster() < 64) || (sizeMB < 2200 && sd.vol()->fatType() == 32))
	{
		Serial.println(F("This card should be reformatted for best performance."));
		Serial.println(F("Use a cluster size of 32 KB for cards larger than 1 GB."));
		Serial.println(F("Only cards larger than 2 GB should be formatted FAT32."));
		// reformatMsg();
		return;
	}
#endif
}

void i2cScanner()
{
	byte error, address;
	int nDevices;

	Serial.println("Scanning...");

	nDevices = 0;
	for (address = 1; address < 127; address++)
	{
		// The i2c_scanner uses the return value of
		// the Write.endTransmisstion to see if
		// a device did acknowledge to the address.
		Wire.beginTransmission(address);
		error = Wire.endTransmission();

		if (error == 0)
		{
			Serial.print("I2C device found at address 0x");
			if (address < 16)
				Serial.print("0");
			Serial.print(address, HEX);
			Serial.println("  !");

			nDevices++;
		}
		else if (error == 4)
		{
			Serial.print("Unknown error at address 0x");
			if (address < 16)
				Serial.print("0");
			Serial.println(address, HEX);
		}
	}
	if (nDevices == 0)
		Serial.println("No I2C devices found\n");
	else
		Serial.println("done\n");
}

void setup()
{

	Serial.begin(115200); // Start the serial terminal
	Serial.println(F("Vibration logger"));

	Wire.begin();
	// i2cScanner();

	if (!rtc.begin())
	{
		Serial.println(F("Couldn't find RTC"));
		Serial.flush();
		while (1)
			delay(10);
	}

	Serial.println(F("Initializing RTC"));
	if (!rtc.isrunning())
	{
		Serial.println(F("RTC is NOT running, let's set the time!"));
		// When time needs to be set on a new device, or after a power loss, the
		// following line sets the RTC to the date & time this sketch was compiled
		rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
		// This line sets the RTC with an explicit date & time, for example to set
		// January 21, 2014 at 3am you would call:
		// rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
	}

	// When time needs to be re-set on a previously configured device, the
	// following line sets the RTC to the date & time this sketch was compiled
	// rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
	// This line sets the RTC with an explicit date & time, for example to set
	// January 21, 2014 at 3am you would call:
	// rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));

	Serial.println(F("Initializing SD card..."));
	// we'll use the initialization code from the utility libraries
	// since we're just testing if the card is working!
#ifdef BASIC_SD
	if (!card.init(SPI_HALF_SPEED, SD_CS_PIN))
	{
		Serial.println(F("initialization failed. Things to check:"));
		// Serial.println("* is a card inserted?");
		// Serial.println("* is your wiring correct?");
		// Serial.println("* did you change the chipSelect pin to match your shield or module?");
		while (1)
			;
	}
#else
	if (!sd.begin(SD_CS_PIN, SPI_SPEED))
	{
		if (sd.card()->errorCode())
		{
			Serial.println(F(
				"\nSD initialization failed.\n"
				"Do not reformat the card!\n"
				"Is the card correctly inserted?\n"
				"Is chipSelect set to the correct value?\n"
				"Does another SPI device need to be disabled?\n"
				"Is there a wiring/soldering problem?"));
			Serial.print(F("errorCode: "));
			Serial.println(int(sd.card()->errorCode()));
			Serial.print(F("ErrorData: "));
			Serial.println(int(sd.card()->errorData()));
			return;
		}
		Serial.println(F("Card successfully initialized."));
		if (sd.vol()->fatType() == 0)
		{
			Serial.println(F("Can't find a valid FAT16/FAT32 partition."));
			// reformatMsg();
			return;
		}
		Serial.println(F("Can't determine error type"));

		while (1)
			;
	}
#endif
	else
	{
		Serial.println(F("Wiring is correct and a card is present."));
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

	Serial.println(F("Done setup"));
}

int calculateVibrationLevel()
{
	// Accelerometer Readings
	int x, y, z;
	int px, py, pz; // previous measurement
	int intensity = 0;
	adxl.readAccel(&px, &py, &pz);
	for (int i = 0; i < VIBRATION_SAMPLES; i++)
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
	// debug
	// Serial.print("freeMemory()=");
	// Serial.println(freeMemory());

	// generate file name
	DateTime now = rtc.now();
	const int filenameLength = 8 + 1 + 3 + 1;
	char filename[filenameLength];
	snprintf(filename, filenameLength, "%d%02d%02d.csv", now.year(), now.month(), now.day());
	// strcpy(filename, "test.txt");
	// Serial.println(strlen(filename));
	// Serial.println(filename);

	// calculate vibration level
	int vibrationLevel = calculateVibrationLevel();
	// Serial.println(vibrationLevel);

	// render data row
	const int dataRowLength = 8 + 1 + 10 + 1;
	char dataRow[dataRowLength];
	snprintf(dataRow, dataRowLength, "%02d:%02d:%02d,%d", (int)now.hour(), (int)now.minute(), (int)now.second(), vibrationLevel);
	// strcpy(dataRow, "testData");

	// print to the serial port
	Serial.println(dataRow);

	// write to file
#ifdef BASIC_SD
	File dataFile = SD.open(filename, FILE_WRITE | O_APPEND);
	if (dataFile) // if the file is available, write to it:
#else
	file_t dataFile;
	if (dataFile.open(filename, O_RDWR | O_CREAT | O_APPEND | O_AT_END))
#endif
	{
		dataFile.println(dataRow);
		dataFile.close();
	}
	else
	{
		Serial.print(F("Error opening file - "));
		Serial.println(filename);
		Serial.print(F("freeMemory()="));
		Serial.println(freeMemory());
	}

	// wait until next measurement
	delay(1000);
}
