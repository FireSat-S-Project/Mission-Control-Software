/**
 * @file    TinyML_FireModel.h
 * @project FireSat-S — Sustainable Wildfire Detection Satellite
 * @brief   On-board AI fire detection inference API (TinyML / Edge AI)
 * @author  FireSat-S Team | AESH 2026
 *
 * @note    Lightweight CNN model deployed on edge AI co-processor (8 W active).
 *          Input : MWIR thermal frame at 100 m GSD
 *          Output: fire detected flag + confidence score + bounding coordinates
 *          Threshold: confidence > 0.92 (tuned to minimise false positives)
 *          Capability: detects fires >= 1 hectare with >95% accuracy
 *
 *          Data reduction benefit:
 *            Only fire alerts (< 200 bytes) are sent immediately via UHF.
 *            Full images are queued for S-band downlink only when fire confirmed.
 *            This reduces comms power by ~85% (fires in ~15% of target passes).
 */

#ifndef TINYML_FIRE_MODEL_H
#define TINYML_FIRE_MODEL_H

#include <stdint.h>
#include <stdbool.h>

/* ── Model parameters ─────────────────────────────────────────────────────── */
#define MODEL_INPUT_WIDTH      64     /* pixels (100m GSD, downsampled)       */
#define MODEL_INPUT_HEIGHT     64     /* pixels                               */
#define MODEL_CONFIDENCE_THRES 0.92f  /* minimum confidence to raise alert    */
#define MODEL_MIN_FIRE_HA      1.0f   /* minimum detectable fire size         */

/* ── Raw MWIR thermal frame from imager ──────────────────────────────────── */
typedef struct {
    uint16_t pixels[64][64];    /* 14-bit thermal values, downsampled        */
    float    lat_center;        /* latitude of frame center (degrees)        */
    float    lon_center;        /* longitude of frame center (degrees)       */
    uint32_t timestamp;         /* Unix time of capture                      */
} IRFrame_t;

/* ── AI inference result ──────────────────────────────────────────────────── */
typedef struct {
    bool     fireDetected;      /* true if confidence > MODEL_CONFIDENCE_THRES */
    float    confidence;        /* 0.0 – 1.0 from CNN output layer            */
    float    lat;               /* estimated fire centre latitude (degrees)   */
    float    lon;               /* estimated fire centre longitude (degrees)  */
    float    area_ha;           /* estimated fire area in hectares            */
    uint32_t timestamp;         /* Unix time of inference                     */
} FireAlert_t;

/* ── Public API ───────────────────────────────────────────────────────────── */

/**
 * @brief  Load model weights into AI co-processor SRAM.
 *         Called once at boot by SensingTask initialisation.
 * @return true on success, false if model flash checksum fails.
 */
bool TinyML_init(void);

/**
 * @brief  Run fire detection inference on one thermal frame.
 *         Execution time: ~120 ms on edge AI co-processor @ 8 W.
 * @param  frame  pointer to captured IRFrame_t from MWIR imager
 * @return FireAlert_t with detection result and confidence score.
 */
FireAlert_t TinyML_runInference(const IRFrame_t *frame);

/**
 * @brief  Return model version string (for telemetry / ground verification).
 * @return null-terminated version string, e.g. "FireSat-CNN-v1.3"
 */
const char *TinyML_getModelVersion(void);

#endif /* TINYML_FIRE_MODEL_H */
