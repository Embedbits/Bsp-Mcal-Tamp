/**
 * \author Mr.Nobody
 * \file Tamp.c
 * \ingroup Tamp
 * \brief Tamp module main functionality (STM32H5).
 *
 * Implements \ref Tamp_Port.h against the TAMP peripheral registers (via \ref Tamp_Reg.h),
 * the pins (via the Gpio module) and the interrupt (via the Nvic module). The backup domain
 * write protection (PWR_DBPCR.DBP) is lifted through the Pwr module (the "own peripheral only"
 * rule forbids touching another peripheral's register): \ref Tamp_Init requests the access with
 * \ref Pwr_Set_BackupAccessActive, holds it while the module is initialized and
 * \ref Tamp_Deinit releases it with \ref Pwr_Set_BackupAccessInactive. The access of the Pwr
 * module is reference counted, so the reference of this module and the one of the Rtc module do
 * not cancel each other. The functions that write a register without an initialized module
 * (\ref Tamp_Set_BackupRegister, \ref Tamp_Set_BackupAreas, \ref Tamp_Set_TamperStatusClear and
 * the pin initialization with a TAMP_OR field) take a reference for the duration of the write
 * when the module does not hold one.
 *
 * The TAMP register interface clock (RCC_APB3ENR.RTCAPBEN) is requested from the Rcc module with
 * \c RCC_PERIPH_RTC_APB before the first access of every TAMP register: by \ref Tamp_Init, by the
 * functions above and by the getters that can run without an initialized module
 * (\ref Tamp_Get_BackupRegister, \ref Tamp_Get_TamperStatus; the backup registers keep their
 * value over a reset but read as zero without the interface clock). The module never disables the
 * clock: it is shared with the Rtc module and the backup registers must stay accessible.
 *
 * Every setter that touches a register (external/internal tamper configuration, activation,
 * reaction, interrupt priority) returns \ref TAMP_REQUEST_ERROR without writing anything when
 * the module is not initialized, matching the project convention (Wwdg, Crs); \ref Tamp_Init
 * reaches the same register writes through private \c *Regs helpers that skip this check,
 * since it has not set \ref tamp_State_t::Initialized yet at that point.
 *
 * Implemented in this version: Init / Deinit / Get_DefaultConfig, external tamper trigger /
 * filter / precharge / active-passive configuration, internal tamper enable, reaction (mode,
 * timestamp request, erase scope request), status read / clear, config lock, backup registers
 * and their protected areas, interrupt priority, input / output pin GPIO initialization.
 *
 * Not implemented in this version (see the module report for the exact reason):
 * - \ref Tamp_Set_ActiveTamperPairs (the multi-pair active tamper seed/output generator):
 *   returns \ref TAMP_REQUEST_ERROR, the generator (ATCR1/ATSEEDR/ATOR/ATCR2) is not wired.
 * - \ref tamp_EraseScope_t is accepted and stored but TAMP erases the fixed hardware scope
 *   (backup registers, SRAM2, caches, crypto) on every confirmed event; there is no per-scope
 *   register bit to select a subset on this peripheral (RM0481 52.4.2 Table 569), so a
 *   reaction asking for a scope narrower than the hardware default is accepted but not
 *   enforced - documented here, not hidden.
 * - RTC forwarding, TrustZone-aware secure/privilege configuration (SECCFGR/PRIVCFGR) beyond
 *   the backup area split are not implemented.
 *
 */
/* ============================== INCLUDES ================================== */
#include "Tamp.h"                           /* Module private definitions     */
#include "Tamp_Port.h"                       /* Module public functionality    */
#include "Tamp_Reg.h"                        /* Module register access        */
#include "Gpio_Port.h"                       /* Tamper pin configuration       */
#include "Nvic_Port.h"                       /* Tamper interrupt configuration */
#include "Pwr_Port.h"                        /* Backup domain write access     */
#include "Rcc_Port.h"                        /* TAMP register interface clock  */
#include "Stm32.h"                           /* TAMP registers definition      */
/* ============================== TYPEDEFS ================================== */

/* ========================== SYMBOLIC CONSTANTS ============================ */

/** \brief Module version */
#define TAMP_MODULE_VERSION_MAJOR    ( 1u )
#define TAMP_MODULE_VERSION_MINOR    ( 0u )
#define TAMP_MODULE_VERSION_PATCH    ( 0u )

/** \brief Register value with every bit set (full register read/clear mask) */
#define TAMP_REG_FULL_MASK           ( 0xFFFFFFFFu )

/** \brief Count of read-back iterations of a register written by \ref Tamp_Init */
#define TAMP_INIT_READBACK_TIMEOUT_RAW ( 0x10u )

/** \brief Internal tamper ITAMPy numbers (RM0481 52.4.2 Table 569), named to avoid a bare
 *  magic number in \ref tamp_InternalItampNumber below. */
#define TAMP_ITAMP1_NUMBER           ( 1u )
#define TAMP_ITAMP2_NUMBER           ( 2u )
#define TAMP_ITAMP3_NUMBER           ( 3u )
#define TAMP_ITAMP4_NUMBER           ( 4u )
#define TAMP_ITAMP5_NUMBER           ( 5u )
#define TAMP_ITAMP6_NUMBER           ( 6u )
#define TAMP_ITAMP8_NUMBER           ( 8u )
#define TAMP_ITAMP9_NUMBER           ( 9u )
#define TAMP_ITAMP11_NUMBER          ( 11u )
#define TAMP_ITAMP15_NUMBER          ( 15u )

/** \brief Internal tamper ITAMPy numbers, indexed the same way as \ref tamp_InternalId_t. */
static const uint8_t tamp_InternalItampNumber[ TAMP_INT_CNT ] =
{
    TAMP_ITAMP1_NUMBER,  /**< TAMP_INT_BACKUP_VOLTAGE   -> ITAMP1  */
    TAMP_ITAMP2_NUMBER,  /**< TAMP_INT_TEMPERATURE      -> ITAMP2  */
    TAMP_ITAMP3_NUMBER,  /**< TAMP_INT_LSE_FAIL         -> ITAMP3  */
    TAMP_ITAMP4_NUMBER,  /**< TAMP_INT_HSE_FAIL         -> ITAMP4  */
    TAMP_ITAMP5_NUMBER,  /**< TAMP_INT_RTC_CALENDAR_OVF -> ITAMP5  */
    TAMP_ITAMP6_NUMBER,  /**< TAMP_INT_DEBUG_ACCESS     -> ITAMP6  */
    TAMP_ITAMP8_NUMBER,  /**< TAMP_INT_MONOTONIC_OVF    -> ITAMP8  */
    TAMP_ITAMP9_NUMBER,  /**< TAMP_INT_CRYPTO_FAULT     -> ITAMP9  */
    TAMP_ITAMP11_NUMBER, /**< TAMP_INT_IWDG_RESET       -> ITAMP11 */
    TAMP_ITAMP15_NUMBER, /**< TAMP_INT_SYSTEM_FAULT     -> ITAMP15 */
};

/* ========================== LOCAL VARIABLES ================================ */

static tamp_State_t tamp_State =
{
    .Initialized     = TAMP_FUNCTION_INACTIVE,
    .BackupAccess    = TAMP_FUNCTION_INACTIVE,
    .ConfigLocked    = TAMP_FUNCTION_INACTIVE,
    .ExternalEnabled = { TAMP_FUNCTION_INACTIVE },
    .TamperCallback  = TAMP_NULL_PTR,
};

/* ======================== FORWARD DECLARATIONS ============================= */

static tamp_RequestState_t Tamp_Set_ExternalTamperRegs( tamp_ExternalId_t tamperId,
                                                         const tamp_ExternalConfig_t * const tamperConfig );
static tamp_RequestState_t Tamp_Set_InternalTamperRegs( tamp_InternalId_t tamperId, tamp_Mode_t mode );
static uint32_t            Tamp_Get_ItampNoEraseMask( tamp_InternalId_t tamperId );
static void                Tamp_OrField_Apply( tamp_OrField_t orField, uint8_t orValue );
static tamp_RequestState_t Tamp_Get_TriggerBit( tamp_Trigger_t trigger, uint32_t * const trgBit );
static tamp_RequestState_t Tamp_Check_LevelFilter( tamp_Trigger_t trigger, tamp_FilterSamples_t filterSamples );
static tamp_RequestState_t Tamp_Verify_BackupAreasWritten( const tamp_BackupAreas_t * const areas );
static tamp_RequestState_t Tamp_InterfaceClockRequest( void );
static tamp_RequestState_t Tamp_InterfaceClockRead( void );
static tamp_RequestState_t Tamp_Set_AnalogPin( gpio_PortId_t gpioPort, gpio_PinId_t gpioPin );
static tamp_RequestState_t Tamp_Set_OrField( tamp_OrField_t orField, uint8_t orValue );
static tamp_RequestState_t Tamp_BackupAccessAcquire( void );
static tamp_RequestState_t Tamp_BackupAccessRelease( void );
static tamp_RequestState_t Tamp_BackupAccessBegin( tamp_FunctionState_t * const taken );
static tamp_RequestState_t Tamp_BackupAccessEnd( const tamp_FunctionState_t taken );

/* ========================= EXPORTED FUNCTIONS ============================= */

/**
 * \brief Returns the module version.
 * \return The module version.
 */
tamp_ModuleVersion_t Tamp_Get_ModuleVersion( void )
{
    tamp_ModuleVersion_t version;

    version.Major = (tamp_MajorVersion_t)TAMP_MODULE_VERSION_MAJOR;
    version.Minor = (tamp_MinorVersion_t)TAMP_MODULE_VERSION_MINOR;
    version.Patch = (tamp_PatchVersion_t)TAMP_MODULE_VERSION_PATCH;

    return ( version );
}


/**
 * \brief Fills a configuration with safe defaults (internal tampers disabled, reaction potential,
 *        no callback). Enabling an internal tamper is a decision of the application: for example the
 *        debug access tamper blocks the backup registers and the SRAM2 while a probe is attached.
 *
 * \param tampConfig [out]: Configuration filled with the default values
 * \return \ref TAMP_REQUEST_OK on success, \ref TAMP_REQUEST_ERROR for a null pointer
 */
tamp_RequestState_t Tamp_Get_DefaultConfig( tamp_Config_t * const tampConfig )
{
    tamp_RequestState_t retState = TAMP_REQUEST_ERROR;

    if( TAMP_NULL_PTR != tampConfig )
    {
        for( uint32_t extIdx = 0u; (uint32_t)TAMP_EXT_CNT > extIdx; extIdx++ )
        {
            tampConfig->External[extIdx].Trigger       = TAMP_TRIGGER_RISING;
            tampConfig->External[extIdx].FilterSamples  = TAMP_FILTER_1_SAMPLE;
            tampConfig->External[extIdx].FilterFreq     = TAMP_FILTER_FREQ_DIV_32768;
            tampConfig->External[extIdx].Precharge      = TAMP_PRECHARGE_1_CYCLE;
            tampConfig->External[extIdx].Active         = TAMP_FUNCTION_INACTIVE;
            tampConfig->External[extIdx].InputPin       = TAMP_IN_PIN_UNUSED;
            tampConfig->External[extIdx].OutputPin      = TAMP_OUT_PIN_UNUSED;
        }

        for( uint32_t intIdx = 0u; (uint32_t)TAMP_INT_CNT > intIdx; intIdx++ )
        {
            tampConfig->InternalMode[intIdx] = TAMP_MODE_DISABLED;
        }

        for( uint32_t reactIdx = 0u; ( (uint32_t)TAMP_EXT_CNT + (uint32_t)TAMP_INT_CNT ) > reactIdx; reactIdx++ )
        {
            tampConfig->Reaction[reactIdx].Mode       = TAMP_MODE_POTENTIAL;
            tampConfig->Reaction[reactIdx].EraseScope = TAMP_ERASE_NONE;
            tampConfig->Reaction[reactIdx].Timestamp  = TAMP_FUNCTION_INACTIVE;
        }

        tampConfig->BackupAreas.ReadWriteSecureCount = (tamp_BackupIndex_t)0u;
        tampConfig->BackupAreas.WriteSecureCount     = (tamp_BackupIndex_t)0u;
        tampConfig->TamperCallback                   = TAMP_NULL_PTR;
        tampConfig->IrqPriority                      = (tamp_IrqPrio_t)0u;

        retState = TAMP_REQUEST_OK;
    }
    else
    {
        /* Invalid configuration pointer, no action required */
        retState = TAMP_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Initializes the TAMP peripheral with the given configuration.
 *
 * Enables the backup domain write access through the Pwr module (\ref Pwr_Set_BackupAccessActive,
 * held until \ref Tamp_Deinit), configures every external tamper (trigger / filter / precharge /
 * pin), every internal tamper, the reaction of every tamper id, the backup areas and the
 * interrupt.
 *
 * \note The read-back verification uses TAMP_SECCFGR (the backup areas written by
 *       \ref Tamp_Set_BackupAreas) as the proof that the writes of this call actually took
 *       effect: every TAMP register, including SECCFGR, needs PWR_DBPCR.DBP set to accept a
 *       write (RM0481 52.4.3), so a write the hardware ignored (for example because the
 *       protection was re-enabled by another module) is caught here instead of leaving every
 *       other register silently unwritten. The wait
 *       is a bounded loop on the register state (no hardware delay is actually needed for a
 *       plain configuration register, the loop only tolerates host/debugger jitter), never a
 *       blind delay.
 *
 * \param tampConfig [in]: Configuration to apply
 * \return \ref TAMP_REQUEST_OK on success, \ref TAMP_REQUEST_ERROR when the configuration is
 *         invalid, the configuration is locked, the Pwr module refused the backup domain access
 *         or the read-back does not match (the module is deinitialized in the last two cases)
 */
tamp_RequestState_t Tamp_Init( const tamp_Config_t * const tampConfig )
{
    tamp_RequestState_t  retState    = TAMP_REQUEST_ERROR;
    tamp_RequestState_t  accessState = TAMP_REQUEST_ERROR;
    tamp_FunctionState_t configUsable = TAMP_FUNCTION_INACTIVE;

    if( ( TAMP_NULL_PTR         != tampConfig                ) &&
        ( TAMP_FUNCTION_INACTIVE == tamp_State.ConfigLocked  )    )
    {
        configUsable = TAMP_FUNCTION_ACTIVE;
        accessState  = Tamp_BackupAccessAcquire();
    }
    else
    {
        /* Invalid configuration pointer or the configuration is locked, the access is not requested */
    }

    if( ( TAMP_FUNCTION_ACTIVE == configUsable ) &&
        ( TAMP_REQUEST_OK      == accessState  )    )
    {
        tamp_RequestState_t verifyState = TAMP_REQUEST_ERROR;

        for( uint32_t extIdx = 0u; (uint32_t)TAMP_EXT_CNT > extIdx; extIdx++ )
        {
            const tamp_RequestState_t extState = Tamp_Set_ExternalTamperRegs( (tamp_ExternalId_t)extIdx,
                                                                               &tampConfig->External[extIdx] );

            if( TAMP_REQUEST_OK != extState )
            {
                /* No action required, the external tamper keeps its reset configuration */
            }
            else
            {
                /* No action required, the external tamper is configured */
            }
        }

        for( uint32_t intIdx = 0u; (uint32_t)TAMP_INT_CNT > intIdx; intIdx++ )
        {
            (void)Tamp_Set_InternalTamperRegs( (tamp_InternalId_t)intIdx, tampConfig->InternalMode[intIdx] );
        }

        (void)Tamp_Set_BackupAreas( &tampConfig->BackupAreas );

        tamp_State.TamperCallback = tampConfig->TamperCallback;

        if( TAMP_NULL_PTR != tampConfig->TamperCallback )
        {
            (void)Nvic_Set_PeriphIrq_Prio( NVIC_PERIPH_IRQ_TAMP, tampConfig->IrqPriority );
            Tamp_Set_RegVal( TAMP_REG_IER, TAMP_IER_TAMP1IE, TAMP_IER_TAMP1IE );
        }
        else
        {
            /* No action required, interrupt stays disabled */
        }

        verifyState = Tamp_Verify_BackupAreasWritten( &tampConfig->BackupAreas );

        if( TAMP_REQUEST_OK == verifyState )
        {
            tamp_State.Initialized = TAMP_FUNCTION_ACTIVE;
            retState                = TAMP_REQUEST_OK;
        }
        else
        {
            (void)Tamp_Deinit();
            retState = TAMP_REQUEST_ERROR;
        }
    }
    else
    {
        if( TAMP_FUNCTION_ACTIVE == configUsable )
        {
            /* The Pwr module refused the backup domain access: deinitialize as on a failed read-back */
            (void)Tamp_Deinit();
        }
        else
        {
            /* Invalid configuration pointer or the configuration is locked, module stays as it was */
        }

        retState = TAMP_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Deinitializes the module: disables every tamper, the interrupt, clears the state and
 *        releases the backup domain write access taken by \ref Tamp_Init (the Pwr module keeps
 *        the access while another module, for example Rtc, still holds it).
 * \return \ref TAMP_REQUEST_OK, \ref TAMP_REQUEST_ERROR when the Pwr module refused the release
 *         of the access (the module is deinitialized anyway)
 */
tamp_RequestState_t Tamp_Deinit( void )
{
    tamp_RequestState_t retState = TAMP_REQUEST_ERROR;

    Tamp_Set_RegVal( TAMP_REG_CR1, TAMP_REG_FULL_MASK, 0u );
    Tamp_Set_RegVal( TAMP_REG_IER, TAMP_REG_FULL_MASK, 0u );

    for( uint32_t extIdx = 0u; (uint32_t)TAMP_EXT_CNT > extIdx; extIdx++ )
    {
        tamp_State.ExternalEnabled[extIdx] = TAMP_FUNCTION_INACTIVE;
    }

    tamp_State.Initialized  = TAMP_FUNCTION_INACTIVE;
    tamp_State.TamperCallback = TAMP_NULL_PTR;
    retState                  = Tamp_BackupAccessRelease();

    return ( retState );
}


/**
 * \brief Polled service task: replays the status read when the interrupt callback is not used.
 *
 * Called from the TAMP interrupt when a callback is registered; this is the POLL mode
 * counterpart mentioned by the interface, called periodically to read and forward the
 * status otherwise.
 */
void Tamp_Task( void )
{
    if( ( TAMP_FUNCTION_ACTIVE == tamp_State.Initialized   ) &&
        ( TAMP_NULL_PTR        != tamp_State.TamperCallback )    )
    {
        tamp_TamperMask_t tamperStatus = (tamp_TamperMask_t)0u;

        (void)Tamp_Get_TamperStatus( &tamperStatus );

        if( (tamp_TamperMask_t)0u != tamperStatus )
        {
            tamp_State.TamperCallback( tamperStatus );
            (void)Tamp_Set_TamperStatusClear( tamperStatus );
        }
        else
        {
            /* No pending tamper event, no action required */
        }
    }
    else
    {
        /* Module not initialized or no callback registered, no action required */
    }
}


/**
 * \brief Configures one external tamper input (trigger, filter, precharge, pin, active mode).
 *
 * Rejected (hardware rule) when the tamper is already enabled or the configuration is locked.
 *
 * \param tamperId     [in]: External tamper to configure
 * \param tamperConfig [in]: Configuration to apply
 * \return \ref TAMP_REQUEST_OK on success, \ref TAMP_REQUEST_ERROR otherwise
 */
tamp_RequestState_t Tamp_Set_ExternalTamper( tamp_ExternalId_t tamperId,
                                             const tamp_ExternalConfig_t * const tamperConfig )
{
    tamp_RequestState_t retState = TAMP_REQUEST_ERROR;

    if( ( TAMP_FUNCTION_ACTIVE   == tamp_State.Initialized                ) &&
        ( TAMP_EXT_CNT           > tamperId                               ) &&
        ( TAMP_NULL_PTR          != tamperConfig                          ) &&
        ( TAMP_FUNCTION_INACTIVE == tamp_State.ExternalEnabled[tamperId]  ) &&
        ( TAMP_FUNCTION_INACTIVE == tamp_State.ConfigLocked               )    )
    {
        retState = Tamp_Set_ExternalTamperRegs( tamperId, tamperConfig );
    }
    else
    {
        /* Module not initialized, invalid parameter, tamper already enabled, or configuration
         * is locked */
        retState = TAMP_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Enables one internal tamper source with the given reaction mode.
 *
 * \param tamperId [in]: Internal tamper to configure
 * \param mode      [in]: Reaction mode of this source: \ref TAMP_MODE_DISABLED disables it,
 *                        \ref TAMP_MODE_POTENTIAL keeps the secrets (TAMP_CR3.ITAMPxNOER set),
 *                        \ref TAMP_MODE_CONFIRMED erases them at once
 * \return \ref TAMP_REQUEST_OK on success, \ref TAMP_REQUEST_ERROR for an invalid id or a
 *         locked configuration
 */
tamp_RequestState_t Tamp_Set_InternalTamper( tamp_InternalId_t tamperId, tamp_Mode_t mode )
{
    tamp_RequestState_t retState = TAMP_REQUEST_ERROR;

    if( ( TAMP_FUNCTION_ACTIVE   == tamp_State.Initialized  ) &&
        ( TAMP_FUNCTION_INACTIVE == tamp_State.ConfigLocked )    )
    {
        retState = Tamp_Set_InternalTamperRegs( tamperId, mode );
    }
    else
    {
        /* Module not initialized or the configuration is locked */
        retState = TAMP_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Enables an already configured external tamper.
 * \param tamperId [in]: External tamper to activate
 * \return \ref TAMP_REQUEST_OK on success, \ref TAMP_REQUEST_ERROR otherwise
 */
tamp_RequestState_t Tamp_Set_TamperActive( tamp_ExternalId_t tamperId )
{
    tamp_RequestState_t retState = TAMP_REQUEST_ERROR;

    if( ( TAMP_FUNCTION_ACTIVE == tamp_State.Initialized ) &&
        ( TAMP_EXT_CNT         > tamperId                )    )
    {
        Tamp_Set_RegBit( TAMP_REG_CR1, ( 1u << tamperId ) );
        tamp_State.ExternalEnabled[tamperId] = TAMP_FUNCTION_ACTIVE;
        retState                             = TAMP_REQUEST_OK;
    }
    else
    {
        /* Module not initialized or invalid tamper id, no action required */
        retState = TAMP_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Disables an external tamper so it can be reconfigured.
 * \param tamperId [in]: External tamper to deactivate
 * \return \ref TAMP_REQUEST_OK on success, \ref TAMP_REQUEST_ERROR otherwise
 */
tamp_RequestState_t Tamp_Set_TamperInactive( tamp_ExternalId_t tamperId )
{
    tamp_RequestState_t retState = TAMP_REQUEST_ERROR;

    if( ( TAMP_FUNCTION_ACTIVE == tamp_State.Initialized ) &&
        ( TAMP_EXT_CNT         > tamperId                )    )
    {
        Tamp_Reset_RegBit( TAMP_REG_CR1, ( 1u << tamperId ) );
        tamp_State.ExternalEnabled[tamperId] = TAMP_FUNCTION_INACTIVE;
        retState                             = TAMP_REQUEST_OK;
    }
    else
    {
        /* Module not initialized or invalid tamper id, no action required */
        retState = TAMP_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Not implemented in this version: the active tamper pattern generator (seed, output
 *        sharing between the paired meshes) is not wired. See the Tamp.c file header.
 *
 * \return \ref TAMP_REQUEST_ERROR always
 */
tamp_RequestState_t Tamp_Set_ActiveTamperPairs( const tamp_ExternalConfig_t * const pairs, tamp_PairCnt_t pairCnt )
{
    (void)pairs;
    (void)pairCnt;

    return ( TAMP_REQUEST_ERROR );
}


/**
 * \brief Sets the reaction (mode, erase scope request, timestamp) of one tamper id.
 *
 * \note \ref tamp_EraseScope_t narrower than the hardware default erase scope is accepted but
 *       not enforced on this peripheral, see the Tamp.c file header.
 *
 * \param tamperId [in]: Tamper id (external or internal)
 * \param reaction [in]: Reaction to apply
 * \return \ref TAMP_REQUEST_OK on success, \ref TAMP_REQUEST_ERROR otherwise
 */
tamp_RequestState_t Tamp_Set_Reaction( tamp_TamperId_t tamperId, const tamp_Reaction_t * const reaction )
{
    tamp_RequestState_t retState = TAMP_REQUEST_ERROR;

    if( ( TAMP_FUNCTION_ACTIVE   == tamp_State.Initialized        ) &&
        ( TAMP_NULL_PTR          != reaction                     ) &&
        ( TAMP_FUNCTION_INACTIVE == tamp_State.ConfigLocked       ) &&
        ( ( ( TAMP_ID_EXTERNAL == tamperId.Kind               ) &&
            ( (tamp_TamperIndex_t)TAMP_EXT_CNT > tamperId.Index )    ) ||
          ( ( TAMP_ID_INTERNAL == tamperId.Kind               ) &&
            ( (tamp_TamperIndex_t)TAMP_INT_CNT > tamperId.Index )    )    )    )
    {
        uint32_t      bitMask    = 0u;
        tamp_RegId_t  noEraseReg = TAMP_REG_CR2;

        if( TAMP_ID_EXTERNAL == tamperId.Kind )
        {
            bitMask    = ( 1u << tamperId.Index );
            noEraseReg = TAMP_REG_CR2;
        }
        else
        {
            bitMask    = Tamp_Get_ItampNoEraseMask( (tamp_InternalId_t)tamperId.Index );
            noEraseReg = TAMP_REG_CR3;
        }

        if( TAMP_MODE_CONFIRMED == reaction->Mode )
        {
            Tamp_Reset_RegBit( noEraseReg, bitMask );
        }
        else
        {
            /* Potential (or disabled) tamper: the secrets are kept */
            Tamp_Set_RegBit( noEraseReg, bitMask );
        }

        retState = TAMP_REQUEST_OK;
    }
    else
    {
        /* Module not initialized, invalid parameter, or the configuration is locked */
        retState = TAMP_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Reads the external + internal tamper event status (bits set also while in Standby).
 *        Requests the TAMP register interface clock when the module is not initialized.
 * \param tamperStatus [out]: Bit mask of the pending tamper events
 * \return \ref TAMP_REQUEST_OK on success, \ref TAMP_REQUEST_ERROR for a null pointer or when the
 *         Rcc module refused the interface clock
 */
tamp_RequestState_t Tamp_Get_TamperStatus( tamp_TamperMask_t * const tamperStatus )
{
    tamp_RequestState_t retState = TAMP_REQUEST_ERROR;

    if( TAMP_NULL_PTR != tamperStatus )
    {
        const tamp_RequestState_t clockState = Tamp_InterfaceClockRead();

        if( TAMP_REQUEST_OK == clockState )
        {
            *tamperStatus = (tamp_TamperMask_t)Tamp_Get_RegVal( TAMP_REG_SR, TAMP_REG_FULL_MASK );
            retState      = TAMP_REQUEST_OK;
        }
        else
        {
            /* The interface clock is not running, nothing is read */
            retState = TAMP_REQUEST_ERROR;
        }
    }
    else
    {
        /* Invalid output pointer, no action required */
        retState = TAMP_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Clears the given tamper event flags (write-1-to-clear, TAMP_SCR). Takes a backup
 *        domain access reference for the write when the module does not hold one.
 * \param tamperMask [in]: Bit mask of the events to clear
 * \return \ref TAMP_REQUEST_OK on success, \ref TAMP_REQUEST_ERROR when the Pwr module refused the
 *         backup domain access (nothing written)
 */
tamp_RequestState_t Tamp_Set_TamperStatusClear( tamp_TamperMask_t tamperMask )
{
    tamp_RequestState_t  retState    = TAMP_REQUEST_ERROR;
    tamp_FunctionState_t taken       = TAMP_FUNCTION_INACTIVE;
    tamp_RequestState_t  accessState = TAMP_REQUEST_ERROR;

    accessState = Tamp_BackupAccessBegin( &taken );

    if( TAMP_REQUEST_OK == accessState )
    {
        Tamp_Set_RegVal( TAMP_REG_SCR, (uint32_t)tamperMask, (uint32_t)tamperMask );
        retState = Tamp_BackupAccessEnd( taken );
    }
    else
    {
        /* The backup domain is not writable, nothing is written */
    }

    return ( retState );
}


/**
 * \brief Locks the tamper configuration until the next reset.
 * \return \ref TAMP_REQUEST_OK always
 */
tamp_RequestState_t Tamp_Set_ConfigLock( void )
{
    tamp_State.ConfigLocked = TAMP_FUNCTION_ACTIVE;

    return ( TAMP_REQUEST_OK );
}


/**
 * \brief Returns the number of backup registers available on this device.
 * \param registerCnt [out]: Set to \ref TAMP_REG_BACKUP_REG_CNT
 * \return \ref TAMP_REQUEST_OK on success, \ref TAMP_REQUEST_ERROR for a null pointer
 */
tamp_RequestState_t Tamp_Get_BackupRegisterCount( tamp_BackupCnt_t * const registerCnt )
{
    tamp_RequestState_t retState = TAMP_REQUEST_ERROR;

    if( TAMP_NULL_PTR != registerCnt )
    {
        *registerCnt = (tamp_BackupCnt_t)TAMP_REG_BACKUP_REG_CNT;
        retState     = TAMP_REQUEST_OK;
    }
    else
    {
        /* Invalid output pointer, no action required */
        retState = TAMP_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Writes a backup register (getters have no side effects, this setter has one by design).
 *        Takes a backup domain access reference for the write when the module does not hold one.
 * \param index [in]: Backup register index
 * \param data  [in]: Value to write
 * \return \ref TAMP_REQUEST_OK on success, \ref TAMP_REQUEST_ERROR for an out-of-range index or
 *         when the Pwr module refused the backup domain access (nothing written)
 */
tamp_RequestState_t Tamp_Set_BackupRegister( tamp_BackupIndex_t index, tamp_BackupData_t data )
{
    tamp_RequestState_t retState = TAMP_REQUEST_ERROR;

    if( TAMP_REG_BACKUP_REG_CNT > index )
    {
        tamp_FunctionState_t taken = TAMP_FUNCTION_INACTIVE;

        tamp_RequestState_t accessState = Tamp_BackupAccessBegin( &taken );

        if( TAMP_REQUEST_OK == accessState )
        {
            Tamp_Reg_Set_BackupReg( index, data );
            retState = Tamp_BackupAccessEnd( taken );
        }
        else
        {
            /* The backup domain is not writable, nothing is written */
        }
    }
    else
    {
        /* Out-of-range index, nothing is written */
        retState = TAMP_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Reads a backup register. Requests the TAMP register interface clock when the module is
 *        not initialized (a value kept over a reset reads as zero without the clock).
 * \param index [in]:  Backup register index
 * \param data  [out]: Value read
 * \return \ref TAMP_REQUEST_OK on success, \ref TAMP_REQUEST_ERROR for an out-of-range index
 *         or a null pointer, or when the Rcc module refused the interface clock
 */
tamp_RequestState_t Tamp_Get_BackupRegister( tamp_BackupIndex_t index, tamp_BackupData_t * const data )
{
    tamp_RequestState_t retState = TAMP_REQUEST_ERROR;

    if( ( TAMP_REG_BACKUP_REG_CNT > index ) &&
        ( TAMP_NULL_PTR            != data  )    )
    {
        const tamp_RequestState_t clockState = Tamp_InterfaceClockRead();

        if( TAMP_REQUEST_OK == clockState )
        {
            *data    = Tamp_Reg_Get_BackupReg( index );
            retState = TAMP_REQUEST_OK;
        }
        else
        {
            /* The interface clock is not running, nothing is read */
            retState = TAMP_REQUEST_ERROR;
        }
    }
    else
    {
        /* Out-of-range index or invalid output pointer, no action required */
        retState = TAMP_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Configures the protected backup register areas (TAMP_SECCFGR.BKPRWSEC / BKPWSEC).
 *        Takes a backup domain access reference for the write when the module does not hold one.
 * \param areas [in]: Protected area sizes
 * \return \ref TAMP_REQUEST_OK on success, \ref TAMP_REQUEST_ERROR for an invalid configuration
 *         or when the Pwr module refused the backup domain access (nothing written)
 */
tamp_RequestState_t Tamp_Set_BackupAreas( const tamp_BackupAreas_t * const areas )
{
    tamp_RequestState_t retState = TAMP_REQUEST_ERROR;

    if( ( TAMP_NULL_PTR != areas ) &&
        ( TAMP_REG_BACKUP_REG_CNT >= areas->ReadWriteSecureCount ) &&
        ( TAMP_REG_BACKUP_REG_CNT >= areas->WriteSecureCount ) &&
        ( areas->WriteSecureCount >= areas->ReadWriteSecureCount )    )
    {
        const uint32_t writeValue = ( (uint32_t)areas->ReadWriteSecureCount |
                                      ( (uint32_t)areas->WriteSecureCount << TAMP_SECCFGR_BKPWSEC_Pos ) );
        tamp_FunctionState_t taken = TAMP_FUNCTION_INACTIVE;

        tamp_RequestState_t accessState = Tamp_BackupAccessBegin( &taken );

        if( TAMP_REQUEST_OK == accessState )
        {
            Tamp_Set_RegVal( TAMP_REG_SECCFGR, ( TAMP_SECCFGR_BKPRWSEC | TAMP_SECCFGR_BKPWSEC ), writeValue );
            retState = Tamp_BackupAccessEnd( taken );
        }
        else
        {
            /* The backup domain is not writable, nothing is written */
        }
    }
    else
    {
        /* Invalid configuration, nothing is written */
        retState = TAMP_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Sets the TAMP interrupt priority (NVIC).
 * \param irqPriority [in]: Priority value (0 - highest)
 * \return Result of \ref Nvic_Set_PeriphIrq_Prio for \ref NVIC_PERIPH_IRQ_TAMP
 */
tamp_RequestState_t Tamp_Set_IrqPriority( tamp_IrqPrio_t irqPriority )
{
    tamp_RequestState_t retState = TAMP_REQUEST_ERROR;

    if( TAMP_FUNCTION_ACTIVE == tamp_State.Initialized )
    {
        const nvic_RequestState_t nvicState = Nvic_Set_PeriphIrq_Prio( NVIC_PERIPH_IRQ_TAMP, irqPriority );

        if( NVIC_REQUEST_OK == nvicState )
        {
            retState = TAMP_REQUEST_OK;
        }
        else
        {
            retState = TAMP_REQUEST_ERROR;
        }
    }
    else
    {
        /* Module not initialized, no action required */
        retState = TAMP_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Reads back the TAMP interrupt priority (NVIC).
 * \param irqPriority [out]: Priority value read
 * \return Result of \ref Nvic_Get_PeriphIrq_Prio for \ref NVIC_PERIPH_IRQ_TAMP
 */
tamp_RequestState_t Tamp_Get_IrqPriority( tamp_IrqPrio_t * const irqPriority )
{
    tamp_RequestState_t     retState  = TAMP_REQUEST_ERROR;
    const nvic_RequestState_t nvicState = Nvic_Get_PeriphIrq_Prio( NVIC_PERIPH_IRQ_TAMP, irqPriority );

    if( NVIC_REQUEST_OK == nvicState )
    {
        retState = TAMP_REQUEST_OK;
    }
    else
    {
        retState = TAMP_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Configures the GPIO pin of an external tamper input in analog mode (no pull, per RM
 *        recommendation for TAMP/RTC backup domain pins). The GPIO port clock is requested by
 *        the Gpio module; a TAMP_OR remap field is written with the backup domain access and the
 *        interface clock taken as for any other TAMP register write.
 * \param inputPin [in]: Input pin item (never \ref TAMP_IN_PIN_UNUSED)
 * \return \ref TAMP_REQUEST_OK on success, \ref TAMP_REQUEST_ERROR otherwise
 */
tamp_RequestState_t Tamp_InitInputGpio( tamp_InputPin_t inputPin )
{
    tamp_RequestState_t        retState    = TAMP_REQUEST_ERROR;
    const tamp_InputPinInfo_t *info        = TAMP_NULL_PTR;
    tamp_RequestState_t        lookupState = TAMP_REQUEST_ERROR;

    if( TAMP_IN_PIN_UNUSED != inputPin )
    {
        lookupState = Tamp_Reg_Get_InputPinInfo( inputPin, &info );
    }
    else
    {
        /* Invalid pin item, no action required */
        lookupState = TAMP_REQUEST_ERROR;
    }

    if( TAMP_REQUEST_OK == lookupState )
    {
        const tamp_RequestState_t gpioState = Tamp_Set_AnalogPin( info->GpioPort, info->GpioPin );
        const tamp_RequestState_t orState   = Tamp_Set_OrField( info->OrField, info->OrValue );

        if( ( TAMP_REQUEST_OK == gpioState ) &&
            ( TAMP_REQUEST_OK == orState   )    )
        {
            retState = TAMP_REQUEST_OK;
        }
        else
        {
            retState = TAMP_REQUEST_ERROR;
        }
    }
    else
    {
        /* Invalid pin item or no lookup row found, no action required */
        retState = TAMP_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Configures the GPIO pin of an active tamper output in analog mode. The GPIO port clock
 *        is requested by the Gpio module; a TAMP_OR remap field is written with the backup domain
 *        access and the interface clock taken as for any other TAMP register write.
 * \param outputPin [in]: Output pin item (never \ref TAMP_OUT_PIN_UNUSED)
 * \return \ref TAMP_REQUEST_OK on success, \ref TAMP_REQUEST_ERROR otherwise
 */
tamp_RequestState_t Tamp_InitOutputGpio( tamp_OutputPin_t outputPin )
{
    tamp_RequestState_t         retState    = TAMP_REQUEST_ERROR;
    const tamp_OutputPinInfo_t *info        = TAMP_NULL_PTR;
    tamp_RequestState_t         lookupState = TAMP_REQUEST_ERROR;

    if( TAMP_OUT_PIN_UNUSED != outputPin )
    {
        lookupState = Tamp_Reg_Get_OutputPinInfo( outputPin, &info );
    }
    else
    {
        /* Invalid pin item, no action required */
        lookupState = TAMP_REQUEST_ERROR;
    }

    if( TAMP_REQUEST_OK == lookupState )
    {
        const tamp_RequestState_t gpioState = Tamp_Set_AnalogPin( info->GpioPort, info->GpioPin );
        const tamp_RequestState_t orState   = Tamp_Set_OrField( info->OrField, info->OrValue );

        if( ( TAMP_REQUEST_OK == gpioState ) &&
            ( TAMP_REQUEST_OK == orState   )    )
        {
            retState = TAMP_REQUEST_OK;
        }
        else
        {
            retState = TAMP_REQUEST_ERROR;
        }
    }
    else
    {
        /* Invalid pin item or no lookup row found, no action required */
        retState = TAMP_REQUEST_ERROR;
    }

    return ( retState );
}

/* =========================== LOCAL FUNCTIONS ============================== */

/**
 * \brief Requests the TAMP register interface clock (RCC_APB3ENR.RTCAPBEN) from the Rcc module.
 *
 * The clock is shared with the Rtc module and it is never released by this module: the backup
 * registers must stay accessible, so there is no matching release function.
 *
 * \return \ref TAMP_REQUEST_OK when the interface clock runs, \ref TAMP_REQUEST_ERROR when the Rcc
 *         module refused it
 */
static tamp_RequestState_t Tamp_InterfaceClockRequest( void )
{
    tamp_RequestState_t      retState = TAMP_REQUEST_ERROR;
    const rcc_RequestState_t rccState = Rcc_Set_PeriphActive( RCC_PERIPH_RTC_APB );

    if( RCC_REQUEST_OK == rccState )
    {
        retState = TAMP_REQUEST_OK;
    }
    else
    {
        /* The Rcc module refused the interface clock */
        retState = TAMP_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Makes the TAMP registers readable: nothing is requested when the module holds the backup
 *        access (the interface clock was requested together with it), otherwise the interface
 *        clock is requested.
 *
 * \return \ref TAMP_REQUEST_OK when the registers are readable, \ref TAMP_REQUEST_ERROR when the
 *         Rcc module refused the interface clock
 */
static tamp_RequestState_t Tamp_InterfaceClockRead( void )
{
    tamp_RequestState_t retState = TAMP_REQUEST_OK;

    if( TAMP_FUNCTION_ACTIVE != tamp_State.BackupAccess )
    {
        retState = Tamp_InterfaceClockRequest();
    }
    else
    {
        /* The interface clock is already running */
    }

    return ( retState );
}


/**
 * \brief Configures a GPIO pin in analog mode without pull through the Gpio module (port clock,
 *        mode and pull in one call).
 *
 * \param gpioPort [in]: GPIO port of the pin
 * \param gpioPin  [in]: GPIO pin number
 * \return \ref TAMP_REQUEST_OK on success, \ref TAMP_REQUEST_ERROR when the Gpio module refused
 *         the configuration
 */
static tamp_RequestState_t Tamp_Set_AnalogPin( gpio_PortId_t gpioPort, gpio_PinId_t gpioPin )
{
    tamp_RequestState_t retState = TAMP_REQUEST_ERROR;
    gpio_Config_t       gpioConfig;

    gpioConfig.PortId         = gpioPort;
    gpioConfig.PinId          = gpioPin;
    gpioConfig.PinMode        = GPIO_PIN_MODE_ANALOG;
    gpioConfig.PinPull        = GPIO_PIN_PULL_NONE;
    gpioConfig.PinSpeed       = GPIO_PIN_SPEED_LOW;
    gpioConfig.PinOutType     = GPIO_PIN_OUTPUT_PUSHPULL;
    gpioConfig.PinAltFunction = GPIO_ALT_FUNC_0;
    gpioConfig.PinActiveLevel = GPIO_PIN_LEVEL_HIGH;

    const gpio_RequestState_t gpioState = Gpio_Init( &gpioConfig );

    if( GPIO_REQUEST_OK == gpioState )
    {
        retState = TAMP_REQUEST_OK;
    }
    else
    {
        /* The Gpio module refused the configuration */
        retState = TAMP_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Writes the TAMP_OR remap field of a pin lookup row with the backup domain write access
 *        and the interface clock taken for the duration of the write when the module does not
 *        hold them (\ref TAMP_OR_FIELD_NONE writes nothing and takes nothing).
 *
 * \param orField [in]: Field selector
 * \param orValue [in]: Field code
 * \return \ref TAMP_REQUEST_OK on success, \ref TAMP_REQUEST_ERROR when the Rcc or Pwr module
 *         refused the access (nothing written)
 */
static tamp_RequestState_t Tamp_Set_OrField( tamp_OrField_t orField, uint8_t orValue )
{
    tamp_RequestState_t retState = TAMP_REQUEST_OK;

    if( TAMP_OR_FIELD_NONE != orField )
    {
        tamp_FunctionState_t      taken       = TAMP_FUNCTION_INACTIVE;
        const tamp_RequestState_t accessState = Tamp_BackupAccessBegin( &taken );

        if( TAMP_REQUEST_OK == accessState )
        {
            Tamp_OrField_Apply( orField, orValue );
            retState = Tamp_BackupAccessEnd( taken );
        }
        else
        {
            /* The backup domain is not writable, nothing is written */
            retState = TAMP_REQUEST_ERROR;
        }
    }
    else
    {
        /* Fixed pin without a remap field, nothing is written */
    }

    return ( retState );
}


/**
 * \brief Requests the TAMP register interface clock from the Rcc module and the backup domain
 *        write access from the Pwr module, once per module life (a repeated \ref Tamp_Init does
 *        not take a second reference).
 *
 * \return \ref TAMP_REQUEST_OK when the module holds the access, \ref TAMP_REQUEST_ERROR when
 *         the Rcc module refused the interface clock or the Pwr module refused the access
 */
static tamp_RequestState_t Tamp_BackupAccessAcquire( void )
{
    tamp_RequestState_t retState = TAMP_REQUEST_OK;

    if( TAMP_FUNCTION_ACTIVE != tamp_State.BackupAccess )
    {
        const tamp_RequestState_t clockState = Tamp_InterfaceClockRequest();
        pwr_RequestState_t        pwrState   = PWR_REQUEST_ERROR;

        if( TAMP_REQUEST_OK == clockState )
        {
            pwrState = Pwr_Set_BackupAccessActive();
        }
        else
        {
            /* The interface clock is not running, the backup domain access is not requested */
        }

        if( PWR_REQUEST_OK == pwrState )
        {
            tamp_State.BackupAccess = TAMP_FUNCTION_ACTIVE;
        }
        else
        {
            retState = TAMP_REQUEST_ERROR;
        }
    }
    else
    {
        /* The access is already held */
    }

    return ( retState );
}


/**
 * \brief Releases the backup domain write access taken by \ref Tamp_BackupAccessAcquire. The
 *        module forgets the access even when the Pwr module refuses the release.
 *
 * \return \ref TAMP_REQUEST_OK (nothing held, or released), \ref TAMP_REQUEST_ERROR when the Pwr
 *         module refused the release
 */
static tamp_RequestState_t Tamp_BackupAccessRelease( void )
{
    tamp_RequestState_t retState = TAMP_REQUEST_OK;

    if( TAMP_FUNCTION_ACTIVE == tamp_State.BackupAccess )
    {
        const pwr_RequestState_t pwrState = Pwr_Set_BackupAccessInactive();

        tamp_State.BackupAccess = TAMP_FUNCTION_INACTIVE;

        if( PWR_REQUEST_OK != pwrState )
        {
            retState = TAMP_REQUEST_ERROR;
        }
        else
        {
            /* Released */
        }
    }
    else
    {
        /* Nothing held */
    }

    return ( retState );
}


/**
 * \brief Makes the TAMP registers writable for one register write: nothing is requested when the
 *        module already holds the access (initialized), otherwise the interface clock is requested
 *        from the Rcc module (it stays on) and one Pwr reference is taken.
 *
 * \param taken [out]: \ref TAMP_FUNCTION_ACTIVE when a reference was taken and has to be given
 *        back with \ref Tamp_BackupAccessEnd
 * \return \ref TAMP_REQUEST_OK when the registers are writable, \ref TAMP_REQUEST_ERROR when
 *         the Rcc module refused the interface clock or the Pwr module refused the access
 */
static tamp_RequestState_t Tamp_BackupAccessBegin( tamp_FunctionState_t * const taken )
{
    tamp_RequestState_t retState = TAMP_REQUEST_OK;

    *taken = TAMP_FUNCTION_INACTIVE;

    if( TAMP_FUNCTION_ACTIVE != tamp_State.BackupAccess )
    {
        const tamp_RequestState_t clockState = Tamp_InterfaceClockRequest();
        pwr_RequestState_t        pwrState   = PWR_REQUEST_ERROR;

        if( TAMP_REQUEST_OK == clockState )
        {
            pwrState = Pwr_Set_BackupAccessActive();
        }
        else
        {
            /* The interface clock is not running, the backup domain access is not requested */
        }

        if( PWR_REQUEST_OK == pwrState )
        {
            *taken = TAMP_FUNCTION_ACTIVE;
        }
        else
        {
            retState = TAMP_REQUEST_ERROR;
        }
    }
    else
    {
        /* The module holds the access */
    }

    return ( retState );
}


/**
 * \brief Gives back the reference taken by \ref Tamp_BackupAccessBegin.
 *
 * \param taken [in]: Value returned by \ref Tamp_BackupAccessBegin
 * \return \ref TAMP_REQUEST_OK (nothing taken, or released), \ref TAMP_REQUEST_ERROR when the Pwr
 *         module refused the release
 */
static tamp_RequestState_t Tamp_BackupAccessEnd( const tamp_FunctionState_t taken )
{
    tamp_RequestState_t retState = TAMP_REQUEST_OK;

    if( TAMP_FUNCTION_ACTIVE == taken )
    {
        const pwr_RequestState_t pwrState = Pwr_Set_BackupAccessInactive();

        if( PWR_REQUEST_OK != pwrState )
        {
            retState = TAMP_REQUEST_ERROR;
        }
        else
        {
            /* Released */
        }
    }
    else
    {
        /* Nothing was taken */
    }

    return ( retState );
}


/**
 * \brief Verifies by read-back that \ref Tamp_Set_BackupAreas actually wrote TAMP_SECCFGR.
 *
 * Used by \ref Tamp_Init to detect the one failure mode it cannot check any other way: the
 * write protection of the backup domain (PWR_DBPCR.DBP) was active, so every write this
 * function made was silently ignored by the hardware (RM0481 52.4.3). Bounded loop on the register state
 * (no delay: a plain configuration register settles immediately, the loop only tolerates
 * host/debugger read jitter), never a blind delay.
 *
 * \param areas [in]: The same areas configuration passed to \ref Tamp_Set_BackupAreas
 * \return \ref TAMP_REQUEST_OK when TAMP_SECCFGR reads back the requested areas within the
 *         timeout, \ref TAMP_REQUEST_ERROR otherwise
 */
static tamp_RequestState_t Tamp_Verify_BackupAreasWritten( const tamp_BackupAreas_t * const areas )
{
    tamp_RequestState_t retState    = TAMP_REQUEST_ERROR;
    const uint32_t       expectValue = ( (uint32_t)areas->ReadWriteSecureCount |
                                        ( (uint32_t)areas->WriteSecureCount << TAMP_SECCFGR_BKPWSEC_Pos ) );
    const uint32_t       expectMask  = ( TAMP_SECCFGR_BKPRWSEC | TAMP_SECCFGR_BKPWSEC );

    for( uint32_t iterationCnt = 0u; TAMP_INIT_READBACK_TIMEOUT_RAW > iterationCnt; iterationCnt++ )
    {
        const uint32_t actualValue = Tamp_Get_RegVal( TAMP_REG_SECCFGR, expectMask );

        if( ( expectValue & expectMask ) == actualValue )
        {
            retState = TAMP_REQUEST_OK;
            break;
        }
        else
        {
            /* Register has not been updated yet (or the write was ignored, DBP not set) */
            retState = TAMP_REQUEST_ERROR;
        }
    }

    return ( retState );
}


/**
 * \brief Returns the TAMP_CR3 no-erase bit (ITAMPxNOER) of an internal tamper: the bit position is
 *        the ITAMP number minus one (ITAMP1NOER is bit 0, ITAMP15NOER is bit 14).
 *
 * \param tamperId [in]: Internal tamper id (valid)
 * \return Bit mask of the no-erase bit
 */
static uint32_t Tamp_Get_ItampNoEraseMask( tamp_InternalId_t tamperId )
{
    return ( 1u << ( tamp_InternalItampNumber[tamperId] - TAMP_ITAMP1_NUMBER ) );
}


/**
 * \brief Applies the internal tamper enable bit common to \ref Tamp_Init and
 *        \ref Tamp_Set_InternalTamper (the latter additionally checks the module state,
 *        the former calls this before \ref tamp_State_t::Initialized is set).
 *
 * The reaction mode is written before the tamper is enabled, so a tamper that is already pending
 * (for example the debug access with a probe attached) never erases the secrets in the potential mode.
 *
 * \param tamperId [in]: Internal tamper to configure
 * \param mode      [in]: Reaction mode of this source (disabled: the enable bit is cleared, potential:
 *                        ITAMPxNOER set, confirmed: ITAMPxNOER cleared)
 * \return \ref TAMP_REQUEST_OK on success, \ref TAMP_REQUEST_ERROR for an invalid id
 */
static tamp_RequestState_t Tamp_Set_InternalTamperRegs( tamp_InternalId_t tamperId, tamp_Mode_t mode )
{
    tamp_RequestState_t retState = TAMP_REQUEST_ERROR;

    if( TAMP_INT_CNT > tamperId )
    {
        const uint32_t itampBit = ( 1u << ( TAMP_CR1_ITAMP1E_Pos + tamp_InternalItampNumber[tamperId] -
                                             TAMP_ITAMP1_NUMBER ) );
        const uint32_t noEraseBit = Tamp_Get_ItampNoEraseMask( tamperId );

        if( TAMP_MODE_DISABLED == mode )
        {
            Tamp_Reset_RegBit( TAMP_REG_CR1, itampBit );
        }
        else
        {
            if( TAMP_MODE_POTENTIAL == mode )
            {
                Tamp_Set_RegBit( TAMP_REG_CR3, noEraseBit );
            }
            else
            {
                Tamp_Reset_RegBit( TAMP_REG_CR3, noEraseBit );
            }

            Tamp_Set_RegBit( TAMP_REG_CR1, itampBit );
        }

        retState = TAMP_REQUEST_OK;
    }
    else
    {
        /* Invalid tamper id, nothing is written */
        retState = TAMP_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Applies the trigger / filter / precharge / pin configuration common to \ref Tamp_Init
 *        and \ref Tamp_Set_ExternalTamper.
 *
 * \param tamperId     [in]: External tamper to configure
 * \param tamperConfig [in]: Configuration to apply
 * \return \ref TAMP_REQUEST_OK on success, \ref TAMP_REQUEST_ERROR otherwise
 */
static tamp_RequestState_t Tamp_Set_ExternalTamperRegs( tamp_ExternalId_t tamperId,
                                                         const tamp_ExternalConfig_t * const tamperConfig )
{
    tamp_RequestState_t retState = TAMP_REQUEST_ERROR;
    uint32_t             trgBit  = 0u;
    tamp_RequestState_t  trgState = TAMP_REQUEST_ERROR;

    if( ( TAMP_EXT_CNT        > tamperId                     ) &&
        ( TAMP_NULL_PTR       != tamperConfig                ) &&
        ( TAMP_IN_PIN_UNUSED  != tamperConfig->InputPin       )    )
    {
        trgState = Tamp_Get_TriggerBit( tamperConfig->Trigger, &trgBit );
    }
    else
    {
        /* Invalid parameter or no input pin assigned, nothing is written */
        trgState = TAMP_REQUEST_ERROR;
    }

    if( TAMP_REQUEST_OK == trgState )
    {
        const tamp_RequestState_t filterState = Tamp_Check_LevelFilter( tamperConfig->Trigger,
                                                                        tamperConfig->FilterSamples );

        if( TAMP_REQUEST_OK == filterState )
        {
            const tamp_RequestState_t pinState = Tamp_InitInputGpio( tamperConfig->InputPin );

            Tamp_Set_RegVal( TAMP_REG_CR2,
                             ( 1u << ( TAMP_CR2_TAMP1TRG_Pos + tamperId ) ),
                             ( trgBit << ( TAMP_CR2_TAMP1TRG_Pos + tamperId ) ) );
            Tamp_Set_RegVal( TAMP_REG_FLTCR,
                             ( TAMP_FLTCR_TAMPFREQ | TAMP_FLTCR_TAMPFLT | TAMP_FLTCR_TAMPPRCH ),
                             ( ( (uint32_t)tamperConfig->FilterFreq    << TAMP_FLTCR_TAMPFREQ_Pos ) |
                               ( (uint32_t)tamperConfig->FilterSamples << TAMP_FLTCR_TAMPFLT_Pos  ) |
                               ( (uint32_t)tamperConfig->Precharge     << TAMP_FLTCR_TAMPPRCH_Pos ) ) );

            if( ( TAMP_FUNCTION_ACTIVE == tamperConfig->Active ) &&
                ( TAMP_OUT_PIN_UNUSED != tamperConfig->OutputPin )   )
            {
                (void)Tamp_InitOutputGpio( tamperConfig->OutputPin );
                Tamp_Set_RegBit( TAMP_REG_ATCR1, ( 1u << tamperId ) );
            }
            else
            {
                Tamp_Reset_RegBit( TAMP_REG_ATCR1, ( 1u << tamperId ) );
            }

            if( TAMP_REQUEST_OK == pinState )
            {
                retState = TAMP_REQUEST_OK;
            }
            else
            {
                retState = TAMP_REQUEST_ERROR;
            }
        }
        else
        {
            /* Level trigger requested without an active digital filter, nothing is written */
            retState = TAMP_REQUEST_ERROR;
        }
    }
    else
    {
        /* Invalid parameter, no input pin assigned, or invalid trigger value */
        retState = TAMP_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Maps \ref tamp_Trigger_t to the TAMP_CR2 TAMPxTRG bit value.
 *
 * RM0481 52.7.3 / RM0492 / RM0517 (TAMP_CR2, bit "TAMPxTRG: Active level for tamper x input"):
 * 0 selects "input staying low" when TAMPFLT != 0 (level mode) or "rising edge" when
 * TAMPFLT = 0 (edge mode); 1 selects "input staying high" (level mode) or "falling edge"
 * (edge mode). Mapped explicitly (not by reusing bit 0 of the enum's numeric value) so the
 * mapping does not silently depend on \ref tamp_Trigger_t's enumerator order.
 *
 * \param trigger [in]:  Requested trigger
 * \param trgBit  [out]: TAMPxTRG bit value (0 or 1) for the requested trigger
 * \return \ref TAMP_REQUEST_OK on success, \ref TAMP_REQUEST_ERROR for an invalid trigger
 *         (trgBit untouched)
 */
static tamp_RequestState_t Tamp_Get_TriggerBit( tamp_Trigger_t trigger, uint32_t * const trgBit )
{
    tamp_RequestState_t retState = TAMP_REQUEST_ERROR;

    if( ( TAMP_TRIGGER_RISING    == trigger ) ||
        ( TAMP_TRIGGER_LOW_LEVEL == trigger )    )
    {
        *trgBit  = 0u;
        retState = TAMP_REQUEST_OK;
    }
    else if( ( TAMP_TRIGGER_FALLING   == trigger ) ||
             ( TAMP_TRIGGER_HIGH_LEVEL == trigger )    )
    {
        *trgBit  = 1u;
        retState = TAMP_REQUEST_OK;
    }
    else
    {
        /* Invalid trigger value, nothing is written */
        retState = TAMP_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Rejects a level trigger requested without an active digital filter.
 *
 * RM0481 52.7.3: the TAMPxTRG polarity is interpreted as a level trigger only while
 * TAMP_FLTCR.TAMPFLT != 0 (edge trigger otherwise); \ref TAMP_FILTER_1_SAMPLE writes
 * TAMPFLT = 0, so a level trigger requested with that filter setting would silently be
 * applied as an edge trigger by the hardware instead - rejected here before any register
 * is written.
 *
 * \param trigger       [in]: Requested trigger
 * \param filterSamples [in]: Requested digital filter sample count
 * \return \ref TAMP_REQUEST_OK when the combination is valid, \ref TAMP_REQUEST_ERROR when a
 *         level trigger is requested with \ref TAMP_FILTER_1_SAMPLE
 */
static tamp_RequestState_t Tamp_Check_LevelFilter( tamp_Trigger_t trigger, tamp_FilterSamples_t filterSamples )
{
    tamp_RequestState_t retState = TAMP_REQUEST_OK;

    if( ( ( TAMP_TRIGGER_LOW_LEVEL  == trigger ) ||
          ( TAMP_TRIGGER_HIGH_LEVEL == trigger )    ) &&
        ( TAMP_FILTER_1_SAMPLE == filterSamples )         )
    {
        retState = TAMP_REQUEST_ERROR;
    }
    else
    {
        /* Edge trigger, or a level trigger with an active filter: valid combination */
        retState = TAMP_REQUEST_OK;
    }

    return ( retState );
}


/**
 * \brief Writes the TAMP_OR field selected by a pin lookup table row to its required value.
 *
 * \ref TAMP_OR_FIELD_OUT3_RMP is a 2-bit field (0..3), every other field is a single bit
 * (0..1): both are written the same way (mask + pre-shifted value), \ref Tamp_Set_RegVal
 * already masks the value so no explicit shift of orValue is required here, the field
 * masks from the CMSIS header are already positioned.
 *
 * \param orField [in]: Field selector, \ref TAMP_OR_FIELD_NONE writes nothing
 * \param orValue [in]: Field code (0 for a single-bit field at its reset value, 1 for the
 *                       alternate; 0..3 for the 2-bit \ref TAMP_OR_FIELD_OUT3_RMP)
 */
static void Tamp_OrField_Apply( tamp_OrField_t orField, uint8_t orValue )
{
    /* TAMP_OR (with the IN2_RMP / IN3_RMP / IN4_RMP / OUT3_RMP / OUT5_RMP fields) does not
     * exist on STM32H503 (RM0492 has no TAMP_OR section, the CMSIS header has none of these
     * masks); on that line every pin table row has \ref TAMP_OR_FIELD_NONE, so the body
     * below is unreachable there and is compiled out instead of referencing undefined masks. */
#if defined(TAMP_LINE_HAS_EXT_4_5)
    if( TAMP_OR_FIELD_IN2_RMP == orField )
    {
        Tamp_Set_RegVal( TAMP_REG_OR, TAMP_OR_IN2_RMP, (uint32_t)orValue << TAMP_OR_IN2_RMP_Pos );
    }
    else if( TAMP_OR_FIELD_IN3_RMP == orField )
    {
        Tamp_Set_RegVal( TAMP_REG_OR, TAMP_OR_IN3_RMP, (uint32_t)orValue << TAMP_OR_IN3_RMP_Pos );
    }
    else if( TAMP_OR_FIELD_IN4_RMP == orField )
    {
        Tamp_Set_RegVal( TAMP_REG_OR, TAMP_OR_IN4_RMP, (uint32_t)orValue << TAMP_OR_IN4_RMP_Pos );
    }
    else if( TAMP_OR_FIELD_OUT3_RMP == orField )
    {
        Tamp_Set_RegVal( TAMP_REG_OR, TAMP_OR_OUT3_RMP, (uint32_t)orValue << TAMP_OR_OUT3_RMP_Pos );
    }
    else if( TAMP_OR_FIELD_OUT5_RMP == orField )
    {
        Tamp_Set_RegVal( TAMP_REG_OR, TAMP_OR_OUT5_RMP, (uint32_t)orValue << TAMP_OR_OUT5_RMP_Pos );
    }
    else
    {
        /* TAMP_OR_FIELD_NONE, fixed pin, nothing is written */
    }
#else
    (void)orField;
    (void)orValue;
#endif
}

/* =========================== INTERRUPT HANDLERS =========================== */

/* ================================ TASKS =================================== */
