// -------------------------------------------------------------------------- //
//! @file   previpass.c
//! @brief  Satellite pass prediction algorithms
//! @author Kineis
//! @date   2020-01-14
// -------------------------------------------------------------------------- //


// -------------------------------------------------------------------------- //
// Includes
// -------------------------------------------------------------------------- //

#include <math.h>
#include "previpass.h"
#include "previpass_util.h"


// -------------------------------------------------------------------------- //
//! @addtogroup ARGOS-PASS-PREDICTION-LIBS
//! @{
// -------------------------------------------------------------------------- //


// -------------------------------------------------------------------------- //
// Defines values
// -------------------------------------------------------------------------- //

//! Maximum number of configuration. Should be greater than satellite number.
//! Should not be greater than 64 (uint64_t bitmap).
#define MAX_SATELLITES_HANDLED_IN_COVISI 64

//! Memory pool size. Used for linked list. 16 bytes per pass. 35 pass per day.
#define MY_MALLOC_MAX_BYTES 2500


// -------------------------------------------------------------------------- //
//! Holds configuration of a satellite: IDs and uplink/downlink information.
// -------------------------------------------------------------------------- //

struct SatelliteConfiguration_t {
	// Information on satellite ID
	uint16_t satHexId : 6 ; //!< Hexadecimal satellite id

	// Configuration type
	uint16_t downlinkStatus : 3 ; //!< 4 maximum, see SatDownlinkStatus_t
	uint16_t uplinkStatus   : 3 ; //!< 5 maximum, see SatUplinkStatus_t

	// Padding
	uint16_t unused : 4 ; //!< Spare bits
};


// -------------------------------------------------------------------------- //
// Static variables for prepas function
// -------------------------------------------------------------------------- //

//! Memory pool
static uint8_t  __mallocBytesPool[MY_MALLOC_MAX_BYTES];

//! Memory pool index
static uint16_t __mallocIdx;


// -------------------------------------------------------------------------- //
//! \brief Values for default AOP entry.
//!
//! This function should be used when initializing AOP array.
//!
//! \see AopSatelliteEntry_t
//!
//! \return Empty structure with all capacities set to OFF
// -------------------------------------------------------------------------- //

struct AopSatelliteEntry_t PREVIPASS_default_aop_satellite_entry(void)
{
	return (struct AopSatelliteEntry_t) { 0x0,
		0,
		SAT_DNLK_OFF,
		SAT_UPLK_OFF,
		{0, 0, 0, 0, 0, 0},
		0,
		0,
		0,
		0,
		0,
		0 };
}


// -------------------------------------------------------------------------- //
// Convertion between constellation status type A and generic status.
// -------------------------------------------------------------------------- //

void PREVIPASS_status_format_a_to_generic(enum SatDownlinkStatusFormatA_t downlinkStatusFormatA,
	enum SatUplinkStatusFormatA_t   uplinkStatusFormatA,
	SatHexId_t                 satHexId,
	enum SatDownlinkStatus_t       *downlinkStatusGeneric,
	enum SatUplinkStatus_t         *uplinkStatusGeneric
)
{
	// Direct translation for downlink status
	switch (downlinkStatusFormatA) {
	case FMT_A_SAT_DNLK_A3_RX_CAPACITY:
		*downlinkStatusGeneric = SAT_DNLK_ON_WITH_A3;
		break;
	case FMT_A_SAT_DNLK_A4_RX_CAPACITY:
		*downlinkStatusGeneric = SAT_DNLK_ON_WITH_A4;
		break;
	case FMT_A_SAT_DNLK_SPARE_INFO:
	case FMT_A_SAT_DNLK_DOWNLINK_OFF:
	default:
		*downlinkStatusGeneric = SAT_DNLK_OFF;

	}

	// Special case for NK and NN
	if (satHexId == HEXID_NOAA_K || satHexId == HEXID_NOAA_N) {
		// No downlink
		*downlinkStatusGeneric = SAT_DNLK_OFF;
	}

	// Direct translation for uplink status
	switch (uplinkStatusFormatA) {
	case FMT_A_SAT_UPLK_A3_CAPACITY:
		*uplinkStatusGeneric = SAT_UPLK_ON_WITH_A3;
		break;
	case FMT_A_SAT_UPLK_A4_CAPACITY:
		*uplinkStatusGeneric = SAT_UPLK_ON_WITH_A4;
		break;
	case FMT_A_SAT_UPLK_NEO_CAPACITY:
		*uplinkStatusGeneric = SAT_UPLK_ON_WITH_NEO;
		break;
	case FMT_A_SAT_UPLK_OUT_OF_SERVICE:
	default:
		*uplinkStatusGeneric = SAT_UPLK_OFF;

	}

	// Special case for NK and NN
	if (satHexId == HEXID_NOAA_K || satHexId == HEXID_NOAA_N) {
		// All values except OUT_OF_SERVICE mean Argos 2
		if (*uplinkStatusGeneric != SAT_UPLK_OFF)
			*uplinkStatusGeneric = SAT_UPLK_ON_WITH_A2;

	}
}


// -------------------------------------------------------------------------- //
// Convertion between constellation status type B and generic status.
// -------------------------------------------------------------------------- //

void PREVIPASS_status_format_b_to_generic(
	enum SatDownlinkStatusFormatB_t  downlinkStatusFormatB,
	enum SatOperatingStatusFormatB_t operatingStatusFormatB,
	enum SatDownlinkStatus_t        *downlinkStatusGeneric,
	enum SatUplinkStatus_t          *uplinkStatusGeneric
)
{
	// Enable downlink
	if (downlinkStatusFormatB == FMT_B_SAT_DNLK_DOWNLINK_ON) {
		// Use provided type
		// Direct translation for downlink status
		switch (operatingStatusFormatB) {
		case FMT_B_SAT_A3_CAPACITY:
			*downlinkStatusGeneric = SAT_DNLK_ON_WITH_A3;
			break;
		case FMT_B_SAT_A4_CAPACITY:
			*downlinkStatusGeneric = SAT_DNLK_ON_WITH_A4;
			break;
		case FMT_B_SAT_NEO_CAPACITY:
		case FMT_B_SAT_OUT_OF_SERVICE:
		default:
			*downlinkStatusGeneric = SAT_DNLK_OFF;

		}

	} else {
		*downlinkStatusGeneric = SAT_DNLK_OFF;
	}

	// Direct translation for uplink status
	switch (operatingStatusFormatB) {
	case FMT_B_SAT_A3_CAPACITY:
		*uplinkStatusGeneric = SAT_UPLK_ON_WITH_A3;
		break;
	case FMT_B_SAT_A4_CAPACITY:
		*uplinkStatusGeneric = SAT_UPLK_ON_WITH_A4;
		break;
	case FMT_B_SAT_NEO_CAPACITY:
		*uplinkStatusGeneric = SAT_UPLK_ON_WITH_NEO;
		break;
	case FMT_B_SAT_OUT_OF_SERVICE:
	default:
		*uplinkStatusGeneric = SAT_UPLK_OFF;

	}
}


// -------------------------------------------------------------------------- //
// Convertion between constellation status and generic
// -------------------------------------------------------------------------- //

void PREVIPASS_status_generic_to_format_a(
	enum SatDownlinkStatus_t         downlinkStatusGeneric,
	enum SatUplinkStatus_t           uplinkStatusGeneric,
	enum SatDownlinkStatusFormatA_t *downlinkStatusFormatA,
	enum SatUplinkStatusFormatA_t   *uplinkStatusFormatA
)
{
	// Direct translation for downlink status
	switch (downlinkStatusGeneric) {
	case SAT_DNLK_ON_WITH_A3:
		*downlinkStatusFormatA = FMT_A_SAT_DNLK_A3_RX_CAPACITY;
		break;
	case SAT_DNLK_ON_WITH_A4:
		*downlinkStatusFormatA = FMT_A_SAT_DNLK_A4_RX_CAPACITY;
		break;
	case SAT_DNLK_OFF:
	default:
		*downlinkStatusFormatA = FMT_A_SAT_DNLK_DOWNLINK_OFF;

	}

	// Direct translation for uplink status
	switch (uplinkStatusGeneric) {
	case SAT_UPLK_ON_WITH_A3:
		*uplinkStatusFormatA = FMT_A_SAT_UPLK_A3_CAPACITY;
		break;
	case SAT_UPLK_ON_WITH_A4:
		*uplinkStatusFormatA = FMT_A_SAT_UPLK_A4_CAPACITY;
		break;
	case SAT_UPLK_ON_WITH_NEO:
		*uplinkStatusFormatA = FMT_A_SAT_UPLK_NEO_CAPACITY;
		break;
	case SAT_UPLK_ON_WITH_A2: // Not avalaible in type B
	case SAT_UPLK_OFF:
	default:
		*uplinkStatusFormatA = FMT_A_SAT_UPLK_OUT_OF_SERVICE;

	}
}


// -------------------------------------------------------------------------- //
//! Reset allocation index.
// -------------------------------------------------------------------------- //

static void PREVIPASS_resetMyMalloc(void)
{
	__mallocIdx = 0;
}


// -------------------------------------------------------------------------- //
//! Allocate some bytes in memory pool.
//!
//! \note One shall call the PREVIPASS_resetMyMalloc function before to start
//!    new pass prediction algorithm to make a cleanup on memory.
//!
//! \param[in] nbBytes
//!   Number of bytes to allocate in pool
//!
//! \return A pointer to allocated structure of type, null if no more room
// -------------------------------------------------------------------------- //

static void *PREVIPASS_poolMalloc
(
	uint16_t nbBytes
)
{
	// If enough space left in memory
	if (__mallocIdx + nbBytes > MY_MALLOC_MAX_BYTES)
		return NULL;


	uint16_t newIdx = __mallocIdx;

	__mallocIdx += nbBytes;

	// Return the address in pool of bytes
	return __mallocBytesPool + newIdx;
}


// -------------------------------------------------------------------------- //
//! Add a new element into a linked list. Element is copied using memory
//! allocation inside this function. Take care of usage and not allocate too
//! much data.
//!
//! \see PREVIPASS_poolMalloc
//!
//! \param[in] linkedListPtr
//!    Link list in which to add a new element
//! \param[in] value
//!    Structure content to be added in the list
//!
//! \return True when the element has been inserted.
// -------------------------------------------------------------------------- //

static bool PREVIPASS_insertSortedLinkedList
(
	struct SatPassLinkedListElement_t    **linkedListPtr,
	struct SatelliteNextPassPrediction_t value
)
{
	// New element allocation
	struct SatPassLinkedListElement_t *newElement = (struct SatPassLinkedListElement_t *)
		PREVIPASS_poolMalloc(sizeof(struct SatPassLinkedListElement_t));
	if (newElement == NULL)
		return false;


	// New element value
	newElement->element = value;
	newElement->next = NULL;

	// Find insertion place based on pass time
	struct SatPassLinkedListElement_t *current = *linkedListPtr;
	struct SatPassLinkedListElement_t **ptr = linkedListPtr;

	while (current != NULL && current->element.epoch < value.epoch) {
		ptr = &(current->next);
		current = *ptr;
	}

	// Insertion
	newElement->next = *ptr;
	*ptr = newElement;

	return true;
}


// -------------------------------------------------------------------------- //
//! \brief Geometric computation of passes
//!
//! Satellite passes prediction. Circular method taking into account drag
//! coefficient (friction effect) and J2 term of earth potential.
//!
//! Passes are detected by comparing current beacon to satellite distance to a
//! minimum distance based on satellite altitude.
//!
//! Analyse is done each computationStep seconds. When leaving a visibility, a
//! fixed amount of time is added to faster the computation and jump closed to
//! the next pass.
//!
//! Satellites are filtered on their downlink and uplink capacities.
//!
//! A linear time margin is added in order to compensate potential satellite
//! derivation when AOP are old. It is added at the beginning and the end of
//! each pass.
//!
//! \see PREVIPASS_compute_next_pass
//!
//! \param[in] config
//!    Configuration of passes computation
//! \param[in] aopTable
//!    Array of info about each satellite
//! \param[in] nbSatsInAopTable
//!    Number of satellite in AOP table
//! \param[in] downlinkStatus
//!    Minimum donwlink capacity
//! \param[in] uplinkStatus
//!    Minimum uplink capacity
//! \param[out] previsionPassesList
//!    Pointer to linked list pointer
// -------------------------------------------------------------------------- //

static bool PREVIPASS_estimate_with_status
(
	struct PredictionPassConfiguration_t *config,
	struct AopSatelliteEntry_t           *aopTable,
	uint8_t                        nbSatsInAopTable,
	enum SatDownlinkStatus_t            downlinkStatus,
	enum SatUplinkStatus_t              uplinkStatus,
	struct SatPassLinkedListElement_t   **previsionPassesList
)
{
	// Beacon position in cartesian coordinates
	float xBeaconCartesian = cosf(config->beaconLatitude  * C_MATH_DEG_TO_RAD)
		* cosf(config->beaconLongitude * C_MATH_DEG_TO_RAD);
	float yBeaconCartesian = cosf(config->beaconLatitude  * C_MATH_DEG_TO_RAD)
		* sinf(config->beaconLongitude * C_MATH_DEG_TO_RAD);
	float zBeaconCartesian = sinf(config->beaconLatitude  * C_MATH_DEG_TO_RAD);

	// Beginning of prediction (sec)
	uint32_t computationStartSec;

	PREVIPASS_UTIL_date_calendar_stu90(config->start, &computationStartSec);

	// End of prediction (sec)
	uint32_t computationEndSec;

	PREVIPASS_UTIL_date_calendar_stu90(config->end, &computationEndSec);

	// Main loop : computation for each satellite
	for (uint8_t iSat = 0 ; iSat < nbSatsInAopTable && aopTable[iSat].satHexId != 0 ; ++iSat) {
		// Detection of invalid satellite
		if (aopTable[iSat].bulletin.year == 0)
			continue;


		// Detection of offline satellite
		if (aopTable[iSat].downlinkStatus == SAT_DNLK_OFF
				&& aopTable[iSat].uplinkStatus == SAT_UPLK_OFF)
			continue;

		// Detection of incompatible satellite
		if (aopTable[iSat].downlinkStatus < downlinkStatus
				|| aopTable[iSat].uplinkStatus < uplinkStatus)
			continue;

		// Unique computation of cos and sin of inclination
		float sinInclination = sinf(aopTable[iSat].inclinationDeg * C_MATH_DEG_TO_RAD);
		float cosInclination = cosf(aopTable[iSat].inclinationDeg * C_MATH_DEG_TO_RAD);

		// Conversion from m per day to km per day
		float semiMajorAxisDriftKmPerDay = aopTable[iSat].semiMajorAxisDriftMeterPerDay /
							1000;

		// Computation of minimum squared distance
		float visibilityMinDistance2 = PREVIPASS_UTIL_sat_elevation_distance2(
						config->minElevation,
						aopTable[iSat].semiMajorAxisKm);

		// Number of seconds since bulletin epoch
		uint32_t bullSec90;

		PREVIPASS_UTIL_date_calendar_stu90(aopTable[iSat].bulletin, &bullSec90);

		// Conversions
		float orbitPeriodSec = aopTable[iSat].orbitPeriodMin * 60.f;
		float meanMotionBaseRevPerSec = C_MATH_TWO_PI / orbitPeriodSec;
		float ascNodeRad = aopTable[iSat].ascNodeLongitudeDeg * C_MATH_DEG_TO_RAD;
		float ascNodeDriftRad = aopTable[iSat].ascNodeDriftDeg * C_MATH_DEG_TO_RAD;
		float earthRevPerSec = ascNodeDriftRad / orbitPeriodSec;
		uint32_t secondsSinceBulletin = computationStartSec - bullSec90;
		uint32_t computationDurationSinceBulletinSeconds = computationEndSec - bullSec90;

		// Mean motion computation
		float numberOfRevSinceBulletin = secondsSinceBulletin / orbitPeriodSec;
		float meanMotionDriftRevPerSec = -1.5f
			* semiMajorAxisDriftKmPerDay
			/ aopTable[iSat].semiMajorAxisKm
			/ 86400.0f
			* C_MATH_TWO_PI
			* numberOfRevSinceBulletin;
		float meanMotionRevPerSec = meanMotionBaseRevPerSec + meanMotionDriftRevPerSec;

		// Use current pass
		float distance2 = PREVIPASS_UTIL_sat_point_distance2(secondsSinceBulletin,
				xBeaconCartesian,
				yBeaconCartesian,
				zBeaconCartesian,
				meanMotionRevPerSec,
				sinInclination,
				cosInclination,
				ascNodeRad,
				earthRevPerSec);
		if (distance2 < visibilityMinDistance2) {
			// Go back of at least one pass duration
			// TODO MJT Add analytic computation of this value
			secondsSinceBulletin -= 1200;
		}

		// Start computation loop for current satellite
		uint8_t isInPass = 0;
		uint32_t passDurationSec = 0;
		float elevationMax = 0;
		uint32_t passNumber = 0;

		while (secondsSinceBulletin < computationDurationSinceBulletinSeconds) {
			// Reset software watchdog during loop to avoid false alarm
#ifdef PREPAS_EMBEDDED_STARVATION_CHECK
			extern void vIDLETSK_enable_ressource_starvation_check(bool b_enable_flag);
			vIDLETSK_enable_ressource_starvation_check(false);
			vIDLETSK_enable_ressource_starvation_check(true);
#endif

			// Stop computation if maximum number of passes per satellite is reached
			if (passNumber >= config->maxPasses)
				break;


			// Compute current cartesian distance
			distance2 = PREVIPASS_UTIL_sat_point_distance2(secondsSinceBulletin,
					xBeaconCartesian,
					yBeaconCartesian,
					zBeaconCartesian,
					meanMotionRevPerSec,
					sinInclination,
					cosInclination,
					ascNodeRad,
					earthRevPerSec);

			// A new pass startes or a pass continue
			if (distance2 < visibilityMinDistance2) {
				// Set the pass flag
				isInPass = 1;

				// Add step to pass time
				passDurationSec += config->computationStepSecond;

				// Compute current satellite elevation and keep current pass maximum
				float v = 2.f * asinf(sqrtf(distance2) / 2.f);
				float elevation = (aopTable[iSat].semiMajorAxisKm
							* sinf(v))
							/ sqrtf(C_MATH_EARTH_RADIUS
							* C_MATH_EARTH_RADIUS
							+ aopTable[iSat].semiMajorAxisKm
							* aopTable[iSat].semiMajorAxisKm
							- 2
							* C_MATH_EARTH_RADIUS
							* aopTable[iSat].semiMajorAxisKm
							* cosf(v));
				elevation = C_MATH_RAD_TO_DEG * acosf(elevation);
				if (elevation > elevationMax)
					elevationMax = elevation;


				// Go to next time by one step
				secondsSinceBulletin += config->computationStepSecond;
				continue;
			}

			// Leave a current pass for the first time
			if (isInPass == 1) {
				// Keep pass when elevation is not too high and duration is enough
				if (elevationMax < config->maxElevation &&
					config->minPassDurationMinute * 60 <= passDurationSec) {
					// Count this pass
					++passNumber;

					// Fill sat configuration
					struct SatelliteNextPassPrediction_t ppItem;

					ppItem.satHexId       = aopTable[iSat].satHexId;
					ppItem.downlinkStatus = aopTable[iSat].downlinkStatus;
					ppItem.uplinkStatus   = aopTable[iSat].uplinkStatus;

					// Fill structure to convert fields in a raw epoch from 1970
					uint32_t timeMarginSec =
							(uint32_t)(config->timeMarginMinPer6months
							* 60
							* secondsSinceBulletin
							/ (86400 * 365 / 2.f));
					ppItem.epoch = bullSec90
						+ secondsSinceBulletin
						- passDurationSec
						- timeMarginSec
						+ EPOCH_90_TO_70_OFFSET;
					ppItem.duration = passDurationSec + 2 * timeMarginSec;
					ppItem.elevationMax = (uint64_t) elevationMax;

					// Add pass to list
					if (PREVIPASS_insertSortedLinkedList(previsionPassesList,
										ppItem) == false)
						return false;

				}

				// Next pass will be at in..
				//! TODO MJT : add analytic computation of this value
				secondsSinceBulletin += 4500 ; // 75 min

				// Continue outside visibility
				isInPass = 0;
				elevationMax = 0;
				passDurationSec = 0;
				continue;
			}

			// Continue outside of a visibility
			// Step is defined according to distance
			//! TODO MJT : add analytic computation of this value
			if (distance2 < 4 * visibilityMinDistance2)
				secondsSinceBulletin += config->computationStepSecond;

			if ((distance2 >= 4 * visibilityMinDistance2)
					&& (distance2 <= (16 * visibilityMinDistance2)))
				secondsSinceBulletin += 4 * config->computationStepSecond;
			if (distance2 > 16 * visibilityMinDistance2)
				secondsSinceBulletin += 20 * config->computationStepSecond;

		}
	}

	return true;
}


// -------------------------------------------------------------------------- //
//! \brief update transceiver action based on new satellite passes capacity
//!        when downlink remains OFF
//!
//! \see PREVIPASS_process_existing_sorted_passes(
//! \see PREVIPASS_update_action
//!
//! \param[in] prevDownlinkStatus
//!            downlink status of last time the beacon called the service
//!            PREVIPASS_process_existing_sorted_passes
//! \param[in] prevUplinkStatus
//!            uplink status of last time the beacon called the service
//!            PREVIPASS_process_existing_sorted_passes
//! \param[in,out] retAction
//!            in: get UL/DL capicty of new pass
//!            out: update transceiver in accordance of parameters above
// -------------------------------------------------------------------------- //
static void PREVIPASS_update_action_dl_still_not_active(
	enum SatDownlinkStatus_t prevDownlinkStatus,
	enum SatUplinkStatus_t   prevUplinkStatus,
	struct NextPassTransceiverCapacity_t *retAction
)
{
	if (prevUplinkStatus == SAT_UPLK_OFF) {
		// Uplink was not active : the difference is that uplink only
		// has been enabled
		retAction->trcvrActionForNextPass = ENABLE_TX_ONLY;
	} else {
		// Now uplink is active

		if (retAction->minUplinkStatus == SAT_UPLK_OFF) {
			// Uplink has been switched off
			retAction->trcvrActionForNextPass = DISABLE_TX_RX;
		} else {
			// Modulation change only

			if (prevUplinkStatus == retAction->minUplinkStatus) {
				// Uplink modulation is the same in the next satellite
				retAction->trcvrActionForNextPass = KEEP_TRANSCEIVER_STATE;
			} else {
				// Modulation has changed
				retAction->trcvrActionForNextPass = CHANGE_TX_MOD;
			}
		}
	}
}

// -------------------------------------------------------------------------- //
//! \brief update transceiver action based on new satellite passes capacity
//!        when downlink remains ON
//!
//! \see PREVIPASS_process_existing_sorted_passes(
//! \see PREVIPASS_update_action
//!
//! \param[in] prevDownlinkStatus
//!            downlink status of last time the beacon called the service
//!            PREVIPASS_process_existing_sorted_passes
//! \param[in] prevUplinkStatus
//!            uplink status of last time the beacon called the service
//!            PREVIPASS_process_existing_sorted_passes
//! \param[in,out] retAction
//!            in: get UL/DL capicty of new pass
//!            out: update transceiver in accordance of parameters above
// -------------------------------------------------------------------------- //
static void PREVIPASS_update_action_dl_still_active(
	enum SatDownlinkStatus_t prevDownlinkStatus,
	enum SatUplinkStatus_t   prevUplinkStatus,
	struct NextPassTransceiverCapacity_t *retAction
)
{
	if (prevUplinkStatus == SAT_UPLK_OFF) {
		// Uplink was not active

		if (retAction->minUplinkStatus == SAT_UPLK_OFF) {
			// Modulation change only

			if (prevDownlinkStatus == retAction->maxDownlinkStatus) {
				// Uplink modulation is the same in the next satellite
				retAction->trcvrActionForNextPass = KEEP_TRANSCEIVER_STATE;
			} else {
				// Modulation has changed
				retAction->trcvrActionForNextPass = CHANGE_RX_MOD;
			}
		} else {
			// Both channels are active
			retAction->trcvrActionForNextPass = ENABLE_TX_AND_RX;
		}
	} else {
		// Uplink was active

		if (retAction->minUplinkStatus == SAT_UPLK_OFF) {
			// Uplink has been switched off
			retAction->trcvrActionForNextPass = ENABLE_RX_ONLY;
		} else {
			// Both channels still active

			if (prevDownlinkStatus == retAction->maxDownlinkStatus) {
				// Downlink modulation did not change

				if (prevUplinkStatus != retAction->minUplinkStatus) {
					// Uplink modulation only changed
					retAction->trcvrActionForNextPass = CHANGE_TX_MOD;
				}
			} else {
				// Downlink modulation has changed

				if (prevUplinkStatus == retAction->minUplinkStatus) {
					// Downlink modulation only changed
					retAction->trcvrActionForNextPass = CHANGE_RX_MOD;
				} else {
					// Both modulations have changed
					retAction->trcvrActionForNextPass = CHANGE_TX_PLUS_RX_MOD;
				}
			}
		}
	}
}
// -------------------------------------------------------------------------- //
//! \brief based on new satellite passes capacity, update the maximum downlink
//!        and the minimum uplink capcity
//!
//! \see PREVIPASS_process_existing_sorted_passes(
//! \see PREVIPASS_update_action
//!
//! \param[in] pass_pred
//!            pass to be considered
//! \param[in,out] retAction
//!            in: get minimum uplink and maximum downlink capicty already configured
//!            out: update minimum uplink and maximum downlink capacity
// -------------------------------------------------------------------------- //
static void PREVIPASS_update_max_dl_min_ul_status(
	struct SatelliteNextPassPrediction_t pass_pred,
	struct NextPassTransceiverCapacity_t *retAction
)
{
	struct SatelliteConfiguration_t one_pass_sat_cfg = { 0 };

	one_pass_sat_cfg.satHexId       = pass_pred.satHexId;
	one_pass_sat_cfg.downlinkStatus = pass_pred.downlinkStatus;
	one_pass_sat_cfg.uplinkStatus   = pass_pred.uplinkStatus;

	// Retain the maximum capacity for downlink
	if (retAction->maxDownlinkStatus == SAT_DNLK_OFF
			|| retAction->maxDownlinkStatus < one_pass_sat_cfg.downlinkStatus)
		retAction->maxDownlinkStatus =
				(enum SatDownlinkStatus_t) one_pass_sat_cfg.downlinkStatus;

	// Retain the minimum capacity for uplink
	if (retAction->minUplinkStatus == SAT_UPLK_OFF
			|| retAction->minUplinkStatus > one_pass_sat_cfg.uplinkStatus) {
		// NEO sat only with one sat
		if (retAction->minUplinkStatus == SAT_UPLK_OFF
			|| one_pass_sat_cfg.uplinkStatus != SAT_UPLK_ON_WITH_NEO)
			retAction->minUplinkStatus =
				(enum SatUplinkStatus_t) one_pass_sat_cfg.uplinkStatus;
	}
}

// -------------------------------------------------------------------------- //
//! \brief update transceiver action based on new satellite passes capacity
//!        in any conditions
//!
//! \see PREVIPASS_process_existing_sorted_passes(
//! \see PREVIPASS_update_action
//!
//! \param[in] prevDownlinkStatus
//!            sdownlink status of last time the beacon called the service
//!            PREVIPASS_process_existing_sorted_passes
//! \param[in] prevUplinkStatus uplink status of last time the beacon called the service
//!            PREVIPASS_process_existing_sorted_passes
//! \param[in,out] retAction
//!            in: get UL/DL capicty of new pass
//!            out: update transceiver in accordance of parameters above
// -------------------------------------------------------------------------- //
static void PREVIPASS_update_action(
	enum SatDownlinkStatus_t prevDownlinkStatus,
	enum SatUplinkStatus_t   prevUplinkStatus,
	struct NextPassTransceiverCapacity_t *retAction
)
{
	if (prevDownlinkStatus == SAT_DNLK_OFF) {
		// Dowlink was not active

		if (retAction->maxDownlinkStatus == SAT_DNLK_OFF) {
			// Downlink still not active
			PREVIPASS_update_action_dl_still_not_active(prevDownlinkStatus,
				prevUplinkStatus, retAction);

		} else {
			// Downlink has been switched on

			if (retAction->minUplinkStatus == SAT_UPLK_OFF) {
				// Uplink also is not active
				retAction->trcvrActionForNextPass = ENABLE_RX_ONLY;
			} else {
				// Uplink only is active
				retAction->trcvrActionForNextPass = ENABLE_TX_AND_RX;
			}
		}
	} else {
		// Downlink was active

		if (retAction->maxDownlinkStatus == SAT_DNLK_OFF) {
			// Downlink has been switched off

			if (retAction->minUplinkStatus == SAT_UPLK_OFF) {
				// Both channels go offline
				retAction->trcvrActionForNextPass = DISABLE_TX_RX;
			} else {
				// Uplink only is active
				retAction->trcvrActionForNextPass = ENABLE_TX_ONLY;
			}
		} else {
			// Downlink still active
			PREVIPASS_update_action_dl_still_active(prevDownlinkStatus,
				prevUplinkStatus, retAction);
		}
	}
}

// -------------------------------------------------------------------------- //
// Define transceiver action based on passes list.
// -------------------------------------------------------------------------- //
struct NextPassTransceiverCapacity_t PREVIPASS_process_existing_sorted_passes(
	uint32_t                    currentTime,
	struct SatPassLinkedListElement_t *previsionPassesList
)
{
	// Records a bitmap of the active satellites present at time T, according to
	// the current pass configurations
	uint64_t new_current_active_sat_in_pass_bitmap = 0;

	static uint64_t prev_current_active_sat_in_pass_bitmap;
	static enum SatDownlinkStatus_t prevDownlinkStatus = SAT_DNLK_OFF;
	static enum SatUplinkStatus_t   prevUplinkStatus   = SAT_UPLK_OFF;

	// Default initial return value for action to be done on transceiver
	struct NextPassTransceiverCapacity_t retAction = { UNKNOWN_TRANSCEIVER_ACTION,
		SAT_DNLK_OFF,
		SAT_UPLK_OFF };

	// Null pointer check
	if (previsionPassesList == NULL)
		return retAction;


	// By default, do not change transceiver status for next transmit occasion
	retAction.trcvrActionForNextPass = KEEP_TRANSCEIVER_STATE;

	// Go thru all passes
	struct SatPassLinkedListElement_t *list_elt_ptr = previsionPassesList;

	while (list_elt_ptr != NULL) {
		// First get the start/end limits of the scanned pass
		uint32_t startOfPassEpoch = list_elt_ptr->element.epoch;
		uint32_t endOfPassEpoch   = list_elt_ptr->element.epoch
			+ list_elt_ptr->element.duration;

		// If current epoch is within pass limits, set a bit in bitmap to tell the matching
		// satellite config is to be taken into account
		if ((currentTime >= startOfPassEpoch)
				&& (currentTime <  endOfPassEpoch)) {

			new_current_active_sat_in_pass_bitmap |=
				(1 << list_elt_ptr->element.satHexId);

			PREVIPASS_update_max_dl_min_ul_status(list_elt_ptr->element, &retAction);
		}

		// Go to next pass
		list_elt_ptr = list_elt_ptr->next;
	}

	// According to the bitmap, we can know which configs are to be enabled on transceiver side
	if (new_current_active_sat_in_pass_bitmap
			!= prev_current_active_sat_in_pass_bitmap) {
		// At least one pass in progress
		if (new_current_active_sat_in_pass_bitmap != 0) {
			PREVIPASS_update_action(prevDownlinkStatus, prevUplinkStatus, &retAction);
		} else {
			// End of all current passes
			retAction.trcvrActionForNextPass = DISABLE_TX_RX;
		}
	} else {
		// No change since last call
		retAction.trcvrActionForNextPass = KEEP_TRANSCEIVER_STATE;
	}

	// Update states after eventual change
	prev_current_active_sat_in_pass_bitmap = new_current_active_sat_in_pass_bitmap;
	prevDownlinkStatus = retAction.maxDownlinkStatus;
	prevUplinkStatus   = retAction.minUplinkStatus;

	return retAction;
}


// -------------------------------------------------------------------------- //
// Main Prepas library function
// -------------------------------------------------------------------------- //

struct SatPassLinkedListElement_t *PREVIPASS_compute_new_prediction_pass_times
(
	struct PredictionPassConfiguration_t *config,
	struct AopSatelliteEntry_t           *aopTable,
	uint8_t                        nbSatsInAopTable,
	bool                          *memoryPoolOverflow
)
{
	// Accept all status
	return PREVIPASS_compute_new_prediction_pass_times_with_status(config,
			aopTable,
			nbSatsInAopTable,
			SAT_DNLK_OFF,
			SAT_UPLK_OFF,
			memoryPoolOverflow);
}


// -------------------------------------------------------------------------- //
// Main Prepas library function with status filtering
// -------------------------------------------------------------------------- //

struct SatPassLinkedListElement_t *PREVIPASS_compute_new_prediction_pass_times_with_status
(
	struct PredictionPassConfiguration_t *config,
	struct AopSatelliteEntry_t           *aopTable,
	uint8_t                        nbSatsInAopTable,
	enum SatDownlinkStatus_t            downlinkStatus,
	enum SatUplinkStatus_t              uplinkStatus,
	bool                          *memoryPoolOverflow
)
{
	// Reset all previsous allocation in memory pool
	PREVIPASS_resetMyMalloc();

	// Start computation
	struct SatPassLinkedListElement_t *previsionPassesList = NULL;
	*memoryPoolOverflow = false;
	if (!PREVIPASS_estimate_with_status(config,
				aopTable,
				nbSatsInAopTable,
				downlinkStatus,
				uplinkStatus,
				&previsionPassesList)) {
		// Computation did not store all passes in memory pool
		*memoryPoolOverflow = true;
	}

	// Give access to the results to caller
	return previsionPassesList;
}


// -------------------------------------------------------------------------- //
// Get next pass
// -------------------------------------------------------------------------- //

bool PREVIPASS_compute_next_pass(
	struct PredictionPassConfiguration_t *config,
	struct AopSatelliteEntry_t           *aopTable,
	uint8_t                        nbSatsInAopTable,
	struct SatelliteNextPassPrediction_t *nextPass
)
{
	// Accept all status
	return PREVIPASS_compute_next_pass_with_status(config,
			aopTable,
			nbSatsInAopTable,
			SAT_DNLK_OFF,
			SAT_UPLK_OFF,
			nextPass);
}


// -------------------------------------------------------------------------- //
// Get next pass depending on configuration
// -------------------------------------------------------------------------- //

bool PREVIPASS_compute_next_pass_with_status(
	struct PredictionPassConfiguration_t *config,
	struct AopSatelliteEntry_t           *aopTable,
	uint8_t                        nbSatsInAopTable,
	enum SatDownlinkStatus_t            downlinkStatus,
	enum SatUplinkStatus_t              uplinkStatus,
	struct SatelliteNextPassPrediction_t *nextPass
)
{
	// Force configuration (1 year max)
	struct PredictionPassConfiguration_t localConfig = *config;

	localConfig.end = localConfig.start;
	localConfig.end.day += 1;
	localConfig.maxPasses = 1;

	// Standard computation
	struct SatPassLinkedListElement_t *previsionPassesList = NULL;
	bool memoryPoolOverflow;

	previsionPassesList = PREVIPASS_compute_new_prediction_pass_times_with_status(
			&localConfig,
			aopTable,
			nbSatsInAopTable,
			downlinkStatus,
			uplinkStatus,
			&memoryPoolOverflow);
	if (previsionPassesList == NULL)
		return false;


	// Pass found
	*nextPass = previsionPassesList->element;
	return true;
}


// -------------------------------------------------------------------------- //
//! @} (end addtogroup ARGOS-PASS-PREDICTION-LIBS)
// -------------------------------------------------------------------------- //
