#include "m8qasync.hpp"
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
extern Timer      *system_timer;
extern FileSystem *main_filesystem;

// State machine helper macros
#define STATE_CHANGE(x, y)                       \
	do {                                             \
		DEBUG_TRACE("M8QAsyncReceiver::STATE_CHANGE: " #x " -> " #y ); \
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


M8QAsyncReceiver::M8QAsyncReceiver() {
	STATE_CHANGE(idle, idle);
	m_ana_database_len = 0;
    m_ano_database_len = 0;
	m_ano_start_pos = 0;
	m_timeout.running = false;
	m_num_power_on = 0;
	m_powering_off = false;
}

M8QAsyncReceiver::~M8QAsyncReceiver() {
}

void M8QAsyncReceiver::power_on(const GPSNavSettings& nav_settings) {

	// Track number of power on requests
	m_num_power_on++;

	// Cancel any ongoing poweroff request
	m_powering_off = false;

	// Reset observation counters of interest
	if (nav_settings.max_nav_samples) {
		m_num_nav_samples = 0;
		m_nav_settings.max_nav_samples = nav_settings.max_nav_samples;
	}

	if (nav_settings.max_sat_samples) {
		m_num_sat_samples = 0;
		m_nav_settings.max_sat_samples = nav_settings.max_sat_samples;
	}

	if (STATE_EQUAL(idle)) {
		// Copy navigation settings to be used for this session
		m_nav_settings = nav_settings;
		STATE_CHANGE(idle, poweron);
	}
}

void M8QAsyncReceiver::check_for_power_off() {

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

void M8QAsyncReceiver::power_off() {
	// Just decrement the number of users
	if (m_num_power_on)
		m_num_power_on--;
}

void M8QAsyncReceiver::enter_shutdown() {
    m_ubx_comms.deinit();

#if 0 == NO_GPS_POWER_REG
    // Disable the power supply for the GPS
    GPIOPins::clear(BSP::GPIO::GPIO_GPS_PWR_EN);
#else
	// Use GPIO_GPS_EXT_INT as a shutdown
    GPIOPins::clear(BSP::GPIO::GPIO_GPS_EXT_INT);
#endif
}

void M8QAsyncReceiver::exit_shutdown() {
	// Configure UBX comms if not already done
    m_ubx_comms.init();
    m_ubx_comms.subscribe(*this);
    m_ubx_comms.set_debug_enable(m_nav_settings.debug_enable);

#if 0 == NO_GPS_POWER_REG
    // Enable the power supply for the GPS
    GPIOPins::set(BSP::GPIO::GPIO_GPS_PWR_EN);
    PMU::delay_ms(1000); // Necessary to allow the device to boot
#else
	// Use GPIO_GPS_EXT_INT as a wake-up
    GPIOPins::set(BSP::GPIO::GPIO_GPS_EXT_INT);
#endif
}

void M8QAsyncReceiver::state_machine() {
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

	// Check for power off criteria each time the state machine runs
	check_for_power_off();
}

void M8QAsyncReceiver::run_state_machine(unsigned int time_ms) {
	system_scheduler->cancel_task(m_state_machine_handle);
	m_state_machine_handle = system_scheduler->post_task_prio([this]() {
		state_machine();
	}, "M8Q", Scheduler::DEFAULT_PRIORITY, time_ms);
}

void M8QAsyncReceiver::react(const UBXCommsEventSendComplete&) {
	if (m_op_state == OpState::PENDING) {
		cancel_timeout();
		m_op_state = OpState::SUCCESS;
		run_state_machine();
	}
}

void M8QAsyncReceiver::react(const UBXCommsEventAckNack& ack) {
	//DEBUG_TRACE("UBXCommsEventAckNack:%u", ack.ack);
	if (m_op_state == OpState::PENDING) {
		cancel_timeout();
		m_op_state = ack.ack ? OpState::SUCCESS : OpState::NACK;
		run_state_machine();
	}
}

void M8QAsyncReceiver::react(const UBXCommsEventSatReport& s) {
    if (m_nav_settings.sat_tracking) {
    	static UBXCommsEventSatReport sat;
    	std::memcpy(&sat, &s, sizeof(sat));
        system_scheduler->post_task_prio([this]() {
            //DEBUG_TRACE("UBXCommsEventSatReport: numSvs=%u", (unsigned int)sat.sat.numSvs);
        	m_num_sat_samples++;
            GPSEventSatReport e(sat.sat.numSvs, 0);
            for (unsigned int i = 0; i < sat.sat.numSvs; i++) {
                //DEBUG_TRACE("UBXCommsEventSatReport: svInfo[%u].svId=%u", i, (unsigned int)sat.sat.svInfo[i].svId);
                //DEBUG_TRACE("UBXCommsEventSatReport: svInfo[%u].qualityInd=%u", i, (unsigned int)sat.sat.svInfo[i].qualityInd);
                //DEBUG_TRACE("UBXCommsEventSatReport: svInfo[%u].cno=%u", i, (unsigned int)sat.sat.svInfo[i].cno);

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

        	run_state_machine();

        }, "SatReport");
    }
}

void M8QAsyncReceiver::react(const UBXCommsEventNavReport& n) {
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

    	run_state_machine();

    }, "NavReport");
}

void M8QAsyncReceiver::react(const UBXCommsEventMgaAck& ack) {
	if (STATE_EQUAL(fetchdatabase)) {
		m_expected_dbd_messages = ack.num_dbd_messages;
	}
}

void M8QAsyncReceiver::react(const UBXCommsEventMgaDBD& dbd) {
	if (STATE_EQUAL(fetchdatabase)) {
		cancel_timeout();
		if ((m_ana_database_len + dbd.length) < sizeof(m_navigation_database)) {
			std::memcpy(&m_navigation_database[m_ana_database_len], dbd.database, dbd.length);
			m_ana_database_len += dbd.length;
		}
		initiate_timeout();
	} else if (STATE_EQUAL(senddatabase) || STATE_EQUAL(sendofflinedatabase)) {
		std::memcpy(&m_navigation_database[m_mga_ack_count], dbd.database, dbd.length);
		m_mga_ack_count += dbd.length;
	}
}

void M8QAsyncReceiver::react(const UBXCommsEventDebug& e) {
    system_scheduler->post_task_prio([e]() {
        DEBUG_TRACE("UBXComms<-GNSS: buffer=%s", Binascii::hexlify(std::string((char *)e.buffer, e.length)).c_str());
    }, "Debug");
}

void M8QAsyncReceiver::react(const UBXCommsEventError& e) {
    system_scheduler->post_task_prio([this, e]() {
        DEBUG_TRACE("UBXCommsEventError: type=%02x count=%u", e.error_type, m_uart_error_count);
    }, "Debug");
	if (STATE_EQUAL(poweron) || STATE_EQUAL(poweroff)) {
		if (++m_uart_error_count >= 10) {
			m_uart_error_count = 0;
			cancel_timeout();
			m_op_state = OpState::ERROR;
			run_state_machine();
		}
	}
}

void M8QAsyncReceiver::initiate_timeout(unsigned int timeout_ms) {
	cancel_timeout();
	m_timeout.handle = system_scheduler->post_task_prio([this]() {
		on_timeout();
	}, "Timeout", Scheduler::DEFAULT_PRIORITY, timeout_ms);
}

void M8QAsyncReceiver::process_timeout() {
	if (m_timeout.running && system_timer->get_counter() >= m_timeout.end) {
		m_timeout.running = false;
		on_timeout();
	}
}

void M8QAsyncReceiver::on_timeout() {
	if (m_op_state == OpState::PENDING) {
		m_op_state = OpState::TIMEOUT;
		m_ubx_comms.cancel_expect();
		state_machine();
	} else if (STATE_EQUAL(receive)) {
		// No NAV/SAT information received within requested receive timeout
		DEBUG_ERROR("M8QAsyncReceiver::on_timeout: no NAV/SAT info received");
		m_unrecoverable_error = true;
		notify<GPSEventError>({});
	}
}

void M8QAsyncReceiver::cancel_timeout() {
	system_scheduler->cancel_task(m_timeout.handle);
}

void M8QAsyncReceiver::state_poweron_enter() {
	m_step = 0;
	m_retries = 3;
	m_op_state = OpState::IDLE;
	m_uart_error_count = 0;
	m_fix_was_found = false;
	m_unrecoverable_error = false;
	exit_shutdown();
    notify<GPSEventPowerOn>({});
}

void M8QAsyncReceiver::state_poweron() {
	while (true) {
		if (m_op_state == OpState::IDLE) {
			m_op_state = OpState::PENDING;
			if (m_step == 0) {
				sync_baud_rate(9600);
				break;
			} else {
				DEBUG_ERROR("M8QAsyncReceiver: failed to sync comms");
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
			DEBUG_TRACE("M8QAsyncReceiver: baud rate framing error detected");
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

void M8QAsyncReceiver::state_poweron_exit() {
}

void M8QAsyncReceiver::state_poweroff_enter() {
	m_step = 0;
	m_retries = 3;
	m_op_state = OpState::IDLE;
	m_uart_error_count = 0;
}

void M8QAsyncReceiver::state_poweroff() {
	if (!m_powering_off) {
		STATE_CHANGE(poweroff, configure);
		return;
	}
	enter_shutdown();
    notify(GPSEventPowerOff(m_fix_was_found));
    STATE_CHANGE(poweroff, idle);
}

void M8QAsyncReceiver::state_poweroff_exit() {
}

void M8QAsyncReceiver::state_configure_enter() {
	m_step = 0;
	m_retries = 3;
	m_op_state = OpState::IDLE;
}

void M8QAsyncReceiver::state_configure() {
	while (true) {
		if (m_op_state == OpState::IDLE) {
			m_op_state = OpState::PENDING;
			if (m_step == 0) {
				setup_uart_port();
				m_step++;
				m_op_state = OpState::IDLE;
				run_state_machine(500); // Wait 500 ms for UART config to apply
				break;
			} else if (m_step == 1) {
				sync_baud_rate(460800);
				break;
			} else if (m_step == 2) {
				setup_gnss_channel_sharing();
				break;
			} else if (m_step == 3) {
				// Wait 500 ms for configuration to be applied
				m_step++;
				m_op_state = OpState::IDLE;
				run_state_machine(200);
				break;
			} else if (m_step == 4) {
				save_config();
				break;
			} else if (m_step == 5) {
				m_step++;
				m_op_state = OpState::IDLE;
				soft_reset(); // This operation has no response
			} else if (m_step == 6) {
				// Soft reset can take 1s to complete
				m_step++;
				m_op_state = OpState::IDLE;
				run_state_machine(1000);
				break;
			} else if (m_step == 7) {
				disable_odometer();
				break;
			} else if (m_step == 8) {
				disable_timepulse_output(0);
				break;
			} else if (m_step == 9) {
				disable_timepulse_output(1);
				break;
			} else if (m_step == 10) {
				setup_power_management();
				break;
			} else if (m_step == 11) {
				setup_continuous_mode();
				break;
			} else if (m_step == 12) {
				setup_simple_navigation_settings();
				break;
			} else if (m_step == 13) {
				setup_expert_navigation_settings();
				break;
			} else if (m_step == 14) {
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
				DEBUG_ERROR("M8QAsyncReceiver::state_configure: failed");
				m_unrecoverable_error = true;
				notify<GPSEventError>({});
				break;
			}
			m_op_state = OpState::IDLE;
		}
	}
}

void M8QAsyncReceiver::state_configure_exit() {
}

void M8QAsyncReceiver::state_startreceive_enter() {
	m_step = 0;
	m_retries = 3;
	m_op_state = OpState::IDLE;
}

void M8QAsyncReceiver::state_startreceive() {
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
				DEBUG_ERROR("M8QAsyncReceiver::state_start_receive: failed");
				m_unrecoverable_error = true;
				notify<GPSEventError>({});
				break;
			}
			m_op_state = OpState::IDLE;
		}
	}
}

void M8QAsyncReceiver::state_startreceive_exit() {
}

void M8QAsyncReceiver::state_receive_enter() {
	// Allow maximum 5 seconds for receiver to start outputting navigation samples
	initiate_timeout(5000);
}

void M8QAsyncReceiver::state_receive() {
}

void M8QAsyncReceiver::state_receive_exit() {
	cancel_timeout();
}

void M8QAsyncReceiver::state_stopreceive_enter() {
	m_step = 0;
	m_retries = 3;
	m_op_state = OpState::IDLE;
}

void M8QAsyncReceiver::state_stopreceive() {

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
                } else
                    m_step++;
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
				DEBUG_ERROR("M8QAsyncReceiver: stop receive failed");
				m_unrecoverable_error = true;
				notify<GPSEventError>({});
				break;
			}
			m_op_state = OpState::IDLE;
		}
	}
}

void M8QAsyncReceiver::state_stopreceive_exit() {
}

void M8QAsyncReceiver::state_fetchdatabase_enter() {
	m_step = 0;
	m_retries = 3;
	m_op_state = OpState::IDLE;
	m_ana_database_len = 0;
	m_expected_dbd_messages = 0;
	m_ubx_comms.start_dbd_filter();
}

void M8QAsyncReceiver::state_fetchdatabase() {
    if (!m_nav_settings.assistnow_autonomous_enable) {
        DEBUG_TRACE("M8QAsyncReceiver: fetchdatabase: ANA not enabled");
        STATE_CHANGE(fetchdatabase, poweroff);
        return;
    }
    if (m_ano_database_len) {
        DEBUG_TRACE("M8QAsyncReceiver: fetchdatabase: ANO in use, not fetching");
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
				DEBUG_TRACE("M8QAsyncReceiver::state_fetchdatabase: validating database size %u bytes expected %u msgs", m_ana_database_len, m_expected_dbd_messages);
				unsigned int actual_count = 0;
				if (!m_ubx_comms.is_expected_msg_count(m_navigation_database, m_ana_database_len,
						m_expected_dbd_messages, actual_count, MessageClass::MSG_CLASS_MGA, MGA::ID_DBD)) {
					if (--m_retries == 0) {
						DEBUG_WARN("M8QAsyncReceiver::state_fetch_database: failed: %u/%u msgs received",
								actual_count, m_expected_dbd_messages
								);
						dump_navigation_database(m_ana_database_len);
						m_ana_database_len = 0;
						STATE_CHANGE(fetchdatabase, poweroff);
						break;
					} else {
						DEBUG_TRACE("M8QAsyncReceiver::state_fetchdatabase: validation failed: %u/%u msgs received: retry",
								actual_count, m_expected_dbd_messages);
						m_ana_database_len = 0;
						m_expected_dbd_messages = 0;
						m_op_state = OpState::IDLE;
						m_step = 0;
						continue;
					}
				} else {
					DEBUG_TRACE("M8QAsyncReceiver::state_fetchdatabase: success");
					// Subtract the size of the MGA-ACK message from the database length
					// so it doesn't get sent out
					m_ana_database_len = m_ana_database_len - std::min(m_ana_database_len, msg_size<MGA::MSG_ACK>());
				}
				STATE_CHANGE(fetchdatabase, poweroff);
				break;
			}
		} else if (m_op_state == OpState::ERROR) {
			DEBUG_TRACE("M8QAsyncReceiver::state_fetchdatabase: UART comms error");
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

void M8QAsyncReceiver::state_fetchdatabase_exit() {
	m_ubx_comms.stop_dbd_filter();
}

void M8QAsyncReceiver::state_senddatabase_enter() {
	m_step = 0;
	m_op_state = OpState::IDLE;
	m_mga_ack_count = 0;
	m_ubx_comms.start_dbd_filter();
	m_retries = 10;
	DEBUG_TRACE("M8QAsyncReceiver::state_senddatabase: sending length %u bytes", m_ana_database_len);
}

void M8QAsyncReceiver::state_senddatabase() {
    if (!m_nav_settings.assistnow_autonomous_enable) {
        DEBUG_TRACE("M8QAsyncReceiver: senddatabase: ANA not enabled");
        STATE_CHANGE(senddatabase, startreceive);
        return;
    }
	while (true) {
		if (m_op_state == OpState::IDLE) {
			m_op_state = OpState::PENDING;
			if (m_step < m_ana_database_len) {
				// Send buffer contents in chunks raw and notify when sent
				unsigned int sz = std::min(64U, (m_ana_database_len - m_step));
				m_ubx_comms.send(&m_navigation_database[m_step], sz, true, true);
				m_step += sz;
				break;
			} else {
				unsigned int actual_count = 0;
				if (!m_ubx_comms.is_expected_msg_count(m_navigation_database, m_mga_ack_count,
						m_expected_dbd_messages, actual_count, MessageClass::MSG_CLASS_MGA,
						MGA::ID_ACK)) {
					if (--m_retries == 0) {
						DEBUG_WARN("M8QAsyncReceiver::state_senddatabase: missing MGA-ACK: %u/%u acks recieved",
								actual_count, m_expected_dbd_messages);
						STATE_CHANGE(senddatabase, startreceive);
						break;
					} else {
						DEBUG_TRACE("M8QAsyncReceiver::state_senddatabase: MGA-ACK: %u/%u acks recieved",
								actual_count, m_expected_dbd_messages);
						m_op_state = OpState::IDLE;
						run_state_machine(10); // Poll again in 10 ms
						break;
					}
				} else {
					DEBUG_TRACE("M8QAsyncReceiver::state_senddatabase: success: %u/%u acks recieved",
							actual_count, m_expected_dbd_messages);
					STATE_CHANGE(senddatabase, startreceive);
				}
				break;
			}
		} else if (m_op_state == OpState::SUCCESS) {
			// Skip to next record
			m_op_state = OpState::IDLE;
			run_state_machine(1);  // Require 1 ms delay between transmitted data
			break;
		} else if (m_op_state == OpState::PENDING) {
			break;
		} else {
			DEBUG_WARN("M8QAsyncReceiver::state_send_database: failed");
			STATE_CHANGE(senddatabase, startreceive);
			break;
		}
	}
}

void M8QAsyncReceiver::state_senddatabase_exit() {
	m_ubx_comms.stop_dbd_filter();
}

void M8QAsyncReceiver::state_sendofflinedatabase_enter() {

    m_op_state = OpState::IDLE;
    m_step = 0;
    m_mga_ack_count = 0;
    m_ano_database_len = 0;

    if (!m_nav_settings.assistnow_offline_enable) {
        DEBUG_TRACE("M8QAsyncReceiver: sendofflinedatabase: ANO not enabled");
        return;
    }

    if (!rtc->is_set()) {
        DEBUG_TRACE("M8QAsyncReceiver: sendofflinedatabase: time not yet set");
        return;
    }

    try {
        LFSFile file(main_filesystem, "gps_config.dat", LFS_O_RDONLY);
        m_ubx_comms.copy_mga_ano_to_buffer(file, m_navigation_database, sizeof(m_navigation_database),
                                           rtc->gettime(),
                                           m_ano_database_len, m_expected_dbd_messages, m_ano_start_pos);
        m_ubx_comms.start_dbd_filter();
        DEBUG_TRACE("M8QAsyncReceiver::state_sendofflinedatabase: database len %u bytes",
                    m_ano_database_len);
    } catch (...) {
        DEBUG_ERROR("M8QAsyncReceiver::state_sendofflinedatabase: error opening MGA ANO file");
        m_op_state = OpState::ERROR;
    }
}

void M8QAsyncReceiver::state_sendofflinedatabase() {
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
					DEBUG_WARN("M8QAsyncReceiver::state_sendofflinedatabase missing MGA-ACK: %u/%u acks received",
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
			DEBUG_WARN("M8QAsyncReceiver::state_sendofflinedatabase: failed");
			STATE_CHANGE(sendofflinedatabase, startreceive);
			break;
		}
	}
}

void M8QAsyncReceiver::state_sendofflinedatabase_exit() {
	m_ubx_comms.stop_dbd_filter();
}

void M8QAsyncReceiver::state_idle_enter() {
	m_nav_settings.max_nav_samples = 0;
	m_nav_settings.max_sat_samples = 0;
}

void M8QAsyncReceiver::state_idle() {
}

void M8QAsyncReceiver::state_idle_exit() {
}

void M8QAsyncReceiver::sync_baud_rate(unsigned int baud) {
	DEBUG_TRACE("M8QAsyncReceiver::sync_baud_rate: baud=%u", baud);
	m_ubx_comms.set_baudrate(baud);

	CFG::MSG::MSG_MSG_NORATE cfg_msg_invalid =
    {
        .msgClass = MessageClass::MSG_CLASS_BAD,
        .msgID = 0,
    };

	DEBUG_TRACE("M8QAsyncReceiver::sync_baud_rate: GPS CFG-MSG[bad] ->");

	initiate_timeout(500);
	m_ubx_comms.send_packet_with_expect(MessageClass::MSG_CLASS_CFG, CFG::ID_MSG, cfg_msg_invalid,
			MessageClass::MSG_CLASS_ACK, ACK::ID_NACK);
}

void M8QAsyncReceiver::save_config() {
    DEBUG_TRACE("M8QAsyncReceiver::save_config: GPS CFG-CFG ->");
    CFG::CFG::MSG_CFG cfg_msg_cfg_cfg =
    {
        .clearMask   = 0,
        .saveMask    = CFG::CFG::CLEARMASK_IOPORT   |
					   CFG::CFG::CLEARMASK_MSGCONF  |
					   CFG::CFG::CLEARMASK_INFMSG   |
					   CFG::CFG::CLEARMASK_NAVCONF  |
					   CFG::CFG::CLEARMASK_RXMCONF  |
					   CFG::CFG::CLEARMASK_SENCONF  |
					   CFG::CFG::CLEARMASK_RINVCONF |
					   CFG::CFG::CLEARMASK_ANTCONF  |
					   CFG::CFG::CLEARMASK_LOGCONF  |
					   CFG::CFG::CLEARMASK_FTSCONF,
        .loadMask    = 0,
        .deviceMask  = CFG::CFG::DEVMASK_BBR,
    };

    initiate_timeout();
    m_ubx_comms.send_packet_with_expect(MessageClass::MSG_CLASS_CFG, CFG::ID_CFG, cfg_msg_cfg_cfg);
}

void M8QAsyncReceiver::soft_reset() {
    DEBUG_TRACE("M8QAsyncReceiver::soft_reset: GPS CFG-RST->");
    CFG::RST::MSG_RST cfg_msg_cfg_rst =
    {
        .navBbrMask = 0x0000,
        .resetMode = UBX::CFG::RST::RESETMODE_HARDWARE_RESET_IMMEDIATE,
        .reserved1 = 0,
    };
    m_ubx_comms.send_packet(MessageClass::MSG_CLASS_CFG, CFG::ID_RST, cfg_msg_cfg_rst);
	m_ubx_comms.wait_send();
}

void M8QAsyncReceiver::setup_uart_port() {
    DEBUG_TRACE("M8QAsyncReceiver::setup_uart_port: GPS CFG-PRT ->");
    CFG::PRT::MSG_UART uart_prt =
    {
        .portID = CFG::PRT::PORTID_UART,
        .reserved1 = 0,
        .txReady = 0,
        .mode = CFG::PRT::MODE_CHARLEN_8BIT | CFG::PRT::MODE_PARITY_NO | CFG::PRT::MODE_STOP_BITS_1,
        .baudRate = 460800,
        .inProtoMask = CFG::PRT::PROTOMASK_UBX,
        .outProtoMask = CFG::PRT::PROTOMASK_UBX,
        .flags = 0,
        .reserved2 = {0}
    };
    m_ubx_comms.send_packet(MessageClass::MSG_CLASS_CFG, CFG::ID_PRT, uart_prt);
	m_ubx_comms.wait_send();
}

void M8QAsyncReceiver::setup_power_management() {
    DEBUG_TRACE("M8QAsyncReceiver::setup_power_management: GPS CFG-PM2 ->");
    CFG::PM2::MSG_PM2 cfg_msg_cfg_pm2 =
    {
        .version = 0x02,
        .reserved1 = 0,
        .maxStartupStateDur = 0,
        .reserved2 = 0,
        .flags = CFG::PM2::FLAGS_DONOTENTEROFF | CFG::PM2::FLAGS_UPDATEEPH | CFG::PM2::FLAGS_MODE_CYCLIC | CFG::PM2::FLAGS_LIMITPEAKCURR_DISABLED,
        .updatePeriod = 1000,
        .searchPeriod = 10000,
        .gridOffset = 0,
        .onTime = 0,
        .minAcqTime = 300,
        .reserved3 = {0},
        .extintInactivityMs = 0
    };

#if 1 == NO_GPS_POWER_REG
	// Configure EXT_INT pin for power management
    cfg_msg_cfg_pm2.flags |= CFG::PM2::FLAGS_EXTINTWAKE | CFG::PM2::FLAGS_EXTINTBACKUP;
#endif

    initiate_timeout();
    m_ubx_comms.send_packet_with_expect(MessageClass::MSG_CLASS_CFG, CFG::ID_PM2, cfg_msg_cfg_pm2);
}

void M8QAsyncReceiver::setup_continuous_mode() {
    DEBUG_TRACE("M8QAsyncReceiver::setup_continuous_mode: GPS CFG-RXM ->");
    CFG::RXM::MSG_RXM cfg_msg_cfg_rxm =
    {
        .reserved1 = 0,
        .lpMode = CFG::RXM::CONTINUOUS_MODE
    };

    initiate_timeout();
    m_ubx_comms.send_packet_with_expect(MessageClass::MSG_CLASS_CFG, CFG::ID_RXM, cfg_msg_cfg_rxm);
}

void M8QAsyncReceiver::setup_simple_navigation_settings() {
    DEBUG_TRACE("M8QAsyncReceiver::setup_simple_navigation_settings: GPS CFG-NAV5 ->");
    CFG::NAV5::MSG_NAV5 cfg_msg_cfg_nav5 =
    {
        .mask = CFG::NAV5::MASK_DYN | CFG::NAV5::MASK_MIN_EL | CFG::NAV5::MASK_POS_FIX_MODE | CFG::NAV5::MASK_POS_MASK | CFG::NAV5::MASK_TIME_MASK | CFG::NAV5::MASK_STATIC_HOLD_MASK | CFG::NAV5::MASK_DGPS_MASK | CFG::NAV5::MASK_CNO_THRESHOLD | CFG::NAV5::MASK_UTC,
        .dynModel = (CFG::NAV5::DynModel)m_nav_settings.dyn_model,
        .fixMode = (CFG::NAV5::FixMode)m_nav_settings.fix_mode,
        .fixedAlt = 0,
        .fixedAltVar = 10000,
        .minElev = 5,
        .drLimit = 0,
        .pDop = 250,
        .tDop = 250,
        .pAcc = 100,
        .tAcc = 350,
        .staticHoldThresh = 0,
        .dgnssTimeOut = 60,
        .cnoThreshNumSVs = 0,
        .cnoThresh = 0,
        .reserved1 = {0},
        .staticHoldMaxDist = 0,
        .utcStandard = 0,
        .reserved2 = {0}
    };

    initiate_timeout();
    m_ubx_comms.send_packet_with_expect(MessageClass::MSG_CLASS_CFG, CFG::ID_NAV5, cfg_msg_cfg_nav5);
}

void M8QAsyncReceiver::setup_expert_navigation_settings() {
    DEBUG_TRACE("M8QAsyncReceiver::setup_expert_navigation_settings: GPS CFG-NAVX5 ->");
    CFG::NAVX5::MSG_NAVX5 cfg_msg_cfg_navx5 =
    {
        .version = 0x0002,
        .mask1 = CFG::NAVX5::MASK1_MIN_MAX | CFG::NAVX5::MASK1_MIN_CNO | CFG::NAVX5::MASK1_INITIAL_3D_FIX | CFG::NAVX5::MASK1_WKN_ROLL | CFG::NAVX5::MASK1_ACK_AID | CFG::NAVX5::MASK1_PPP | CFG::NAVX5::MASK1_AOP,
        .mask2 = 0,
        .reserved1 = {0},
        .minSVs = 3,
        .maxSVs = 32,
        .minCNO = 6,
        .reserved2 = 0,
        .iniFix3D = 0,
        .reserved3 = {0},
        .ackAiding = 1,
        .wknRollover = 0, // Use firmware default
        .sigAttenCompMode = 0,
        .reserved4 = 0,
        .reserved5 = {0},
        .reserved6 = {0},
        .usePPP = 0,
        .aopCfg = m_nav_settings.assistnow_autonomous_enable,
        .reserved7 = {0},
        .aopOrbMaxErr = 100,
        .reserved8 = {0},
        .reserved9 = {0},
        .useAdr = 0
    };

    initiate_timeout();
    m_ubx_comms.send_packet_with_expect(MessageClass::MSG_CLASS_CFG, CFG::ID_NAVX5, cfg_msg_cfg_navx5);
}

void M8QAsyncReceiver::supply_time_assistance() {
    DEBUG_TRACE("M8QAsyncReceiver::supply_time_assistance: GPS MGA-INI-TIME-UTC ->");
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

void M8QAsyncReceiver::disable_odometer() {
    DEBUG_TRACE("M8QAsyncReceiver::disable_odometer: GPS CFG-ODO ->");
    CFG::ODO::MSG_ODO cfg_msg_cfg_odo =
    {
        .version = 0x00,
        .reserved1 = {0},
        .flags = 0,
        .odoCfg = 0,
        .reserved2 = {0},
        .cogMaxSpeed = 1,
        .cogMaxPosAcc = 50,
        .reserved3 = {0},
        .velLpGain = 153,
        .cogLpGain = 76,
        .reserved4 = {0}
    };

    initiate_timeout();
    m_ubx_comms.send_packet_with_expect(MessageClass::MSG_CLASS_CFG, CFG::ID_ODO, cfg_msg_cfg_odo);
}

void M8QAsyncReceiver::disable_timepulse_output(unsigned int idx) {
    DEBUG_TRACE("M8QAsyncReceiver::disable_timepulse_output: GPS CFG-TP5[%u] ->", idx);
    CFG::TP5::MSG_TP5 cfg_msg_cfg_tp5 =
    {
        .tpIdx = (uint8_t)idx,
        .version = 0x01,
        .reserved1 = {0},
        .antCableDelay = 0,
        .rfGroupDelay = 0,
        .freqPeriod = 1000000,
        .freqPeriodLock = 1000000,
        .pulseLenRatio = 0,
        .pulseLenRatioLock = 100000,
        .userConfigDelay = 0,
        .flags = 0
    };

    initiate_timeout();
    m_ubx_comms.send_packet_with_expect(MessageClass::MSG_CLASS_CFG, CFG::ID_TP5, cfg_msg_cfg_tp5);
}

void M8QAsyncReceiver::enable_nav_pvt_message()
{
    DEBUG_TRACE("M8QAsyncReceiver::enable_nav_pvt_message: GPS CFG-MSG ->");
    CFG::MSG::MSG_MSG cfg_msg_nav_pvt =
    {
        .msgClass = MessageClass::MSG_CLASS_NAV,
        .msgID = NAV::ID_PVT,
        .rate = 1
    };

    initiate_timeout();
    m_ubx_comms.send_packet_with_expect(MessageClass::MSG_CLASS_CFG, CFG::ID_MSG, cfg_msg_nav_pvt);
}

void M8QAsyncReceiver::enable_nav_dop_message()
{
    DEBUG_TRACE("M8QAsyncReceiver::enable_nav_dop_message: GPS CFG-MSG ->");
    CFG::MSG::MSG_MSG cfg_msg_nav_dop =
    {
        .msgClass = MessageClass::MSG_CLASS_NAV,
        .msgID = NAV::ID_DOP,
        .rate = 1
    };

    initiate_timeout();
    m_ubx_comms.send_packet_with_expect(MessageClass::MSG_CLASS_CFG, CFG::ID_MSG, cfg_msg_nav_dop);
}

void M8QAsyncReceiver::enable_nav_status_message()
{
    DEBUG_TRACE("M8QAsyncReceiver::enable_nav_status_message: GPS CFG-MSG ->");
    CFG::MSG::MSG_MSG cfg_msg_nav_status =
    {
        .msgClass = MessageClass::MSG_CLASS_NAV,
        .msgID = NAV::ID_STATUS,
        .rate = 1
    };

    initiate_timeout();
    m_ubx_comms.send_packet_with_expect(MessageClass::MSG_CLASS_CFG, CFG::ID_MSG, cfg_msg_nav_status);
}

void M8QAsyncReceiver::enable_nav_sat_message()
{
    DEBUG_TRACE("M8QAsyncReceiver::enable_nav_sat_message: GPS CFG-MSG ->");
    CFG::MSG::MSG_MSG cfg_msg_nav_sat =
    {
        .msgClass = MessageClass::MSG_CLASS_NAV,
        .msgID = NAV::ID_SAT,
        .rate = 1
    };

    initiate_timeout();
    m_ubx_comms.send_packet_with_expect(MessageClass::MSG_CLASS_CFG, CFG::ID_MSG, cfg_msg_nav_sat);
}

void M8QAsyncReceiver::disable_nav_pvt_message()
{
    DEBUG_TRACE("M8QAsyncReceiver::disable_nav_pvt_message: GPS CFG-MSG ->");
    CFG::MSG::MSG_MSG cfg_msg_nav_pvt =
    {
        .msgClass = MessageClass::MSG_CLASS_NAV,
        .msgID = NAV::ID_PVT,
        .rate = 0
    };

    initiate_timeout();
    m_ubx_comms.send_packet_with_expect(MessageClass::MSG_CLASS_CFG, CFG::ID_MSG, cfg_msg_nav_pvt);
}

void M8QAsyncReceiver::disable_nav_dop_message()
{
    DEBUG_TRACE("M8QAsyncReceiver::disable_nav_dop_message: GPS CFG-MSG ->");
    CFG::MSG::MSG_MSG cfg_msg_nav_dop =
    {
        .msgClass = MessageClass::MSG_CLASS_NAV,
        .msgID = NAV::ID_DOP,
        .rate = 0
    };

    initiate_timeout();
    m_ubx_comms.send_packet_with_expect(MessageClass::MSG_CLASS_CFG, CFG::ID_MSG, cfg_msg_nav_dop);
}

void M8QAsyncReceiver::disable_nav_status_message()
{
    DEBUG_TRACE("M8QAsyncReceiver::disable_nav_status_message: GPS CFG-MSG ->");
    CFG::MSG::MSG_MSG cfg_msg_nav_status =
    {
        .msgClass = MessageClass::MSG_CLASS_NAV,
        .msgID = NAV::ID_STATUS,
        .rate = 0
    };

    initiate_timeout();
    m_ubx_comms.send_packet_with_expect(MessageClass::MSG_CLASS_CFG, CFG::ID_MSG, cfg_msg_nav_status);
}

void M8QAsyncReceiver::disable_nav_sat_message()
{
    DEBUG_TRACE("M8QAsyncReceiver::disable_nav_sat_message: GPS CFG-MSG ->");
    CFG::MSG::MSG_MSG cfg_msg_nav_sat =
    {
        .msgClass = MessageClass::MSG_CLASS_NAV,
        .msgID = NAV::ID_SAT,
        .rate = 0
    };

    initiate_timeout();
    m_ubx_comms.send_packet_with_expect(MessageClass::MSG_CLASS_CFG, CFG::ID_MSG, cfg_msg_nav_sat);
}

void M8QAsyncReceiver::setup_gnss_channel_sharing() {
    DEBUG_TRACE("M8QAsyncReceiver::setup_gnss_channel_sharing: GPS CFG-GNSS ->");
    CFG::GNSS::MSG_GNSS cfg_msg_cfg_gnss =
    {
        .version = 0x00,
        .numTrkChHw = 0,
        .numTrkChUse = 0xFF, // Use max number of tracking channels
        .numConfigBlocks = 7,
        .data =
        {
            {
                .gnssId = CFG::GNSS::GNSSID_GPS,
                .resTrkCh = 8,
                .maxTrkCh = 16,
                .reserved1 = 0,
                .flags = CFG::GNSS::FLAGS_GPS_L1CA | CFG::GNSS::FLAGS_ENABLE
            },
            {
                .gnssId = CFG::GNSS::GNSSID_SBAS,
                .resTrkCh = 1,
                .maxTrkCh = 3,
                .reserved1 = 0,
                .flags = CFG::GNSS::FLAGS_SBAS_L1CA
            },
            {
                .gnssId = CFG::GNSS::GNSSID_GALILEO,
                .resTrkCh = 4,
                .maxTrkCh = 8,
                .reserved1 = 0,
                .flags = CFG::GNSS::FLAGS_GALILEO_E1 | CFG::GNSS::FLAGS_ENABLE
            },
            {
                .gnssId = CFG::GNSS::GNSSID_BEIDOU,
                .resTrkCh = 8,
                .maxTrkCh = 16,
                .reserved1 = 0,
                .flags = CFG::GNSS::FLAGS_BEIDOU_B1I
            },
            {
                .gnssId = CFG::GNSS::GNSSID_IMES,
                .resTrkCh = 0,
                .maxTrkCh = 8,
                .reserved1 = 0,
                .flags = CFG::GNSS::FLAGS_IMES_L1
            },
            {
                .gnssId = CFG::GNSS::GNSSID_QZSS,
                .resTrkCh = 0,
                .maxTrkCh = 3,
                .reserved1 = 0,
                // To avoid cross-correlation issues, it is recommended that GPS and QZSS are always both enabled or both disabled
                .flags = CFG::GNSS::FLAGS_QZSS_L1CA | CFG::GNSS::FLAGS_ENABLE
            },
            {
                .gnssId = CFG::GNSS::GNSSID_GLONASS,
                .resTrkCh = 8,
                .maxTrkCh = 14,
                .reserved1 = 0,
                .flags = CFG::GNSS::FLAGS_GLONASS_L1 | CFG::GNSS::FLAGS_ENABLE
            }
        }
    };

    initiate_timeout();
    m_ubx_comms.send_packet_with_expect(MessageClass::MSG_CLASS_CFG, CFG::ID_GNSS, cfg_msg_cfg_gnss);
}

void M8QAsyncReceiver::fetch_navigation_database() {
	UBX::Empty msg_dbd = {};
    initiate_timeout();
    m_ubx_comms.send_packet(MessageClass::MSG_CLASS_MGA, MGA::ID_DBD, msg_dbd);
}

void M8QAsyncReceiver::send_offline_database() {
}

void M8QAsyncReceiver::dump_navigation_database(unsigned int len) {
	for (unsigned int i = 0; i < len; i++)
		printf("%02X", m_navigation_database[i]);
	printf("\n");
}
