#include "config_store_fs.hpp"

#include "dte_protocol.hpp"
#include "fake_battery_mon.hpp"

#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"


#define BLOCK_COUNT   (256)
#define BLOCK_SIZE    (64*1024)
#define PAGE_SIZE     (256)
#define MAX_FILE_SIZE (4*1024*1024)


extern FileSystem *main_filesystem;
extern BatteryMonitor *battery_monitor;
static LFSFileSystem *ram_filesystem;

using namespace std::literals::string_literals;



TEST_GROUP(ConfigStore)
{
	RamFlash *ram_flash;
	FakeBatteryMonitor *fake_battery_monitor;

	void setup() {
		fake_battery_monitor = new FakeBatteryMonitor;
		battery_monitor = fake_battery_monitor;
		ram_flash = new RamFlash(BLOCK_COUNT, BLOCK_SIZE, PAGE_SIZE);
		ram_filesystem = new LFSFileSystem(ram_flash);
		ram_filesystem->format();
		ram_filesystem->mount();
		main_filesystem = ram_filesystem;
	}

	void teardown() {
		ram_filesystem->umount();
		delete ram_filesystem;
		delete ram_flash;
		delete fake_battery_monitor;
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
	CHECK_EQUAL(DEVICE_MODEL_NAME, store->read_param<std::string>(ParamID::DEVICE_MODEL));
	CHECK_EQUAL(0U, (unsigned int)store->read_param<BaseUnderwaterDetectSource>(ParamID::UNDERWATER_DETECT_SOURCE));
	CHECK_EQUAL(1.1, store->read_param<double>(ParamID::UNDERWATER_DETECT_THRESH));

	delete store;
}

TEST(ConfigStore, CheckBaseTypeReadAccess)
{
	LFSConfigurationStore *store;
	store = new LFSConfigurationStore(*main_filesystem);
	store->init();

	// Modify some parameter values
	std::string model = "GenTracker";
	store->write_param(ParamID::PROFILE_NAME, model);

	BaseType x = store->read_param<BaseType>(ParamID::PROFILE_NAME);
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
	store->write_param(ParamID::PROFILE_NAME, model);
	store->save_params();

	// Delete the object and recreate a new one
	delete store;
	store = new LFSConfigurationStore(*main_filesystem);
	store->init();  // This will read in the saved file

	// Check modified parameters
	CHECK_EQUAL(1234U, store->read_param<unsigned int>(ParamID::ARGOS_DECID));
	CHECK_EQUAL(model, store->read_param<std::string>(ParamID::PROFILE_NAME));

	delete store;
}

TEST(ConfigStore, CheckConfigStoreResetsBadVariantType)
{
	LFSConfigurationStore *store;
	store = new LFSConfigurationStore(*main_filesystem);

	store->init();

	// Modify some parameter values
	std::string model = "GenTracker";
	store->write_param(ParamID::ARGOS_DECID, model);
	store->save_params();

	// Delete the object and recreate a new one
	delete store;
	store = new LFSConfigurationStore(*main_filesystem);
	store->init();  // This will read in the saved file

	// Check default value has been restored
	CHECK_EQUAL(0U, store->read_param<unsigned int>(ParamID::ARGOS_DECID));

	delete store;
}

TEST(ConfigStore, CheckPartiallyCorruptedConfigurationStoreRetainsProtectedParams)
{
	LFSConfigurationStore *store;
	store = new LFSConfigurationStore(*main_filesystem);

	store->init();

	// Set protected parameter values
	unsigned int dec_id = 1234U;
	unsigned int hex_id = 0x1234567U;
	store->write_param(ParamID::ARGOS_DECID, dec_id);
	store->write_param(ParamID::ARGOS_HEXID, hex_id);

	// Save parameters
	store->save_params();

	{
		// Overwrite first 4 bytes (configuration version)
		LFSFile f(main_filesystem, "config.dat", LFS_O_WRONLY);
		uint32_t clobbered_version = 0;
		f.write(&clobbered_version, sizeof(uint32_t));
	}

	// Delete the object and recreate a new one
	delete store;
	store = new LFSConfigurationStore(*main_filesystem);
	store->init();  // This will read in the partially saved file

	// Check default value has been restored
	CHECK_EQUAL(dec_id, store->read_param<unsigned int>(ParamID::ARGOS_DECID));
	CHECK_EQUAL(hex_id, store->read_param<unsigned int>(ParamID::ARGOS_HEXID));

	delete store;
}


TEST(ConfigStore, CheckFullyCorruptedConfigurationStore)
{
	LFSConfigurationStore *store;
	store = new LFSConfigurationStore(*main_filesystem);

	store->init();

	// Set protected parameter values
	unsigned int dec_id = 1234U;
	unsigned int hex_id = 0x1234567U;
	store->write_param(ParamID::ARGOS_DECID, dec_id);
	store->write_param(ParamID::ARGOS_HEXID, hex_id);

	// Save parameters
	store->save_params();

	{
		// Overwrite first 1024 bytes (configuration version)
		LFSFile f(main_filesystem, "config.dat", LFS_O_WRONLY);
		uint8_t clobber[1024];
		f.write(clobber, sizeof(clobber));
	}

	// Delete the object and recreate a new one
	delete store;
	store = new LFSConfigurationStore(*main_filesystem);
	store->init();  // This will read in the partially saved file

	// Check default values are restored
	CHECK_EQUAL(0U, store->read_param<unsigned int>(ParamID::ARGOS_DECID));
	CHECK_EQUAL(0U, store->read_param<unsigned int>(ParamID::ARGOS_HEXID));

	delete store;
}

TEST(ConfigStore, CheckFactoryResetRetainsProtectedParams)
{
	LFSConfigurationStore *store;
	store = new LFSConfigurationStore(*main_filesystem);

	store->init();

	// Set protected parameter values
	unsigned int dec_id = 1234U;
	unsigned int hex_id = 0x1234567U;
	store->write_param(ParamID::ARGOS_DECID, dec_id);
	store->write_param(ParamID::ARGOS_HEXID, hex_id);

	// Save parameters
	store->save_params();

	// Factory reset
	store->factory_reset();

	// Delete the object and recreate a new one
	delete store;
	store = new LFSConfigurationStore(*main_filesystem);
	store->init();  // This will read in the partially saved file

	// Check default value has been restored
	CHECK_EQUAL(dec_id, store->read_param<unsigned int>(ParamID::ARGOS_DECID));
	CHECK_EQUAL(hex_id, store->read_param<unsigned int>(ParamID::ARGOS_HEXID));

	delete store;
}

TEST(ConfigStore, CheckDefaultZoneSettings)
{
	LFSConfigurationStore *store;
	store = new LFSConfigurationStore(*main_filesystem);

	store->init();

	CHECK_TRUE(store->read_param<BaseZoneType>(ParamID::ZONE_TYPE) == BaseZoneType::CIRCLE);
	CHECK_FALSE(store->read_param<bool>(ParamID::ZONE_ENABLE_OUT_OF_ZONE_DETECTION_MODE));
	CHECK_TRUE(store->read_param<bool>(ParamID::ZONE_ENABLE_ACTIVATION_DATE));
	CHECK_TRUE(store->read_param<std::time_t>(ParamID::ZONE_ACTIVATION_DATE) == (std::time_t)1577836800U);
	CHECK_TRUE(store->read_param<BaseArgosDepthPile>(ParamID::ZONE_ARGOS_DEPTH_PILE) == BaseArgosDepthPile::DEPTH_PILE_1);
	CHECK_TRUE(store->read_param<BaseArgosPower>(ParamID::ZONE_ARGOS_POWER) == BaseArgosPower::POWER_200_MW);
	CHECK_TRUE(store->read_param<unsigned int>(ParamID::ZONE_ARGOS_REPETITION_SECONDS) == 240);
	CHECK_TRUE(store->read_param<BaseArgosMode>(ParamID::ZONE_ARGOS_MODE) == BaseArgosMode::LEGACY);
	CHECK_TRUE(store->read_param<unsigned int>(ParamID::ZONE_ARGOS_DUTY_CYCLE) == 0xFFFFFFU);
	CHECK_TRUE(store->read_param<unsigned int>(ParamID::ZONE_ARGOS_NTRY_PER_MESSAGE) == 0U);
	CHECK_TRUE(store->read_param<unsigned int>(ParamID::ZONE_GNSS_DELTA_ARG_LOC_ARGOS_SECONDS) == 3600U);
	CHECK_TRUE(store->read_param<unsigned int>(ParamID::ZONE_GNSS_HDOPFILT_THR) == 2U);
	CHECK_TRUE(store->read_param<unsigned int>(ParamID::ZONE_GNSS_HACCFILT_THR) == 5U);
	CHECK_TRUE(store->read_param<unsigned int>(ParamID::ZONE_GNSS_ACQ_TIMEOUT) == 240U);
	CHECK_TRUE(store->read_param<double>(ParamID::ZONE_CENTER_LONGITUDE) == -123.3925);
	CHECK_TRUE(store->read_param<double>(ParamID::ZONE_CENTER_LATITUDE) == -48.8752);
	CHECK_TRUE(store->read_param<unsigned int>(ParamID::ZONE_RADIUS) == 1000U);
	delete store;
}

TEST(ConfigStore, CheckDefaultPassPredictIsAvailable)
{
	LFSConfigurationStore *store;
	store = new LFSConfigurationStore(*main_filesystem);

	store->init();

	BasePassPredict pp = store->read_pass_predict();
	CHECK_EQUAL(8, pp.num_records);

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

TEST(ConfigStore, CheckPassPredictVersionCodeMismatch)
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

	// Corrupt the prepass file first 4 bytes
	{
		// Overwrite first 4 bytes (configuration version)
		LFSFile f(main_filesystem, "pass_predict.dat", LFS_O_WRONLY);
		uint8_t clobber[4];
		f.write(clobber, sizeof(clobber));
	}

	store = new LFSConfigurationStore(*main_filesystem);

	{
		store->init();
		BasePassPredict pp = store->read_pass_predict();
		CHECK_EQUAL(8, pp.num_records);
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
	CHECK_EQUAL("V0.1"s, store->read_param<std::string>(ParamID::FW_APP_VERSION));  // Should not change
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
	store->save_params();
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
	store->save_params();
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
	store->save_params();
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
	store->save_params();
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
	store->save_params();
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
	store->save_params();
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
	store->save_params();
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
	store->save_params();
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
	store->save_params();
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
	store->save_params();
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
	store->save_params();
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
	store->save_params();
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
	store->save_params();
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
	store->save_params();
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
	store->save_params();
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
	store->save_params();
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
	store->save_params();
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
	store->save_params();
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
	store->save_params();
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
	store->save_params();
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
	store->save_params();
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
	store->save_params();
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
	store->save_params();
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
	store->save_params();
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
	store->save_params();
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
	unsigned int dloc_arg_nom = 1440*60U;
	unsigned int acquisition_timeout = 10;
	bool lb_en = false;
	unsigned int lb_hdop_filter_threshold = 5;
	bool lb_gnss_en = false;
	unsigned int lb_dloc_arg_nom = 720*60U;
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
	unsigned int hacc_filter_threshold = 50;
	bool hacc_filter_enable = true;
	bool gnss_en = true;
	unsigned int dloc_arg_nom = 1440*60U;
	unsigned int acquisition_timeout = 10;
	bool lb_en = true;
	unsigned int lb_hdop_filter_threshold = 5;
	unsigned int lb_hacc_filter_threshold = 100;
	bool lb_gnss_en = false;
	unsigned int lb_dloc_arg_nom = 720*60U;
	unsigned int lb_acquisition_timeout = 30;
	unsigned int lb_thresh = 10;

	store->write_param(ParamID::GNSS_HDOPFILT_THR, hdop_filter_threshold);
	store->write_param(ParamID::GNSS_HDOPFILT_EN, hdop_filter_enable);
	store->write_param(ParamID::GNSS_HACCFILT_THR, hacc_filter_threshold);
	store->write_param(ParamID::GNSS_HACCFILT_EN, hacc_filter_enable);
	store->write_param(ParamID::GNSS_EN, gnss_en);
	store->write_param(ParamID::DLOC_ARG_NOM, dloc_arg_nom);
	store->write_param(ParamID::GNSS_ACQ_TIMEOUT, acquisition_timeout);
	store->write_param(ParamID::LB_EN, lb_en);
	store->write_param(ParamID::LB_GNSS_HDOPFILT_THR, lb_hdop_filter_threshold);
	store->write_param(ParamID::LB_GNSS_HACCFILT_THR, lb_hacc_filter_threshold);
	store->write_param(ParamID::LB_GNSS_EN, lb_gnss_en);
	store->write_param(ParamID::DLOC_ARG_LB, lb_dloc_arg_nom);
	store->write_param(ParamID::LB_GNSS_ACQ_TIMEOUT, lb_acquisition_timeout);
	store->write_param(ParamID::LB_TRESHOLD, lb_thresh);

	// Notify battery level above threshold
	fake_battery_monitor->m_level = 100;

	GNSSConfig gnss_config;
	store->get_gnss_configuration(gnss_config);

	CHECK_EQUAL(acquisition_timeout, gnss_config.acquisition_timeout);
	CHECK_TRUE(dloc_arg_nom == gnss_config.dloc_arg_nom);
	CHECK_EQUAL(gnss_en, gnss_config.enable);
	CHECK_EQUAL(hdop_filter_enable, gnss_config.hdop_filter_enable);
	CHECK_EQUAL(hdop_filter_threshold, gnss_config.hdop_filter_threshold);
	CHECK_EQUAL(hacc_filter_enable, gnss_config.hacc_filter_enable);
	CHECK_EQUAL(hacc_filter_threshold, gnss_config.hacc_filter_threshold);

	// Notify battery level equal threshold
	fake_battery_monitor->m_level = 10;

	store->get_gnss_configuration(gnss_config);

	CHECK_EQUAL(lb_acquisition_timeout, gnss_config.acquisition_timeout);
	CHECK_TRUE(lb_dloc_arg_nom == gnss_config.dloc_arg_nom);
	CHECK_EQUAL(lb_gnss_en, gnss_config.enable);
	CHECK_EQUAL(hdop_filter_enable, gnss_config.hdop_filter_enable);
	CHECK_EQUAL(lb_hdop_filter_threshold, gnss_config.hdop_filter_threshold);
	CHECK_EQUAL(hacc_filter_enable, gnss_config.hacc_filter_enable);
	CHECK_EQUAL(lb_hacc_filter_threshold, gnss_config.hacc_filter_threshold);

	// Notify battery level below threshold
	fake_battery_monitor->m_level = 1;

	store->get_gnss_configuration(gnss_config);

	CHECK_EQUAL(lb_acquisition_timeout, gnss_config.acquisition_timeout);
	CHECK_TRUE(lb_dloc_arg_nom == gnss_config.dloc_arg_nom);
	CHECK_EQUAL(lb_gnss_en, gnss_config.enable);
	CHECK_EQUAL(hdop_filter_enable, gnss_config.hdop_filter_enable);
	CHECK_EQUAL(lb_hdop_filter_threshold, gnss_config.hdop_filter_threshold);
	CHECK_EQUAL(hacc_filter_enable, gnss_config.hacc_filter_enable);
	CHECK_EQUAL(lb_hacc_filter_threshold, gnss_config.hacc_filter_threshold);

	// Notify battery level 1% above threshold whilst in LB mode
	fake_battery_monitor->m_level = 11;

	store->get_gnss_configuration(gnss_config);

	CHECK_EQUAL(lb_acquisition_timeout, gnss_config.acquisition_timeout);
	CHECK_TRUE(lb_dloc_arg_nom == gnss_config.dloc_arg_nom);
	CHECK_EQUAL(lb_gnss_en, gnss_config.enable);
	CHECK_EQUAL(hdop_filter_enable, gnss_config.hdop_filter_enable);
	CHECK_EQUAL(lb_hdop_filter_threshold, gnss_config.hdop_filter_threshold);
	CHECK_EQUAL(hacc_filter_enable, gnss_config.hacc_filter_enable);
	CHECK_EQUAL(lb_hacc_filter_threshold, gnss_config.hacc_filter_threshold);

	// Notify battery level 5% above threshold whilst in LB mode
	fake_battery_monitor->m_level = 15;

	store->get_gnss_configuration(gnss_config);

	CHECK_EQUAL(acquisition_timeout, gnss_config.acquisition_timeout);
	CHECK_TRUE(dloc_arg_nom == gnss_config.dloc_arg_nom);
	CHECK_EQUAL(gnss_en, gnss_config.enable);
	CHECK_EQUAL(hdop_filter_enable, gnss_config.hdop_filter_enable);
	CHECK_EQUAL(hdop_filter_threshold, gnss_config.hdop_filter_threshold);
	CHECK_EQUAL(hacc_filter_enable, gnss_config.hacc_filter_enable);
	CHECK_EQUAL(hacc_filter_threshold, gnss_config.hacc_filter_threshold);
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
	unsigned int lb_ntry_per_message = 5U;
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
	store->write_param(ParamID::LB_NTRY_PER_MESSAGE, lb_ntry_per_message);
	store->write_param(ParamID::LB_ARGOS_DEPTH_PILE, lb_depth_pile);
	store->write_param(ParamID::LB_ARGOS_DUTY_CYCLE, lb_duty_cycle);
	store->write_param(ParamID::LB_ARGOS_MODE, lb_mode);
	store->write_param(ParamID::LB_ARGOS_POWER, lb_power);
	store->write_param(ParamID::TR_LB, lb_tr_nom);
	store->write_param(ParamID::LB_TRESHOLD, lb_thresh);

	// Notify battery level above threshold
	fake_battery_monitor->m_level = 100;

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
	fake_battery_monitor->m_level = 10;

	store->get_argos_configuration(argos_config);

	CHECK_EQUAL((unsigned int)lb_depth_pile, (unsigned int)argos_config.depth_pile);
	CHECK_EQUAL(dry_time_before_tx, argos_config.dry_time_before_tx);
	CHECK_EQUAL(lb_duty_cycle, argos_config.duty_cycle);
	CHECK_EQUAL(frequency, argos_config.frequency);
	CHECK_EQUAL((unsigned int)lb_mode, (unsigned int)argos_config.mode);
	CHECK_EQUAL(lb_ntry_per_message, argos_config.ntry_per_message);
	CHECK_EQUAL((unsigned int)lb_power, (unsigned int)argos_config.power);
	CHECK_EQUAL(lb_tr_nom, argos_config.tr_nom);
	CHECK_EQUAL(tx_counter, argos_config.tx_counter);

	// Notify battery level below threshold
	fake_battery_monitor->m_level = 1;

	store->get_argos_configuration(argos_config);

	CHECK_EQUAL((unsigned int)lb_depth_pile, (unsigned int)argos_config.depth_pile);
	CHECK_EQUAL(dry_time_before_tx, argos_config.dry_time_before_tx);
	CHECK_EQUAL(lb_duty_cycle, argos_config.duty_cycle);
	CHECK_EQUAL(frequency, argos_config.frequency);
	CHECK_EQUAL((unsigned int)lb_mode, (unsigned int)argos_config.mode);
	CHECK_EQUAL(lb_ntry_per_message, argos_config.ntry_per_message);
	CHECK_EQUAL((unsigned int)lb_power, (unsigned int)argos_config.power);
	CHECK_EQUAL(lb_tr_nom, argos_config.tr_nom);
	CHECK_EQUAL(tx_counter, argos_config.tx_counter);

	// Notify battery level 1% above threshold
	fake_battery_monitor->m_level = 11;

	store->get_argos_configuration(argos_config);

	CHECK_EQUAL((unsigned int)lb_depth_pile, (unsigned int)argos_config.depth_pile);
	CHECK_EQUAL(dry_time_before_tx, argos_config.dry_time_before_tx);
	CHECK_EQUAL(lb_duty_cycle, argos_config.duty_cycle);
	CHECK_EQUAL(frequency, argos_config.frequency);
	CHECK_EQUAL((unsigned int)lb_mode, (unsigned int)argos_config.mode);
	CHECK_EQUAL(lb_ntry_per_message, argos_config.ntry_per_message);
	CHECK_EQUAL((unsigned int)lb_power, (unsigned int)argos_config.power);
	CHECK_EQUAL(lb_tr_nom, argos_config.tr_nom);
	CHECK_EQUAL(tx_counter, argos_config.tx_counter);

	// Notify battery level 5% above threshold
	fake_battery_monitor->m_level = 15;

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

TEST(ConfigStore, ZoneExclusionCriteriaChecking) {

	LFSConfigurationStore *store;
	store = new LFSConfigurationStore(*main_filesystem);
	store->init();

	CHECK_FALSE(store->is_zone_exclusion());

	// Set last GPS coordinates
	GPSLogEntry gps_location;
	gps_location.info.day = 1;
	gps_location.info.month = 1;
	gps_location.info.year = 2021;
	gps_location.info.hour = 0;
	gps_location.info.min = 0;
	gps_location.info.sec = 0;
	gps_location.info.valid = 1;
	gps_location.info.lon = -2.118413;
	gps_location.info.lat = 51.3765242;
	store->notify_gps_location(gps_location);

	// Set up exclusion zone
	BaseZoneType zone_type = BaseZoneType::CIRCLE;
	store->write_param(ParamID::ZONE_TYPE, zone_type);
	double zone_center_longitude_x = -2.118413;
	store->write_param(ParamID::ZONE_CENTER_LONGITUDE, zone_center_longitude_x);
	double zone_center_latitude_y = 51.3765242;
	store->write_param(ParamID::ZONE_CENTER_LATITUDE, zone_center_latitude_y);
	unsigned int zone_delta_arg_loc_argos_seconds = 100;
	store->write_param(ParamID::ZONE_GNSS_DELTA_ARG_LOC_ARGOS_SECONDS, zone_delta_arg_loc_argos_seconds);
	unsigned int zone_radius_m = 100000;  // 100 km
	store->write_param(ParamID::ZONE_RADIUS, zone_radius_m);
	bool zone_enable_out_of_zone_detection_mode = true;
	store->write_param(ParamID::ZONE_ENABLE_OUT_OF_ZONE_DETECTION_MODE, zone_enable_out_of_zone_detection_mode);
	unsigned int zone_gnss_acquisition_timeout_seconds = 30;
	store->write_param(ParamID::ZONE_GNSS_ACQ_TIMEOUT, zone_gnss_acquisition_timeout_seconds);
	unsigned int zone_hdop_filter_threshold = 3;
	store->write_param(ParamID::ZONE_GNSS_HDOPFILT_THR, zone_hdop_filter_threshold);

	// Inside zone
	CHECK_FALSE(store->is_zone_exclusion());

	// Outside zone
	zone_center_longitude_x = -1.0;
	store->write_param(ParamID::ZONE_CENTER_LONGITUDE, zone_center_longitude_x);
	zone_center_latitude_y = 53;
	store->write_param(ParamID::ZONE_CENTER_LATITUDE, zone_center_latitude_y);
	CHECK_TRUE(store->is_zone_exclusion());

	// Enable activation date later than GPS time
	bool zone_enable_activation_date = true;
	store->write_param(ParamID::ZONE_ENABLE_ACTIVATION_DATE, zone_enable_activation_date	);
	std::time_t zone_activation_date = convert_epochtime(2021, 2, 1, 0, 0, 0);
	store->write_param(ParamID::ZONE_ACTIVATION_DATE, zone_activation_date);
	CHECK_FALSE(store->is_zone_exclusion());

	// Enable activation date before GPS time
	zone_activation_date = convert_epochtime(2020, 12, 31, 23, 59, 0);
	store->write_param(ParamID::ZONE_ACTIVATION_DATE, zone_activation_date);
	CHECK_TRUE(store->is_zone_exclusion());

	// Put back inside zone
	zone_center_longitude_x = -2.118413;
	store->write_param(ParamID::ZONE_CENTER_LONGITUDE, zone_center_longitude_x);
	zone_center_latitude_y = 51.3765242;
	store->write_param(ParamID::ZONE_CENTER_LATITUDE, zone_center_latitude_y);
	CHECK_FALSE(store->is_zone_exclusion());

	// Outside zone but monitoring disabled
	zone_enable_out_of_zone_detection_mode = false;
	store->write_param(ParamID::ZONE_ENABLE_OUT_OF_ZONE_DETECTION_MODE, zone_enable_out_of_zone_detection_mode);
	zone_center_longitude_x = -1.0;
	store->write_param(ParamID::ZONE_CENTER_LONGITUDE, zone_center_longitude_x);
	zone_center_latitude_y = 53;
	store->write_param(ParamID::ZONE_CENTER_LATITUDE, zone_center_latitude_y);
	CHECK_FALSE(store->is_zone_exclusion());
}

TEST(ConfigStore, RetrieveArgosConfigZoneExclusionMode)
{
	LFSConfigurationStore *store;
	store = new LFSConfigurationStore(*main_filesystem);
	store->init();

	// Set last GPS coordinates
	GPSLogEntry gps_location;
	gps_location.info.day = 1;
	gps_location.info.month = 1;
	gps_location.info.year = 2021;
	gps_location.info.hour = 0;
	gps_location.info.min = 0;
	gps_location.info.sec = 0;
	gps_location.info.valid = 1;
	gps_location.info.lon = -2.118413;
	gps_location.info.lat = 51.3765242;
	store->notify_gps_location(gps_location);

	// Set up exclusion zone
	BaseZoneType zone_type = BaseZoneType::CIRCLE;
	store->write_param(ParamID::ZONE_TYPE, zone_type);
	double zone_center_longitude_x = -2.118413;
	store->write_param(ParamID::ZONE_CENTER_LONGITUDE, zone_center_longitude_x);
	double zone_center_latitude_y = 51.3765242;
	store->write_param(ParamID::ZONE_CENTER_LATITUDE, zone_center_latitude_y);
	unsigned int zone_delta_arg_loc_argos_seconds = 100;
	store->write_param(ParamID::ZONE_GNSS_DELTA_ARG_LOC_ARGOS_SECONDS, zone_delta_arg_loc_argos_seconds);
	unsigned int zone_radius_m = 100000;  // 100 km
	store->write_param(ParamID::ZONE_RADIUS, zone_radius_m);
	bool zone_enable_out_of_zone_detection_mode = true;
	store->write_param(ParamID::ZONE_ENABLE_OUT_OF_ZONE_DETECTION_MODE, zone_enable_out_of_zone_detection_mode);
	unsigned int zone_gnss_acquisition_timeout_seconds = 30;
	store->write_param(ParamID::ZONE_GNSS_ACQ_TIMEOUT, zone_gnss_acquisition_timeout_seconds);
	unsigned int zone_hdop_filter_threshold = 3;
	store->write_param(ParamID::ZONE_GNSS_HDOPFILT_THR, zone_hdop_filter_threshold);
	unsigned int zone_argos_duty_cycle = 0xAAAAAAU;
	store->write_param(ParamID::ZONE_ARGOS_DUTY_CYCLE, zone_argos_duty_cycle);
	unsigned int zone_argos_ntry_per_message = 4;
	store->write_param(ParamID::ZONE_ARGOS_NTRY_PER_MESSAGE, zone_argos_ntry_per_message);
	BaseArgosDepthPile zone_argos_depth_pile = BaseArgosDepthPile::DEPTH_PILE_1;
	store->write_param(ParamID::ZONE_ARGOS_DEPTH_PILE, zone_argos_depth_pile);
	BaseArgosMode zone_argos_mode = BaseArgosMode::LEGACY;
	store->write_param(ParamID::ZONE_ARGOS_MODE, zone_argos_mode);
	BaseArgosPower zone_argos_power = BaseArgosPower::POWER_3_MW;
	store->write_param(ParamID::ZONE_ARGOS_POWER, zone_argos_power);
	unsigned int zone_argos_time_repetition_seconds = 120;
	store->write_param(ParamID::ZONE_ARGOS_REPETITION_SECONDS, zone_argos_time_repetition_seconds);

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
	zone_center_longitude_x = -1.0;
	store->write_param(ParamID::ZONE_CENTER_LONGITUDE, zone_center_longitude_x);
	zone_center_latitude_y = 53;
	store->write_param(ParamID::ZONE_CENTER_LATITUDE, zone_center_longitude_x);

	store->get_argos_configuration(argos_config);

	CHECK_EQUAL((unsigned int)zone_argos_depth_pile, (unsigned int)argos_config.depth_pile);
	CHECK_EQUAL(dry_time_before_tx, argos_config.dry_time_before_tx);
	CHECK_EQUAL(zone_argos_duty_cycle, argos_config.duty_cycle);
	CHECK_EQUAL(frequency, argos_config.frequency);
	CHECK_EQUAL((unsigned int)zone_argos_mode, (unsigned int)argos_config.mode);
	CHECK_EQUAL(zone_argos_ntry_per_message, argos_config.ntry_per_message);
	CHECK_EQUAL((unsigned int)zone_argos_power, (unsigned int)argos_config.power);
	CHECK_EQUAL(zone_argos_time_repetition_seconds, argos_config.tr_nom);
	CHECK_EQUAL(tx_counter, argos_config.tx_counter);
}


TEST(ConfigStore, RetrieveGPSConfigZoneExclusionMode)
{
	LFSConfigurationStore *store;
	store = new LFSConfigurationStore(*main_filesystem);
	store->init();

	// Set last GPS coordinates
	GPSLogEntry gps_location;
	gps_location.info.day = 1;
	gps_location.info.month = 1;
	gps_location.info.year = 2021;
	gps_location.info.hour = 0;
	gps_location.info.min = 0;
	gps_location.info.sec = 0;
	gps_location.info.valid = 1;
	gps_location.info.lon = -2.118413;
	gps_location.info.lat = 51.3765242;
	store->notify_gps_location(gps_location);

	// Set up exclusion zone
	BaseZoneType zone_type = BaseZoneType::CIRCLE;
	store->write_param(ParamID::ZONE_TYPE, zone_type);
	double zone_center_longitude_x = -2.118413;
	store->write_param(ParamID::ZONE_CENTER_LONGITUDE, zone_center_longitude_x);
	double zone_center_latitude_y = 51.3765242;
	store->write_param(ParamID::ZONE_CENTER_LATITUDE, zone_center_latitude_y);
	unsigned int zone_delta_arg_loc_argos_seconds = 100;
	store->write_param(ParamID::ZONE_GNSS_DELTA_ARG_LOC_ARGOS_SECONDS, zone_delta_arg_loc_argos_seconds);
	unsigned int zone_radius_m = 100000;  // 100 km
	store->write_param(ParamID::ZONE_RADIUS, zone_radius_m);
	bool zone_enable_out_of_zone_detection_mode = true;
	store->write_param(ParamID::ZONE_ENABLE_OUT_OF_ZONE_DETECTION_MODE, zone_enable_out_of_zone_detection_mode);
	unsigned int zone_gnss_acquisition_timeout_seconds = 30;
	store->write_param(ParamID::ZONE_GNSS_ACQ_TIMEOUT, zone_gnss_acquisition_timeout_seconds);
	unsigned int zone_hdop_filter_threshold = 3;
	store->write_param(ParamID::ZONE_GNSS_HDOPFILT_THR, zone_hdop_filter_threshold);

	// Set default params
	unsigned int hdop_filter_threshold = 10;
	bool hdop_filter_enable = true;
	bool gnss_en = true;
	unsigned int dloc_arg_nom = 1440*60U;
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

	// Set outside zone (DLOC specified)
	zone_center_longitude_x = -1.0;
	store->write_param(ParamID::ZONE_CENTER_LONGITUDE, zone_center_longitude_x);
	zone_center_latitude_y = 53;
	store->write_param(ParamID::ZONE_CENTER_LATITUDE, zone_center_latitude_y);

	store->get_gnss_configuration(gnss_config);

	CHECK_EQUAL(zone_gnss_acquisition_timeout_seconds, gnss_config.acquisition_timeout);
	CHECK_EQUAL((unsigned int)zone_delta_arg_loc_argos_seconds, (unsigned int)gnss_config.dloc_arg_nom);
	CHECK_EQUAL(gnss_en, gnss_config.enable);
	CHECK_EQUAL(hdop_filter_enable, gnss_config.hdop_filter_enable);
	CHECK_EQUAL(zone_hdop_filter_threshold, gnss_config.hdop_filter_threshold);
}
