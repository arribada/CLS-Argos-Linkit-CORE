#include "config_store_fs.hpp"

#include "dte_protocol.hpp"

#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"


#define BLOCK_COUNT   (256)
#define BLOCK_SIZE    (64*1024)
#define PAGE_SIZE     (256)
#define MAX_FILE_SIZE (4*1024*1024)


extern FileSystem *main_filesystem;
static LFSRamFileSystem *ram_filesystem;

using namespace std::literals::string_literals;



TEST_GROUP(ConfigStore)
{
	void setup() {
		ram_filesystem = new LFSRamFileSystem(BLOCK_COUNT, BLOCK_SIZE, PAGE_SIZE);
		ram_filesystem->format();
		ram_filesystem->mount();
		main_filesystem = ram_filesystem;
	}

	void teardown() {
		ram_filesystem->umount();
		delete ram_filesystem;
	}
};


TEST(ConfigStore, CreateConfigStoreWithDefaultParams)
{
	LFSConfigurationStore *store;
	store = new LFSConfigurationStore(*main_filesystem);

	store->init();
	CHECK_TRUE(store->is_valid());

	// Check some defaults are correct
	CHECK_EQUAL(0U, store->read_param<unsigned int>(ParamID::ARGOS_DECID));
	CHECK_EQUAL("GenTracker"s, store->read_param<std::string>(ParamID::DEVICE_MODEL));

	delete store;
}

TEST(ConfigStore, CheckBaseTypeReadAccess)
{
	LFSConfigurationStore *store;
	store = new LFSConfigurationStore(*main_filesystem);
	store->init();

	// Modify some parameter values
	std::string model = "GenTracker";
	store->write_param(ParamID::DEVICE_MODEL, model);

	BaseType x = store->read_param<BaseType>(ParamID::DEVICE_MODEL);
	CHECK_EQUAL(model, std::get<std::string>(x));


	delete store;
}

TEST(ConfigStore, CheckConfigStorePersistence)
{
	LFSConfigurationStore *store;
	store = new LFSConfigurationStore(*main_filesystem);

	store->init();

	// Modify some parameter values
	std::string model = "GenTracker";
	unsigned int dec_id = 1234U;
	store->write_param(ParamID::ARGOS_DECID, dec_id);
	store->write_param(ParamID::DEVICE_MODEL, model);

	// Delete the object and recreate a new one
	delete store;
	store = new LFSConfigurationStore(*main_filesystem);
	store->init();  // This will read in the saved file

	// Check modified parameters
	CHECK_EQUAL(1234U, store->read_param<unsigned int>(ParamID::ARGOS_DECID));
	CHECK_EQUAL(model, store->read_param<std::string>(ParamID::DEVICE_MODEL));

	delete store;
}

TEST(ConfigStore, CheckDefaultZoneFile)
{
	LFSConfigurationStore *store;
	store = new LFSConfigurationStore(*main_filesystem);

	store->init();

	BaseZone& default_zone = store->read_zone();
	CHECK_EQUAL(1, default_zone.zone_id);
	CHECK_FALSE(default_zone.enable_monitoring);

	delete store;
}

TEST(ConfigStore, CheckDefaultPassPredictNotExist)
{
	LFSConfigurationStore *store;
	store = new LFSConfigurationStore(*main_filesystem);

	store->init();

	CHECK_THROWS(ErrorCode, store->read_pass_predict());

	delete store;
}

TEST(ConfigStore, CheckZoneCreationAndPersistence)
{
	LFSConfigurationStore *store;
	store = new LFSConfigurationStore(*main_filesystem);

	store->init();

	{
		BaseZone zone;
		zone.zone_id = 1;
		store->write_zone(zone);
	}

	delete store;
	store = new LFSConfigurationStore(*main_filesystem);

	{
		store->init();
		BaseZone &zone = store->read_zone();
		CHECK_EQUAL(1, zone.zone_id);
	}

	delete store;
}

TEST(ConfigStore, CheckPassPredictCreationAndPersistence)
{
	LFSConfigurationStore *store;
	store = new LFSConfigurationStore(*main_filesystem);

	store->init();

	{
		BasePassPredict pp;
		pp.num_records = 10;
		store->write_pass_predict(pp);
	}

	delete store;
	store = new LFSConfigurationStore(*main_filesystem);

	{
		store->init();
		BasePassPredict &pp = store->read_pass_predict();
		CHECK_EQUAL(10, pp.num_records);
	}

	delete store;
}

TEST(ConfigStore, PARAM_ARGOS_DECID)
{
	LFSConfigurationStore *store;
	store = new LFSConfigurationStore(*main_filesystem);
	store->init();

	unsigned int dec_id = 1234U;
	store->write_param(ParamID::ARGOS_DECID, dec_id);
	CHECK_EQUAL(dec_id, store->read_param<unsigned int>(ParamID::ARGOS_DECID));
	delete store;
}

TEST(ConfigStore, PARAM_ARGOS_HEXID)
{
	LFSConfigurationStore *store;
	store = new LFSConfigurationStore(*main_filesystem);
	store->init();

	unsigned int hex_id = 1234U;
	store->write_param(ParamID::ARGOS_HEXID, hex_id);
	CHECK_EQUAL(hex_id, store->read_param<unsigned int>(ParamID::ARGOS_HEXID));
	delete store;
}

TEST(ConfigStore, PARAM_FW_APP_VERSION)
{
	LFSConfigurationStore *store;
	store = new LFSConfigurationStore(*main_filesystem);
	store->init();

	std::string s = "New Firmware App Version";
	store->write_param(ParamID::FW_APP_VERSION, s);
	CHECK_EQUAL(s, store->read_param<std::string>(ParamID::FW_APP_VERSION));
	delete store;
}

TEST(ConfigStore, PARAM_LAST_TX)
{
	LFSConfigurationStore *store;
	store = new LFSConfigurationStore(*main_filesystem);
	store->init();

	std::time_t t = 1234U;
	store->write_param(ParamID::LAST_TX, t);
	CHECK_EQUAL(t, store->read_param<std::time_t>(ParamID::LAST_TX));
	delete store;
}

TEST(ConfigStore, PARAM_TX_COUNTER)
{
	LFSConfigurationStore *store;
	store = new LFSConfigurationStore(*main_filesystem);
	store->init();

	unsigned int t = 64U;
	store->write_param(ParamID::TX_COUNTER, t);
	CHECK_EQUAL(t, store->read_param<unsigned int>(ParamID::TX_COUNTER));
	delete store;
}


TEST(ConfigStore, PARAM_BATT_SOC)
{
	LFSConfigurationStore *store;
	store = new LFSConfigurationStore(*main_filesystem);
	store->init();

	unsigned int t = 50U;
	store->write_param(ParamID::BATT_SOC, t);
	CHECK_EQUAL(t, store->read_param<unsigned int>(ParamID::BATT_SOC));
	delete store;
}

TEST(ConfigStore, PARAM_LAST_FULL_CHARGE_DATE)
{
	LFSConfigurationStore *store;
	store = new LFSConfigurationStore(*main_filesystem);
	store->init();

	std::time_t t = 123224U;
	store->write_param(ParamID::LAST_FULL_CHARGE_DATE, t);
	CHECK_EQUAL(t, store->read_param<std::time_t>(ParamID::LAST_FULL_CHARGE_DATE));
	delete store;
}

TEST(ConfigStore, PARAM_PROFILE_NAME)
{
	LFSConfigurationStore *store;
	store = new LFSConfigurationStore(*main_filesystem);
	store->init();

	std::string s = "New Profile Name";
	store->write_param(ParamID::PROFILE_NAME, s);
	CHECK_EQUAL(s, store->read_param<std::string>(ParamID::PROFILE_NAME));
	delete store;
}

TEST(ConfigStore, PARAM_ARGOS_AOP_DATE)
{
	LFSConfigurationStore *store;
	store = new LFSConfigurationStore(*main_filesystem);
	store->init();

	std::time_t t = 123224U;
	store->write_param(ParamID::ARGOS_AOP_DATE, t);
	CHECK_EQUAL(t, store->read_param<std::time_t>(ParamID::ARGOS_AOP_DATE));
	delete store;
}

TEST(ConfigStore, PARAM_ARGOS_FREQ)
{
	LFSConfigurationStore *store;
	store = new LFSConfigurationStore(*main_filesystem);
	store->init();

	double t = 401.68;
	store->write_param(ParamID::ARGOS_FREQ, t);
	CHECK_EQUAL(t, store->read_param<double>(ParamID::ARGOS_FREQ));

	delete store;
	store = new LFSConfigurationStore(*main_filesystem);
	store->init();

	CHECK_EQUAL(t, store->read_param<double>(ParamID::ARGOS_FREQ));
	delete store;
}

TEST(ConfigStore, PARAM_ARGOS_POWER)
{
	LFSConfigurationStore *store;
	store = new LFSConfigurationStore(*main_filesystem);
	store->init();

	BaseArgosPower t = BaseArgosPower::POWER_200_MW;
	store->write_param(ParamID::ARGOS_POWER, t);
	CHECK_TRUE(t == store->read_param<BaseArgosPower>(ParamID::ARGOS_POWER));

	delete store;
	store = new LFSConfigurationStore(*main_filesystem);
	store->init();

	CHECK_TRUE(t == store->read_param<BaseArgosPower>(ParamID::ARGOS_POWER));
	delete store;
}

TEST(ConfigStore, PARAM_TR_NOM)
{
	LFSConfigurationStore *store;
	store = new LFSConfigurationStore(*main_filesystem);
	store->init();

	unsigned int t = 45;
	store->write_param(ParamID::TR_NOM, t);
	CHECK_EQUAL(t, store->read_param<unsigned int>(ParamID::TR_NOM));

	delete store;
	store = new LFSConfigurationStore(*main_filesystem);
	store->init();

	CHECK_EQUAL(t, store->read_param<unsigned int>(ParamID::TR_NOM));
	delete store;
}

TEST(ConfigStore, PARAM_ARGOS_MODE)
{
	LFSConfigurationStore *store;
	store = new LFSConfigurationStore(*main_filesystem);
	store->init();

	BaseArgosMode t = BaseArgosMode::DUTY_CYCLE;
	store->write_param(ParamID::ARGOS_MODE, t);
	CHECK_TRUE(t == store->read_param<BaseArgosMode>(ParamID::ARGOS_MODE));

	delete store;
	store = new LFSConfigurationStore(*main_filesystem);
	store->init();

	CHECK_TRUE(t == store->read_param<BaseArgosMode>(ParamID::ARGOS_MODE));
	delete store;
}

TEST(ConfigStore, PARAM_NTRY_PER_MESSAGE)
{
	LFSConfigurationStore *store;
	store = new LFSConfigurationStore(*main_filesystem);
	store->init();

	unsigned int t = 3;
	store->write_param(ParamID::NTRY_PER_MESSAGE, t);
	CHECK_EQUAL(t, store->read_param<unsigned int>(ParamID::NTRY_PER_MESSAGE));

	delete store;
	store = new LFSConfigurationStore(*main_filesystem);
	store->init();

	CHECK_EQUAL(t, store->read_param<unsigned int>(ParamID::NTRY_PER_MESSAGE));
	delete store;
}

TEST(ConfigStore, PARAM_DUTY_CYCLE)
{
	LFSConfigurationStore *store;
	store = new LFSConfigurationStore(*main_filesystem);
	store->init();

	unsigned int t = 0b101010101010101010101010;
	store->write_param(ParamID::DUTY_CYCLE, t);
	CHECK_EQUAL(t, store->read_param<unsigned int>(ParamID::DUTY_CYCLE));

	delete store;
	store = new LFSConfigurationStore(*main_filesystem);
	store->init();

	CHECK_EQUAL(t, store->read_param<unsigned int>(ParamID::DUTY_CYCLE));
	delete store;
}

TEST(ConfigStore, PARAM_GNSS_EN)
{
	LFSConfigurationStore *store;
	store = new LFSConfigurationStore(*main_filesystem);
	store->init();

	bool t = true;
	store->write_param(ParamID::GNSS_EN, t);
	CHECK_EQUAL(t, store->read_param<bool>(ParamID::GNSS_EN));

	delete store;
	store = new LFSConfigurationStore(*main_filesystem);
	store->init();

	CHECK_EQUAL(t, store->read_param<bool>(ParamID::GNSS_EN));
	delete store;
}

TEST(ConfigStore, PARAM_DLOC_ARG_NOM)
{
	LFSConfigurationStore *store;
	store = new LFSConfigurationStore(*main_filesystem);
	store->init();

	unsigned int t = 10U;
	store->write_param(ParamID::DLOC_ARG_NOM, t);
	CHECK_EQUAL(t, store->read_param<unsigned int >(ParamID::DLOC_ARG_NOM));

	delete store;
	store = new LFSConfigurationStore(*main_filesystem);
	store->init();

	CHECK_EQUAL(t, store->read_param<unsigned int >(ParamID::DLOC_ARG_NOM));
	delete store;
}

TEST(ConfigStore, PARAM_ARGOS_DEPTH_PILE)
{
	LFSConfigurationStore *store;
	store = new LFSConfigurationStore(*main_filesystem);
	store->init();

	BaseArgosDepthPile t = BaseArgosDepthPile::DEPTH_PILE_12;
	store->write_param(ParamID::ARGOS_DEPTH_PILE, t);
	CHECK_TRUE(t == store->read_param<BaseArgosDepthPile>(ParamID::ARGOS_DEPTH_PILE));

	delete store;
	store = new LFSConfigurationStore(*main_filesystem);
	store->init();

	CHECK_TRUE(t == store->read_param<BaseArgosDepthPile>(ParamID::ARGOS_DEPTH_PILE));
	delete store;
}

TEST(ConfigStore, PARAM_GNSS_HDOPFILT_EN)
{
	LFSConfigurationStore *store;
	store = new LFSConfigurationStore(*main_filesystem);
	store->init();

	bool t = true;
	store->write_param(ParamID::GNSS_HDOPFILT_EN, t);
	CHECK_EQUAL(t, store->read_param<bool>(ParamID::GNSS_HDOPFILT_EN));

	delete store;
	store = new LFSConfigurationStore(*main_filesystem);
	store->init();

	CHECK_EQUAL(t, store->read_param<bool>(ParamID::GNSS_HDOPFILT_EN));
	delete store;
}

TEST(ConfigStore, PARAM_GNSS_HDOPFILT_THR)
{
	LFSConfigurationStore *store;
	store = new LFSConfigurationStore(*main_filesystem);
	store->init();

	unsigned int t = 10U;
	store->write_param(ParamID::GNSS_HDOPFILT_THR, t);
	CHECK_EQUAL(t, store->read_param<unsigned int>(ParamID::GNSS_HDOPFILT_THR));

	delete store;
	store = new LFSConfigurationStore(*main_filesystem);
	store->init();

	CHECK_EQUAL(t, store->read_param<unsigned int>(ParamID::GNSS_HDOPFILT_THR));
	delete store;
}

TEST(ConfigStore, PARAM_GNSS_ACQ_TIMEOUT)
{
	LFSConfigurationStore *store;
	store = new LFSConfigurationStore(*main_filesystem);
	store->init();

	unsigned int t = 30U;
	store->write_param(ParamID::GNSS_ACQ_TIMEOUT, t);
	CHECK_EQUAL(t, store->read_param<unsigned int>(ParamID::GNSS_ACQ_TIMEOUT));

	delete store;
	store = new LFSConfigurationStore(*main_filesystem);
	store->init();

	CHECK_EQUAL(t, store->read_param<unsigned int>(ParamID::GNSS_ACQ_TIMEOUT));
	delete store;
}

TEST(ConfigStore, PARAM_UNDERWATER_EN)
{
	LFSConfigurationStore *store;
	store = new LFSConfigurationStore(*main_filesystem);
	store->init();

	bool t = true;
	store->write_param(ParamID::UNDERWATER_EN, t);
	CHECK_EQUAL(t, store->read_param<bool>(ParamID::UNDERWATER_EN));

	delete store;
	store = new LFSConfigurationStore(*main_filesystem);
	store->init();

	CHECK_EQUAL(t, store->read_param<bool>(ParamID::UNDERWATER_EN));
	delete store;
}

TEST(ConfigStore, PARAM_DRY_TIME_BEFORE_TX)
{
	LFSConfigurationStore *store;
	store = new LFSConfigurationStore(*main_filesystem);
	store->init();

	unsigned int t = 10U;
	store->write_param(ParamID::DRY_TIME_BEFORE_TX, t);
	CHECK_EQUAL(t, store->read_param<unsigned int>(ParamID::DRY_TIME_BEFORE_TX));

	delete store;
	store = new LFSConfigurationStore(*main_filesystem);
	store->init();

	CHECK_EQUAL(t, store->read_param<unsigned int>(ParamID::DRY_TIME_BEFORE_TX));
	delete store;
}


TEST(ConfigStore, PARAM_SAMPLING_UNDER_FREQ)
{
	LFSConfigurationStore *store;
	store = new LFSConfigurationStore(*main_filesystem);
	store->init();

	unsigned int t = 1440U;
	store->write_param(ParamID::SAMPLING_UNDER_FREQ, t);
	CHECK_EQUAL(t, store->read_param<unsigned int>(ParamID::SAMPLING_UNDER_FREQ));

	delete store;
	store = new LFSConfigurationStore(*main_filesystem);
	store->init();

	CHECK_EQUAL(t, store->read_param<unsigned int>(ParamID::SAMPLING_UNDER_FREQ));
	delete store;
}

TEST(ConfigStore, PARAM_LB_EN)
{
	LFSConfigurationStore *store;
	store = new LFSConfigurationStore(*main_filesystem);
	store->init();

	bool t = true;
	store->write_param(ParamID::LB_EN, t);
	CHECK_EQUAL(t, store->read_param<bool>(ParamID::LB_EN));

	delete store;
	store = new LFSConfigurationStore(*main_filesystem);
	store->init();

	CHECK_EQUAL(t, store->read_param<bool>(ParamID::LB_EN));
	delete store;
}

TEST(ConfigStore, PARAM_LB_TRESHOLD)
{
	LFSConfigurationStore *store;
	store = new LFSConfigurationStore(*main_filesystem);
	store->init();

	unsigned int t = 5;
	store->write_param(ParamID::LB_TRESHOLD, t);
	CHECK_EQUAL(t, store->read_param<unsigned int>(ParamID::LB_TRESHOLD));

	delete store;
	store = new LFSConfigurationStore(*main_filesystem);
	store->init();

	CHECK_EQUAL(t, store->read_param<unsigned int>(ParamID::LB_TRESHOLD));
	delete store;
}

TEST(ConfigStore, PARAM_LB_ARGOS_POWER)
{
	LFSConfigurationStore *store;
	store = new LFSConfigurationStore(*main_filesystem);
	store->init();

	BaseArgosPower t = BaseArgosPower::POWER_200_MW;
	store->write_param(ParamID::LB_ARGOS_POWER, t);
	CHECK_TRUE(t == store->read_param<BaseArgosPower>(ParamID::LB_ARGOS_POWER));

	delete store;
	store = new LFSConfigurationStore(*main_filesystem);
	store->init();

	CHECK_TRUE(t == store->read_param<BaseArgosPower>(ParamID::LB_ARGOS_POWER));
	delete store;
}

TEST(ConfigStore, PARAM_TR_LB)
{
	LFSConfigurationStore *store;
	store = new LFSConfigurationStore(*main_filesystem);
	store->init();

	unsigned int t = 1200U;
	store->write_param(ParamID::TR_LB, t);
	CHECK_EQUAL(t, store->read_param<unsigned int>(ParamID::TR_LB));

	delete store;
	store = new LFSConfigurationStore(*main_filesystem);
	store->init();

	CHECK_EQUAL(t, store->read_param<unsigned int>(ParamID::TR_LB));
	delete store;
}

TEST(ConfigStore, PARAM_LB_ARGOS_MODE)
{
	LFSConfigurationStore *store;
	store = new LFSConfigurationStore(*main_filesystem);
	store->init();

	BaseArgosMode t = BaseArgosMode::LEGACY;
	store->write_param(ParamID::LB_ARGOS_MODE, t);
	CHECK_TRUE(t == store->read_param<BaseArgosMode>(ParamID::LB_ARGOS_MODE));

	delete store;
	store = new LFSConfigurationStore(*main_filesystem);
	store->init();

	CHECK_TRUE(t == store->read_param<BaseArgosMode>(ParamID::LB_ARGOS_MODE));
	delete store;
}

TEST(ConfigStore, PARAM_LB_ARGOS_DUTY_CYCLE)
{
	LFSConfigurationStore *store;
	store = new LFSConfigurationStore(*main_filesystem);
	store->init();

	unsigned int t = 0b101010101010101010101010;
	store->write_param(ParamID::LB_ARGOS_DUTY_CYCLE, t);
	CHECK_EQUAL(t, store->read_param<unsigned int>(ParamID::LB_ARGOS_DUTY_CYCLE));

	delete store;
	store = new LFSConfigurationStore(*main_filesystem);
	store->init();

	CHECK_EQUAL(t, store->read_param<unsigned int>(ParamID::LB_ARGOS_DUTY_CYCLE));
	delete store;
}

TEST(ConfigStore, PARAM_LB_GNSS_EN)
{
	LFSConfigurationStore *store;
	store = new LFSConfigurationStore(*main_filesystem);
	store->init();

	bool t = true;
	store->write_param(ParamID::LB_GNSS_EN, t);
	CHECK_EQUAL(t, store->read_param<bool>(ParamID::LB_GNSS_EN));

	delete store;
	store = new LFSConfigurationStore(*main_filesystem);
	store->init();

	CHECK_EQUAL(t, store->read_param<bool>(ParamID::LB_GNSS_EN));
	delete store;
}

TEST(ConfigStore, PARAM_DLOC_ARG_LB)
{
	LFSConfigurationStore *store;
	store = new LFSConfigurationStore(*main_filesystem);
	store->init();

	unsigned int t = 720U;
	store->write_param(ParamID::DLOC_ARG_LB, t);
	CHECK_EQUAL(t, store->read_param<unsigned int >(ParamID::DLOC_ARG_LB));

	delete store;
	store = new LFSConfigurationStore(*main_filesystem);
	store->init();

	CHECK_EQUAL(t, store->read_param<unsigned int >(ParamID::DLOC_ARG_LB));
	delete store;
}

TEST(ConfigStore, PARAM_LB_GNSS_HDOPFILT_THR)
{
	LFSConfigurationStore *store;
	store = new LFSConfigurationStore(*main_filesystem);
	store->init();

	unsigned int t = 10U;
	store->write_param(ParamID::LB_GNSS_HDOPFILT_THR, t);
	CHECK_EQUAL(t, store->read_param<unsigned int>(ParamID::LB_GNSS_HDOPFILT_THR));

	delete store;
	store = new LFSConfigurationStore(*main_filesystem);
	store->init();

	CHECK_EQUAL(t, store->read_param<unsigned int>(ParamID::LB_GNSS_HDOPFILT_THR));
	delete store;
}

TEST(ConfigStore, PARAM_LB_GNSS_ACQ_TIMEOUT)
{
	LFSConfigurationStore *store;
	store = new LFSConfigurationStore(*main_filesystem);
	store->init();

	unsigned int t = 30U;
	store->write_param(ParamID::LB_GNSS_ACQ_TIMEOUT, t);
	CHECK_EQUAL(t, store->read_param<unsigned int>(ParamID::LB_GNSS_ACQ_TIMEOUT));

	delete store;
	store = new LFSConfigurationStore(*main_filesystem);
	store->init();

	CHECK_EQUAL(t, store->read_param<unsigned int>(ParamID::LB_GNSS_ACQ_TIMEOUT));
	delete store;
}

TEST(ConfigStore, RetrieveGPSConfigDefaultMode)
{
	LFSConfigurationStore *store;
	store = new LFSConfigurationStore(*main_filesystem);
	store->init();

	// Set default params and LB params
	unsigned int hdop_filter_threshold = 10;
	bool hdop_filter_enable = true;
	bool gnss_en = true;
	BaseAqPeriod dloc_arg_nom = BaseAqPeriod::AQPERIOD_1440_MINS;
	unsigned int acquisition_timeout = 10;
	bool lb_en = false;
	unsigned int lb_hdop_filter_threshold = 5;
	bool lb_gnss_en = false;
	BaseAqPeriod lb_dloc_arg_nom = BaseAqPeriod::AQPERIOD_720_MINS;
	unsigned int lb_acquisition_timeout = 30;

	store->write_param(ParamID::GNSS_HDOPFILT_THR, hdop_filter_threshold);
	store->write_param(ParamID::GNSS_HDOPFILT_EN, hdop_filter_enable);
	store->write_param(ParamID::GNSS_EN, gnss_en);
	store->write_param(ParamID::DLOC_ARG_NOM, dloc_arg_nom);
	store->write_param(ParamID::GNSS_ACQ_TIMEOUT, acquisition_timeout);
	store->write_param(ParamID::LB_EN, lb_en);
	store->write_param(ParamID::LB_GNSS_HDOPFILT_THR, lb_hdop_filter_threshold);
	store->write_param(ParamID::LB_GNSS_EN, lb_gnss_en);
	store->write_param(ParamID::DLOC_ARG_LB, lb_dloc_arg_nom);
	store->write_param(ParamID::LB_GNSS_ACQ_TIMEOUT, lb_acquisition_timeout);

	GNSSConfig gnss_config;
	store->get_gnss_configuration(gnss_config);

	CHECK_EQUAL(acquisition_timeout, gnss_config.acquisition_timeout);
	CHECK_TRUE(dloc_arg_nom == gnss_config.dloc_arg_nom);
	CHECK_EQUAL(gnss_en, gnss_config.enable);
	CHECK_EQUAL(hdop_filter_enable, gnss_config.hdop_filter_enable);
	CHECK_EQUAL(hdop_filter_threshold, gnss_config.hdop_filter_threshold);
}

TEST(ConfigStore, RetrieveGPSConfigLBMode)
{
	LFSConfigurationStore *store;
	store = new LFSConfigurationStore(*main_filesystem);
	store->init();

	// Set default params and LB params
	unsigned int hdop_filter_threshold = 10;
	bool hdop_filter_enable = true;
	bool gnss_en = true;
	BaseAqPeriod dloc_arg_nom = BaseAqPeriod::AQPERIOD_1440_MINS;
	unsigned int acquisition_timeout = 10;
	bool lb_en = true;
	unsigned int lb_hdop_filter_threshold = 5;
	bool lb_gnss_en = false;
	BaseAqPeriod lb_dloc_arg_nom = BaseAqPeriod::AQPERIOD_720_MINS;
	unsigned int lb_acquisition_timeout = 30;
	unsigned int lb_thresh = 10;

	store->write_param(ParamID::GNSS_HDOPFILT_THR, hdop_filter_threshold);
	store->write_param(ParamID::GNSS_HDOPFILT_EN, hdop_filter_enable);
	store->write_param(ParamID::GNSS_EN, gnss_en);
	store->write_param(ParamID::DLOC_ARG_NOM, dloc_arg_nom);
	store->write_param(ParamID::GNSS_ACQ_TIMEOUT, acquisition_timeout);
	store->write_param(ParamID::LB_EN, lb_en);
	store->write_param(ParamID::LB_GNSS_HDOPFILT_THR, lb_hdop_filter_threshold);
	store->write_param(ParamID::LB_GNSS_EN, lb_gnss_en);
	store->write_param(ParamID::DLOC_ARG_LB, lb_dloc_arg_nom);
	store->write_param(ParamID::LB_GNSS_ACQ_TIMEOUT, lb_acquisition_timeout);
	store->write_param(ParamID::LB_TRESHOLD, lb_thresh);

	// Notify battery level above threshold
	store->notify_battery_level(100U);

	GNSSConfig gnss_config;
	store->get_gnss_configuration(gnss_config);

	CHECK_EQUAL(acquisition_timeout, gnss_config.acquisition_timeout);
	CHECK_TRUE(dloc_arg_nom == gnss_config.dloc_arg_nom);
	CHECK_EQUAL(gnss_en, gnss_config.enable);
	CHECK_EQUAL(hdop_filter_enable, gnss_config.hdop_filter_enable);
	CHECK_EQUAL(hdop_filter_threshold, gnss_config.hdop_filter_threshold);

	// Notify battery level equal threshold
	store->notify_battery_level(10U);

	store->get_gnss_configuration(gnss_config);

	CHECK_EQUAL(lb_acquisition_timeout, gnss_config.acquisition_timeout);
	CHECK_TRUE(lb_dloc_arg_nom == gnss_config.dloc_arg_nom);
	CHECK_EQUAL(lb_gnss_en, gnss_config.enable);
	CHECK_EQUAL(hdop_filter_enable, gnss_config.hdop_filter_enable);
	CHECK_EQUAL(lb_hdop_filter_threshold, gnss_config.hdop_filter_threshold);

	// Notify battery level below threshold
	store->notify_battery_level(1U);

	store->get_gnss_configuration(gnss_config);

	CHECK_EQUAL(lb_acquisition_timeout, gnss_config.acquisition_timeout);
	CHECK_TRUE(lb_dloc_arg_nom == gnss_config.dloc_arg_nom);
	CHECK_EQUAL(lb_gnss_en, gnss_config.enable);
	CHECK_EQUAL(hdop_filter_enable, gnss_config.hdop_filter_enable);
	CHECK_EQUAL(lb_hdop_filter_threshold, gnss_config.hdop_filter_threshold);

}

TEST(ConfigStore, RetrieveArgosConfigDefaultMode)
{
	LFSConfigurationStore *store;
	store = new LFSConfigurationStore(*main_filesystem);
	store->init();

	// Set default params and LB params
	BaseArgosDepthPile depth_pile = BaseArgosDepthPile::DEPTH_PILE_12;
	unsigned int dry_time_before_tx = 10;
	unsigned int duty_cycle = 0xFFFFFFU;
	double frequency = 0;
	BaseArgosMode mode = BaseArgosMode::DUTY_CYCLE;
	unsigned int ntry_per_message = 1;
	BaseArgosPower power = BaseArgosPower::POWER_500_MW;
	unsigned int tr_nom = 60;
	unsigned int tx_counter = 12;
	bool lb_en = false;
	BaseArgosDepthPile lb_depth_pile = BaseArgosDepthPile::DEPTH_PILE_4;
	unsigned int lb_duty_cycle = 0xAAAAAAU;
	BaseArgosMode lb_mode = BaseArgosMode::LEGACY;
	BaseArgosPower lb_power = BaseArgosPower::POWER_40_MW;
	unsigned int lb_tr_nom = 120;

	store->write_param(ParamID::ARGOS_DEPTH_PILE, depth_pile);
	store->write_param(ParamID::DRY_TIME_BEFORE_TX, dry_time_before_tx);
	store->write_param(ParamID::DUTY_CYCLE, duty_cycle);
	store->write_param(ParamID::ARGOS_FREQ, frequency);
	store->write_param(ParamID::ARGOS_MODE, mode);
	store->write_param(ParamID::NTRY_PER_MESSAGE, ntry_per_message);
	store->write_param(ParamID::ARGOS_POWER, power);
	store->write_param(ParamID::TR_NOM, tr_nom);
	store->write_param(ParamID::TX_COUNTER, tx_counter);
	store->write_param(ParamID::LB_EN, lb_en);
	store->write_param(ParamID::LB_ARGOS_DEPTH_PILE, lb_depth_pile);
	store->write_param(ParamID::LB_ARGOS_DUTY_CYCLE, lb_duty_cycle);
	store->write_param(ParamID::LB_ARGOS_MODE, lb_mode);
	store->write_param(ParamID::LB_ARGOS_POWER, lb_power);
	store->write_param(ParamID::TR_LB, lb_tr_nom);

	ArgosConfig argos_config;
	store->get_argos_configuration(argos_config);

	CHECK_EQUAL((unsigned int)depth_pile, (unsigned int)argos_config.depth_pile);
	CHECK_EQUAL(dry_time_before_tx, argos_config.dry_time_before_tx);
	CHECK_EQUAL(duty_cycle, argos_config.duty_cycle);
	CHECK_EQUAL(frequency, argos_config.frequency);
	CHECK_EQUAL((unsigned int)mode, (unsigned int)argos_config.mode);
	CHECK_EQUAL(ntry_per_message, argos_config.ntry_per_message);
	CHECK_EQUAL((unsigned int)power, (unsigned int)argos_config.power);
	CHECK_EQUAL(tr_nom, argos_config.tr_nom);
	CHECK_EQUAL(tx_counter, argos_config.tx_counter);
}

TEST(ConfigStore, RetrieveArgosConfigLBMode)
{
	LFSConfigurationStore *store;
	store = new LFSConfigurationStore(*main_filesystem);
	store->init();

	// Set default params and LB params
	BaseArgosDepthPile depth_pile = BaseArgosDepthPile::DEPTH_PILE_12;
	unsigned int dry_time_before_tx = 10;
	unsigned int duty_cycle = 0xFFFFFFU;
	double frequency = 0;
	BaseArgosMode mode = BaseArgosMode::DUTY_CYCLE;
	unsigned int ntry_per_message = 1;
	BaseArgosPower power = BaseArgosPower::POWER_500_MW;
	unsigned int tr_nom = 60;
	unsigned int tx_counter = 12;
	bool lb_en = true;
	BaseArgosDepthPile lb_depth_pile = BaseArgosDepthPile::DEPTH_PILE_4;
	unsigned int lb_duty_cycle = 0xAAAAAAU;
	BaseArgosMode lb_mode = BaseArgosMode::LEGACY;
	BaseArgosPower lb_power = BaseArgosPower::POWER_40_MW;
	unsigned int lb_tr_nom = 120;
	unsigned int lb_thresh = 10U;

	store->write_param(ParamID::ARGOS_DEPTH_PILE, depth_pile);
	store->write_param(ParamID::DRY_TIME_BEFORE_TX, dry_time_before_tx);
	store->write_param(ParamID::DUTY_CYCLE, duty_cycle);
	store->write_param(ParamID::ARGOS_FREQ, frequency);
	store->write_param(ParamID::ARGOS_MODE, mode);
	store->write_param(ParamID::NTRY_PER_MESSAGE, ntry_per_message);
	store->write_param(ParamID::ARGOS_POWER, power);
	store->write_param(ParamID::TR_NOM, tr_nom);
	store->write_param(ParamID::TX_COUNTER, tx_counter);
	store->write_param(ParamID::LB_EN, lb_en);
	store->write_param(ParamID::LB_ARGOS_DEPTH_PILE, lb_depth_pile);
	store->write_param(ParamID::LB_ARGOS_DUTY_CYCLE, lb_duty_cycle);
	store->write_param(ParamID::LB_ARGOS_MODE, lb_mode);
	store->write_param(ParamID::LB_ARGOS_POWER, lb_power);
	store->write_param(ParamID::TR_LB, lb_tr_nom);
	store->write_param(ParamID::LB_TRESHOLD, lb_thresh);

	// Notify battery level above threshold
	store->notify_battery_level(100U);

	ArgosConfig argos_config;
	store->get_argos_configuration(argos_config);

	CHECK_EQUAL((unsigned int)depth_pile, (unsigned int)argos_config.depth_pile);
	CHECK_EQUAL(dry_time_before_tx, argos_config.dry_time_before_tx);
	CHECK_EQUAL(duty_cycle, argos_config.duty_cycle);
	CHECK_EQUAL(frequency, argos_config.frequency);
	CHECK_EQUAL((unsigned int)mode, (unsigned int)argos_config.mode);
	CHECK_EQUAL(ntry_per_message, argos_config.ntry_per_message);
	CHECK_EQUAL((unsigned int)power, (unsigned int)argos_config.power);
	CHECK_EQUAL(tr_nom, argos_config.tr_nom);
	CHECK_EQUAL(tx_counter, argos_config.tx_counter);

	// Notify battery level equal threshold
	store->notify_battery_level(10U);

	store->get_argos_configuration(argos_config);

	CHECK_EQUAL((unsigned int)lb_depth_pile, (unsigned int)argos_config.depth_pile);
	CHECK_EQUAL(dry_time_before_tx, argos_config.dry_time_before_tx);
	CHECK_EQUAL(lb_duty_cycle, argos_config.duty_cycle);
	CHECK_EQUAL(frequency, argos_config.frequency);
	CHECK_EQUAL((unsigned int)lb_mode, (unsigned int)argos_config.mode);
	CHECK_EQUAL(ntry_per_message, argos_config.ntry_per_message);
	CHECK_EQUAL((unsigned int)lb_power, (unsigned int)argos_config.power);
	CHECK_EQUAL(lb_tr_nom, argos_config.tr_nom);
	CHECK_EQUAL(tx_counter, argos_config.tx_counter);

	// Notify battery level below threshold
	store->notify_battery_level(1U);

	store->get_argos_configuration(argos_config);

	CHECK_EQUAL((unsigned int)lb_depth_pile, (unsigned int)argos_config.depth_pile);
	CHECK_EQUAL(dry_time_before_tx, argos_config.dry_time_before_tx);
	CHECK_EQUAL(lb_duty_cycle, argos_config.duty_cycle);
	CHECK_EQUAL(frequency, argos_config.frequency);
	CHECK_EQUAL((unsigned int)lb_mode, (unsigned int)argos_config.mode);
	CHECK_EQUAL(ntry_per_message, argos_config.ntry_per_message);
	CHECK_EQUAL((unsigned int)lb_power, (unsigned int)argos_config.power);
	CHECK_EQUAL(lb_tr_nom, argos_config.tr_nom);
	CHECK_EQUAL(tx_counter, argos_config.tx_counter);
}

TEST(ConfigStore, ZoneExclusionCriteriaChecking) {

	LFSConfigurationStore *store;
	store = new LFSConfigurationStore(*main_filesystem);
	store->init();

	CHECK_FALSE(store->is_zone_exclusion());

	// Set last GPS coordinates
	GPSLogEntry gps_location;
	gps_location.day = 1;
	gps_location.month = 1;
	gps_location.year = 2021;
	gps_location.hour = 0;
	gps_location.min = 0;
	gps_location.sec = 0;
	gps_location.valid = 1;
	gps_location.lon = -2.118413;
	gps_location.lat = 51.3765242;
	store->notify_gps_location(gps_location);

	// Set up exclusion zone
	BaseZone zone;
	zone.zone_id = 0;
	zone.zone_type = BaseZoneType::CIRCLE;
	zone.center_longitude_x = -2.118413;
	zone.center_latitude_y = 51.3765242;
	zone.radius_m = 100000;  // 100 km
	zone.enable_activation_date = false;
	zone.enable_monitoring = true;
	zone.enable_out_of_zone_detection_mode = true;
	store->write_zone(zone);

	// Inside zone
	CHECK_FALSE(store->is_zone_exclusion());

	// Outside zone
	zone.center_longitude_x = -1.0;
	zone.center_latitude_y = 53;
	store->write_zone(zone);
	CHECK_TRUE(store->is_zone_exclusion());

	// Enable activation date later than GPS time
	zone.enable_activation_date = true;
	zone.year = 2021;
	zone.month = 2;
	zone.day = 1;
	zone.hour = 0;
	zone.minute = 0;
	store->write_zone(zone);
	CHECK_FALSE(store->is_zone_exclusion());

	// Enable activation date before GPS time
	zone.enable_activation_date = true;
	zone.year = 2020;
	zone.month = 12;
	zone.day = 31;
	zone.hour = 23;
	zone.minute = 59;
	store->write_zone(zone);
	CHECK_TRUE(store->is_zone_exclusion());

	// Put back inside zone
	zone.center_longitude_x = -2.118413;
	zone.center_latitude_y = 51.3765242;
	store->write_zone(zone);
	CHECK_FALSE(store->is_zone_exclusion());

	// Outside zone but monitoring disabled
	zone.center_longitude_x = -1.0;
	zone.center_latitude_y = 53;
	zone.enable_monitoring = false;
	store->write_zone(zone);
	CHECK_FALSE(store->is_zone_exclusion());
}

TEST(ConfigStore, RetrieveArgosConfigZoneExclusionMode)
{
	LFSConfigurationStore *store;
	store = new LFSConfigurationStore(*main_filesystem);
	store->init();

	// Set last GPS coordinates
	GPSLogEntry gps_location;
	gps_location.day = 1;
	gps_location.month = 1;
	gps_location.year = 2021;
	gps_location.hour = 0;
	gps_location.min = 0;
	gps_location.sec = 0;
	gps_location.valid = 1;
	gps_location.lon = -2.118413;
	gps_location.lat = 51.3765242;
	store->notify_gps_location(gps_location);

	// Set up exclusion zone
	BaseZone zone;
	zone.zone_id = 0;
	zone.zone_type = BaseZoneType::CIRCLE;
	zone.center_longitude_x = -2.118413;
	zone.center_latitude_y = 51.3765242;
	zone.radius_m = 100000;  // 100 km
	zone.enable_activation_date = false;
	zone.enable_monitoring = true;
	zone.enable_out_of_zone_detection_mode = true;
	zone.argos_extra_flags_enable = true;
	zone.argos_duty_cycle = 0xAAAAAAU;
	zone.argos_depth_pile = BaseArgosDepthPile::DEPTH_PILE_1;
	zone.argos_mode = BaseArgosMode::LEGACY;
	zone.argos_power = BaseArgosPower::POWER_3_MW;
	zone.argos_time_repetition_seconds = 120;
	store->write_zone(zone);

	// Set default params
	BaseArgosDepthPile depth_pile = BaseArgosDepthPile::DEPTH_PILE_12;
	unsigned int dry_time_before_tx = 10;
	unsigned int duty_cycle = 0xFFFFFFU;
	double frequency = 0;
	BaseArgosMode mode = BaseArgosMode::DUTY_CYCLE;
	unsigned int ntry_per_message = 1;
	BaseArgosPower power = BaseArgosPower::POWER_500_MW;
	unsigned int tr_nom = 60;
	unsigned int tx_counter = 12;
	bool lb_en = false;

	store->write_param(ParamID::ARGOS_DEPTH_PILE, depth_pile);
	store->write_param(ParamID::DRY_TIME_BEFORE_TX, dry_time_before_tx);
	store->write_param(ParamID::DUTY_CYCLE, duty_cycle);
	store->write_param(ParamID::ARGOS_FREQ, frequency);
	store->write_param(ParamID::ARGOS_MODE, mode);
	store->write_param(ParamID::NTRY_PER_MESSAGE, ntry_per_message);
	store->write_param(ParamID::ARGOS_POWER, power);
	store->write_param(ParamID::TR_NOM, tr_nom);
	store->write_param(ParamID::TX_COUNTER, tx_counter);
	store->write_param(ParamID::LB_EN, lb_en);

	// Inside zone, default params should apply
	ArgosConfig argos_config;
	store->get_argos_configuration(argos_config);

	CHECK_EQUAL((unsigned int)depth_pile, (unsigned int)argos_config.depth_pile);
	CHECK_EQUAL(dry_time_before_tx, argos_config.dry_time_before_tx);
	CHECK_EQUAL(duty_cycle, argos_config.duty_cycle);
	CHECK_EQUAL(frequency, argos_config.frequency);
	CHECK_EQUAL((unsigned int)mode, (unsigned int)argos_config.mode);
	CHECK_EQUAL(ntry_per_message, argos_config.ntry_per_message);
	CHECK_EQUAL((unsigned int)power, (unsigned int)argos_config.power);
	CHECK_EQUAL(tr_nom, argos_config.tr_nom);
	CHECK_EQUAL(tx_counter, argos_config.tx_counter);

	// Set outside zone
	zone.center_longitude_x = -1.0;
	zone.center_latitude_y = 53;
	store->write_zone(zone);

	store->get_argos_configuration(argos_config);

	CHECK_EQUAL((unsigned int)zone.argos_depth_pile, (unsigned int)argos_config.depth_pile);
	CHECK_EQUAL(dry_time_before_tx, argos_config.dry_time_before_tx);
	CHECK_EQUAL(zone.argos_duty_cycle, argos_config.duty_cycle);
	CHECK_EQUAL(frequency, argos_config.frequency);
	CHECK_EQUAL((unsigned int)zone.argos_mode, (unsigned int)argos_config.mode);
	CHECK_EQUAL(ntry_per_message, argos_config.ntry_per_message);
	CHECK_EQUAL((unsigned int)zone.argos_power, (unsigned int)argos_config.power);
	CHECK_EQUAL(zone.argos_time_repetition_seconds, argos_config.tr_nom);
	CHECK_EQUAL(tx_counter, argos_config.tx_counter);
}


TEST(ConfigStore, RetrieveGPSConfigZoneExclusionMode)
{
	LFSConfigurationStore *store;
	store = new LFSConfigurationStore(*main_filesystem);
	store->init();

	// Set last GPS coordinates
	GPSLogEntry gps_location;
	gps_location.day = 1;
	gps_location.month = 1;
	gps_location.year = 2021;
	gps_location.hour = 0;
	gps_location.min = 0;
	gps_location.sec = 0;
	gps_location.valid = 1;
	gps_location.lon = -2.118413;
	gps_location.lat = 51.3765242;
	store->notify_gps_location(gps_location);

	// Set up exclusion zone
	BaseZone zone;
	zone.zone_id = 0;
	zone.zone_type = BaseZoneType::CIRCLE;
	zone.center_longitude_x = -2.118413;
	zone.center_latitude_y = 51.3765242;
	zone.radius_m = 100000;  // 100 km
	zone.enable_activation_date = false;
	zone.enable_monitoring = true;
	zone.enable_out_of_zone_detection_mode = true;
	zone.gnss_extra_flags_enable = true;
	zone.gnss_acquisition_timeout_seconds = 30;
	zone.hdop_filter_threshold = 3;
	store->write_zone(zone);

	// Set default params
	unsigned int hdop_filter_threshold = 10;
	bool hdop_filter_enable = true;
	bool gnss_en = true;
	BaseAqPeriod dloc_arg_nom = BaseAqPeriod::AQPERIOD_1440_MINS;
	unsigned int acquisition_timeout = 10;
	bool lb_en = false;

	store->write_param(ParamID::GNSS_HDOPFILT_THR, hdop_filter_threshold);
	store->write_param(ParamID::GNSS_HDOPFILT_EN, hdop_filter_enable);
	store->write_param(ParamID::GNSS_EN, gnss_en);
	store->write_param(ParamID::DLOC_ARG_NOM, dloc_arg_nom);
	store->write_param(ParamID::GNSS_ACQ_TIMEOUT, acquisition_timeout);
	store->write_param(ParamID::LB_EN, lb_en);

	// Inside zone, default params should apply

	GNSSConfig gnss_config;
	store->get_gnss_configuration(gnss_config);

	CHECK_EQUAL(acquisition_timeout, gnss_config.acquisition_timeout);
	CHECK_EQUAL((unsigned int)dloc_arg_nom, (unsigned int)gnss_config.dloc_arg_nom);
	CHECK_EQUAL(gnss_en, gnss_config.enable);
	CHECK_EQUAL(hdop_filter_enable, gnss_config.hdop_filter_enable);
	CHECK_EQUAL(hdop_filter_threshold, gnss_config.hdop_filter_threshold);

	// Set outside zone
	zone.center_longitude_x = -1.0;
	zone.center_latitude_y = 53;
	store->write_zone(zone);

	store->get_gnss_configuration(gnss_config);

	CHECK_EQUAL(zone.gnss_acquisition_timeout_seconds, gnss_config.acquisition_timeout);
	CHECK_EQUAL((unsigned int)dloc_arg_nom, (unsigned int)gnss_config.dloc_arg_nom);
	CHECK_EQUAL(gnss_en, gnss_config.enable);
	CHECK_EQUAL(hdop_filter_enable, gnss_config.hdop_filter_enable);
	CHECK_EQUAL(zone.hdop_filter_threshold, gnss_config.hdop_filter_threshold);
}
