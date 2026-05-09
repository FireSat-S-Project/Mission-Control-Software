/**
 * @file    StorageTask.cpp
 * @project FireSat-S — Sustainable Wildfire Detection Satellite
 * @brief   FreeRTOS task — on-board flash storage and S-band downlink buffer
 * @author  FireSat-S Team | AESH 2026
 *
 * Responsibilities:
 *  - Blocks indefinitely on xQueueStorage waiting for confirmed-fire IRFrame_t
 *    packets posted by SensingTask.
 *  - Writes each frame to flash with a CRC32 integrity header (radiation
 *    bit-flip protection — mandatory for space-grade storage).
 *  - Maintains a circular write index (not a monotonically incrementing counter)
 *    so framesStored never exceeds MAX_STORED_FRAMES and never wraps to
 *    undefined behaviour.
 *  - Oldest frames are silently overwritten once flash is full (ring buffer
 *    semantics). A warning log is emitted on each overwrite.
 *  - Runs at LOW priority (1) — executes only when higher-priority tasks
 *    (OrbitalTask, SensingTask, CommunicationTask) are blocked or sleeping.
 *
 * Flash wear leveling:
 *  The circular write index distributes writes across MAX_STORED_FRAMES flash
 *  pages, limiting any single page to at most (mission_lifetime / MAX_STORED_FRAMES)
 *  write cycles.  A full wear-leveling algorithm (sector rotation with a bad-block
 *  table) should be added before Flight Model.
 *
 * CRC:
 *  Each stored packet includes a CRC32 computed over the raw IRFrame_t bytes.
 *  On S-band downlink, the ground segment verifies the CRC and requests
 *  retransmission if a mismatch is detected (radiation-induced single-event upset).
 *  TODO: replace the stub with HAL_CRC_Calculate() on STM32H7.
 */

#include "TinyML_FireModel.h"
#include "Queues.h"
#include "Logger.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>
#include <stdint.h>

/*——— Flash storage parameters ———————————————————————————————————————————————*/

#define FLASH_PAGE_SIZE_BYTES   4096U

/** Maximum confirmed-fire frames held on flash at any one time. */
#define MAX_STORED_FRAMES       256U

/*——— Stored frame packet: IRFrame_t + CRC32 header ——————————————————————————*/

typedef struct {
    uint32_t   crc32;          /**< CRC32 over the raw IRFrame_t bytes          */
    uint32_t   magic;          /**< 0xF1RE5A7E — quick flash-validity sentinel  */
    IRFrame_t  frame;          /**< Thermal data + geo-tag + timestamp          */
} StoredFrame_t;

#define STORED_FRAME_MAGIC   0xF1RE5A7EUL

/*——— Module state ————————————————————————————————————————————————————————————*/

TaskHandle_t xStorageTaskHandle = NULL;

/**
 * @brief  Circular write index into the flash frame store.
 *
 * FIXED: previously used a monotonically incrementing counter that could
 * exceed MAX_STORED_FRAMES (e.g. reach 300, 400, 1000) without bound.
 * Now wraps modulo MAX_STORED_FRAMES — always a valid flash page index.
 */
static uint32_t s_writeIndex = 0U;

/**
 * @brief  Total frames written since boot (monotonic, for telemetry only).
 *         Not used as a flash address — s_writeIndex is.
 */
static uint32_t s_framesWrittenTotal = 0U;

/*——— Private helpers ————————————————————————————————————————————————————————*/

/**
 * @brief  Compute CRC32 over an arbitrary byte buffer.
 *         Polynomial: 0xEDB88320 (IEEE 802.3 standard, same as STM32 HW CRC).
 *
 *         TODO: replace software CRC with HAL_CRC_Calculate() on STM32H7 to
 *               use the hardware CRC accelerator (saves ~100 µs per frame).
 *
 * @param  data  Pointer to data buffer.
 * @param  len   Buffer length in bytes.
 * @return CRC32 value.
 */
static uint32_t compute_crc32(const void *data, size_t len)
{
    const uint8_t *p    = (const uint8_t *)data;
    uint32_t       crc  = 0xFFFFFFFFUL;

    while (len--) {
        crc ^= (uint32_t)(*p++);
        for (uint8_t bit = 0U; bit < 8U; bit++) {
            if (crc & 1U) {
                crc = (crc >> 1U) ^ 0xEDB88320UL;
            } else {
                crc >>= 1U;
            }
        }
    }
    return crc ^ 0xFFFFFFFFUL;
}

/**
 * @brief  Write one StoredFrame_t to the flash page at s_writeIndex.
 *
 * @param  sf  Pointer to fully populated StoredFrame_t (including CRC).
 * @return true on successful write, false on driver error.
 */
static bool flash_write_frame(const StoredFrame_t *sf)
{
    if (sf == NULL) return false;

    /* TODO: Flash_writePage(s_writeIndex * FLASH_PAGE_SIZE_BYTES,
     *                       sf, sizeof(*sf))
     *       — replace with actual STM32H7 flash driver call.
     *
     * Ensure sizeof(StoredFrame_t) <= FLASH_PAGE_SIZE_BYTES (currently
     * sizeof(IRFrame_t) = 64×64×2 + 16 = 8208 bytes, so use 3 pages or
     * switch to a larger page boundary — align in hw config before flight).
     */
    (void)sf;
    return true;   /* prototype: always succeeds */
}

/*============================================================================*/

void StorageTask(void *pvParameters)
{
    (void)pvParameters;

    Logger_log("StorageTask: ready — flash buffer max %u frames  "
               "page %u bytes  struct %u bytes",
               MAX_STORED_FRAMES,
               FLASH_PAGE_SIZE_BYTES,
               (unsigned)sizeof(StoredFrame_t));

    IRFrame_t frame;

    for (;;) {

        /*——— Block indefinitely until SensingTask posts a confirmed-fire frame */
        if (xQueueReceive(xQueueStorage, &frame, portMAX_DELAY) != pdPASS) {
            continue;   /* should never happen with portMAX_DELAY, but be safe */
        }

        /*——— Build a stored-frame packet with CRC ——————————————————————————*/
        StoredFrame_t sf;
        sf.magic = STORED_FRAME_MAGIC;
        memcpy(&sf.frame, &frame, sizeof(IRFrame_t));
        sf.crc32 = compute_crc32(&sf.frame, sizeof(IRFrame_t));

        /*——— Check for flash-full (ring-buffer overwrite) ——————————————————*/
        if (s_writeIndex == 0U && s_framesWrittenTotal > 0U) {
            Logger_logLevel(LOG_WARNING,
                            "StorageTask: flash buffer full (%u frames) — "
                            "oldest overwritten (write index wrapped)",
                            MAX_STORED_FRAMES);
        }

        /*——— Write to flash ——————————————————————————————————————————————*/
        if (!flash_write_frame(&sf)) {
            Logger_logLevel(LOG_ERROR,
                            "StorageTask: flash write failed at index %lu",
                            (unsigned long)s_writeIndex);
            /* Do NOT advance the index on write failure — retry next frame. */
            continue;
        }

        /*——— Advance circular write index (FIXED: was framesStored++) —————
         *
         * Old (buggy):  framesStored++;
         *   → could reach 300, 400, 1000 with no bound, corrupting flash
         *     address calculations in flash_write_frame().
         *
         * New (correct): circular modulo wrap — always a valid page index.
         *————————————————————————————————————————————————————————————————————*/
        s_writeIndex = (s_writeIndex + 1U) % MAX_STORED_FRAMES;
        s_framesWrittenTotal++;

        Logger_log("StorageTask: frame stored  total=%lu  idx=%lu  "
                   "crc=0x%08lX  lat=%.3f  lon=%.3f  ts=%lu",
                   (unsigned long)s_framesWrittenTotal,
                   (unsigned long)((s_writeIndex == 0U)
                                    ? MAX_STORED_FRAMES - 1U
                                    : s_writeIndex - 1U),
                   (unsigned long)sf.crc32,
                   frame.lat_center,
                   frame.lon_center,
                   (unsigned long)frame.timestamp);

    } /* for (;;) */
}
