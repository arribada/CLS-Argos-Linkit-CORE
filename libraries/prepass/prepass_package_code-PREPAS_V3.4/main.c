// -------------------------------------------------------------------------- //
//! @file   main.c
//! @brief  Satellite pass prediction test
//! @author Kineis
//! @date   2020-01-14
// -------------------------------------------------------------------------- //

// -------------------------------------------------------------------------- //
//! \page pass_prediction_example_page Satellite pass prediction example
//!
//! This page is presenting how to use the satellite pass prediction software library
//! for KINEIS constellation.
//!
//! Refer to the \ref main function to get the flow of the demonstration.
//!
//!
//! \section code_doc Code documentation
//!
//!
//! Refer to following module for a detailled documentation of the entire code
//! * \ref ARGOS-PASS-PREDICTION-EXAMPLE library files
//!
//!
//! \section bulletin_file_format Bulletin file format
//!
//!
//! The bulletin file contains the orbital parameters of the Argos/KINEIS Constellation Satellites
//! (aka AOP, Adapted Orbital Parameters).
//!
//! These AOP caracterize the orbit characteristics of each satellite.
//!
//! Updated orbital parameters are needed to insure the calculation of accurate satellite pass
//! predictions by the App or the Embedded SW in the time
//!
//! AOP period of validity / Pass predictions calculation max interval : ~3-6 months (depending also
//! on external parameters such as satellite station-keeping capability or solar activity intensity)
//!
//! \note The more up-to-date AOP are, the closest calculation period relative to the AOP dates is,
//! the more accurate the satellite pass predictions will be.
//!
//! Up-to-date AOP are available :
//! * by downloading the corresponding AOP file through ArgosWeb site (typically updated twice a
//!   day) -> "satellite pass predictions" webpage -> "Download satellite AOP" button
//! * by listening the Argos Downlink Allcast messages (typically updated once a day), continuously
//!   broascasted by the Argos satellites with a Downlink capability onboard
//!
//! The AOP file content looks like:
//!
//! ````
//!  MA A 5 3 0 2020  1 26 22 59 44  7195.550  98.5444  327.835  -25.341  101.3587   0.00
//!  MB 9 3 0 0 2020  1 26 22 33 39  7195.632  98.7141  344.177  -25.340  101.3600   0.00
//!  MC B 7 3 0 2020  1 26 23 29 29  7194.917  98.7183  330.404  -25.336  101.3449   0.00
//!  15 5 0 0 0 2020  1 26 23 50  6  7180.549  98.7298  289.399  -25.260  101.0419  -1.78
//!  18 8 0 0 0 2020  1 26 22 12  6  7226.170  99.0661  343.180  -25.499  102.0039  -1.80
//!  19 C 6 0 0 2020  1 26 22  3 52  7226.509  99.1913  291.936  -25.500  102.0108  -1.98
//!  SR D 4 3 0 2020  1 26 22  3 53  7160.246  98.5358  118.029  -25.154  100.6148   0.00
//! ````
//!
//! Each line describes one satellite. The following fields are separated with spaces.
//! * satName (2 char)          : satellite name (2 characters). Current constellation is:
//!     * NOAA_K  = NK or 15
//!     * ANGELS  = A1
//!     * NOAA_N  = NN or 18
//!     * METOP_B = MB
//!     * METOP_A = MA
//!     * METOP_C = MC
//!     * NOAA_P  = NP or 19
//!     * SARAL   = SR
//! * satHexId (int)            : Hexadecimal satellite id. Current constellation is:
//!     * NOAA_K  = 0x5
//!     * ANGELS  = 0x6
//!     * NOAA_N  = 0x8
//!     * METOP_B = 0x9
//!     * METOP_A = 0xA
//!     * METOP_C = 0xB
//!     * NOAA_P  = 0xC
//!     * SARAL   = 0xD
//! * satDcsId (int)            : A-DCS address. Not used by prepas.
//! * downlinkStatus (int)      : Downlink satellite status format A (0=OFF, 1=A4, 3=A3)
//! * uplinkStatus (int)        : Uplink satellite status format A(0=OFF, 1=NEO, 2=A2, 3=A3, 4=A4)
//! * bulletin (Date and time)  : Bulletin epoch (year month day hour min sec)
//! * semiMajorAxisKm (float)   : Semi-major axis (km)
//! * inclinationDeg(float)     : Orbit inclination (deg)
//! * ascNodeLongitudeDeg(float): Longitude of ascending node (deg)
//! * ascNodeDriftDeg(float)    : Ascending node drift during one revolution (deg)
//! * orbitPeriodMin(float)     : Orbit period (min)
//! * semiMajorAxisDriftMeterPerDay(float): Drift of semi-major axis (m/day)
//!
//!
//! \section prediction_config_file_format Prediction configuration file format
//!
//!
//! This input file contains all the configuration parameters and the beacon informations needed to
//! perform pass perdictions calculation.
//!
//! File content looks like this:
//!
//! ````
//! 43.5497 1.485 2020 01 27 00 00 00 2020 01 28 00 00 00 5.0 90.0 5.0 1000 5.0 30
//! ````
//!
//! * 43.5497                       //< Geodetic latitude of the beacon (deg.) [-90, 90]
//! * 1.485                         //< Geodetic longitude of the beacon (deg.E)[0, 360]
//! * 2020 01 27 00 00 00 //< Beginning of prediction (Y/M/D, hh:mm:ss)
//! * 2020 01 28 00 00 00 //< End of prediction (Y/M/D, hh:mm:ss)
//! * 5.0                           //< Minimum elevation of passes [0, 90](default 5 deg)
//! * 90.0                          //< Maximum elevation of passes  [maxElevation >=
//!                                   //< minElevation] (default 90 deg)
//! * 5.0                           //< Minimum duration (default 5 minutes)
//! * 1000                          //< Maximum number of passes per satellite (default
//!                                   //< 1000)
//! * 5                             //< Linear time margin (in minutes/6months) (default
//!                                   //< 5 minutes/6months)
//! * 30                            //< Computation step (default 30s)
//!
//!
//! \section prediction_output_file_format Prediction output file format
//!
//!
//! The output file is made of 2 parts.
//!
//!
//! \subsection prediction_output_file_format_pass pass list
//!
//!
//! This output file contains all the pass predictions calculated for the considered beacon
//! location, pass configurations and choosen time period advised in configuration files.
//!
//! Each line corresponds to one satellite pass over the beacon. It is characterized by :
//! * satellite identifier
//! * pass begin date
//! * pass duration in minutes
//! * maximum site elevation in degrees achieved during the pass
//!
//! Example:
//!
//! ````
//! sat SR date begin 2020/01/27 03:17:30 duration (min) 6.500000 site Max (deg)  8
//! sat SR date begin 2020/01/27 04:53:00 duration (min) 12.500000 site Max (deg)  62
//! sat NP date begin 2020/01/27 05:24:30 duration (min) 13.000000 site Max (deg)  49
//! sat SR date begin 2020/01/27 06:34:00 duration (min) 10.000000 site Max (deg)  16
//! ````
//!
//! \subsection prediction_output_file_format_trcvr_action Transceiver action changes
//!
//!
//! This output file contains also the changes to be performed on transceiver side, depending on
//! the capacity of the satellite present over the beacon.
//!
//! It actually displays the output of \ref PREVIPASS_process_existing_sorted_passes function in a
//! readable manner.
//!
//! Each line corresponds to a change on ARGOS/KINEIS transceiver, and when it should be done. It
//! is characterized by:
//! * date of change (Y/M/D, hh:mm:ss)
//! * minimul uplinlk protocol to ensure transmission:
//!     * '__' No downlink capacity
//!     * 'A2' Argos 2 uplink
//!     * 'A3' Argos 3 uplink
//!     * 'A4' Argos 4 uplink
//! * maximum downlink protcol to ensure reception:
//!     * '__' No downlink capacity
//!     * 'A3' Argos 3 downlink
//!     * 'A4' Argos 4 downlink
//! * transceiver action for this pass per \ref TransceiverAction_t
//!     * 0 UNKNOWN_TRANSCEIVER_ACTION, //!< No action
//!     * 1 ENABLE_TX_ONLY, //!< Enable TX only
//!     * 2 ENABLE_RX_ONLY, //!< Enable RX only
//!     * 3 ENABLE_TX_AND_RX, //!< Enable RX after TX
//!     * 4 DISABLE_TX_RX, //!< Disable RX/TX
//!     * 5 CHANGE_TX_MOD, //!< New TX capacity detected
//!     * 6 CHANGE_RX_MOD, //!< New RX capacity detected
//!     * 7 CHANGE_TX_PLUS_RX_MOD, //!< New TX and RX capacity detected
//!     * 8 KEEP_TRANSCEIVER_STATE      //!< Keep transceiver state
//!
//! Example:
//!
//! ````
//! 2020-01-27T03:17:30 A3|A3|3
//! 2020-01-27T03:24:00 __|__|4
//! 2020-01-27T04:53:00 A3|A3|3
//! 2020-01-27T05:05:30 __|__|4
//! 2020-01-27T05:24:30 A3|__|1
//! 2020-01-27T05:37:30 __|__|4
//! ````
// -------------------------------------------------------------------------- //

// -------------------------------------------------------------------------- //
//! Includes
// -------------------------------------------------------------------------- //

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "previpass.h"

// -------------------------------------------------------------------------- //
//! @addtogroup ARGOS-PASS-PREDICTION-EXAMPLE
//! @brief Satellite pass prediction usage example
//! @{
// -------------------------------------------------------------------------- //

// -------------------------------------------------------------------------- //
//! Defines values
// -------------------------------------------------------------------------- //

//! Keep in memory a maximum of 7 days, with at worst 16 passes per satellite
//! when at poles
#define MAX_SATELLITE_PASS_COUNT (AOP_SATELLITES_COUNT*16*7)

//! Buffer size for IO file
#define ONE_LINE_BUFFER_SIZE 1024

//! Choose IO functions (prevent compilation warning under Windows/mingw)
#ifdef __MINGW32__
#define fprintf  __mingw_fprintf
#define sscanf  __mingw_sscanf
#define sprintf __mingw_sprintf
#endif


// -------------------------------------------------------------------------- //
//! @brief Get current system time in microsecond.
// -------------------------------------------------------------------------- //

int64_t currentSystemTimeUSec(void)
{
	struct timeval tv;

	gettimeofday(&tv, NULL);

	return tv.tv_sec * 1000000LL + tv.tv_usec;
}


#ifndef PREPAS_EMBEDDED
// -------------------------------------------------------------------------- //
//! @brief Read prepras configuration from a file.
//!
//! \param[in] configurationFilePath input file containing the prediction pass configuration. Refer
//!            to \ref prediction_config_file_format section for details about the format
//! \param[out] prepasConfiguration
//!
//! \return status false if not able to parse file, true if ok
bool readConfiguration(const char *configurationFilePath,
			struct PredictionPassConfiguration_t *prepasConfiguration)
{
	//! Open file
	FILE *confFile = fopen(configurationFilePath, "r");

	if (!confFile)
		return false;


	//! Read configuration at once
	char line[ONE_LINE_BUFFER_SIZE];

	if (!fgets(line, ONE_LINE_BUFFER_SIZE, confFile)) {
		//! Unable to read
		return false;
	}
	uint64_t lineSize = strlen(line);

	if (lineSize == ONE_LINE_BUFFER_SIZE - 1) {
		//! Overflow
		return false;
	}
	if (lineSize < 10) {
		//! Empty line
		return false;
	}

	//! Scan
	int nbFileds = sscanf(line,
			"%f%f%hu%hhu%hu%hhu%hhu%hhu%hu%hhu%hu%hhu%hhu%hhu%f%f%f%u%f%u",
			&prepasConfiguration->beaconLatitude,
			&prepasConfiguration->beaconLongitude,
			&prepasConfiguration->start.year,
			&prepasConfiguration->start.month,
			&prepasConfiguration->start.day,
			&prepasConfiguration->start.hour,
			&prepasConfiguration->start.minute,
			&prepasConfiguration->start.second,
			&prepasConfiguration->end.year,
			&prepasConfiguration->end.month,
			&prepasConfiguration->end.day,
			&prepasConfiguration->end.hour,
			&prepasConfiguration->end.minute,
			&prepasConfiguration->end.second,
			&prepasConfiguration->minElevation,
			&prepasConfiguration->maxElevation,
			&prepasConfiguration->minPassDurationMinute,
			&prepasConfiguration->maxPasses,
			&prepasConfiguration->timeMarginMinPer6months,
			&prepasConfiguration->computationStepSecond);
	if (nbFileds != 20) {
		//! Bad line
		return false;
	}

	//! Close file
	fclose(confFile);

	return true;
}
#endif


#ifndef PREPAS_EMBEDDED
// -------------------------------------------------------------------------- //
//! @brief Read AOP from a bulletin file and allocate dynamically a structure to store them.
//!
//! \param[in] bulletinFilePath input file containing the AOP information. Refer to
//!            \ref bulletin_file_format section for details about the format
//! \param[out] aopTable
//! \param[out] _nbSatsInAopTable
//!
//! \return status false if not able to parse file, true if ok
bool readOrbitParameter(const char *bulletinFilePath, struct AopSatelliteEntry_t *aopTable[],
			uint8_t *_nbSatsInAopTable)
{
	//! Open file
	FILE *bulletinFile = fopen(bulletinFilePath, "r");

	if (!bulletinFile)
		return false;


	//! Read each line of AOP file
	*_nbSatsInAopTable = 0;
	char line[ONE_LINE_BUFFER_SIZE];
	*aopTable = NULL;
	while (!feof(bulletinFile)) {
		if (!fgets(line, ONE_LINE_BUFFER_SIZE, bulletinFile)) {
			//! Unable to read
			return false;
		}
		uint64_t lineSize = strlen(line);

		if (lineSize == ONE_LINE_BUFFER_SIZE - 1) {
			//! Overflow
			return false;
		}
		if (lineSize < 10) {
			//! Empty line
			continue;
		}

		//! Scan
		struct AopSatelliteEntry_t aopSatelliteEntry;
		char satNameTwoChars[2];
		uint8_t downlinkOpStatus,
			uplinkOpStatus;
		int nbFileds = sscanf(line,
				"%2s%hhx%hhu%hhu%hhu%hu%hhu%hu%hhu%hhu%hhu%f%f%f%f%f%f",
				satNameTwoChars,
				&aopSatelliteEntry.satHexId,
				&aopSatelliteEntry.satDcsId,
				&downlinkOpStatus,
				&uplinkOpStatus,
				&aopSatelliteEntry.bulletin.year,
				&aopSatelliteEntry.bulletin.month,
				&aopSatelliteEntry.bulletin.day,
				&aopSatelliteEntry.bulletin.hour,
				&aopSatelliteEntry.bulletin.minute,
				&aopSatelliteEntry.bulletin.second,
				&aopSatelliteEntry.semiMajorAxisKm,
				&aopSatelliteEntry.inclinationDeg,
				&aopSatelliteEntry.ascNodeLongitudeDeg,
				&aopSatelliteEntry.ascNodeDriftDeg,
				&aopSatelliteEntry.orbitPeriodMin,
				&aopSatelliteEntry.semiMajorAxisDriftMeterPerDay);
		if (nbFileds != 17) {
			//! Bad line
			continue;
		}

		//! Conversion to generic status
		PREVIPASS_status_format_a_to_generic(downlinkOpStatus,
				uplinkOpStatus,
				aopSatelliteEntry.satHexId,
				&aopSatelliteEntry.downlinkStatus,
				&aopSatelliteEntry.uplinkStatus);

		//! Allocation or reallocation
		*aopTable = (struct AopSatelliteEntry_t *) realloc(*aopTable,
				sizeof(struct AopSatelliteEntry_t) * (*_nbSatsInAopTable + 1));

		//! Use scanned values
		(*aopTable)[*_nbSatsInAopTable] = aopSatelliteEntry;

		//! Go to next sat
		++(*_nbSatsInAopTable);
	}

	//! Close file
	fclose(bulletinFile);

	return true;
}
#endif


// -------------------------------------------------------------------------- //
//! @brief Convert satellite code to two chars name.
// -------------------------------------------------------------------------- //

void getSatName(uint8_t satHexId, char _satNameTwoChars[])
{
	switch (satHexId) {
	case 0x6:
		strcpy(_satNameTwoChars, "A1");
		break;
	case 0xA:
		strcpy(_satNameTwoChars, "MA");
		break;
	case 0x9:
		strcpy(_satNameTwoChars, "MB");
		break;
	case 0xB:
		strcpy(_satNameTwoChars, "MC");
		break;
	case 0x5:
		strcpy(_satNameTwoChars, "NK");
		break;
	case 0x8:
		strcpy(_satNameTwoChars, "NN");
		break;
	case 0xC:
		strcpy(_satNameTwoChars, "NP");
		break;
	case 0xD:
		strcpy(_satNameTwoChars, "SR");
		break;
	default:
		strcpy(_satNameTwoChars, "XX");
	}
}


// -------------------------------------------------------------------------- //
//! @brief Display one pass
//!
//! Write one pass prediction to output stream (e.g. file or console)
//!
//! Refer to \ref prediction_output_file_format section for details about the format
//!
//! \param[out] _stream outputfile
//! \param[in] _nextPass
// -------------------------------------------------------------------------- //

void writeOnePass(FILE *_stream, struct SatelliteNextPassPrediction_t *_nextPass)
{
	struct CalendarDateTime_t viewable_timedata;

	PREVIPASS_UTIL_date_stu90_calendar(_nextPass->epoch - EPOCH_90_TO_70_OFFSET,
			&viewable_timedata);

	//! Sat name
	char satNameTwoChars[3];

	getSatName(_nextPass->satHexId, satNameTwoChars);

	//! Display one pass
	fprintf(_stream,
			"sat %2s date begin %4hu/%02hhu/%02hu %02hhu:%02hhu:%02hhu duration (min) %f site Max (deg)  %i\n",
			satNameTwoChars,
			viewable_timedata.year,
			viewable_timedata.month,
			viewable_timedata.day,
			viewable_timedata.hour,
			viewable_timedata.minute,
			viewable_timedata.second,
			_nextPass->duration/60.,
			_nextPass->elevationMax);

}


// -------------------------------------------------------------------------- //
//! @brief Display passes linked list.
//!
//! Write the pass prediction list to output stream (e.g. file or console)
//!
//! \ref writeOnePass is called for each pass of the list. Refer to this function for details about
//! the output format
//!
//! \param[out] _stream outputfile
//! \param[in] _previsionPassesList
// -------------------------------------------------------------------------- //

void writeOutputList(FILE *_stream, struct SatPassLinkedListElement_t *_previsionPassesList)
{
	//! Write one line per pass
	struct SatPassLinkedListElement_t *listElementPtr = _previsionPassesList;

	while (listElementPtr != NULL) {
		writeOnePass(_stream, &listElementPtr->element);

		//! Go to next pass
		listElementPtr = listElementPtr->next;
	}
}


// -------------------------------------------------------------------------- //
//! @brief Main function of Prepas test.
// -------------------------------------------------------------------------- //

int main(void)
{
	// ----------------------------------------------------------------------- //
	//! **CONFIGURATION**
	// ----------------------------------------------------------------------- //

	//! Misc configuration
	uint64_t nbComputations = 1000;
	bool stdoutDisplay = false;
	uint32_t processPassStep = 5;

#ifndef PREPAS_EMBEDDED
	//! In case of PC app, parse prediction configuration from configurationFilePath, call
	//! \ref readConfiguration in a way to fill-up \ref PredictionPassConfiguration_t

	//!
	//! \verbatim
	char configurationFilePath[] = "Data/PrepasConf.txt";
	struct PredictionPassConfiguration_t prepasConfiguration;
	//! \endverbatim

	if (!readConfiguration(configurationFilePath, &prepasConfiguration))
		return -1;

#else
	//! In case of emmbedded app, directly fill up the \ref PredictionPassConfiguration_t struct

	//!
	//! \verbatim
	struct PredictionPassConfiguration_t prepasConfiguration = {

		43.55f,                       //< Geodetic latitude of the beacon (deg.) [-90, 90]
		1.485f,                       //< Geodetic longitude of the beacon (deg.E)[0, 360]
		{ 2020, 03, 28, 00, 00, 00 }, //< Beginning of prediction (Y/M/D, hh:mm:ss)
		{ 2020, 03, 29, 00, 00, 00 }, //< End of prediction (Y/M/D, hh:mm:ss)
		5.0f,                         //< Minimum elevation of passes [0, 90](default 5 deg)
		90.0f,                        //< Maximum elevation of passes  [maxElevation >=
						//< minElevation] (default 90 deg)
		5.0f,                         //< Minimum duration (default 5 minutes)
		1000,                         //< Maximum number of passes per satellite (default
						//< 1000)
		5,                            //< Linear time margin (in minutes/6months) (default
						//< 5 minutes/6months)
		30                            //< Computation step (default 30s)
	};
	//! \endverbatim
#endif

#ifndef PREPAS_EMBEDDED
	//! In case of PC app, parse AOP data from bulletinFilePath, call \ref readOrbitParameter
	//! in a way to fil-up \ref AopSatelliteEntry_t

	//!
	//! \verbatim
	char bulletinFilePath[] = "Data/bulletin.dat";
	struct AopSatelliteEntry_t *aopTable = NULL;
	//! \endverbatim
	uint8_t nbSatsInAopTable;

	if (!readOrbitParameter(bulletinFilePath, &aopTable, &nbSatsInAopTable))
		return -1;

#else
	//! In case of emmbedded app, directly fill up the \ref AopSatelliteEntry_t struct

	//!
	//! \verbatim
	struct AopSatelliteEntry_t aopTable[] = {

		{ 0xA, 5, SAT_DNLK_ON_WITH_A3, SAT_UPLK_ON_WITH_A3, {2020, 3, 26, 22, 17, 58},
7195.543f, 98.5333f, 337.191f, -25.341f, 101.3586f, 0.00f},
		{ 0x9, 3, SAT_DNLK_OFF,        SAT_UPLK_ON_WITH_A3, {2020, 3, 26, 23, 33, 23},
7195.595f, 98.7011f, 329.290f, -25.340f, 101.3592f, 0.00f},
		{ 0xB, 7, SAT_DNLK_ON_WITH_A3, SAT_UPLK_ON_WITH_A3, {2020, 3, 26, 22, 45, 19},
7195.624f, 98.7080f, 341.538f, -25.340f, 101.3598f, 0.00f},
		{ 0x5, 0, SAT_DNLK_OFF,        SAT_UPLK_ON_WITH_A2, {2020, 3, 26, 22,  0, 17},
7180.518f, 98.7247f, 317.478f, -25.259f, 101.0413f, -1.78f},
		{ 0x8, 0, SAT_DNLK_OFF,        SAT_UPLK_ON_WITH_A2, {2020, 3, 26, 22,  9, 30},
7226.140f, 99.0541f, 345.355f, -25.499f, 102.0034f, -1.80f},
		{ 0xC, 6, SAT_DNLK_OFF,        SAT_UPLK_ON_WITH_A3, {2020, 3, 26, 23, 48, 50},
7226.486f, 99.1949f, 268.101f, -25.500f, 102.0103f, -1.98f},
		{ 0xD, 4, SAT_DNLK_ON_WITH_A3, SAT_UPLK_ON_WITH_A3, {2020, 3, 26, 22, 32,  0},
7160.258f, 98.5403f, 110.937f, -25.154f, 100.6151f, 0.00f}
	};
	uint8_t nbSatsInAopTable = 7;
	//! \endverbatim
#endif


	// ----------------------------------------------------------------------- //
	//! **EXAMPLE OF MULTIPLE PASSES STRATEGY**
	// ----------------------------------------------------------------------- //

	struct SatPassLinkedListElement_t *previsionPassesList = NULL;

	//! This loop is only for performance measurement purpose. Prepas users should
	//! call \ref PREVIPASS_compute_new_prediction_pass_times only once.
	int64_t startTime = currentSystemTimeUSec();

	for (uint64_t i = 0 ; i < nbComputations ; ++i) {
		//! Main prepas computation
		bool memoryPoolOverflow;

		previsionPassesList =
			PREVIPASS_compute_new_prediction_pass_times(&prepasConfiguration,
					aopTable,
					nbSatsInAopTable,
					&memoryPoolOverflow);
		if (memoryPoolOverflow) {
			fprintf(stdout, "Memory pool full. Cf MY_MALLOC_MAX_BYTES (previpass.c)\n");
			break;
		}
	}

	//! Display performance measurements
	double computationTime = (double) (currentSystemTimeUSec() - startTime)
		/ nbComputations / 1000.;
	fprintf(stdout, "One prediction time: %f msec\n", computationTime);

	//! Display results
#ifndef PREPAS_EMBEDDED
	//! In case of PC app, the pass prediction is written in a result file resultsFilePath
	//! through the \ref writeOutputList function.

	//!
	//! \verbatim
	char resultsFilePath[]  = "Data/PrepasRun.txt";
	//! \endverbatim
	FILE *resultsFile;

	resultsFile = fopen(resultsFilePath, "w");
	if (!resultsFile)
		return -1;

	writeOutputList(resultsFile, previsionPassesList);
#endif
	//! In case stdoutDisplay is enabled, the pass prediction is written to standard output
	//! through the \ref writeOutputList function.
	if (stdoutDisplay)
		writeOutputList(stdout, previsionPassesList);


	//! Use algorithm
	uint32_t computationStartSec;

	PREVIPASS_UTIL_date_calendar_stu90(prepasConfiguration.start, &computationStartSec);
			computationStartSec += EPOCH_90_TO_70_OFFSET;
	uint32_t computationEndSec;

	PREVIPASS_UTIL_date_calendar_stu90(prepasConfiguration.end, &computationEndSec);
			computationEndSec += EPOCH_90_TO_70_OFFSET;

	//! This loop is only for performance measurement purpose. Prepas users should
	//! use \ref PREVIPASS_process_existing_sorted_passes only with a time loop.
	//! See example below.
	startTime = currentSystemTimeUSec();
	for (uint64_t i = 0 ; i < nbComputations ; ++i) {
		for (uint32_t currentTimeStu90 = computationStartSec;
				currentTimeStu90 < computationEndSec;
				currentTimeStu90 += processPassStep) {
			//! Performance test
			PREVIPASS_process_existing_sorted_passes(currentTimeStu90,
					previsionPassesList);
		}
	}
	computationTime = (double) (currentSystemTimeUSec() - startTime)
		/ processPassStep
		/ nbComputations;
	fprintf(stdout, "Process time: %f usec\n", computationTime);

	//! Example of use of \ref PREVIPASS_process_existing_sorted_passes for prepas users
	for (uint32_t currentTimeStu90 = computationStartSec;
			currentTimeStu90 < computationEndSec;
			currentTimeStu90 += processPassStep) {
		//! Perform post processing now each time we get a new epoch time to test
		struct NextPassTransceiverCapacity_t trcvrAction =
			PREVIPASS_process_existing_sorted_passes(
					currentTimeStu90,
					previsionPassesList);

		//! Display changes
		if (trcvrAction.trcvrActionForNextPass != KEEP_TRANSCEIVER_STATE) {
			//! Calendar date time
			struct CalendarDateTime_t currentTimeCalendar;

			PREVIPASS_UTIL_date_stu90_calendar(currentTimeStu90 - EPOCH_90_TO_70_OFFSET,
					&currentTimeCalendar);

			//! Readable transmitter change
			char readableStatus[8] = "__|__|_";

			if (trcvrAction.minUplinkStatus != SAT_UPLK_OFF) {
				readableStatus[0] = 'A';
				readableStatus[1] = (char)('0' + trcvrAction.minUplinkStatus);
			}
			if (trcvrAction.maxDownlinkStatus != SAT_DNLK_OFF) {
				readableStatus[3] = 'A';
				readableStatus[4] = (char)('0' + trcvrAction.maxDownlinkStatus);
			}
			readableStatus[6] = (char)('0' + trcvrAction.trcvrActionForNextPass);

			//! Display
			char buffer[ONE_LINE_BUFFER_SIZE];

			sprintf(buffer,
					"%04hu-%02hhu-%02huT%02hhu:%02hhu:%02hhu %s\n",
					currentTimeCalendar.year,
					currentTimeCalendar.month,
					currentTimeCalendar.day,
					currentTimeCalendar.hour,
					currentTimeCalendar.minute,
					currentTimeCalendar.second,
					readableStatus);
			if (stdoutDisplay)
				fprintf(stdout, "%s", buffer);


#ifndef PREPAS_EMBEDDED
			fprintf(resultsFile, "%s", buffer);
#endif

		}
	}


	// ----------------------------------------------------------------------- //
	//! **EXAMPLE OF SINGLE PASS STRATEGY**
	// ----------------------------------------------------------------------- //

	//! Get next pass without status criteria
	struct SatelliteNextPassPrediction_t nextPass;

	if (PREVIPASS_compute_next_pass(&prepasConfiguration,
				aopTable,
				nbSatsInAopTable,
				&nextPass)) {
		if (stdoutDisplay)
			writeOnePass(stdout, &nextPass);


#ifndef PREPAS_EMBEDDED
		writeOnePass(resultsFile, &nextPass);
#endif
	}

	//! Now do computation from 10:30, NN->MB->MA order
	prepasConfiguration.start.hour   = 10;
	prepasConfiguration.start.minute = 30;

	//! Get next pass after 10:30 with at least A2 uplink (i.e. all sats)
	if (PREVIPASS_compute_next_pass_with_status(&prepasConfiguration,
				aopTable,
				nbSatsInAopTable,
				SAT_DNLK_OFF,
				SAT_UPLK_ON_WITH_A2,
				&nextPass)) {
		if (stdoutDisplay)
			writeOnePass(stdout, &nextPass);

#ifndef PREPAS_EMBEDDED
		writeOnePass(resultsFile, &nextPass);
#endif
	}

	//! Get next pass after 10:30 with at least A3 uplink (NP, MA, MB, MC, SR)
	if (PREVIPASS_compute_next_pass_with_status(&prepasConfiguration,
				aopTable,
				nbSatsInAopTable,
				SAT_DNLK_OFF,
				SAT_UPLK_ON_WITH_A3,
				&nextPass)) {
		if (stdoutDisplay)
			writeOnePass(stdout, &nextPass);

#ifndef PREPAS_EMBEDDED
		writeOnePass(resultsFile, &nextPass);
#endif
	}

	//! Get next pass after 10:30 with downlink (MA, MC, SR)
	if (PREVIPASS_compute_next_pass_with_status(&prepasConfiguration,
				aopTable,
				nbSatsInAopTable,
				SAT_DNLK_ON_WITH_A3,
				SAT_UPLK_ON_WITH_A3,
				&nextPass)) {
		if (stdoutDisplay)
			writeOnePass(stdout, &nextPass);

#ifndef PREPAS_EMBEDDED
		writeOnePass(resultsFile, &nextPass);
#endif
	}

	//! Performance measurement
	startTime = currentSystemTimeUSec();
	for (uint64_t i = 0 ; i < nbComputations ; ++i) {
		PREVIPASS_compute_next_pass_with_status(&prepasConfiguration,
				aopTable,
				nbSatsInAopTable,
				SAT_DNLK_ON_WITH_A3,
				SAT_UPLK_ON_WITH_A3,
				&nextPass);
	}
	computationTime = (double) (currentSystemTimeUSec() - startTime)
		/ nbComputations;
	fprintf(stdout, "Next pass time: %f usec\n", computationTime);

#ifndef PREPAS_EMBEDDED
	//! Close results file
	fclose(resultsFile);
#endif

	return 0;
}


// -------------------------------------------------------------------------- //
//! @} (end addtogroup ARGOS-PASS-PREDICTION-EXAMPLE)
// -------------------------------------------------------------------------- //
