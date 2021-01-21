// -------------------------------------------------------------------------- //
//! @file   previpass_util.h
//! @brief  Satellite pass prediction utils functions prototypes
//! @author Kineis
//! @date   2020-01-14
// -------------------------------------------------------------------------- //

#pragma once


// -------------------------------------------------------------------------- //
// Includes
// -------------------------------------------------------------------------- //

#include <stdint.h>


// -------------------------------------------------------------------------- //
//! @addtogroup ARGOS-PASS-PREDICTION-LIBS
//! @{
// -------------------------------------------------------------------------- //


// -------------------------------------------------------------------------- //
// Defines values
// -------------------------------------------------------------------------- //

//! Pi/2 value
#define C_MATH_HALF_PI    1.570796327f

//! Pi value
#define C_MATH_PI 3.1415926535f

//! 2*Pi value
#define C_MATH_TWO_PI 6.283185307f

//! Conversion factor between degrees and radians
#define C_MATH_DEG_TO_RAD 0.017453292f

//! Conversion factor between radians and degrees
#define C_MATH_RAD_TO_DEG 57.29577951f

//! Earth mean radius
#define C_MATH_EARTH_RADIUS  6378.137f

//! Constant offset to transform epoch 90s to 70s time
#define EPOCH_90_TO_70_OFFSET  631152000L


// -------------------------------------------------------------------------- //
// Defines functions
// -------------------------------------------------------------------------- //

//! Returns the minimum between two values.
#define MIN(A, B) ((A) < (B) ? (A) : (B))

//! Returns the minimum between two values.
#define MAX(A, B) ((A) > (B) ? (A) : (B))

//! Returns the absolute value of a number.
#define ABS(N) ((N) >= 0 ? (N) : -(N))


// -------------------------------------------------------------------------- //
//! Computation configuration
// -------------------------------------------------------------------------- //

struct CalendarDateTime_t {
	uint16_t year ;   //!< Year ( year > 1900 )
	uint8_t  month ;  //!< Month ( 1 <= month <= 12 )
	uint16_t day ;    //!< Day ( 1 <= day <= 31 )
	uint8_t  hour ;   //!< Hour ( 0 <= hour <= 23 )
	uint8_t  minute ; //!< Minute ( 0 <= minute <= 59 )
	uint8_t  second ; //!< Second ( 0 <= second <= 59 )
};


// -------------------------------------------------------------------------- //
//! Compute cartesian squared distance between satellite and its visibility
//! circle at a defined elevation on a circular earth.
//!
//! \param[in] elevationDeg
//!    Minimum elevation of satellite in beacon reference (degrees)
//! \param[in] semiMajorAxisKm
//!    Semi major axis of atellite orbit (km)
//!
//! @returns Maximum squared distance to be in visibility circle
// -------------------------------------------------------------------------- //

float
PREVIPASS_UTIL_sat_elevation_distance2
(
	float elevationDeg,
	float semiMajorAxisKm
);


// -------------------------------------------------------------------------- //
//! Compute squared distance between a satellite and a point in cartesian
//! coordinates. Satellite position is computed from an orbital bulletin.
//!
//! \param[in] secondsSinceBulletin
//!    Number of seconds since bulletin epoch
//! \param[in] xBeaconCartesian
//!    Cartesian position of the point in cartesian Earth ref, X coordinate
//! \param[in] yBeaconCartesian
//!    Cartesian position of the point in cartesian Earth ref, Y coordinate
//! \param[in] zBeaconCartesian
//!    Cartesian position of the point in cartesian Earth ref, Z coordinate
//! \param[in] mean_motion
//!    Mean motion (revolution per second)
//! \param[in] sin_inclination
//!    Sinus of inclination
//! \param[in] cos_inclination
//!    Cosinus of inclination
//! \param[in] ascNodeRad
//!    Longitude of ascending node (radians)
//! \param[in] earthRevPerSec (revolution per second)
//!    Earth rotation speed relative to the orbital plan
//!
//! @returns Square of distance between point and satellite
// -------------------------------------------------------------------------- //

float
PREVIPASS_UTIL_sat_point_distance2
(
	uint32_t secondsSinceBulletin,
	float    xBeaconCartesian,
	float    yBeaconCartesian,
	float    zBeaconCartesian,
	float    mean_motion,
	float    sin_inclination,
	float    cos_inclination,
	float    ascNodeRad,
	float    earthRevPerSec
);


// -------------------------------------------------------------------------- //
//! Date conversion: calendar to seconds since 1990-01-01T00:00:00.
//!
//! @note This function can also be called with a number of days since January
//! 1st. Month should be set to 1 in this case.
//!
//! \param[in] dateTime
//!    Calendar date
//! \param[out] sec90
//!    Number of seconds since 1990
// -------------------------------------------------------------------------- //

void
PREVIPASS_UTIL_date_calendar_stu90
(
	struct CalendarDateTime_t  dateTime,
	uint32_t                  *sec90
);


// -------------------------------------------------------------------------- //
//! Date conversion: seconds since 1990-01-01T00:00:00 to calendar.
//!
//! \param[in] sec90
//!    Number of seconds since 1990
//! \param[out] dateTime
//!    Calendar date
// -------------------------------------------------------------------------- //

void
PREVIPASS_UTIL_date_stu90_calendar
(
	uint32_t                   sec90,
	struct CalendarDateTime_t *dateTime
);


// -------------------------------------------------------------------------- //
//! @} (end addtogroup ARGOS-PASS-PREDICTION-LIBS)
// -------------------------------------------------------------------------- //
