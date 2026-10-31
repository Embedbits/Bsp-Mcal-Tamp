/**
 * \author Mr.Nobody
 * \file Tamp_Types.h
 * \ingroup Tamp
 * \brief Tamp module global types definition
 *
 * This file contains the types definitions used across the module and are
 * available for other modules through the Port file.
 *
 * Pin tables (external tamper input / active tamper output) were hand-transcribed from the
 * text dump printed by a one-off reader script (no C code emission, only a diagnostic dump
 * of {signal: pin} per device line) that combines two sources, never invented:
 *   1. CubeMX MCU-level XML of the STM32CubeMX database (one representative part per
 *      device line, the largest-package part of the line), its pin/signal list
 *      ("TAMP_INx" / "TAMP_OUTx" entries): the fixed (port,pin) of every
 *      TAMP_INx / TAMP_OUTx signal that exists on the line. TAMP has no GPIO_AF entry at all
 *      (checked the GPIO IP Modes XML of the same database: zero PinSignal for "TAMP"), so
 *      this is a different CubeMX table than the GPIO-AF one used by the other pin-table
 *      modules.
 *   2. RM0481 Rev 5 (STM32H523/533/543/553/562/563/573) 52.7.17 and RM0517 Rev 1 (STM32H5E4/E5/F4/F5)
 *      58.6.17 "TAMP option register (TAMP_OR)": IN2_RMP / IN3_RMP / IN4_RMP / OUT3_RMP[1:0] /
 *      OUT5_RMP give the alternate (port,pin) of a signal (byte-identical text in both RMs).
 *      RM0492 Rev 3 (STM32H503) documents no TAMP_OR section at all: STM32H503 TAMP_IN1/IN2/
 *      OUT1/OUT2 are fixed, no alternates.
 * Device line availability of each signal / alternate was cross-checked by hand against the
 * physical pin list of the representative package (port I - PI8 / PI11 - only exists in the
 * CMSIS header of STM32H562/563/573/5E4/5E5/5F4/5F5). Guarded here by
 * `TAMP_LINE_HAS_EXT_6_7_8` (defined further down in this file), not by a direct
 * `defined(GPIOI)` check: this file does not
 * include the CMSIS header, so an earlier `defined(GPIOI)` guard here evaluated false even
 * on lines that do have port I, because `GPIOI` was never defined when this header was
 * preprocessed on its own (found when STM32H503 - which has neither - compiled fine while
 * the lines that do have port I referenced an undeclared enumerator).
 *
 */

/**
 * \defgroup Tamp Tamp
 * \brief Tamp module
 */

#ifndef TAMP_TAMP_TYPES_H
#define TAMP_TAMP_TYPES_H
/* ============================== INCLUDES ================================== */
#include "stdint.h"                         /* Module types definition        */
/* ========================== SYMBOLIC CONSTANTS ============================ */

/** Null pointer definition */
#define TAMP_NULL_PTR                        ( ( void* ) 0u )

/** Number of protected backup register areas (zone 1 / zone 2 / zone 3) */
#define TAMP_BACKUP_AREA_CNT                  ( 3u )

/**
 * \brief Composite device-line feature macros, defined once here (so every guard below is a
 *        single condition) instead of repeating the device-line OR list at every use.
 *
 * TAMP_LINE_HAS_EXT_6_7_8: lines with TAMP_EXT_3/6/7/8 and TAMP_IN3/6/7/8, TAMP_OUT5/6/7/8
 * (RM0481 / RM0517 "full" TAMP instance).
 * TAMP_LINE_HAS_EXT_4_5: lines with TAMP_EXT_4/5 (every line above, plus H523/533/543/553).
 */
#if defined(STM32H562xx) || \
    defined(STM32H563xx) || \
    defined(STM32H573xx) || \
    defined(STM32H5E4xx) || \
    defined(STM32H5E5xx) || \
    defined(STM32H5F4xx) || \
    defined(STM32H5F5xx)
#define TAMP_LINE_HAS_EXT_6_7_8                ( 1u )
#endif

#if defined(STM32H523xx) || \
    defined(STM32H533xx) || \
    defined(STM32H543xx) || \
    defined(STM32H553xx) || \
    defined(TAMP_LINE_HAS_EXT_6_7_8)
#define TAMP_LINE_HAS_EXT_4_5                  ( 1u )
#endif

/* ========================== EXPORTED MACROS =============================== */

/* ============================== TYPEDEFS ================================== */

/** \brief Type signaling major version of SW module */
typedef uint8_t tamp_MajorVersion_t;

/** \brief Type signaling minor version of SW module */
typedef uint8_t tamp_MinorVersion_t;

/** \brief Type signaling patch version of SW module */
typedef uint8_t tamp_PatchVersion_t;

/** \brief Type signaling actual version of SW module */
typedef struct
{
    tamp_MajorVersion_t Major; /**< Major version */
    tamp_MinorVersion_t Minor; /**< Minor version */
    tamp_PatchVersion_t Patch; /**< Patch version */
}   tamp_ModuleVersion_t;


/** Enumeration used to signal request processing state */
typedef enum
{
    TAMP_REQUEST_ERROR = 0u, /**< Processing request failed  */
    TAMP_REQUEST_OK          /**< Processing request succeed */
}   tamp_RequestState_t;


/** Function status enumeration */
typedef enum
{
    TAMP_FUNCTION_INACTIVE = 0u, /**< Function status is inactive */
    TAMP_FUNCTION_ACTIVE         /**< Function status is active   */
}   tamp_FunctionState_t;


/** Flag states enumeration */
typedef enum
{
    TAMP_FLAG_INACTIVE = 0u, /**< Inactive flag state */
    TAMP_FLAG_ACTIVE         /**< Active flag state   */
}   tamp_FlagState_t;


/** \brief Interrupt priority type definition */
typedef uint32_t tamp_IrqPrio_t;


/**
 * \brief External tamper input identification.
 *
 * TAMP_EXT_1 .. TAMP_EXT_8 map one to one to the hardware TAMPx inputs. Guarded per line:
 * every STM32H5 line has at least TAMP_EXT_1/2, STM32H523/533/543/553 add TAMP_EXT_4/5,
 * STM32H562/563/573/5E4/5E5/5F4/5F5 have the full TAMP_EXT_1..8.
 * Source: availability_STM32H5.md Tamp row + the CubeMX signal list (see the file header above).
 */
typedef enum
{
    TAMP_EXT_1 = 0u, /**< TAMP1 external tamper input (all STM32H5 lines)          */
    TAMP_EXT_2,      /**< TAMP2 external tamper input (all STM32H5 lines)          */
#if defined(TAMP_LINE_HAS_EXT_6_7_8)
    TAMP_EXT_3,      /**< TAMP3 external tamper input                              */
#endif
#if defined(TAMP_LINE_HAS_EXT_4_5)
    TAMP_EXT_4,      /**< TAMP4 external tamper input                              */
    TAMP_EXT_5,      /**< TAMP5 external tamper input                              */
#endif
#if defined(TAMP_LINE_HAS_EXT_6_7_8)
    TAMP_EXT_6,      /**< TAMP6 external tamper input                              */
    TAMP_EXT_7,      /**< TAMP7 external tamper input                              */
    TAMP_EXT_8,      /**< TAMP8 external tamper input                              */
#endif
    TAMP_EXT_CNT     /**< Count of available external tamper inputs on this device */
}   tamp_ExternalId_t;


/**
 * \brief External tamper input pin table.
 *
 * One enumerator per valid (TAMP input number, port, pin) combination actually present on the
 * device line (never a free pin choice, never an encoded integer). Where RM0481 / RM0517 offer
 * a TAMP_OR remap bit, every valid alternative is its own enumerator; \ref Tamp_InitInputGpio
 * derives both the GPIO port/pin and the TAMP_OR bit value from the chosen enumerator through
 * \ref Tamp_Reg_Get_InputPinInfo, so the TAMP_OR bit is a consequence of the enum value, never a
 * separate free parameter.
 */
typedef enum
{
    TAMP_IN1_PIN_PC13 = 0u,     /**< TAMP_IN1 on PC13 (fixed, all lines)                      */
    TAMP_IN2_PIN_PA0,           /**< TAMP_IN2 on PA0 (TAMP_OR.IN2_RMP = 0, all lines with IN2) */
#if defined(TAMP_LINE_HAS_EXT_6_7_8)
    TAMP_IN2_PIN_PI8,           /**< TAMP_IN2 on PI8 (TAMP_OR.IN2_RMP = 1)                     */
#endif
#if defined(TAMP_LINE_HAS_EXT_6_7_8)
    TAMP_IN3_PIN_PC1,           /**< TAMP_IN3 on PC1 (TAMP_OR.IN3_RMP = 0)                     */
    TAMP_IN3_PIN_PE6,           /**< TAMP_IN3 on PE6 (TAMP_OR.IN3_RMP = 1)                     */
#endif
#if defined(TAMP_LINE_HAS_EXT_4_5)
    TAMP_IN4_PIN_PA2,           /**< TAMP_IN4 on PA2 (TAMP_OR.IN4_RMP = 0)                     */
#if defined(TAMP_LINE_HAS_EXT_6_7_8)
    TAMP_IN4_PIN_PI11,          /**< TAMP_IN4 on PI11 (TAMP_OR.IN4_RMP = 1)                    */
#endif
    TAMP_IN5_PIN_PA1,           /**< TAMP_IN5 on PA1 (fixed)                                   */
#endif
#if defined(TAMP_LINE_HAS_EXT_6_7_8)
    TAMP_IN6_PIN_PE3,           /**< TAMP_IN6 on PE3 (fixed)                                   */
    TAMP_IN7_PIN_PE4,           /**< TAMP_IN7 on PE4 (fixed)                                   */
    TAMP_IN8_PIN_PE5,           /**< TAMP_IN8 on PE5 (fixed)                                   */
#endif
    TAMP_IN_PIN_UNUSED,         /**< No pin assigned to the tamper input                       */
    TAMP_IN_PIN_CNT             /**< Count of available tamper input pin items                */
}   tamp_InputPin_t;


/**
 * \brief Active tamper output pin table (same recipe as \ref tamp_InputPin_t).
 */
typedef enum
{
    TAMP_OUT1_PIN_PA0 = 0u,     /**< TAMP_OUT1 on PA0 (fixed)                                  */
    TAMP_OUT2_PIN_PC13,         /**< TAMP_OUT2 on PC13 (fixed)                                 */
#if defined(TAMP_LINE_HAS_EXT_4_5)
    TAMP_OUT3_PIN_PC13,         /**< TAMP_OUT3 on PC13 (TAMP_OR.OUT3_RMP = 00)                 */
#if defined(TAMP_LINE_HAS_EXT_6_7_8)
    TAMP_OUT3_PIN_PI8,          /**< TAMP_OUT3 on PI8 (TAMP_OR.OUT3_RMP = 01)                  */
#endif
    TAMP_OUT3_PIN_PE3,          /**< TAMP_OUT3 on PE3 (TAMP_OR.OUT3_RMP = 10)                  */
    TAMP_OUT3_PIN_PA2,          /**< TAMP_OUT3 on PA2 (TAMP_OR.OUT3_RMP = 11)                  */
    TAMP_OUT4_PIN_PA1,          /**< TAMP_OUT4 on PA1 (fixed)                                  */
#endif
#if defined(TAMP_LINE_HAS_EXT_6_7_8)
    TAMP_OUT5_PIN_PI11,         /**< TAMP_OUT5 on PI11 (TAMP_OR.OUT5_RMP = 0)                  */
    TAMP_OUT5_PIN_PC1,          /**< TAMP_OUT5 on PC1 (TAMP_OR.OUT5_RMP = 1)                   */
    TAMP_OUT6_PIN_PE6,          /**< TAMP_OUT6 on PE6 (fixed)                                  */
    TAMP_OUT7_PIN_PE5,          /**< TAMP_OUT7 on PE5 (fixed)                                  */
    TAMP_OUT8_PIN_PE4,          /**< TAMP_OUT8 on PE4 (fixed)                                  */
#endif
    TAMP_OUT_PIN_UNUSED,        /**< No output pin assigned (passive tamper)                   */
    TAMP_OUT_PIN_CNT            /**< Count of available tamper output pin items                */
}   tamp_OutputPin_t;


/** \brief Internal tamper source identification (only the ones the device line has). */
typedef enum
{
    TAMP_INT_BACKUP_VOLTAGE = 0u, /**< ITAMP1: backup domain voltage monitoring       */
    TAMP_INT_TEMPERATURE,         /**< ITAMP2: temperature monitoring                 */
    TAMP_INT_LSE_FAIL,            /**< ITAMP3: LSE monitoring (LSECSS)                */
    TAMP_INT_HSE_FAIL,            /**< ITAMP4: HSE monitoring                         */
    TAMP_INT_RTC_CALENDAR_OVF,    /**< ITAMP5: RTC calendar overflow                  */
    TAMP_INT_DEBUG_ACCESS,        /**< ITAMP6: JTAG / SWD access                      */
    TAMP_INT_MONOTONIC_OVF,       /**< ITAMP8: monotonic counter 1 overflow           */
    TAMP_INT_CRYPTO_FAULT,        /**< ITAMP9: cryptographic peripherals fault        */
    TAMP_INT_IWDG_RESET,          /**< ITAMP11: IWDG reset while a tamper flag is set */
    TAMP_INT_SYSTEM_FAULT,        /**< ITAMP15: system fault                          */
    TAMP_INT_CNT                  /**< Count of available internal tamper sources     */
}   tamp_InternalId_t;


/** \brief Common tamper identification (external + internal) used by the reaction and status API. */
typedef enum
{
    TAMP_ID_EXTERNAL = 0u, /**< The tamper id refers to \ref tamp_ExternalId_t */
    TAMP_ID_INTERNAL       /**< The tamper id refers to \ref tamp_InternalId_t */
}   tamp_IdKind_t;


/** \brief Index of a tamper input inside \ref tamp_ExternalId_t or \ref tamp_InternalId_t. */
typedef uint8_t tamp_TamperIndex_t;


/** \brief Number of external tamper pairs passed to \ref Tamp_Set_ActiveTamperPairs. */
typedef uint8_t tamp_PairCnt_t;


/** \brief Tamper id (kind + the index of \ref tamp_ExternalId_t or \ref tamp_InternalId_t). */
typedef struct
{
    tamp_IdKind_t      Kind;  /**< Selects which of the two indices below applies              */
    tamp_TamperIndex_t Index; /**< Index into \ref tamp_ExternalId_t or \ref tamp_InternalId_t */
}   tamp_TamperId_t;


/** \brief External tamper trigger condition. */
typedef enum
{
    TAMP_TRIGGER_RISING = 0u, /**< Edge detection, rising edge  */
    TAMP_TRIGGER_FALLING,     /**< Edge detection, falling edge */
    TAMP_TRIGGER_LOW_LEVEL,   /**< Level detection, low level   */
    TAMP_TRIGGER_HIGH_LEVEL   /**< Level detection, high level  */
}   tamp_Trigger_t;


/** \brief Digital filter sample count (TAMPFREQ-clocked debounce of the tamper input). */
typedef enum
{
    TAMP_FILTER_1_SAMPLE = 0u, /**< No filter, 1 sample is enough to trigger the event */
    TAMP_FILTER_2_SAMPLES,     /**< 2 consecutive samples required                     */
    TAMP_FILTER_4_SAMPLES,     /**< 4 consecutive samples required                     */
    TAMP_FILTER_8_SAMPLES      /**< 8 consecutive samples required                     */
}   tamp_FilterSamples_t;


/** \brief Digital filter sampling frequency, derived from tamp_ker_ck (RTCCLK, typ. LSE / 32768 Hz). */
typedef enum
{
    TAMP_FILTER_FREQ_DIV_32768 = 0u, /**< RTCCLK / 32768 */
    TAMP_FILTER_FREQ_DIV_16384,      /**< RTCCLK / 16384 */
    TAMP_FILTER_FREQ_DIV_8192,       /**< RTCCLK / 8192  */
    TAMP_FILTER_FREQ_DIV_4096,       /**< RTCCLK / 4096  */
    TAMP_FILTER_FREQ_DIV_2048,       /**< RTCCLK / 2048  */
    TAMP_FILTER_FREQ_DIV_1024,       /**< RTCCLK / 1024  */
    TAMP_FILTER_FREQ_DIV_512,        /**< RTCCLK / 512   */
    TAMP_FILTER_FREQ_DIV_256         /**< RTCCLK / 256   */
}   tamp_FilterFreq_t;


/** \brief Pull-up precharge duration applied to the input before it is sampled. */
typedef enum
{
    TAMP_PRECHARGE_1_CYCLE = 0u, /**< 1 RTCCLK cycle  */
    TAMP_PRECHARGE_2_CYCLES,     /**< 2 RTCCLK cycles */
    TAMP_PRECHARGE_4_CYCLES,     /**< 4 RTCCLK cycles */
    TAMP_PRECHARGE_8_CYCLES      /**< 8 RTCCLK cycles */
}   tamp_Precharge_t;


/** \brief Tamper reaction mode. */
typedef enum
{
    TAMP_MODE_DISABLED = 0u,  /**< Internal tamper not enabled; as a reaction it behaves as potential */
    TAMP_MODE_POTENTIAL,      /**< Flag and interrupt only, software decides                          */
    TAMP_MODE_CONFIRMED       /**< Hardware erase of the configured secrets at once                   */
}   tamp_Mode_t;


/** \brief Bit mask of the device secrets erased by a confirmed tamper (OR of these flags). */
typedef enum
{
    TAMP_ERASE_NONE        = 0x00u, /**< Nothing is erased (potential mode only)    */
    TAMP_ERASE_BACKUP_REGS = 0x01u, /**< TAMP backup registers are erased           */
    TAMP_ERASE_BACKUP_SRAM = 0x02u, /**< Backup SRAM is erased                      */
    TAMP_ERASE_SRAM2       = 0x04u, /**< SRAM2 content is erased                    */
    TAMP_ERASE_CACHES      = 0x08u, /**< ICACHE / DCACHE content is erased          */
    TAMP_ERASE_CRYPTO      = 0x10u  /**< SAES / AES / HASH / PKA secrets are erased */
}   tamp_EraseScope_t;


/** \brief Bit mask of external + internal tamper events (status read / clear). */
typedef uint32_t tamp_TamperMask_t;


/** \brief External tamper input configuration. */
typedef struct
{
    tamp_Trigger_t       Trigger;       /**< Edge / level condition that raises the event                      */
    tamp_FilterSamples_t FilterSamples; /**< Digital filter sample count                                       */
    tamp_FilterFreq_t    FilterFreq;    /**< Digital filter sampling frequency                                 */
    tamp_Precharge_t     Precharge;     /**< Pull-up precharge duration                                        */
    tamp_FunctionState_t Active;        /**< TAMP_FUNCTION_ACTIVE selects the active (mesh) mode, else passive */
    tamp_InputPin_t      InputPin;      /**< Input pin item, never \ref TAMP_IN_PIN_UNUSED                     */
    tamp_OutputPin_t     OutputPin;     /**< Output pin of the active mode, else \ref TAMP_OUT_PIN_UNUSED      */
}   tamp_ExternalConfig_t;


/** \brief Reaction configuration of one tamper (external or internal). */
typedef struct
{
    tamp_Mode_t          Mode;       /**< Confirmed or potential                        */
    tamp_EraseScope_t    EraseScope; /**< Secrets erased when the reaction is confirmed */
    tamp_FunctionState_t Timestamp;  /**< Whether the event is timestamped in the RTC   */
}   tamp_Reaction_t;


/** \brief Backup register index (0 .. \ref Tamp_Get_BackupRegisterCount - 1). */
typedef uint8_t tamp_BackupIndex_t;


/** \brief Backup register count type. */
typedef uint8_t tamp_BackupCnt_t;


/** \brief 32-bit data of a backup register. */
typedef uint32_t tamp_BackupData_t;


/** \brief Protected backup register areas (zone boundaries, see RM0481 Figure 687). */
typedef struct
{
    tamp_BackupIndex_t ReadWriteSecureCount; /**< Zone 1: BKPRWSEC, read/write secure only     */
    tamp_BackupIndex_t WriteSecureCount;     /**< Zone 2: BKPWSEC, read nonsecure/write secure */
}   tamp_BackupAreas_t;


/** \brief External / internal tamper event callback, called from the TAMP interrupt. */
typedef void ( tamp_TamperIsr_t )( tamp_TamperMask_t tamperMask );


/** \brief Full module configuration used by \ref Tamp_Init. */
typedef struct
{
    tamp_ExternalConfig_t External[ TAMP_EXT_CNT ];                /**< External tamper configuration table           */
    tamp_Mode_t           InternalMode[ TAMP_INT_CNT ];            /**< Mode of every internal tamper                 */
    tamp_Reaction_t       Reaction[ TAMP_EXT_CNT + TAMP_INT_CNT ]; /**< Reaction per id, external first               */
    tamp_BackupAreas_t    BackupAreas;                             /**< Backup register protected areas configuration */
    tamp_TamperIsr_t     *TamperCallback;                          /**< \ref TAMP_NULL_PTR disables the interrupt     */
    tamp_IrqPrio_t        IrqPriority;                             /**< TAMP interrupt priority (0 - highest)         */
}   tamp_Config_t;

/* ========================== EXPORTED VARIABLES ============================ */

/* ========================= EXPORTED FUNCTIONS ============================= */


#endif /* TAMP_TAMP_TYPES_H */
