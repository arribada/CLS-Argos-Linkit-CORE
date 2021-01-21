# INTRODUCTION

This project is packaging the satellites pass prediction software library for KINEIS
constellation and an example on how to use it.

Two flavours of the example are depicted:
* EMBEDDED TARGET: There is no file manipulation, langage C structures are fill-up with the
configuration parameters. The output is written in langace C structures as well.
* PC APPLICATION: Before running the library, some files containing the configurations are parsed
to update the C structures above. The output structure is also written into
a file.


# PC APPLICATION


## Configuration files

````
├── Data
│   ├── bulletin.dat     # configuration of satellite constellation
│   ├── PrepasConf.txt   # configuration for pass to be computed
````

### AOP

The satellite configuration file AOP... looks like below. It contains the orbital parameters of the
Argos/KINEIS Constellation Satellites (aka AOP, Adapted Orbital Parameters). Refer to
\ref bulletin_file_format section for details. The format is actually the same one as files
downloaded from argos web.
````
MA A 5 3 0 2020  1 26 22 59 44  7195.550  98.5444  327.835  -25.341  101.3587   0.00
MB 9 3 0 0 2020  1 26 22 33 39  7195.632  98.7141  344.177  -25.340  101.3600   0.00
MC B 7 3 0 2020  1 26 23 29 29  7194.917  98.7183  330.404  -25.336  101.3449   0.00
15 5 0 0 0 2020  1 26 23 50  6  7180.549  98.7298  289.399  -25.260  101.0419  -1.78
18 8 0 0 0 2020  1 26 22 12  6  7226.170  99.0661  343.180  -25.499  102.0039  -1.80
19 C 6 0 0 2020  1 26 22  3 52  7226.509  99.1913  291.936  -25.500  102.0108  -1.98
SR D 4 3 0 2020  1 26 22  3 53  7160.246  98.5358  118.029  -25.154  100.6148   0.00
````

### Prediction pass

The pass configuration file looks like this. It contains all the input parameters to calculate
satellite pass predictions for a beacon. Refer to \ref prediction_config_file_format section for
details.
````
43.5497 1.485 2020 01 27 00 00 00 2020 01 28 00 00 00 5.0 90.0 5.0 1000 5.0 30
````

## How to build

Note this source-code package contains a makefile using GCC toolchain.
Depending on your objectives, you may custom your own makefile and toolchain.

To build the PC application, compile the code without ``PREPAS_EMBEDDED``
compilation flag. Edit the Makefile, then comment it or remove it:
````
OPT = -Og -std=c99 #-DPREPAS_EMBEDDED
````
or
````
OPT = -Og -std=c99
````

Once built with ``make`` command, the binary file is available in
````
build/prepas_test_example.elf
````

## How to run and output results

Execute the binary file. The satellite pass prediction is outputed in following file. Refer to
setcion \ref prediction_output_file_format for detils about the format.
````
Data/prepasRun.txt file
````

## How to validate

With default configuration of the package, the prepasRun.txt file should be
identical to the reference file below. Nevertheless, depending on your OS (windows or linux) you
used to build the application, the end of lines of both files may be different.

````
├── Data
│   ├── PrepasRef.txt                    # pass predictions output from this package
````


# EMBEDDED TARGET


## Configuration structures

### AOP

In case of embedded app, directly fill up the \ref AopSatelliteEntry_t struct thanks to the AOP
file downloaded from https://argos-system.cls.fr/
\note the format are not exactly the same:

* "Satellite identification" field disappears
* "Satellite identification in the downlink allcast data" must be preceded by "0x" (hexadecimal value)
* "Instrument identification" remains as is
* "Downlink operating status" uses generic format enum \ref SatDownlinkStatus_t (note
  function \ref PREVIPASS_status_format_a_to_generic does same conversion)
      * 0 -> SAT_DNLK_OFF
      * 1 -> SAT_DNLK_ON_WITH_A4
      * 2 -> SAT_DNLK_OFF
      * 3 -> SAT_DNLK_ON_WITH_A3
      * If sat ID is NOAA_K (0x5) or NOAA_N (0x8) -> SAT_DNLK_OFF
* "Uplink operating status" uses generic format enum \ref SatUplinkStatus_t (note
  function \ref PREVIPASS_status_format_a_to_generic does same covnersion)
      * 0 -> If sat ID NOAA_K (0x5) or NOAA_N (0x8), SAT_UPLK_ON_WITH_A2 else SAT_UPLK_ON_WITH_A3
      * 1 -> If sat ID NOAA_K (0x5) or NOAA_N (0x8), SAT_UPLK_ON_WITH_A2 else SAT_UPLK_ON_WITH_NEO
      * 2 -> If sat ID NOAA_K (0x5) or NOAA_N (0x8), SAT_UPLK_ON_WITH_A2 else SAT_UPLK_ON_WITH_A4
      * 3 -> SAT_UPLK_OFF
* "Date" field is written between brackets {}
* Orbital parameters are written with a float format


Here is an example of configuration of this structure:
````
struct AopSatelliteEntry_t aopTable[] = {
        { 0xA, 5, SAT_DNLK_ON_WITH_A3, SAT_UPLK_ON_WITH_A3, { 2020, 1, 26, 22, 59, 44 }, 7195.550f, 98.5444f, 327.835f, -25.341f, 101.3587f,  0.00f },
        { 0x9, 3, SAT_DNLK_OFF,        SAT_UPLK_ON_WITH_A3, { 2020, 1, 26, 22, 33, 39 }, 7195.632f, 98.7141f, 344.177f, -25.340f, 101.3600f,  0.00f },
        { 0xB, 7, SAT_DNLK_ON_WITH_A3, SAT_UPLK_ON_WITH_A3, { 2020, 1, 26, 23, 29, 29 }, 7194.917f, 98.7183f, 330.404f, -25.336f, 101.3449f,  0.00f },
        { 0x5, 0, SAT_DNLK_OFF,        SAT_UPLK_ON_WITH_A2, { 2020, 1, 26, 23, 50,  6 }, 7180.549f, 98.7298f, 289.399f, -25.260f, 101.0419f, -1.78f },
        { 0x8, 0, SAT_DNLK_OFF,        SAT_UPLK_ON_WITH_A2, { 2020, 1, 26, 22, 12,  6 }, 7226.170f, 99.0661f, 343.180f, -25.499f, 102.0039f, -1.80f },
        { 0xC, 6, SAT_DNLK_OFF,        SAT_UPLK_ON_WITH_A3, { 2020, 1, 26, 22,  3, 52 }, 7226.509f, 99.1913f, 291.936f, -25.500f, 102.0108f, -1.98f },
        { 0xD, 4, SAT_DNLK_ON_WITH_A3, SAT_UPLK_ON_WITH_A3, { 2020, 1, 26, 22,  3, 53 }, 7160.246f, 98.5358f, 118.029f, -25.154f, 100.6148f,  0.00f }
};
uint8_t nbSatsInAopTable = 7;
````

### Prediction pass

In case of emmbedded app, directly fill up the \ref PredictionPassConfiguration_t struct with all
the parameters required to compute prediction passes.

````
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
````

## How to build

Note this source-code package contains a makefile using GCC toolchain.
Depending on your objectives, you may custom your own makefile and toolchain
to cross-compile for your target.

To build the embedded target binary, compile the code with ``PREPAS_EMBEDDED``
compilation flag. Edit the Makefile, then uncomment it:

````
OPT = -Og -std=c99 -DPREPAS_EMBEDDED
````

Once built with ``make`` command, the binary file is available in
````
build/prepas_test_example.elf
````

## How to run

The binary file above is ready to be flashed on your target.

## How to validate

It is possible to make the binary to output logs to standard output, by configuring stdoutDisplay
variable below to true (in main.c file).

````
bool stdoutDisplay = true;
````
By doing this, the embedded binary will output the result of the computed pass prediction.
It should look like the content of reference file
````
Data/PrepasRef.txt
````


# SOURCE CODE DOCUMENTATION


It is organized in two pages:
* \ref pass_prediction_example_page example files
* \ref pass_prediction_lib_page library files


# CODE SIZE


As an indication this embedded example was profiled on ARM cortex-M4 (NUCLEO-L476RG board).
````
                          text    data     bss     dec     hex filename
without call to prepas:   23840     496    1968   26304    66c0 build/prepas_profiling_ide.elf
with    call to prepas:   34296     496    4488   39280    9970 build/prepas_profiling_ide.elf
````

The code compiled when calling prepas library is adding:
* +10,2 kB of code:
    * prepas: 2,8 kB
    * mainly math lib is the rest of the added amount. Indeed prepas is coded using floats so far.
* +2,54 kB of data:
    * prepas: 2,54 kB. It is actually the amount of memory allocated to store the prediction results
\ref MY_MALLOC_MAX_BYTES. Depending on your needs, this can be reduced a lot.


# PROFILING


## Single pass strategy (EMBEDDED )

Some profiling is provided here for ARM architecture (CORTEX-M4 with FPU) with full optimization.

Compilation flags and link flags used were:
````
* CFLAGS  = -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard -u _printf_float -O3
-DPREPAS_EMBEDDED -Wall -fdata-sections -ffunction-sections -fstack-usage -g -gdwarf-2
* LDFLAGS = -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard -u _printf_float
-specs=nano.specs -lc -lm -lnosys -Wl,-Map=$(BUILD_DIR)/$(TARGET).map,--cref -Wl,--gc-sections
````

Timing execution was:
 * 2221825 cycles for \ref PREVIPASS_compute_next_pass with 7 satellites in constellation.
 * 763293  cycles for \ref PREVIPASS_compute_next_pass_with_status with filter on
SAT_DNLK_ON_WITH_A3 and SAT_UPLK_ON_WITH_A3, with 7 satellites in constellation.

## Single pass strategy (PC)

Same profiling was done on PC. Timings provided here can be used to compute ratios, between the
different prediction pass configurations.

Time of call to the function which gives the next pass:
- 365us for \ref PREVIPASS_compute_next_pass with 7 satellites in constellation.
- 125us for \ref PREVIPASS_compute_next_pass_with_status with filter on SAT_DNLK_ON_WITH_A3 and
SAT_UPLK_ON_WITH_A3, with 7 satellites in constellation.

Conclusion: the call time is very short. It can vary depending on the time distance of the next pass
for each satellite.

Furthermore, filtering directly in the *estimate* function removes unnecessary predictions.

## Multi-pass strategy (PC)

### Prediction size impact

Timing of the pass prediction function according to the duration of the prediction to compute.

| Duration (day) | computation step (s) | fucntion timing (ms) |
| ------ | ------ | ------ |
| 1 | 30 | 2.235 |
| 2 | 30 | 4.455 |
| 3 | 30 | 6.707 |
| 4 | 30 | 8.884 |

\note timing is linear with prediction duration.

### Computation step impact

Timing of the pass prediction function according to the computation step.

| Duration (day) | computation step (s) | function timing (ms) |
| ------ | ------ | ------ |
| 1 | 5 | 13.579 |
| 1 | 10 | 6.869 |
| 1 | 15 | 4.485 |
| 1 | 20 | 3.355 |
| 1 | 25 | 2.724 |
| 1 | 30 | 2.235 |

\note prediction timing inversely proportional to the computation step.

### Cyclic use of the prediction

Average timing to check transceiver's current state (\ref PREVIPASS_process_existing_sorted_passes)
according to the duration of the prediction. Indeed, this function is parsing the entire list of
predictions.

| Duration (day) | computation step (s) | Average timing (us) |
| ------ | ------ | ------ |
| 1 | 30 | 192.877 |
| 2 | 30 | 868.278 |
| 3 | 30 | 2010.628 |
| 4 | 30 | 3547.920 |

\note the prediction usage time increases exponentially with the duration of the prediction.
This is due to the growing length of the chained list containing the predictions.
Thus, it is better to regularly call the function to compute predictions
(i.e \ref PREVIPASS_compute_new_prediction_pass_times or
\ref PREVIPASS_compute_new_prediction_pass_times_with_status) for short periods (i.e. one day max).

This problem could be circumvented by gradually removing past passes. The downside is that going
backwards following a GPS resync could lead to losing passes (unlikely).


# SOURCE TREE


Below is a list of the files present in this package.

Configuration files (to be used when building PC application only)
````
├── Data
│   ├── bulletin.dat     # AOP file downloaded from argos web (PC version)
│   ├── PrepasConf.txt   # configuration for pass to be computed (PC version)
````
Example source code (to be built)
````
├── main.c
````
Library source code (to be built)
````
├── previpass.c             # core code
├── previpass.h             # library header file listing main APIs
├── previpass_util.c        # utils for date and distance computations
└── previpass_util.h        # utils for date and distance computations
````
Build and doc framework
````
├── Doxyfile
├── Makefile
├── README.md
├── logo_kineis.png
````
Validation reference file
````
├── Data
│   ├── PrepasRef.txt                    # pass predictions output from this package
````
