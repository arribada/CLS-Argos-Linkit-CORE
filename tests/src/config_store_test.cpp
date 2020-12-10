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
	CHECK_EQUAL(0U, store->read_param<unsigned int>(ParamID::DEVICE_MODEL));

	delete store;
}

TEST(ConfigStore, CheckBaseTypeReadAccess)
{
	LFSConfigurationStore *store;
	store = new LFSConfigurationStore(*main_filesystem);
	store->init();

	// Modify some parameter values
	unsigned int model = 1U;
	store->write_param(ParamID::DEVICE_MODEL, model);

	BaseType x = store->read_param<BaseType>(ParamID::DEVICE_MODEL);
	CHECK_EQUAL(model, std::get<unsigned int>(x));


	delete store;
}

TEST(ConfigStore, CheckConfigStorePersistence)
{
	LFSConfigurationStore *store;
	store = new LFSConfigurationStore(*main_filesystem);

	store->init();

	// Modify some parameter values
	unsigned int model = 1U;
	unsigned int dec_id = 1234U;
	store->write_param(ParamID::ARGOS_DECID, dec_id);
	store->write_param(ParamID::DEVICE_MODEL, model);

	// Delete the object and recreate a new one
	delete store;
	store = new LFSConfigurationStore(*main_filesystem);
	store->init();  // This will read in the saved file

	// Check modified parameters
	CHECK_EQUAL(1234U, store->read_param<unsigned int>(ParamID::ARGOS_DECID));
	CHECK_EQUAL(model, store->read_param<unsigned int>(ParamID::DEVICE_MODEL));

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

	BaseArgosPower t = BaseArgosPower::POWER_250_MW;
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

	BaseArgosPower t = BaseArgosPower::POWER_250_MW;
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
