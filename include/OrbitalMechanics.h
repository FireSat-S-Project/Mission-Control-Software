/**
 * @file    OrbitalMechanics.h
 * @project FireSat-S — Sustainable Wildfire Detection Satellite
 * @brief   Orbital position estimation via SGP4 TLE propagation (no GPS)
 * @author  FireSat-S Team | AESH 2026
 *
 * @note    Satellite position is computed on-board from Two-Line Element (TLE)
 *          sets uplinked from ground stations. This eliminates GPS hardware
 *          (saves ~2 W, ~0.4 kg) and improves reliability at 700 km altitude
 *          where GPS signal geometry is suboptimal.
 *
 *          Algorithm: Simplified General Perturbations 4 (SGP4)
 *          Reference: Vallado et al., "Revisiting Spacetrack Report #3"
 *          Accuracy:  < 1 km position error for TLE age < 3 days
 */

#ifndef ORBITAL_MECHANICS_H
#define ORBITAL_MECHANICS_H

#include <stdint.h>
#include <stdbool.h>

/* ── Mission orbital parameters ───────────────────────────────────────────── */
#define ORBIT_ALTITUDE_KM      700.0f   /* nominal altitude                   */
#define ORBIT_INCLINATION_DEG   98.2f   /* Sun-Synchronous Orbit (SSO)        */
#define ORBIT_PERIOD_MIN        98.8f   /* one full orbit                     */
#define ORBIT_VELOCITY_KMS       7.5f   /* orbital speed                      */
#define ACTIVE_FRACTION          0.08f  /* fraction of orbit over target zone */

/* ── Target geofence — Algeria + Tunisia ─────────────────────────────────── */
#define GEO_LAT_MIN   18.0f   /* degrees North                               */
#define GEO_LAT_MAX   38.0f   /* degrees North                               */
#define GEO_LON_MIN   -3.0f   /* degrees East                                */
#define GEO_LON_MAX   13.5f   /* degrees East                                */

/* ── TLE element set (uplinked from ground, stored on-board) ─────────────── */
typedef struct {
    char    line1[70];          /* TLE line 1 — epoch, drag, etc.            */
    char    line2[70];          /* TLE line 2 — inclination, RAAN, e, etc.   */
    uint32_t uplink_timestamp;  /* Unix time of last ground uplink           */
    float   age_hours;          /* TLE age — validity degrades after 3 days  */
} TLESet_t;

/* ── Current satellite geographic position ───────────────────────────────── */
typedef struct {
    float    lat;               /* geodetic latitude  (degrees, -90 to +90)  */
    float    lon;               /* geodetic longitude (degrees, -180 to +180)*/
    float    alt_km;            /* altitude above WGS84 ellipsoid (km)       */
    float    velocity_kms;      /* orbital speed (km/s)                      */
    uint32_t timestamp;         /* Unix time of this position fix            */
    bool     tle_valid;         /* false if TLE age > 72 hours               */
} GeoPosition_t;

/* ── Public API ───────────────────────────────────────────────────────────── */

/**
 * @brief  Initialise SGP4 propagator with stored TLE set.
 * @return true on success, false if stored TLE is missing or corrupt.
 */
bool OrbitalMechanics_init(void);

/**
 * @brief  Propagate current satellite position using SGP4.
 *         Called by OrbitalTask every 10 seconds.
 * @return GeoPosition_t struct with lat/lon/alt and validity flag.
 */
GeoPosition_t OrbitalMechanics_getCurrentPosition(void);

/**
 * @brief  Check if current position is inside the target geofence.
 * @param  pos  pointer to a GeoPosition_t from getCurrentPosition()
 * @return true if satellite is over Algeria / Tunisia target zone.
 */
bool OrbitalMechanics_isInTargetZone(const GeoPosition_t *pos);

/**
 * @brief  Update stored TLE set from ground uplink data.
 * @param  tle   new TLE set received via S-band or UHF uplink command
 * @return true on successful validation and storage.
 */
bool OrbitalMechanics_updateTLE(const TLESet_t *tle);

/**
 * @brief  Estimate seconds until next entry into target zone.
 *         Used by PowerManager to pre-warm MWIR cooler in advance.
 * @return seconds until next overpass, or 0 if currently in zone.
 */
uint32_t OrbitalMechanics_secondsToNextPass(void);

#endif /* ORBITAL_MECHANICS_H */

