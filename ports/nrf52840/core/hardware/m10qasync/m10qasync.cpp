#include "m10qasync.hpp"
#include "gpio.hpp"
#include "bsp.hpp"
#include "ubx_comms.hpp"
#include "ubx.hpp"
#include "pmu.hpp"
#include "debug.hpp"
#include "scheduler.hpp"
#include "error.hpp"
#include "rtc.hpp"
#include "timeutils.hpp"
#include "binascii.hpp"

using namespace UBX;

extern Scheduler  *system_scheduler;
extern RTC        *rtc;
extern FileSystem *main_filesystem;

// Required baud rates
#define DEFAULT_BAUDRATE    9600
#define MAX_BAUDRATE        460800

// State machine helper macros
#define STATE_CHANGE(x, y)                       \
	do {                                             \
		DEBUG_TRACE("M10QAsyncReceiver::STATE_CHANGE: " #x " -> " #y ); \
		m_state = y;                                 \
		state_ ## x ##_exit();                       \
		state_ ## y ##_enter();                      \
		run_state_machine();						 \
	} while (0)

#define STATE_EQUAL(x) \
	(m_state == x)

#define STATE_CALL(x) \
	do {                    \
		state_ ## x();      \
	} while (0)


M10QAsyncReceiver::M10QAsyncReceiver() {
	STATE_CHANGE(idle, idle);
	m_ana_database_len = 0;
    m_ano_database_len = 0;
	m_ano_start_pos = 0;
	m_timeout.running = false;
	m_num_power_on = 0;
	m_powering_off = false;
}

M10QAsyncReceiver::~M10QAsyncReceiver() {
}

void M10QAsyncReceiver::power_on(const GPSNavSettings& nav_settings) {
	DEBUG_INFO("M10QAsyncReceiver::power_on");
    // Track number of power on requests
    m_num_power_on++;

    // Cancel any ongoing poweroff request
    m_powering_off = false;

    // Reset observation counters of interest
    if (nav_settings.max_nav_samples) {
        m_num_nav_samples = 0;
        m_nav_settings.max_nav_samples = nav_settings.max_nav_samples;
        m_num_consecutive_fixes = m_nav_settings.num_consecutive_fixes;
    }

    if (nav_settings.max_sat_samples) {
        m_num_sat_samples = 0;
        m_nav_settings.max_sat_samples = nav_settings.max_sat_samples;
    }

    if (STATE_EQUAL(idle)) {
        // Copy navigation settings to be used for this session
        m_nav_settings = nav_settings;
        m_num_consecutive_fixes = m_nav_settings.num_consecutive_fixes;
        STATE_CHANGE(idle, poweron);
    }
}

void M10QAsyncReceiver::check_for_power_off() {

	// Don't power off unless there are clients or the system is already powering off
	if (m_num_power_on || m_powering_off)
		return;

	// Try to cleanup in a way that will preserve navigation data
	// before powering off
	m_powering_off = true;

	// Check for unrecoverable error as we won't be able to shutdown cleanly in this case
	// because it is likely caused by comms failures to the GPS module
	if (m_unrecoverable_error) {
		// Do a forced shutdown
		STATE_CHANGE(idle, poweroff);
		return;
	}

	// Try to shutdown cleanly
	if (STATE_EQUAL(idle)) {
		return;
	} else if (STATE_EQUAL(receive)) {
		STATE_CHANGE(receive, stopreceive);
	} else if (STATE_EQUAL(poweron)) {
		STATE_CHANGE(poweron, poweroff);
	} else if (STATE_EQUAL(configure)) {
		STATE_CHANGE(configure, poweroff);
	} else if (STATE_EQUAL(startreceive)) {
		STATE_CHANGE(startreceive, poweroff);
	} else if (STATE_EQUAL(senddatabase)) {
		STATE_CHANGE(senddatabase, poweroff);
	} else if (STATE_EQUAL(sendofflinedatabase)) {
		STATE_CHANGE(sendofflinedatabase, poweroff);
	} else {
		STATE_CHANGE(idle, poweroff);
	}
}

void M10QAsyncReceiver::power_off() {
	DEBUG_INFO("M10QAsyncReceiver::power_off");
    // Just decrement the number of users
    if (m_num_power_on)
        m_num_power_on--;
    check_for_power_off();
}

void M10QAsyncReceiver::enter_shutdown() {
    m_ubx_comms.deinit();

#if 0 == NO_GPS_POWER_REG
    // Disable the power supply for the GPS
    GPIOPins::clear(BSP::GPIO::GPIO_GPS_PWR_EN);
    GPIOPins::clear(BSP::GPIO::GPIO_GPS_RST);
#else
    // Use GPIO_GPS_EXT_INT as a shutdown
    GPIOPins::clear(BSP::GPIO::GPIO_GPS_EXT_INT);
#endif
}

void M10QAsyncReceiver::exit_shutdown() {
	// Configure UBX comms if not already done
    m_ubx_comms.init();
    m_ubx_comms.subscribe(*this);
    m_ubx_comms.set_debug_enable(m_nav_settings.debug_enable);

#if 0 == NO_GPS_POWER_REG
    // Enable the power supply for the GPS
    GPIOPins::set(BSP::GPIO::GPIO_GPS_PWR_EN);
    GPIOPins::set(BSP::GPIO::GPIO_GPS_RST);
    PMU::delay_ms(1000); // Necessary to allow the device to boot
#else
    // Use GPIO_GPS_EXT_INT as a wake-up
    GPIOPins::set(BSP::GPIO::GPIO_GPS_EXT_INT);
#endif
}

void M10QAsyncReceiver::state_machine() {
	switch (m_state) {
	case State::poweron:
		STATE_CALL(poweron);
		break;
	case State::poweroff:
		STATE_CALL(poweroff);
		break;
	case State::configure:
		STATE_CALL(configure);
		break;
	case State::senddatabase:
		STATE_CALL(senddatabase);
		break;
	case State::sendofflinedatabase:
		STATE_CALL(sendofflinedatabase);
		break;
	case State::startreceive:
		STATE_CALL(startreceive);
		break;
	case State::receive:
		STATE_CALL(receive);
		break;
	case State::stopreceive:
		STATE_CALL(stopreceive);
		break;
	case State::fetchdatabase:
		STATE_CALL(fetchdatabase);
		break;
	default:
	case State::idle:
		break;
	}
}

void M10QAsyncReceiver::run_state_machine(unsigned int time_ms) {
	system_scheduler->cancel_task(m_state_machine_handle);
	m_state_machine_handle = system_scheduler->post_task_prio([this]() {
		state_machine();
	}, "M10Q", Scheduler::DEFAULT_PRIORITY, time_ms);
}

void M10QAsyncReceiver::react(const UBXCommsEventSendComplete&) {
	if (m_op_state == OpState::PENDING) {
		cancel_timeout();
		m_op_state = OpState::SUCCESS;
		run_state_machine();
	}
}

void M10QAsyncReceiver::react(const UBXCommsEventAckNack& ack) {
	if (m_op_state == OpState::PENDING) {
		cancel_timeout();
		m_op_state = ack.ack ? OpState::SUCCESS : OpState::NACK;
		run_state_machine();
	}
}

void M10QAsyncReceiver::react(const UBXCommsEventCfgValget& valget) {
    
    unsigned int length = valget.length;
    DEBUG_TRACE("UBXCommsEventCfgValget received: length %u", length);
    uint8_t* msg = new uint8_t[length];
    std::memcpy(msg, valget.msg, length);
    
    // Parse the fixed header fields
    uint8_t version = msg[0];
    uint8_t layer = msg[1];
    uint16_t position = static_cast<uint16_t>(msg[2]) | (static_cast<uint16_t>(msg[3]) << 8); // Combine bytes for position
    
    DEBUG_TRACE("Version: %u", version);
    DEBUG_TRACE("Layer: %u", layer);
    DEBUG_TRACE("Position: %u", position);
    
    // Decode the cfgData entries
    const unsigned int cfgDataStart = 4; // cfgData starts after the 4-byte header
    unsigned int offset = cfgDataStart;  // Track the current position in cfgData
    
    while (offset < length) {
        // Each cfgData entry starts with a 4-byte key
        uint32_t key = static_cast<uint32_t>(msg[offset]) |
                       (static_cast<uint32_t>(msg[offset + 1]) << 8) |
                       (static_cast<uint32_t>(msg[offset + 2]) << 16) |
                       (static_cast<uint32_t>(msg[offset + 3]) << 24);
        offset += 4;

        // Get the size of the parameter value
        uint8_t paramSize = UBX::CFG::getParameterSize(key);

        // Check for unknown keys or if remaining length is insufficient
        if (paramSize == 0 || offset + paramSize > length) {
            DEBUG_TRACE("Unknown or invalid key size for key: 0x%08X", key);
            break;
        }

        // Extract and cast the parameter value based on its size
        int64_t paramValue = 0; // Use int64_t to handle larger int values for readability

        if (paramSize == 1) {
            paramValue = static_cast<int8_t>(msg[offset]);
        } else if (paramSize == 2) {
            paramValue = static_cast<int16_t>(msg[offset] | (msg[offset + 1] << 8));
        } else if (paramSize == 4) {
            paramValue = static_cast<int32_t>(msg[offset] |
                                              (msg[offset + 1] << 8) |
                                              (msg[offset + 2] << 16) |
                    (msg[offset + 3] << 24));
        } else {
            // For larger sizes or if it doesn't fit typical int sizes, treat as raw data
            paramValue = 0;
            for (uint8_t i = 0; i < paramSize; ++i) {
                paramValue |= static_cast<int64_t>(msg[offset + i]) << (8 * i);
            }
        }

        offset += paramSize;

        // Print the decoded configuration key-value pair
        DEBUG_TRACE("Config Key: 0x%08X, Value: %lld (0x%X)", key, paramValue, paramValue);
    }
    
    
    // Clean up
    delete[] msg;

    // Handle state
    if (m_op_state == OpState::PENDING) {
        m_op_state = OpState::SUCCESS;
        cancel_timeout();
        run_state_machine();
    }
}

void M10QAsyncReceiver::react(const UBXCommsEventSatReport& s) {
    if (m_nav_settings.sat_tracking) {
    	static UBXCommsEventSatReport sat;
    	std::memcpy(&sat, &s, sizeof(sat));
        system_scheduler->post_task_prio([this]() {
        	if (m_nav_settings.debug_enable) {
        		DEBUG_TRACE("UBXCommsEventSatReport: numSvs=%u", (unsigned int)sat.sat.numSvs);
        	}
        	m_num_sat_samples++;
            GPSEventSatReport e(sat.sat.numSvs, 0);
            for (unsigned int i = 0; i < sat.sat.numSvs; i++) {
            	if (m_nav_settings.debug_enable) {
					DEBUG_TRACE("UBXCommsEventSatReport: svInfo[%u].svId=%u", i, (unsigned int)sat.sat.svInfo[i].svId);
					DEBUG_TRACE("UBXCommsEventSatReport: svInfo[%u].qualityInd=%u", i, (unsigned int)sat.sat.svInfo[i].qualityInd);
					DEBUG_TRACE("UBXCommsEventSatReport: svInfo[%u].cno=%u", i, (unsigned int)sat.sat.svInfo[i].cno);
            	}

                if (sat.sat.svInfo[i].qualityInd > e.bestSignalQuality) {
                	e.bestSignalQuality = sat.sat.svInfo[i].qualityInd;
                }
            }
            notify(e);

        	// Check if max number of samples has been reached
        	if (m_nav_settings.max_sat_samples && m_num_sat_samples >= m_nav_settings.max_sat_samples) {
        		m_nav_settings.max_sat_samples = 0;
        		notify<GPSEventMaxSatSamples>({});
        	}
        }, "SatReport");
    }
}

void M10QAsyncReceiver::react(const UBXCommsEventNavReport& n) {
    static UBXCommsEventNavReport nav;
    std::memcpy(&nav, &n, sizeof(nav));
    system_scheduler->post_task_prio([this]() {
        while (STATE_EQUAL(receive)) {

            // Increment number of nav samples received
            m_num_nav_samples++;

            // Restart timeout
            initiate_timeout(5000);

            if (nav.pvt.fixType != UBX::NAV::PVT::FIXTYPE_NO &&
                nav.pvt.valid & UBX::NAV::PVT::VALID_VALID_DATE &&
                nav.pvt.valid & UBX::NAV::PVT::VALID_VALID_TIME)
            {
                rtc->settime(convert_epochtime(nav.pvt.year, nav.pvt.month,
                                                        nav.pvt.day, nav.pvt.hour,
                                                        nav.pvt.min, nav.pvt.sec));
            }

            if ((nav.pvt.fixType != UBX::NAV::PVT::FIXTYPE_2D &&
                nav.pvt.fixType != UBX::NAV::PVT::FIXTYPE_3D)) {
                break;
            }

            if (m_nav_settings.hacc_filter_en &&
                (m_nav_settings.hacc_filter_threshold * 1000) < nav.pvt.hAcc) {
                m_num_consecutive_fixes = m_nav_settings.num_consecutive_fixes;
                break;
            }

            if (m_nav_settings.hdop_filter_en &&
                (100 * m_nav_settings.hdop_filter_threshold) < nav.dop.hDOP) {
                m_num_consecutive_fixes = m_nav_settings.num_consecutive_fixes;
                break;
            }

            if (m_num_consecutive_fixes) {
                if (--m_num_consecutive_fixes) {
                    break;
                }
            }

            GNSSData gnss_data =
            {
                .iTOW      = nav.pvt.iTow,
                .year      = nav.pvt.year,
                .month     = nav.pvt.month,
                .day       = nav.pvt.day,
                .hour      = nav.pvt.hour,
                .min       = nav.pvt.min,
                .sec       = nav.pvt.sec,
                .valid     = nav.pvt.valid,
                .tAcc      = nav.pvt.tAcc,
                .nano      = nav.pvt.nano,
                .fixType   = nav.pvt.fixType,
                .flags     = nav.pvt.flags,
                .flags2    = nav.pvt.flags2,
                .flags3    = nav.pvt.flags3,
                .numSV     = nav.pvt.numSV,
                .lon       = nav.pvt.lon / 10000000.0,
                .lat       = nav.pvt.lat / 10000000.0,
                .height    = nav.pvt.height,
                .hMSL      = nav.pvt.hMSL,
                .hAcc      = nav.pvt.hAcc,
                .vAcc      = nav.pvt.vAcc,
                .velN      = nav.pvt.velN,
                .velE      = nav.pvt.velE,
                .velD      = nav.pvt.velD,
                .gSpeed    = nav.pvt.gSpeed,
                .headMot   = nav.pvt.headMot / 100000.0f,
                .sAcc      = nav.pvt.sAcc,
                .headAcc   = nav.pvt.headAcc / 100000.0f,
                .pDOP      = nav.dop.pDOP / 100.0f,
                .vDOP      = nav.dop.vDOP / 100.0f,
                .hDOP      = nav.dop.hDOP / 100.0f,
                .headVeh   = nav.pvt.headVeh / 100000.0f,
                .ttff      = nav.status.ttff
            };
            m_fix_was_found = true;
            notify(GPSEventPVT(gnss_data));
            return;
        }

        // Check if max number of samples has been reached
        if (m_nav_settings.max_nav_samples && m_num_nav_samples >= m_nav_settings.max_nav_samples) {
            m_nav_settings.max_nav_samples = 0;
            notify<GPSEventMaxNavSamples>({});
        }
    }, "NavReport");
}

void M10QAsyncReceiver::react(const UBXCommsEventMgaAck& ack) {
	if (STATE_EQUAL(fetchdatabase)) {
		m_expected_dbd_messages = ack.num_dbd_messages;
	}
}

void M10QAsyncReceiver::react(const UBXCommsEventMgaDBD& dbd) {
    if (STATE_EQUAL(fetchdatabase)) {
        cancel_timeout();
        if ((m_ana_database_len + dbd.length) < sizeof(m_navigation_database)) {
            std::memcpy(&m_navigation_database[m_ana_database_len], dbd.database, dbd.length);
            m_ana_database_len += dbd.length;
        } else
            m_database_overflow = true;
        initiate_timeout();
    } else if (STATE_EQUAL(senddatabase) || STATE_EQUAL(sendofflinedatabase)) {
        std::memcpy(&m_navigation_database[m_mga_ack_count], dbd.database, dbd.length);
        m_mga_ack_count += dbd.length;
    }
}

void M10QAsyncReceiver::react(const UBXCommsEventDebug& e) {
    system_scheduler->post_task_prio([e]() {
        DEBUG_TRACE("UBXComms<-GNSS: buffer=%s", Binascii::hexlify(std::string((char *)e.buffer, e.length)).c_str());
    }, "Debug");
}

void M10QAsyncReceiver::react(const UBXCommsEventError& e) {
    system_scheduler->post_task_prio([this, e]() {
        DEBUG_TRACE("UBXCommsEventError: type=%02x count=%u", e.error_type, m_uart_error_count);
    }, "Debug");
    if (STATE_EQUAL(poweron)) {
        if (++m_uart_error_count >= 10) {
            m_uart_error_count = 0;
            cancel_timeout();
            m_op_state = OpState::ERROR;
            run_state_machine();
        }
    } else {
        cancel_timeout();
        m_op_state = OpState::ERROR;
        run_state_machine();
    }
}

void M10QAsyncReceiver::initiate_timeout(unsigned int timeout_ms) {
	cancel_timeout();
	m_timeout.handle = system_scheduler->post_task_prio([this]() {
		on_timeout();
	}, "Timeout", Scheduler::DEFAULT_PRIORITY, timeout_ms);
}

void M10QAsyncReceiver::on_timeout() {
	if (m_op_state == OpState::PENDING) {
		m_op_state = OpState::TIMEOUT;
		m_ubx_comms.cancel_expect();
		state_machine();
	} else if (STATE_EQUAL(receive)) {
		// No NAV/SAT information received within requested receive timeout
		DEBUG_ERROR("M10QAsyncReceiver::on_timeout: no NAV/SAT info received");
		m_unrecoverable_error = true;
		notify<GPSEventError>({});
	}
}

void M10QAsyncReceiver::cancel_timeout() {
	system_scheduler->cancel_task(m_timeout.handle);
}

void M10QAsyncReceiver::state_poweron_enter() {
	m_step = 0;
	m_retries = 3;
	m_op_state = OpState::IDLE;
	m_uart_error_count = 0;
	m_fix_was_found = false;
	m_unrecoverable_error = false;
	exit_shutdown();
    notify<GPSEventPowerOn>({});
}

void M10QAsyncReceiver::state_poweron() {
	while (true) {
		if (m_op_state == OpState::IDLE) {
			m_op_state = OpState::PENDING;
			if (m_step == 0) {
				sync_baud_rate(9600);
				break;
			}
			else if (m_step == 1) {
				sync_baud_rate(MAX_BAUDRATE);
				break;
			} else {
				DEBUG_ERROR("M10QAsyncReceiver: failed to sync comms");
				m_unrecoverable_error = true;
				notify<GPSEventError>({});
				break;
			}
		} else if (m_op_state == OpState::SUCCESS) {
			STATE_CHANGE(poweron, configure);
			break;
		} else if (m_op_state == OpState::PENDING) {
			break;
		} else if (m_op_state == OpState::ERROR) {
			DEBUG_TRACE("M10QAsyncReceiver: baud rate framing error detected");
			m_retries = 3;
			m_step++;
			m_op_state = OpState::IDLE;
			run_state_machine(100);
			break;
		} else {
			if (--m_retries == 0) {
				m_retries = 3;
				m_step++;
			}
			m_op_state = OpState::IDLE;
		}
	}
}

void M10QAsyncReceiver::state_poweron_exit() {
}

void M10QAsyncReceiver::state_poweroff_enter() {
	m_step = 0;
	m_retries = 3;
	m_op_state = OpState::IDLE;
	m_uart_error_count = 0;
}

void M10QAsyncReceiver::state_poweroff() {
	if (!m_powering_off) {
		STATE_CHANGE(poweroff, configure);
		return;
	}
	enter_shutdown();
    notify(GPSEventPowerOff(m_fix_was_found));
    STATE_CHANGE(poweroff, idle);
}

void M10QAsyncReceiver::state_poweroff_exit() {
}

void M10QAsyncReceiver::state_configure_enter() {
	m_step = 0;
	m_retries = 3;
	m_op_state = OpState::IDLE;
}

void M10QAsyncReceiver::state_configure() {
	while (true) {
		if (m_op_state == OpState::IDLE) {
            DEBUG_TRACE("M10QAsyncReceiver:state_configure: step:%u", m_step);
			m_op_state = OpState::PENDING;
			if (m_step == 0) {
				setup_uart_port();
				m_step++;
				m_op_state = OpState::IDLE;
				run_state_machine(1000); // Wait 500 ms for UART config to apply
				break;
			} else if (m_step == 1) {
				sync_baud_rate(MAX_BAUDRATE);
				break;
			} else if (m_step == 2) {
				setup_gnss_channel_sharing();
				break;
			} else if (m_step == 3) {
				// Wait 500 ms for configuration to be applied
				m_step++;
				m_op_state = OpState::IDLE;
				run_state_machine(500);
				break;
			} else if (m_step == 4) {
                //TODO : no need to save if using VALSET
				save_config();
				break;
			} else if (m_step == 5) {
				m_step++;
				m_op_state = OpState::IDLE;
				soft_reset(); // This operation has no response
            } else if (m_step == 6) {
				m_step++;
				m_op_state = OpState::IDLE;
				run_state_machine(1000);
				break;
		    } else if (m_step == 7) {
				disable_odometer();
				break;
			} else if (m_step == 8) {
                // only one timepulse to disable on M10Q
				disable_timepulse_output();
				break;
			} else if (m_step == 9) {
				setup_power_management();
				break;
			} else if (m_step == 10) {
				setup_continuous_mode();
				break;
			} else if (m_step == 11) {
				setup_simple_navigation_settings();
				break;
			} else if (m_step == 12) {
				setup_expert_navigation_settings();
				break;
			} else if (m_step == 13) {
				if (rtc->is_set()) {
					supply_time_assistance();
					break;
				} else {
					m_op_state = OpState::IDLE;
					m_step++;
				}
			} else {
			    // Always try to send offline database as priority
			    STATE_CHANGE(configure, sendofflinedatabase);
				break;
			}
		} else if (m_op_state == OpState::SUCCESS) {
			m_step++;
			m_retries = 3;
			m_op_state = OpState::IDLE;
		} else if (m_op_state == OpState::PENDING) {
			break;
		} else {
			if (--m_retries == 0) {
				DEBUG_ERROR("M10QAsyncReceiver::state_configure: failed");
				m_unrecoverable_error = true;
				notify<GPSEventError>({});
				break;
            } else if (m_op_state == OpState::ERROR) {
                // Restart receiver on comms error
                initiate_timeout();
                m_ubx_comms.set_baudrate(MAX_BAUDRATE);
            }
			m_op_state = OpState::IDLE;
		}
	}
}

void M10QAsyncReceiver::state_configure_exit() {
}

void M10QAsyncReceiver::state_startreceive_enter() {
	m_step = 0;
	m_retries = 3;
	m_op_state = OpState::IDLE;
}

void M10QAsyncReceiver::state_startreceive() {
	while (true) {
		if (m_op_state == OpState::IDLE) {
			m_op_state = OpState::PENDING;
			if (m_step == 0) {
				enable_nav_pvt_message();
				break;
			} else if (m_step == 1) {
				enable_nav_dop_message();
				break;
			} else if (m_step == 2) {
				enable_nav_status_message();
				break;
            } else if (m_step == 3 && m_nav_settings.sat_tracking) {
                enable_nav_sat_message();
                break;
			} else {
				STATE_CHANGE(startreceive, receive);
				break;
			}
		} else if (m_op_state == OpState::SUCCESS) {
			m_step++;
			m_retries = 3;
			m_op_state = OpState::IDLE;
		} else if (m_op_state == OpState::PENDING) {
			break;
		} else {
			if (--m_retries == 0) {
				DEBUG_ERROR("M10QAsyncReceiver::state_start_receive: failed");
				m_unrecoverable_error = true;
				notify<GPSEventError>({});
				break;
            } else if (m_op_state == OpState::ERROR) {
                // Restart receiver on comms error
                initiate_timeout();
                m_ubx_comms.set_baudrate(MAX_BAUDRATE);
            }
			m_op_state = OpState::IDLE;
		}
	}
}

void M10QAsyncReceiver::state_startreceive_exit() {
}

void M10QAsyncReceiver::state_receive_enter() {
    // Allow maximum 5 seconds for receiver to start outputting navigation samples
    initiate_timeout(5000);
    m_op_state = OpState::IDLE;
    m_retries = 3;
}

void M10QAsyncReceiver::state_receive() {
    if (m_op_state == OpState::ERROR) {
        if (--m_retries) {
            // CommsError try to restart the receiver
            m_ubx_comms.set_baudrate(MAX_BAUDRATE);
            initiate_timeout(5000);
            m_op_state = OpState::IDLE;
        } else {
            DEBUG_ERROR("M10Receiver: repeated comms errors");
            m_unrecoverable_error = true;
            notify<GPSEventError>({});
        }
    }
}

void M10QAsyncReceiver::state_receive_exit() {
	cancel_timeout();
}

void M10QAsyncReceiver::state_stopreceive_enter() {
	m_step = 0;
	m_retries = 3;
	m_op_state = OpState::IDLE;
}

void M10QAsyncReceiver::state_stopreceive() {

	if (!m_powering_off) {
		STATE_CHANGE(stopreceive, startreceive);
		return;
	}

	while (true) {
		if (m_op_state == OpState::IDLE) {
			m_op_state = OpState::PENDING;
			if (m_step == 0) {
				disable_nav_pvt_message();
				break;
			} else if (m_step == 1) {
				disable_nav_dop_message();
				break;
			} else if (m_step == 2) {
				disable_nav_status_message();
				break;
            } else if (m_step == 3) {
                if (m_nav_settings.sat_tracking) {
                    disable_nav_sat_message();
                    break;
                } else {
        			m_op_state = OpState::IDLE;
                    m_step++;
                }
			} else if (m_step == 4) {
				m_step++;
				m_op_state = OpState::IDLE;
				run_state_machine(100); // Allow 100 ms to flush out any stray messages
				break;
			} else {
				STATE_CHANGE(stopreceive, fetchdatabase);
				break;
			}
		} else if (m_op_state == OpState::SUCCESS) {
			m_step++;
			m_retries = 3;
			m_op_state = OpState::IDLE;
		} else if (m_op_state == OpState::PENDING) {
			break;
		} else {
			if (--m_retries == 0) {
				DEBUG_ERROR("M10QAsyncReceiver: stop receive failed");
				STATE_CHANGE(stopreceive, poweroff);
				break;
            } else if (m_op_state == OpState::ERROR) {
                // Restart receiver on comms error
                initiate_timeout();
                m_ubx_comms.set_baudrate(MAX_BAUDRATE);
            }
			m_op_state = OpState::IDLE;
		}
	}
}

void M10QAsyncReceiver::state_stopreceive_exit() {
}

void M10QAsyncReceiver::state_fetchdatabase_enter() {
	m_step = 0;
	m_retries = 3;
	m_op_state = OpState::IDLE;
	m_ana_database_len = 0;
	m_expected_dbd_messages = 0;
    m_database_overflow = false;
	m_ubx_comms.start_dbd_filter();
}

void M10QAsyncReceiver::state_fetchdatabase() {
    if (!m_nav_settings.assistnow_autonomous_enable) {
        DEBUG_TRACE("M10QAsyncReceiver: fetchdatabase: ANA not enabled");
        STATE_CHANGE(fetchdatabase, poweroff);
        return;
    }
    if (m_ano_database_len) {
        DEBUG_TRACE("M10QAsyncReceiver: fetchdatabase: ANO in use, not fetching");
        STATE_CHANGE(fetchdatabase, poweroff);
        return;
    }
	while (true) {
		if (m_op_state == OpState::IDLE) {
			m_op_state = OpState::PENDING;
			if (m_step == 0) {
				fetch_navigation_database();
				break;
			} else {
				// This will fetch any MGA-ACK from the navigation buffer so we can
				// validate if the correct number of messages are present
				m_ubx_comms.expect(MessageClass::MSG_CLASS_MGA, MGA::ID_ACK);
				m_ubx_comms.filter_buffer(m_navigation_database, m_ana_database_len);
				m_ubx_comms.cancel_expect();
				DEBUG_TRACE("M10QAsyncReceiver::state_fetchdatabase: validating database size %u bytes expected %u msgs", m_ana_database_len, m_expected_dbd_messages);
				unsigned int actual_count = 0;
				if (!m_ubx_comms.is_expected_msg_count(m_navigation_database, m_ana_database_len,
						m_expected_dbd_messages, actual_count, MessageClass::MSG_CLASS_MGA, MGA::ID_DBD)) {
					if (--m_retries == 0) {
						DEBUG_WARN("M10QAsyncReceiver::state_fetch_database: failed: %u/%u msgs received",
								actual_count, m_expected_dbd_messages
								);
						dump_navigation_database(m_ana_database_len);
						m_ana_database_len = 0;
						STATE_CHANGE(fetchdatabase, poweroff);
						break;
					} else {
                        if (!m_database_overflow) {
                            DEBUG_TRACE("M10Receiver::state_fetchdatabase: validation failed: %u/%u msgs received: retry",
                                    actual_count, m_expected_dbd_messages);
                            m_ana_database_len = 0;
                            m_expected_dbd_messages = 0;
                            m_op_state = OpState::IDLE;
                            m_step = 0;
                            continue;
                        } else {
                            DEBUG_TRACE("M10Receiver::state_fetchdatabase: validation skipped: %u/%u msgs received: DBD buffer full",
                                        actual_count, m_expected_dbd_messages);
                        }
					}
				} else {
					DEBUG_TRACE("M10QAsyncReceiver::state_fetchdatabase: success");
					// Subtract the size of the MGA-ACK message from the database length
					// so it doesn't get sent out
					m_ana_database_len = m_ana_database_len - std::min(m_ana_database_len, msg_size<MGA::MSG_ACK>());
				}
				STATE_CHANGE(fetchdatabase, poweroff);
				break;
			}
		} else if (m_op_state == OpState::ERROR) {
			DEBUG_TRACE("M10QAsyncReceiver::state_fetchdatabase: UART comms error");
			m_ana_database_len = 0;
			STATE_CHANGE(fetchdatabase, poweroff);
			break;
		} else if (m_op_state == OpState::SUCCESS) {
			m_step++;
			m_op_state = OpState::IDLE;
		} else if (m_op_state == OpState::TIMEOUT) {
			m_step++;
			m_op_state = OpState::IDLE;
		} else if (m_op_state == OpState::PENDING) {
			break;
		}
	}
}

void M10QAsyncReceiver::state_fetchdatabase_exit() {
	m_ubx_comms.stop_dbd_filter();
}

void M10QAsyncReceiver::state_senddatabase_enter() {
	m_step = 0;
	m_op_state = OpState::IDLE;
	m_mga_ack_count = 0;
	m_ubx_comms.start_dbd_filter();
	DEBUG_TRACE("M10QAsyncReceiver::state_senddatabase: sending length %u bytes", m_ana_database_len);
}

void M10QAsyncReceiver::state_senddatabase() {
    if (!m_nav_settings.assistnow_autonomous_enable) {
        DEBUG_TRACE("M10QAsyncReceiver: senddatabase: ANA not enabled");
        STATE_CHANGE(senddatabase, startreceive);
        return;
    }
	while (true) {
		if (m_op_state == OpState::IDLE) {
			m_op_state = OpState::PENDING;
			if (m_step < m_ana_database_len) {
				// Send buffer contents in chunks raw and notify when sent
				unsigned int sz = std::min(128U, (m_ana_database_len - m_step));
				m_ubx_comms.send(&m_navigation_database[m_step], sz, true, true);
				m_step += sz;
				break;
			} else {
				unsigned int actual_count = 0;
				if (!m_ubx_comms.is_expected_msg_count(m_navigation_database, m_mga_ack_count,
						m_expected_dbd_messages, actual_count, MessageClass::MSG_CLASS_MGA,
						MGA::ID_ACK)) {
					DEBUG_TRACE("M10QAsyncReceiver::state_senddatabase: missing MGA-ACK: %u/%u acks recieved",
							actual_count, m_expected_dbd_messages);
				} else {
					DEBUG_TRACE("M10QAsyncReceiver::state_senddatabase: success: %u/%u acks recieved",
							actual_count, m_expected_dbd_messages);
				}
				STATE_CHANGE(senddatabase, startreceive);
				break;
			}
		} else if (m_op_state == OpState::SUCCESS) {
			// Skip to next record
			m_op_state = OpState::IDLE;
			run_state_machine(5);  // Wait 5 ms delay between transmitted data
			break;
		} else if (m_op_state == OpState::PENDING) {
			break;
		} else {
			DEBUG_WARN("M10QAsyncReceiver::state_send_database: failed");
            if (m_op_state == OpState::ERROR) {
                // Restart receiver on comms error
                m_ubx_comms.set_baudrate(MAX_BAUDRATE);
            }
			STATE_CHANGE(senddatabase, startreceive);
			break;
		}
	}
}

void M10QAsyncReceiver::state_senddatabase_exit() {
	m_ubx_comms.stop_dbd_filter();
}

void M10QAsyncReceiver::state_sendofflinedatabase_enter() {

    m_op_state = OpState::IDLE;
    m_step = 0;
    m_mga_ack_count = 0;
    m_ano_database_len = 0;

    if (!m_nav_settings.assistnow_offline_enable) {
        DEBUG_TRACE("M10QAsyncReceiver: sendofflinedatabase: ANO not enabled");
        return;
    }

    if (!rtc->is_set()) {
        DEBUG_TRACE("M10QAsyncReceiver: sendofflinedatabase: time not yet set");
        return;
    }

    try {
        LFSFile file(main_filesystem, "gps_config.dat", LFS_O_RDONLY);
        m_ubx_comms.copy_mga_ano_to_buffer(file, m_navigation_database, sizeof(m_navigation_database),
                                           rtc->gettime(),
                                           m_ano_database_len, m_expected_dbd_messages, m_ano_start_pos);
        m_ubx_comms.start_dbd_filter();
        DEBUG_TRACE("M10QAsyncReceiver::state_sendofflinedatabase: database len %u bytes",
                    m_ano_database_len);
    } catch (...) {
        DEBUG_ERROR("M10QAsyncReceiver::state_sendofflinedatabase: error opening MGA ANO file");
        m_op_state = OpState::ERROR;
    }
}

void M10QAsyncReceiver::state_sendofflinedatabase() {
    if (!m_nav_settings.assistnow_offline_enable) {
        STATE_CHANGE(sendofflinedatabase, senddatabase);
        return;
    }

    if (m_ano_database_len == 0) {
        STATE_CHANGE(sendofflinedatabase, senddatabase);
        return;
    }

    while (true) {
		if (m_op_state == OpState::IDLE) {
			m_op_state = OpState::PENDING;
			if (m_step < m_ano_database_len) {
				// Send buffer contents in chunks raw and notify when sent
				unsigned int sz = std::min(512U, (m_ano_database_len - m_step));
				m_ubx_comms.send(&m_navigation_database[m_step], sz, true, true);
				m_step += sz;
				break;
			} else {
				PMU::delay_ms(100);  // Allow any MGA-ACK confirmation messages to arrive
				unsigned int actual_count = 0;
				if (!m_ubx_comms.is_expected_msg_count(m_navigation_database, m_mga_ack_count,
						m_expected_dbd_messages, actual_count, MessageClass::MSG_CLASS_MGA,
						MGA::ID_ACK))
					DEBUG_WARN("M10QAsyncReceiver::state_sendofflinedatabase missing MGA-ACK: %u/%u acks received",
							actual_count, m_expected_dbd_messages);
				STATE_CHANGE(sendofflinedatabase, startreceive);
				break;
			}
		} else if (m_op_state == OpState::SUCCESS) {
			// Skip to next record
			m_retries = 1;
			m_op_state = OpState::IDLE;
			run_state_machine(1);  // Require 1 ms delay between transmitted data
			break;
		} else if (m_op_state == OpState::PENDING) {
			break;
		} else if (m_op_state == OpState::ERROR) {
			STATE_CHANGE(sendofflinedatabase, startreceive);
			break;
		} else {
			DEBUG_WARN("M10QAsyncReceiver::state_sendofflinedatabase: failed");
            if (m_op_state == OpState::ERROR) {
                // Restart receiver on comms error
                m_ubx_comms.set_baudrate(MAX_BAUDRATE);
            }
			STATE_CHANGE(sendofflinedatabase, startreceive);
			break;
		}
	}
}

void M10QAsyncReceiver::state_sendofflinedatabase_exit() {
	m_ubx_comms.stop_dbd_filter();
}

void M10QAsyncReceiver::state_idle_enter() {
	m_nav_settings.max_nav_samples = 0;
	m_nav_settings.max_sat_samples = 0;
}

void M10QAsyncReceiver::state_idle() {
}

void M10QAsyncReceiver::state_idle_exit() {
}

void M10QAsyncReceiver::sync_baud_rate(unsigned int baud) {
    DEBUG_TRACE("M10QAsyncReceiver::sync_baud_rate: Syncing baud rate to %u", baud);

    m_ubx_comms.set_baudrate(baud);

    // Test configuration by sending a known invalid message and expecting a NACK
    CFG::MSG::MSG_MSG_NORATE cfg_msg_invalid = {
        .msgClass = MessageClass::MSG_CLASS_BAD,
        .msgID = 0,
    };

    initiate_timeout(500);
    m_ubx_comms.send_packet_with_expect(MessageClass::MSG_CLASS_CFG, CFG::ID_MSG, cfg_msg_invalid,
                                         MessageClass::MSG_CLASS_ACK, ACK::ID_NACK);
}


void M10QAsyncReceiver::save_config() {
    DEBUG_TRACE("M10QAsyncReceiver::save_config: GPS CFG-CFG ->");
    CFG::CFG::MSG_CFG cfg_msg_cfg_cfg =
    {
        .clearMask   = 0,
        .saveMask    = 0xFFFFFFFF,
        // CFG::CFG::CLEARMASK_IOPORT   |
		// 			   CFG::CFG::CLEARMASK_MSGCONF  |
		// 			   CFG::CFG::CLEARMASK_INFMSG   |
		// 			   CFG::CFG::CLEARMASK_NAVCONF  |
		// 			   CFG::CFG::CLEARMASK_RXMCONF  |
		// 			   CFG::CFG::CLEARMASK_SENCONF  |
		// 			   CFG::CFG::CLEARMASK_RINVCONF |
		// 			   CFG::CFG::CLEARMASK_ANTCONF  |
		// 			   CFG::CFG::CLEARMASK_LOGCONF  |
		// 			   CFG::CFG::CLEARMASK_FTSCONF,
        .loadMask    = 0,
        //.deviceMask  = CFG::CFG::DEVMASK_SPIFLASH, //BBR changed to SPIFLASH for not realy saving but go to next step.
        .deviceMask  = CFG::CFG::DEVMASK_BBR, //BBR changed to SPIFLASH for not realy saving but go to next step.
    };

    initiate_timeout();
    m_ubx_comms.send_packet_with_expect(MessageClass::MSG_CLASS_CFG, CFG::ID_CFG, cfg_msg_cfg_cfg);
}

void M10QAsyncReceiver::soft_reset() {
    DEBUG_TRACE("M10QAsyncReceiver::soft_reset: GPS CFG-RST->");
    CFG::RST::MSG_RST cfg_msg_cfg_rst =
    {
        .navBbrMask = 0x0000,
        .resetMode = CFG::RST::RESETMODE_SOFTWARE_RESET_GNSS_ONLY,
        //.resetMode = CFG::RST::RESETMODE_HARDWARE_RESET_IMMEDIATE,
        .reserved1 = 0,
    };
    m_ubx_comms.send_packet(MessageClass::MSG_CLASS_CFG, CFG::ID_RST, cfg_msg_cfg_rst);
	m_ubx_comms.wait_send();
}

void M10QAsyncReceiver::setup_uart_port() {
    DEBUG_TRACE("M10QAsyncReceiver::setup_uart_port: Configuring UART1 with VALSET ->");

    // Set up each parameter value with its defined size
    CFG::UART1::BAUDRATE.set_value(MAX_BAUDRATE);                          // 9600 as 4 bytes
    CFG::UART1::STOPBITS.set_value(CFG::UART1::StopBits::ONE);  // 1 byte for 1 stop bit
    CFG::UART1::DATABITS.set_value(CFG::UART1::DataBits::EIGHT); // 1 byte for 8 data bits
    CFG::UART1::PARITY.set_value(CFG::UART1::Parity::NONE);     // 1 byte for no parity
    CFG::UART1::ENABLED.set_value(1);                              // 1 byte for enabled flag

    // Set protocol flags for UBX input and output
    CFG::UART1::INPROT_UBX.set_value(1);                                  // Enable UBX protocol as input (1 byte)
    CFG::UART1::OUTPROT_UBX.set_value(1);                                 // Enable UBX protocol as output (1 byte)

    // Clear protocol flags for NMEA input and output
    CFG::UART1::INPROT_NMEA.set_value(0);                                  // Enable UBX protocol as input (1 byte)
    CFG::UART1::OUTPROT_NMEA.set_value(0);                                 // Enable UBX protocol as output (1 byte)
                                                                           
    // TX Ready configurations
    CFG::UART1::TXREADY_ENABLED.set_value(0);                             // Disable TX ready (1 byte)
    CFG::UART1::TXREADY_POLARITY.set_value(0);                            // Set TX ready polarity to high-active (1 byte)
    CFG::UART1::TXREADY_PIN.set_value(0);                                 // Set TX ready pin (e.g., pin number 0) (1 byte)
    CFG::UART1::TXREADY_THRESHOLD.set_value(0);                         // Set TX ready threshold to 2000 bytes (250 * 8 bytes) (2 bytes)
    CFG::UART1::TXREADY_INTERFACE.set_value(0);                           // Link TX ready to UART1 (1 byte)

    // Collect all parameters in a vector
    std::vector<UBX::CFG::UBXParameter> uart1_config = {
        CFG::UART1::ENABLED,
        CFG::UART1::BAUDRATE,
        CFG::UART1::STOPBITS,
        CFG::UART1::DATABITS,
        CFG::UART1::PARITY,
        CFG::UART1::INPROT_UBX,
        CFG::UART1::OUTPROT_UBX,
        CFG::UART1::INPROT_NMEA,
        CFG::UART1::OUTPROT_NMEA,
        CFG::UART1::TXREADY_ENABLED,
        CFG::UART1::TXREADY_POLARITY,
        CFG::UART1::TXREADY_PIN,
        CFG::UART1::TXREADY_THRESHOLD,
        CFG::UART1::TXREADY_INTERFACE 
    };
    uint8_t layers = CFG::VALSET::LAYERS::BBR | CFG::VALSET::LAYERS::RAM;
    //DEBUG_TRACE("M10QAsyncReceiver::setup_uart_port: save %x", layers);
    //uint8_t layers = CFG::VALSET::LAYERS::RAM;
    // Create the VALSET message with dynamically sized parameters
    CFG::VALSET::MSG_VALSET uart1_valset_msg(0x00, layers, uart1_config);
    size_t cfgDataSize = uart1_valset_msg.get_cfgData_size(uart1_config);
    initiate_timeout();

    m_ubx_comms.send_packet_with_expect(MessageClass::MSG_CLASS_CFG, CFG::ID_VALSET, uart1_valset_msg, 
                                            MessageClass::MSG_CLASS_ACK, ACK::ID_ACK, cfgDataSize);
    m_ubx_comms.wait_send();
}


void M10QAsyncReceiver::read_uart_port() {
    DEBUG_TRACE("M10QAsyncReceiver::read_uart_port: Reading back UART1 baud rate with VALGET ->");

    // Define the parameter key directly, using the key ID defined in CFG::UART1
    std::vector<uint32_t> uart1_baudrate_key = { CFG::UART1::BAUDRATE.key };

    // Construct the VALGET message with version, layer, position, and key IDs
    CFG::VALGET::MSG_VALGET uart1_valget_msg(0x00, CFG::VALGET::LAYERS::RAM, 0, uart1_baudrate_key);
    size_t valGetDataSize = uart1_baudrate_key.size() * sizeof(uint32_t);

    // Send the VALGET message and expect a response for the UART1 baud rate
    m_ubx_comms.send_packet_with_expect(MessageClass::MSG_CLASS_CFG, CFG::ID_VALGET, uart1_valget_msg,
                                        MessageClass::MSG_CLASS_CFG, CFG::ID_VALGET, valGetDataSize);

    m_ubx_comms.wait_send();

}

void M10QAsyncReceiver::setup_power_management() {
    DEBUG_TRACE("M10QAsyncReceiver::setup_power_management: Configuring power management with VALSET ->");

    // Set power management parameters
    CFG::PM::OPERATEMODE.set_value(CFG::PM::OPERATEMODE_VALUES::PSMCT);  // Set to cyclic tracking mode
    CFG::PM::POSUPDATEPERIOD.set_value(1000);      // Set position update period to 1000 seconds
    CFG::PM::ACQPERIOD.set_value(10000);           // Set acquisition period to 10000 seconds if failed
    CFG::PM::GRIDOFFSET.set_value(0);              // No offset for GPS week alignment
    CFG::PM::ONTIME.set_value(1);                  // Time in Tracking state (1 second)
    CFG::PM::MINACQTIME.set_value(300);            // Minimum acquisition time (300 ms)
    CFG::PM::MAXACQTIME.set_value(0);              // Maximum acquisition time (no limit)
    CFG::PM::DONOTENTEROFF.set_value(1);           // Do not enter off state if fix fails
    CFG::PM::WAITTIMEFIX.set_value(0);             // No need to wait for time fix
    CFG::PM::UPDATEEPH.set_value(1);               // Regular ephemeris updates

#if 1 == NO_GPS_POWER_REG
    CFG::PM::EXTINTWAKE.set_value(1);              // EXTINT pin controls wake state
    CFG::PM::EXTINTBACKUP.set_value(1);            // EXTINT pin controls backup state
#else 

#endif

    // Collect all parameters for VALSET
    std::vector<CFG::UBXParameter> pm_config = {
        CFG::PM::OPERATEMODE,
        CFG::PM::POSUPDATEPERIOD,
        CFG::PM::ACQPERIOD,
        CFG::PM::GRIDOFFSET,
        CFG::PM::ONTIME,
        CFG::PM::MINACQTIME,
        CFG::PM::MAXACQTIME,
        CFG::PM::DONOTENTEROFF,
        CFG::PM::WAITTIMEFIX,
        CFG::PM::UPDATEEPH,
#if 1 == NO_GPS_POWER_REG
        CFG::PM::EXTINTWAKE,
        CFG::PM::EXTINTBACKUP
#endif
    };

    // Create the VALSET message with power management configuration
    CFG::VALSET::MSG_VALSET pm_valset_msg(0x00, CFG::VALSET::LAYERS::BBR|CFG::VALSET::LAYERS::RAM, pm_config);
    //CFG::VALSET::MSG_VALSET pm_valset_msg(0x00, CFG::VALSET::LAYERS::RAM, pm_config);
    size_t cfgDataSize = pm_valset_msg.get_cfgData_size(pm_config);
    initiate_timeout();

    m_ubx_comms.send_packet_with_expect(MessageClass::MSG_CLASS_CFG, CFG::ID_VALSET, pm_valset_msg, 
                                        MessageClass::MSG_CLASS_ACK, ACK::ID_ACK, cfgDataSize);
}

void M10QAsyncReceiver::setup_continuous_mode() {
    DEBUG_TRACE("M10QAsyncReceiver::setup_continuous_mode: Configuring continuous mode with VALSET ->");

    // Set power management to FULL mode for continuous operation
    CFG::PM::OPERATEMODE.set_value(CFG::PM::FULL);  // Disable all power-saving modes

    // Collect parameters in a vector for the VALSET message
    std::vector<CFG::UBXParameter> pm_config = {
        CFG::PM::OPERATEMODE
    };

    // Create the VALSET message for power management configuration
    CFG::VALSET::MSG_VALSET pm_valset_msg(0x00, CFG::VALSET::LAYERS::BBR|CFG::VALSET::LAYERS::RAM, pm_config);
    //CFG::VALSET::MSG_VALSET pm_valset_msg(0x00, CFG::VALSET::LAYERS::RAM, pm_config);
    size_t cfgDataSize = pm_valset_msg.get_cfgData_size(pm_config);
    initiate_timeout();

    m_ubx_comms.send_packet_with_expect(MessageClass::MSG_CLASS_CFG, CFG::ID_VALSET, pm_valset_msg, 
                                        MessageClass::MSG_CLASS_ACK, ACK::ID_ACK, cfgDataSize);
}

void M10QAsyncReceiver::setup_simple_navigation_settings() {
    DEBUG_TRACE("M10QAsyncReceiver::setup_simple_navigation_settings: Configuring NAVSPG with VALSET ->");

    // Set the navigation parameters for standard precision
    CFG::NAVSPG::FIXMODE.set_value(static_cast<CFG::NAVSPG::FIXMODE_VALUES>(m_nav_settings.fix_mode));  // Set position fix mode dynamically
    CFG::NAVSPG::DYNMODEL.set_value(static_cast<CFG::NAVSPG::DYNMODEL_VALUES>(m_nav_settings.dyn_model));  // Set dynamic platform model dynamically
    CFG::NAVSPG::INIFIX3D.set_value(1);                                    // Require initial 3D fix
    CFG::NAVSPG::WKNROLLOVER.set_value(1024);                              // GPS week rollover number
    CFG::NAVSPG::UTCSTANDARD.set_value(CFG::NAVSPG::UTCSTANDARD_VALUES::UTC_USNO);    // UTC standard (USNO)
    CFG::NAVSPG::INFIL_MINSVS.set_value(4);                                // Minimum satellites for navigation
    CFG::NAVSPG::INFIL_MINCNO.set_value(10);                               // Minimum satellite signal level
    CFG::NAVSPG::OUTFIL_PDOP.set_value(250);                               // Position DOP threshold (25.0)
    CFG::NAVSPG::OUTFIL_TDOP.set_value(250);                               // Time DOP threshold (25.0)
    CFG::NAVSPG::OUTFIL_PACC.set_value(100);                               // Position accuracy mask
    CFG::NAVSPG::OUTFIL_TACC.set_value(350);                               // Time accuracy mask
    CFG::NAVSPG::CONSTR_ALT.set_value(0);                                  // Fixed altitude for 2D fix mode
    CFG::NAVSPG::CONSTR_ALTVAR.set_value(10000);                           // Fixed altitude variance
    CFG::NAVSPG::INFIL_MINELEV.set_value(5);                               // Minimum elevation angle
    CFG::NAVSPG::CONSTR_DGNSSTO.set_value(60);                             // DGNSS timeout
    CFG::NAVSPG::SIGATTCOMP.set_value(CFG::NAVSPG::SIGATTCOMP_VALUES::SIGCOMP_AUTO); // Automatic signal attenuation compensation

    // Collect all parameters in a vector for VALSET
    std::vector<CFG::UBXParameter> navspg_config = {
        CFG::NAVSPG::FIXMODE,
        CFG::NAVSPG::DYNMODEL,
        CFG::NAVSPG::INIFIX3D,
        CFG::NAVSPG::WKNROLLOVER,
        CFG::NAVSPG::UTCSTANDARD,
        CFG::NAVSPG::INFIL_MINSVS,
        CFG::NAVSPG::INFIL_MINCNO,
        CFG::NAVSPG::OUTFIL_PDOP,
        CFG::NAVSPG::OUTFIL_TDOP,
        CFG::NAVSPG::OUTFIL_PACC,
        CFG::NAVSPG::OUTFIL_TACC,
        CFG::NAVSPG::CONSTR_ALT,
        CFG::NAVSPG::CONSTR_ALTVAR,
        CFG::NAVSPG::INFIL_MINELEV,
        CFG::NAVSPG::CONSTR_DGNSSTO,
        CFG::NAVSPG::SIGATTCOMP
    };

    // Create the VALSET message with the navigation parameters
    CFG::VALSET::MSG_VALSET nav_valset_msg(0x00, CFG::VALSET::LAYERS::BBR|CFG::VALSET::LAYERS::RAM, navspg_config);
    //CFG::VALSET::MSG_VALSET nav_valset_msg(0x00, CFG::VALSET::LAYERS::BBR | CFG::VALSET::LAYERS::RAM, navspg_config);
    size_t cfgDataSize = nav_valset_msg.get_cfgData_size(navspg_config);
    initiate_timeout();

    m_ubx_comms.send_packet_with_expect(MessageClass::MSG_CLASS_CFG, CFG::ID_VALSET, nav_valset_msg, 
                                        MessageClass::MSG_CLASS_ACK, ACK::ID_ACK, cfgDataSize);
}

void M10QAsyncReceiver::setup_expert_navigation_settings() {
    DEBUG_TRACE("M10QAsyncReceiver::setup_expert_navigation_settings: Configuring NAVSPG and ANA with VALSET ->");

    // NAVSPG Configuration (Standard Precision Navigation)
    CFG::NAVSPG::INFIL_MINSVS.set_value(3);                         // Minimum satellites for navigation
    CFG::NAVSPG::INFIL_MAXSVS.set_value(32);                        // Maximum satellites for navigation
    CFG::NAVSPG::INFIL_MINCNO.set_value(6);                         // Minimum satellite signal level for navigation
    CFG::NAVSPG::INIFIX3D.set_value(1);                             // Require initial 3D fix
    //CFG::NAVSPG::WKNROLLOVER.set_value(0);                          // Default GPS week rollover min value allowed is 1
    CFG::NAVSPG::ACKAIDING.set_value(1);                            // Acknowledge assistance messages
    CFG::NAVSPG::SIGATTCOMP.set_value(static_cast<uint8_t>(CFG::NAVSPG::SIGATTCOMP_VALUES::SIGCOMP_DISABLE)); // Disable signal attenuation compensation

    // ANA (AssistNow Autonomous) Configuration
    CFG::ANA::USE_ANA.set_value(m_nav_settings.assistnow_autonomous_enable ? 
        static_cast<uint8_t>(CFG::ANA::USE_ANA_VALUES::ANA_ENABLED) : 
        static_cast<uint8_t>(CFG::ANA::USE_ANA_VALUES::ANA_DISABLED));
    CFG::ANA::ORBMAXERR.set_value(100);                             // Maximum orbit error for AssistNow Autonomous

    // Collect parameters into a vector
    std::vector<CFG::UBXParameter> navspg_ana_config = {
        CFG::NAVSPG::INFIL_MINSVS,
        CFG::NAVSPG::INFIL_MAXSVS,
        //CFG::NAVSPG::INFIL_MINCNO, NACK with this 
        CFG::NAVSPG::INIFIX3D,
        //CFG::NAVSPG::WKNROLLOVER, 
        CFG::NAVSPG::ACKAIDING,
        CFG::NAVSPG::SIGATTCOMP,
        CFG::ANA::USE_ANA,
        CFG::ANA::ORBMAXERR
    };

    // Create and send VALSET message
    CFG::VALSET::MSG_VALSET nav_ana_valset_msg(0x00, CFG::VALSET::LAYERS::BBR|CFG::VALSET::LAYERS::RAM, navspg_ana_config);
    //CFG::VALSET::MSG_VALSET nav_ana_valset_msg(0x00, CFG::VALSET::LAYERS::RAM, navspg_ana_config);
    size_t cfgDataSize = nav_ana_valset_msg.get_cfgData_size(navspg_ana_config);
    initiate_timeout();

    m_ubx_comms.send_packet_with_expect(MessageClass::MSG_CLASS_CFG, CFG::ID_VALSET, nav_ana_valset_msg, 
                                        MessageClass::MSG_CLASS_ACK, ACK::ID_ACK, cfgDataSize);
}

void M10QAsyncReceiver::supply_time_assistance() {
    DEBUG_TRACE("M10QAsyncReceiver::supply_time_assistance: GPS MGA-INI-TIME-UTC ->");
    uint16_t year;
    uint8_t month, day, hour, min, sec;

    convert_datetime_to_epoch(rtc->gettime(), year, month, day, hour, min, sec);

    MGA::MSG_INI_TIME_UTC cfg_msg_ini_time_utc =
    {
        .type = 0x10,
        .version = 0x00,
        .ref = 0x00,
        .leapSecs = -128, // Number of leap seconds unknown
        .year = year,
        .month = month,
        .day = day,
        .hour = hour,
        .minute = min,
        .second = sec,
        .reserved1 = 0,
        .ns = 0,
        .tAccS = 2, // Accurate to within 2 seconds, perhaps this can be improved?
        .reserved2 = {0},
        .tAccNs = 0
    };

    initiate_timeout();
    m_ubx_comms.send_packet_with_expect(MessageClass::MSG_CLASS_MGA, MGA::ID_INI_TIME_UTC, cfg_msg_ini_time_utc,
    		MessageClass::MSG_CLASS_MGA, MGA::ID_ACK);
}

void M10QAsyncReceiver::disable_odometer() {
    DEBUG_TRACE("M10QAsyncReceiver::disable_odometer: Disabling odometer with VALSET ->");


     // Set the odometer parameters to the desired values for disabling
    CFG::ODO::USE_ODO.set_value(0x00);            // Disable odometer usage
    CFG::ODO::USE_COG.set_value(0x00);            // Disable low-speed course over ground filter
    CFG::ODO::OUTLPVEL.set_value(0x00);           // Disable low-pass filtered velocity output
    CFG::ODO::OUTLPCOG.set_value(0x00);           // Disable low-pass filtered course over ground output
    CFG::ODO::PROFILE.set_value(CFG::ODO::Profile::RUN); // Set profile to CUSTOM (disabled state)
    CFG::ODO::COG_MAXSPEED.set_value(1);          // Set minimal max speed for course over ground filter (example value)
    CFG::ODO::COG_MAXPOSACC.set_value(50);        // Set example value for max position accuracy
    CFG::ODO::VEL_LPGAIN.set_value(153);          // Set velocity low-pass filter gain
    CFG::ODO::COG_LPGAIN.set_value(76);           // Set course over ground low-pass filter gain

    // Collect all parameters in a vector for MSG_VALSET
    std::vector<CFG::UBXParameter> odo_config = {
        CFG::ODO::USE_ODO,
        CFG::ODO::USE_COG,
        CFG::ODO::OUTLPVEL,
        CFG::ODO::OUTLPCOG,
        CFG::ODO::PROFILE,
        CFG::ODO::COG_MAXSPEED,
        CFG::ODO::COG_MAXPOSACC,
        CFG::ODO::VEL_LPGAIN,
        CFG::ODO::COG_LPGAIN
    };

    // Create the VALSET message with the odometer parameters
    CFG::VALSET::MSG_VALSET odo_valset_msg(0x00, CFG::VALSET::LAYERS::BBR | CFG::VALSET::LAYERS::RAM, odo_config);
    //CFG::VALSET::MSG_VALSET odo_valset_msg(0x00,CFG::VALSET::LAYERS::RAM, odo_config);
    size_t cfgDataSize = odo_valset_msg.get_cfgData_size(odo_config);
    initiate_timeout();

    m_ubx_comms.send_packet_with_expect(MessageClass::MSG_CLASS_CFG, CFG::ID_VALSET, odo_valset_msg, 
                                            MessageClass::MSG_CLASS_ACK, ACK::ID_ACK, cfgDataSize);

}

void M10QAsyncReceiver::disable_timepulse_output() {
    DEBUG_TRACE("M10QAsyncReceiver::disable_timepulse_output: Configuring timepulse output with VALSET ->");
    // Set up the time pulse parameters for disabling
    // CFG::TP::PULSE_LENGTH_DEF.set_value(1); // Set pulse length to time in [us]
    // CFG::TP::ANT_CABLEDELAY.set_value(0);                     // Set antenna cable delay to 0 ns
    // CFG::TP::LEN_LOCK_TP1.set_value(100000);                  // Set locked pulse length to 100,000
    // CFG::TP::USER_DELAY_TP1.set_value(0);                     // Set user-configurable delay to 0 ns
    // CFG::TP::SYNC_GNSS_TP1.set_value(0);                      // Do not sync with GNSS time
    // CFG::TP::USE_LOCKED_TP1.set_value(0);                     // Do not use locked parameters
    // CFG::TP::ALIGN_TO_TOW_TP1.set_value(0);                   // Do not align to top of second
    // //CFG::TP::TIMEGRID_TP1.set_value(CFG::TP::TIMEGRID_TP1::LOCAL); // Set time grid to local reference
                                                              

    // // Collect all parameters in a vector for MSG_VALSET
    // std::vector<CFG::UBXParameter> tp_config = {
    //     CFG::TP::PULSE_LENGTH_DEF,
    //     CFG::TP::ANT_CABLEDELAY,
    //     CFG::TP::LEN_LOCK_TP1,
    //     CFG::TP::USER_DELAY_TP1,
    //     CFG::TP::SYNC_GNSS_TP1,
    //     CFG::TP::USE_LOCKED_TP1,
    //     CFG::TP::ALIGN_TO_TOW_TP1,
    //     CFG::TP::POL_TP1,
    //     //CFG::TP::TIMEGRID_TP1
    // };
    // Minimal configuration for disabling time pulse with 1 Hz setting
    CFG::TP::PULSE_DEF.set_value(CFG::TP::PULSE_DEF::PERIOD);            // Set pulse mode to period
    CFG::TP::PERIOD_TP1.set_value(1000000);     // Set period to 1 second (1 Hz) in microseconds
    CFG::TP::PERIOD_LOCK_TP1.set_value(1000000);              // Set locked period to 1 second in microseconds
    CFG::TP::LEN_TP1.set_value(0);              // Set pulse length to 0 (no pulse)
    CFG::TP::TP1_ENA.set_value(0);              // Disable the time pulse
    CFG::TP::POL_TP1.set_value(0);                            // Set time pulse polarity to default (falling edge)

    // Collect only necessary parameters in the configuration vector
    std::vector<CFG::UBXParameter> tp_config = {
        CFG::TP::PULSE_DEF,
        CFG::TP::PERIOD_TP1,
        CFG::TP::PERIOD_LOCK_TP1,
        CFG::TP::LEN_TP1,
        CFG::TP::POL_TP1,
        CFG::TP::TP1_ENA
    };

    // Create the VALSET message with the timepulse parameters
    CFG::VALSET::MSG_VALSET tp_valset_msg(0x00, CFG::VALSET::LAYERS::BBR | CFG::VALSET::LAYERS::RAM, tp_config);
    //CFG::VALSET::MSG_VALSET tp_valset_msg(0x00, CFG::VALSET::LAYERS::RAM, tp_config);
    size_t cfgDataSize = tp_valset_msg.get_cfgData_size(tp_config);
    initiate_timeout();

    m_ubx_comms.send_packet_with_expect(MessageClass::MSG_CLASS_CFG, CFG::ID_VALSET, tp_valset_msg, 
                                            MessageClass::MSG_CLASS_ACK, ACK::ID_ACK, cfgDataSize);

}

void M10QAsyncReceiver::enable_nav_pvt_message() {
    DEBUG_TRACE("M10QAsyncReceiver::enable_nav_pvt_message: Configuring NAV PVT output on UART1 with VALSET ->");

    // Set output rate for UBX-NAV-PVT message on UART1 port
    CFG::MSGOUT::NAV_PVT_UART1.set_value(1);  // Set rate to 1 (enables message output on UART1)

    // Collect the NAV PVT UART1 configuration parameter in a vector
    std::vector<CFG::UBXParameter> nav_pvt_config = {
        CFG::MSGOUT::NAV_PVT_UART1
    };

    // Create and send the VALSET message with the NAV PVT configuration
    //CFG::VALSET::MSG_VALSET nav_pvt_valset_msg(0x00,CFG::VALSET::LAYERS::RAM, nav_pvt_config);
    CFG::VALSET::MSG_VALSET nav_pvt_valset_msg(0x00, CFG::VALSET::LAYERS::BBR | CFG::VALSET::LAYERS::RAM, nav_pvt_config);
    size_t cfgDataSize = nav_pvt_valset_msg.get_cfgData_size(nav_pvt_config);
    initiate_timeout();

    m_ubx_comms.send_packet_with_expect(MessageClass::MSG_CLASS_CFG, CFG::ID_VALSET, nav_pvt_valset_msg, 
                                        MessageClass::MSG_CLASS_ACK, ACK::ID_ACK, cfgDataSize);
}

void M10QAsyncReceiver::disable_nav_pvt_message() {
    DEBUG_TRACE("M10QAsyncReceiver::disable_nav_pvt_message: Disabling NAV-PVT message with VALSET ->");

    CFG::MSGOUT::NAV_PVT_UART1.set_value(0); // Disable NAV-PVT message on UART1

    std::vector<CFG::UBXParameter> nav_pvt_config = {CFG::MSGOUT::NAV_PVT_UART1};
    //CFG::VALSET::MSG_VALSET nav_pvt_valset_msg(0x00, CFG::VALSET::LAYERS::RAM, nav_pvt_config);
    CFG::VALSET::MSG_VALSET nav_pvt_valset_msg(0x00,CFG::VALSET::LAYERS::BBR | CFG::VALSET::LAYERS::RAM, nav_pvt_config);
    
    size_t cfgDataSize = nav_pvt_valset_msg.get_cfgData_size(nav_pvt_config);
    initiate_timeout();
    
    m_ubx_comms.send_packet_with_expect(MessageClass::MSG_CLASS_CFG, CFG::ID_VALSET, nav_pvt_valset_msg,
                                        MessageClass::MSG_CLASS_ACK, ACK::ID_ACK, cfgDataSize);
}

void M10QAsyncReceiver::enable_nav_dop_message() {
    DEBUG_TRACE("M10QAsyncReceiver::enable_nav_dop_message: Enabling NAV-DOP message with VALSET ->");

    CFG::MSGOUT::NAV_DOP_UART1.set_value(1); // Enable NAV-DOP message on UART1

    std::vector<CFG::UBXParameter> nav_dop_config = {CFG::MSGOUT::NAV_DOP_UART1};
    //CFG::VALSET::MSG_VALSET nav_dop_valset_msg(0x00, CFG::VALSET::LAYERS::RAM, nav_dop_config);
    CFG::VALSET::MSG_VALSET nav_dop_valset_msg(0x00, CFG::VALSET::LAYERS::BBR | CFG::VALSET::LAYERS::RAM, nav_dop_config);
    
    size_t cfgDataSize = nav_dop_valset_msg.get_cfgData_size(nav_dop_config);
    initiate_timeout();
    
    m_ubx_comms.send_packet_with_expect(MessageClass::MSG_CLASS_CFG, CFG::ID_VALSET, nav_dop_valset_msg,
                                        MessageClass::MSG_CLASS_ACK, ACK::ID_ACK, cfgDataSize);
}

void M10QAsyncReceiver::disable_nav_dop_message() {
    DEBUG_TRACE("M10QAsyncReceiver::disable_nav_dop_message: Disabling NAV-DOP message with VALSET ->");

    CFG::MSGOUT::NAV_DOP_UART1.set_value(0); // Disable NAV-DOP message on UART1

    std::vector<CFG::UBXParameter> nav_dop_config = {CFG::MSGOUT::NAV_DOP_UART1};
    //CFG::VALSET::MSG_VALSET nav_dop_valset_msg(0x00, CFG::VALSET::LAYERS::RAM, nav_dop_config);
    CFG::VALSET::MSG_VALSET nav_dop_valset_msg(0x00, CFG::VALSET::LAYERS::BBR | CFG::VALSET::LAYERS::RAM, nav_dop_config);

    size_t cfgDataSize = nav_dop_valset_msg.get_cfgData_size(nav_dop_config);
    initiate_timeout();

    m_ubx_comms.send_packet_with_expect(MessageClass::MSG_CLASS_CFG, CFG::ID_VALSET, nav_dop_valset_msg,
                                        MessageClass::MSG_CLASS_ACK, ACK::ID_ACK, cfgDataSize);
}

void M10QAsyncReceiver::enable_nav_status_message() {
    DEBUG_TRACE("M10QAsyncReceiver::enable_nav_status_message: Enabling NAV-STATUS message with VALSET ->");

    CFG::MSGOUT::NAV_STATUS_UART1.set_value(1); // Enable NAV-STATUS message on UART1

    std::vector<CFG::UBXParameter> nav_status_config = {CFG::MSGOUT::NAV_STATUS_UART1};
    //CFG::VALSET::MSG_VALSET nav_status_valset_msg(0x00, CFG::VALSET::LAYERS::RAM, nav_status_config);
    CFG::VALSET::MSG_VALSET nav_status_valset_msg(0x00, CFG::VALSET::LAYERS::BBR | CFG::VALSET::LAYERS::RAM, nav_status_config);
    
    size_t cfgDataSize = nav_status_valset_msg.get_cfgData_size(nav_status_config);
    initiate_timeout();
    
    m_ubx_comms.send_packet_with_expect(MessageClass::MSG_CLASS_CFG, CFG::ID_VALSET, nav_status_valset_msg,
                                        MessageClass::MSG_CLASS_ACK, ACK::ID_ACK, cfgDataSize);
}

void M10QAsyncReceiver::disable_nav_status_message() {
    DEBUG_TRACE("M10QAsyncReceiver::disable_nav_status_message: Disabling NAV-STATUS message with VALSET ->");

    CFG::MSGOUT::NAV_STATUS_UART1.set_value(0); // Disable NAV-STATUS message on UART1

    std::vector<CFG::UBXParameter> nav_status_config = {CFG::MSGOUT::NAV_STATUS_UART1};
    CFG::VALSET::MSG_VALSET nav_status_valset_msg(0x00, CFG::VALSET::LAYERS::BBR|CFG::VALSET::LAYERS::RAM, nav_status_config);
    //CFG::VALSET::MSG_VALSET nav_status_valset_msg(0x00, CFG::VALSET::LAYERS::RAM, nav_status_config);

    size_t cfgDataSize = nav_status_valset_msg.get_cfgData_size(nav_status_config);
    initiate_timeout();

    m_ubx_comms.send_packet_with_expect(MessageClass::MSG_CLASS_CFG, CFG::ID_VALSET, nav_status_valset_msg,
                                        MessageClass::MSG_CLASS_ACK, ACK::ID_ACK, cfgDataSize);
}

void M10QAsyncReceiver::enable_nav_sat_message() {
    DEBUG_TRACE("M10QAsyncReceiver::enable_nav_sat_message: Enabling NAV-SAT message with VALSET ->");

    CFG::MSGOUT::NAV_SAT_UART1.set_value(1); // Enable NAV-SAT message on UART1

    std::vector<CFG::UBXParameter> nav_sat_config = {CFG::MSGOUT::NAV_SAT_UART1};
    CFG::VALSET::MSG_VALSET nav_sat_valset_msg(0x00, CFG::VALSET::LAYERS::BBR|CFG::VALSET::LAYERS::RAM, nav_sat_config);
    //CFG::VALSET::MSG_VALSET nav_sat_valset_msg(0x00, CFG::VALSET::LAYERS::RAM, nav_sat_config);

    size_t cfgDataSize = nav_sat_valset_msg.get_cfgData_size(nav_sat_config);
    initiate_timeout();

    m_ubx_comms.send_packet_with_expect(MessageClass::MSG_CLASS_CFG, CFG::ID_VALSET, nav_sat_valset_msg,
                                        MessageClass::MSG_CLASS_ACK, ACK::ID_ACK, cfgDataSize);
}

void M10QAsyncReceiver::disable_nav_sat_message() {
    DEBUG_TRACE("M10QAsyncReceiver::disable_nav_sat_message: Disabling NAV-SAT message with VALSET ->");

    CFG::MSGOUT::NAV_SAT_UART1.set_value(0); // Disable NAV-SAT message on UART1

    std::vector<CFG::UBXParameter> nav_sat_config = {CFG::MSGOUT::NAV_SAT_UART1};
    //CFG::VALSET::MSG_VALSET nav_sat_valset_msg(0x00, CFG::VALSET::LAYERS::RAM, nav_sat_config);
    CFG::VALSET::MSG_VALSET nav_sat_valset_msg(0x00, CFG::VALSET::LAYERS::BBR | CFG::VALSET::LAYERS::RAM, nav_sat_config);

    size_t cfgDataSize = nav_sat_valset_msg.get_cfgData_size(nav_sat_config);
    initiate_timeout();

    m_ubx_comms.send_packet_with_expect(MessageClass::MSG_CLASS_CFG, CFG::ID_VALSET, nav_sat_valset_msg,
                                        MessageClass::MSG_CLASS_ACK, ACK::ID_ACK, cfgDataSize);
}

void M10QAsyncReceiver::setup_gnss_channel_sharing() {
    DEBUG_TRACE("M10QAsyncReceiver::setup_gnss_channel_sharing: Configuring GNSS signals with VALSET ->");

    // Set each GNSS signal parameter to enable (1)
    CFG::SIGNAL::GPS_ENA.set_value(1);
    CFG::SIGNAL::GPS_L1CA_ENA.set_value(1);
    CFG::SIGNAL::SBAS_ENA.set_value(1);
    CFG::SIGNAL::SBAS_L1CA_ENA.set_value(0);
    CFG::SIGNAL::GAL_ENA.set_value(1);
    CFG::SIGNAL::GAL_E1_ENA.set_value(1);
    CFG::SIGNAL::BDS_ENA.set_value(0);
    CFG::SIGNAL::BDS_B1_ENA.set_value(0);
    CFG::SIGNAL::BDS_B1C_ENA.set_value(0);
    CFG::SIGNAL::QZSS_ENA.set_value(1);
    CFG::SIGNAL::QZSS_L1CA_ENA.set_value(1);
    CFG::SIGNAL::QZSS_L1S_ENA.set_value(0);
    CFG::SIGNAL::GLO_ENA.set_value(1);
    CFG::SIGNAL::GLO_L1_ENA.set_value(1);

    // Collect all parameters in a vector for MSG_VALSET
    std::vector<UBX::CFG::UBXParameter> gnss_signal_config = {
        CFG::SIGNAL::GPS_ENA,
        CFG::SIGNAL::GPS_L1CA_ENA,
        CFG::SIGNAL::SBAS_ENA,
        CFG::SIGNAL::SBAS_L1CA_ENA,
        CFG::SIGNAL::GAL_ENA,
        CFG::SIGNAL::GAL_E1_ENA,
        CFG::SIGNAL::BDS_ENA,
        CFG::SIGNAL::BDS_B1_ENA,
        CFG::SIGNAL::BDS_B1C_ENA,
        CFG::SIGNAL::QZSS_ENA,
        CFG::SIGNAL::QZSS_L1CA_ENA,
        CFG::SIGNAL::QZSS_L1S_ENA,
        CFG::SIGNAL::GLO_ENA,
        CFG::SIGNAL::GLO_L1_ENA 
    };

    // Create the VALSET message for GNSS signal configuration
    //CFG::VALSET::MSG_VALSET gnss_valset_msg(0x00, CFG::VALSET::LAYERS::BBR | CFG::VALSET::LAYERS::FLASH, gnss_signal_config);
    CFG::VALSET::MSG_VALSET gnss_valset_msg(0x00,  CFG::VALSET::LAYERS::BBR | CFG::VALSET::LAYERS::RAM, gnss_signal_config);
    size_t cfgDataSize = gnss_valset_msg.get_cfgData_size(gnss_signal_config);
    initiate_timeout();

    m_ubx_comms.send_packet_with_expect(MessageClass::MSG_CLASS_CFG, CFG::ID_VALSET, gnss_valset_msg, 
                                            MessageClass::MSG_CLASS_ACK, ACK::ID_ACK, cfgDataSize);

    m_ubx_comms.wait_send();

}

void M10QAsyncReceiver::fetch_navigation_database() {
	UBX::Empty msg_dbd = {};
    initiate_timeout();
    m_ubx_comms.send_packet(MessageClass::MSG_CLASS_MGA, MGA::ID_DBD, msg_dbd);
}

void M10QAsyncReceiver::send_offline_database() {
}

void M10QAsyncReceiver::dump_navigation_database(unsigned int len) {
	for (unsigned int i = 0; i < len; i++)
		printf("%02X", m_navigation_database[i]);
	printf("\n");
}
