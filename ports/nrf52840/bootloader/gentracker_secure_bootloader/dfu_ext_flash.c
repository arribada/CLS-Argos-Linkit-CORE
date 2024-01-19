#include <stdint.h>

#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#ifdef GD25Q16C
	#include "gd25_flash.h"
	#include "GD25Q16C.h"
#else
	#include "is25_flash.h"
	#include "IS25LP128F.h"
#endif
#include "crc32.h"
#include "nordic_common.h"
#include "nrf_dfu_settings.h"
#include "nrf_dfu_utils.h"
#include "nrf_nvic.h"


struct __attribute__((packed)) image_header {
	unsigned int image_size;
	unsigned int crc32;
};


#define FIRMWARE_UPDATE_REGION_SIZE		(1024 * 1024)

#define MAX_IMAGE_SIZE  (FIRMWARE_UPDATE_REGION_SIZE - sizeof(struct image_header))

#ifdef GD25Q16C
static const unsigned int FIRMWARE_UPDATE_BLOCK_OFFSET = GD25_BLOCK_COUNT - (FIRMWARE_UPDATE_REGION_SIZE / GD25_BLOCK_SIZE);
#else
static const unsigned int FIRMWARE_UPDATE_BLOCK_OFFSET = IS25_BLOCK_COUNT - (FIRMWARE_UPDATE_REGION_SIZE / IS25_BLOCK_SIZE);
#endif


static uint32_t compute_crc_in_flash(uint32_t base_addr, uint32_t size)
{
	uint32_t crc = 0;
	uint32_t offset = 0;

	while (size > 0)
	{
		uint8_t data[PAGE_SIZE];
		uint32_t length = MIN(PAGE_SIZE, size);


#ifdef GD25Q16C
		gd25_flash_read(base_addr + offset, data, length);
#else
		is25_flash_read(base_addr + offset, data, length);
#endif

		crc = crc32_compute(data, length, &crc);

		size -= length;
		offset += length;
	}

	return crc;
}


static uint32_t ext_image_copy(uint32_t dst_addr,
                           	   uint32_t src_addr,
							   uint32_t size)
{
    uint32_t ret_val = NRF_SUCCESS;
    uint32_t src_page[CODE_PAGE_SIZE / sizeof(uint32_t)];

    while (size > 0)
    {
        uint32_t bytes = MIN(size, CODE_PAGE_SIZE);

        NRF_LOG_INFO("Copying 0x%x to 0x%x, size: 0x%x", src_addr, dst_addr, bytes);
        NRF_LOG_FLUSH();

        // Read ex flash page

#ifdef GD25Q16C
    	gd25_flash_read(src_addr, (uint8_t *)src_page, bytes);
#else
    	is25_flash_read(src_addr, (uint8_t *)src_page, bytes);
#endif

    	// Erase the target pages
        ret_val = nrf_dfu_flash_erase(dst_addr, 1, NULL);
        if (ret_val != NRF_SUCCESS)
        {
            return ret_val;
        }

        // Flash one page
        ret_val = nrf_dfu_flash_store(dst_addr,
                                      src_page,
                                      ALIGN_NUM(sizeof(uint32_t), bytes),
                                      NULL);
        if (ret_val != NRF_SUCCESS)
        {
            return ret_val;
        }

        size        -= bytes;
        dst_addr    += bytes;
        src_addr    += bytes;

        NRF_LOG_INFO("Done");
        NRF_LOG_FLUSH();
    }

    return ret_val;
}

static int apply_update(uint32_t src_addr, uint32_t image_size)
{
    uint32_t target_addr = nrf_dfu_bank0_start_addr();

    if (ext_image_copy(target_addr, src_addr, image_size) != NRF_SUCCESS)
    	return -1;

    return 0;
}

static int validate_update(uint32_t size, uint32_t expected_crc32)
{
    // Check the CRC of the copied data. Enable if so.
    uint32_t crc = crc32_compute((uint8_t*)nrf_dfu_bank0_start_addr(), size, NULL);

    if (crc == expected_crc32)
    {
        s_dfu_settings.bank_0.bank_code = NRF_DFU_BANK_VALID_APP;
        s_dfu_settings.bank_0.image_crc = crc;
        s_dfu_settings.bank_0.image_size = size;
        s_dfu_settings.boot_validation_app.type = NO_VALIDATION;
        return 0;
    } else {
        s_dfu_settings.bank_0.bank_code = NRF_DFU_BANK_INVALID;
        return -1;
    }
}

void ext_flash_update(void)
{
	struct image_header header;

	NRF_LOG_INFO("ext_flash_update: checking for a new firmware image...");

	// Initialise flash memory
	

#ifdef GD25Q16C
	if (gd25_flash_init())
		return;

	// Read header providing us with size and crc32
	gd25_flash_read(FIRMWARE_UPDATE_BLOCK_OFFSET * GD25_BLOCK_SIZE, (uint8_t *)&header, sizeof(header));

	if (header.image_size <= MAX_IMAGE_SIZE)
	{
		NRF_LOG_INFO("ext_flash_update: new firmware image detected size=%u crc32=%08x",
				     header.image_size, header.crc32);

		uint32_t crc32 = compute_crc_in_flash((FIRMWARE_UPDATE_BLOCK_OFFSET * GD25_BLOCK_SIZE) + sizeof(header), header.image_size);
		if (crc32 != header.crc32)
		{
			NRF_LOG_ERROR("ext_flash_update: computed checksum %08x does not match - aborting", crc32);
			gd25_flash_erase(FIRMWARE_UPDATE_BLOCK_OFFSET);
			gd25_flash_deinit();
			return;
		}


		// Initialise DFU settings
		nrf_dfu_settings_init(false);

		NRF_LOG_INFO("ext_flash_update: applying firmware update...");
		apply_update((FIRMWARE_UPDATE_BLOCK_OFFSET * GD25_BLOCK_SIZE) + sizeof(header), header.image_size);
#else
	if (is25_flash_init())
		return;

	// Read header providing us with size and crc32
	is25_flash_read(FIRMWARE_UPDATE_BLOCK_OFFSET * IS25_BLOCK_SIZE, (uint8_t *)&header, sizeof(header));

	if (header.image_size <= MAX_IMAGE_SIZE)
	{
		NRF_LOG_INFO("ext_flash_update: new firmware image detected size=%u crc32=%08x",
				     header.image_size, header.crc32);

		uint32_t crc32 = compute_crc_in_flash((FIRMWARE_UPDATE_BLOCK_OFFSET * IS25_BLOCK_SIZE) + sizeof(header), header.image_size);
		if (crc32 != header.crc32)
		{
			NRF_LOG_ERROR("ext_flash_update: computed checksum %08x does not match - aborting", crc32);
			is25_flash_erase(FIRMWARE_UPDATE_BLOCK_OFFSET);
			is25_flash_deinit();
			return;
		}

		// Initialise DFU settings
		nrf_dfu_settings_init(false);

		NRF_LOG_INFO("ext_flash_update: applying firmware update...");
		apply_update((FIRMWARE_UPDATE_BLOCK_OFFSET * IS25_BLOCK_SIZE) + sizeof(header), header.image_size);
#endif

		NRF_LOG_INFO("ext_flash_update: validating firmware update...")
		if (validate_update(header.image_size, header.crc32))
		{
			NRF_LOG_ERROR("ext_flash_update: validation error....firmware update process will need to be repeated");
		}
		else
		{
			NRF_LOG_INFO("ext_flash_update: validation success");
		}

		NRF_LOG_INFO("ext_flash_update: erasing ext flash...");
	    NRF_LOG_FLUSH();


#ifdef GD25Q16C
		gd25_flash_erase(FIRMWARE_UPDATE_BLOCK_OFFSET);
#else
		is25_flash_erase(FIRMWARE_UPDATE_BLOCK_OFFSET);
#endif

	    // Save settings
	    nrf_dfu_settings_write_and_backup(NULL);

	    // Reboot
		NRF_LOG_INFO("ext_flash_update: rebooting...");
	    NRF_LOG_FINAL_FLUSH();
		sd_nvic_SystemReset();
	}
	else
	{
		NRF_LOG_INFO("ext_flash_update: no firmware image detected");
	}


#ifdef GD25Q16C
	gd25_flash_deinit();
#else
	is25_flash_deinit();
#endif
}

