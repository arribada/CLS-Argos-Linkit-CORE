#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"

#include <iostream>

extern "C" {
	#include "previpass.h"
}

using namespace std::literals::string_literals;


TEST_GROUP(Previpass)
{
};

TEST(Previpass, BasicTest)
{
	// Taken from the README.md file in the supplied V3.4 library
    struct PredictionPassConfiguration_t prepasConfiguration = {

        43.5497f,                     //< Geodetic latitude of the beacon (deg.) [-90, 90]
        1.485f,                       //< Geodetic longitude of the beacon (deg.E)[0, 360]
        { 2020, 01, 27, 00, 00, 00 }, //< Beginning of prediction (Y/M/D, hh:mm:ss)
        { 2020, 01, 28, 00, 00, 00 }, //< End of prediction (Y/M/D, hh:mm:ss)
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
    struct AopSatelliteEntry_t aopTable[] = {
    		{ 0xA, 5, SAT_DNLK_ON_WITH_A3, SAT_UPLK_ON_WITH_A3, { 2020, 1, 26, 22, 59, 44 }, 7195.550f, 98.5444f, 327.835f, -25.341f, 101.3587f, 0.00f },
			{ 0x9, 3, SAT_DNLK_OFF, SAT_UPLK_ON_WITH_A3, { 2020, 1, 26, 22, 33, 39 }, 7195.632f, 98.7141f, 344.177f, -25.340f, 101.3600f, 0.00f },
			{ 0xB, 7, SAT_DNLK_ON_WITH_A3, SAT_UPLK_ON_WITH_A3, { 2020, 1, 26, 23, 29, 29 }, 7194.917f, 98.7183f, 330.404f, -25.336f, 101.3449f, 0.00f },
			{ 0x5, 0, SAT_DNLK_OFF, SAT_UPLK_ON_WITH_A2, { 2020, 1, 26, 23, 50, 6 }, 7180.549f, 98.7298f, 289.399f, -25.260f, 101.0419f, -1.78f },
			{ 0x8, 0, SAT_DNLK_OFF, SAT_UPLK_ON_WITH_A2, { 2020, 1, 26, 22, 12, 6 }, 7226.170f, 99.0661f, 343.180f, -25.499f, 102.0039f, -1.80f },
			{ 0xC, 6, SAT_DNLK_OFF, SAT_UPLK_ON_WITH_A3, { 2020, 1, 26, 22, 3, 52 }, 7226.509f, 99.1913f, 291.936f, -25.500f, 102.0108f, -1.98f },
			{ 0xD, 4, SAT_DNLK_ON_WITH_A3, SAT_UPLK_ON_WITH_A3, { 2020, 1, 26, 22, 3, 53 }, 7160.246f, 98.5358f, 118.029f, -25.154f, 100.6148f, 0.00f }
    };

    uint8_t nbSatsInAopTable = 7;
    SatelliteNextPassPrediction_t nextPass;
    CHECK_TRUE(PREVIPASS_compute_next_pass(
    		&prepasConfiguration,
			aopTable,
			nbSatsInAopTable,
			&nextPass));
    std::cout << "epoch:\t" << nextPass.epoch << std::endl;
    std::cout << "duration:\t" << nextPass.duration << std::endl;
    std::cout << "elevationMax:\t" << nextPass.elevationMax << std::endl;
    std::cout << "satHexId:\t" << std::hex << nextPass.satHexId << std::endl;
    std::cout << "downlinkStatus:\t" << nextPass.downlinkStatus << std::endl;
    std::cout << "uplinkStatus:\t" << nextPass.uplinkStatus << std::endl;
}
