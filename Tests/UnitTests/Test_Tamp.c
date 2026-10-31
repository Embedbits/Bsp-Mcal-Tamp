/**
 * \author Mr.Nobody
 * \file Test_Tamp.c
 * \ingroup Tamp
 * \brief Unit tests of the Tamp module (STM32H5).
 *
 * Tamp.c and Tamp_Reg.c are compiled unchanged. TAMP registers are emulated by RegMem (plain
 * memory: write-1-to-clear and other hardware side effects are not emulated, tests write and
 * read back the exact bit pattern the code under test is expected to produce). Gpio, Nvic, Pwr
 * and Rcc are mocked by CMock.
 */

/* ============================= INCLUDES =================================== */
#include "unity.h"                          /* Unity testing framework        */
#include "RegMem.h"                         /* Register memory emulation      */
#include "Tamp_Port.h"                       /* Module under test              */
#include "MockGpio_Port.h"                   /* GPIO module mock               */
#include "MockNvic_Port.h"                   /* NVIC module mock               */
#include "MockPwr_Port.h"                    /* PWR module mock                */
#include "MockRcc_Port.h"                    /* RCC module mock                */
#include "Stm32.h"                           /* TAMP registers definition      */
/* ============================= TYPEDEFS =================================== */

/* ======================= FORWARD DECLARATIONS ============================= */

static void UtTamp_Init_Default( void );
static void UtTamp_Expect_AnalogPin( gpio_PortId_t gpioPort, gpio_PinId_t gpioPin );

/* ========================== SYMBOLIC CONSTANTS ============================ */

/** Backup register index used by the tests (any valid index works) */
#define UT_TAMP_BACKUP_INDEX                ( 5u )

/** Out-of-range backup register index (one past the last valid one) */
#define UT_TAMP_BACKUP_INDEX_INVALID        ( 32u )

/** Backup register data pattern used by the tests */
#define UT_TAMP_BACKUP_DATA                 ( 0xA5A5A5A5u )

/** Hardware backup register count (TAMP_BKP0R .. TAMP_BKP31R) */
#define UT_TAMP_BACKUP_REG_CNT               ( 32u )

/** Zone 1 (read/write secure) size used by the backup areas test */
#define UT_TAMP_AREAS_ZONE1_CNT               ( 4u )

/** Zone 2 (write secure) size used by the backup areas test */
#define UT_TAMP_AREAS_ZONE2_CNT               ( 8u )

/** TAMP_CR3 no-erase bits (bit = ITAMP number - 1) of the internal tampers 1-6, 8, 9, 11, 15 */
#define UT_TAMP_ITAMP_NOER_ALL                ( 0x45BFu )

/** TAMP_CR3 no-erase bit of the internal tamper 6 (debug access) */
#define UT_TAMP_ITAMP6_NOER                   ( 0x20u )

/** TAMP_CR3 no-erase bit of the internal tamper 15 (system fault) */
#define UT_TAMP_ITAMP15_NOER                  ( 0x4000u )

/* ========================== LOCAL VARIABLES =============================== */

/** Expected Gpio_Init configuration of the analog tamper pin (kept until the mock is verified) */
static gpio_Config_t utTamp_ExpectedGpio;

/* ============================ TEST FIXTURE ================================ */

void setUp( void )
{
    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Reset() );
}


void tearDown( void )
{
    /* Mocks are verified by generated runner */
}

/* ========================== MODULE VERSION ================================ */

/**
 * \brief The module version is returned with the fields populated.
 * \par Expected results
 * Major / minor / patch fields hold the compiled-in version.
 */
void Ut_Tamp_Get_ModuleVersion_Call_ReturnsVersion( void )
{
    const tamp_ModuleVersion_t version = Tamp_Get_ModuleVersion();

    TEST_ASSERT_EQUAL( 1u, version.Major );
    TEST_ASSERT_EQUAL( 0u, version.Minor );
    TEST_ASSERT_EQUAL( 0u, version.Patch );
}

/* ========================== DEFAULT CONFIG ================================= */

/**
 * \brief A null configuration pointer is rejected.
 * \par Expected results
 * \ref TAMP_REQUEST_ERROR is returned.
 */
void Ut_Tamp_Get_DefaultConfig_NullPtr_ReturnsError( void )
{
    TEST_ASSERT_EQUAL( TAMP_REQUEST_ERROR, Tamp_Get_DefaultConfig( TAMP_NULL_PTR ) );
}


/**
 * \brief A valid configuration pointer is filled with inactive / potential defaults.
 * \par Expected results
 * Every external tamper is inactive with no pin assigned, every reaction is potential
 * with no erase scope, the call succeeds.
 */
void Ut_Tamp_Get_DefaultConfig_ValidPtr_FillsDefaults( void )
{
    tamp_Config_t tampConfig;

    TEST_ASSERT_EQUAL( TAMP_REQUEST_OK, Tamp_Get_DefaultConfig( &tampConfig ) );

    TEST_ASSERT_EQUAL( TAMP_IN_PIN_UNUSED, tampConfig.External[ TAMP_EXT_1 ].InputPin );
    TEST_ASSERT_EQUAL( TAMP_OUT_PIN_UNUSED, tampConfig.External[ TAMP_EXT_1 ].OutputPin );
    TEST_ASSERT_EQUAL( TAMP_FUNCTION_INACTIVE, tampConfig.External[ TAMP_EXT_1 ].Active );
    TEST_ASSERT_EQUAL( TAMP_MODE_POTENTIAL, tampConfig.Reaction[ TAMP_EXT_1 ].Mode );
    TEST_ASSERT_EQUAL( TAMP_ERASE_NONE, tampConfig.Reaction[ TAMP_EXT_1 ].EraseScope );
    TEST_ASSERT_EQUAL( TAMP_NULL_PTR, tampConfig.TamperCallback );
}

/* ========================== BACKUP REGISTERS =============================== */

/**
 * \brief A valid backup register index is written and the value is read back.
 * \par Expected results
 * \ref Tamp_Set_BackupRegister writes the TAMP_BKPxR register directly (no side effects
 * besides the write), \ref Tamp_Get_BackupRegister reads the same value back.
 */
void Ut_Tamp_Set_BackupRegister_ValidIndex_WritesRegister( void )
{
    tamp_BackupData_t readBack = 0u;

    Rcc_Set_PeriphActive_ExpectAndReturn( RCC_PERIPH_RTC_APB, RCC_REQUEST_OK );
    Pwr_Set_BackupAccessActive_ExpectAndReturn( PWR_REQUEST_OK );
    Pwr_Set_BackupAccessInactive_ExpectAndReturn( PWR_REQUEST_OK );

    TEST_ASSERT_EQUAL( TAMP_REQUEST_OK,
                       Tamp_Set_BackupRegister( UT_TAMP_BACKUP_INDEX, UT_TAMP_BACKUP_DATA ) );
    TEST_ASSERT_EQUAL( UT_TAMP_BACKUP_DATA, TAMP->BKP5R );

    Rcc_Set_PeriphActive_ExpectAndReturn( RCC_PERIPH_RTC_APB, RCC_REQUEST_OK );
    TEST_ASSERT_EQUAL( TAMP_REQUEST_OK, Tamp_Get_BackupRegister( UT_TAMP_BACKUP_INDEX, &readBack ) );
    TEST_ASSERT_EQUAL( UT_TAMP_BACKUP_DATA, readBack );
}


/**
 * \brief An out-of-range backup register index is rejected.
 * \par Expected results
 * \ref TAMP_REQUEST_ERROR is returned, nothing is written (mutation check: inverting the
 * bound to `>=` would let index 32 through and this test would then fail, proving the test
 * actually exercises the bound).
 */
void Ut_Tamp_Set_BackupRegister_OutOfRangeIndex_ReturnsError( void )
{
    TEST_ASSERT_EQUAL( TAMP_REQUEST_ERROR,
                       Tamp_Set_BackupRegister( UT_TAMP_BACKUP_INDEX_INVALID, UT_TAMP_BACKUP_DATA ) );
}


/**
 * \brief Reading a backup register through a null output pointer is rejected.
 * \par Expected results
 * \ref TAMP_REQUEST_ERROR is returned.
 */
void Ut_Tamp_Get_BackupRegister_NullPtr_ReturnsError( void )
{
    TEST_ASSERT_EQUAL( TAMP_REQUEST_ERROR, Tamp_Get_BackupRegister( UT_TAMP_BACKUP_INDEX, TAMP_NULL_PTR ) );
}


/**
 * \brief The backup register count matches the hardware (32 registers).
 * \par Expected results
 * \ref Tamp_Get_BackupRegisterCount returns 32.
 */
void Ut_Tamp_Get_BackupRegisterCount_Call_Returns32( void )
{
    tamp_BackupCnt_t registerCnt = 0u;

    TEST_ASSERT_EQUAL( TAMP_REQUEST_OK, Tamp_Get_BackupRegisterCount( &registerCnt ) );
    TEST_ASSERT_EQUAL( UT_TAMP_BACKUP_REG_CNT, registerCnt );
}


/**
 * \brief Protected area sizes are written to TAMP_SECCFGR.BKPRWSEC / BKPWSEC.
 * \par Expected results
 * The register holds the requested zone 1 / zone 2 sizes at their documented bit position.
 */
void Ut_Tamp_Set_BackupAreas_ValidSizes_WritesSeccfgr( void )
{
    tamp_BackupAreas_t areas;

    areas.ReadWriteSecureCount = UT_TAMP_AREAS_ZONE1_CNT;
    areas.WriteSecureCount     = UT_TAMP_AREAS_ZONE2_CNT;

    Rcc_Set_PeriphActive_ExpectAndReturn( RCC_PERIPH_RTC_APB, RCC_REQUEST_OK );
    Pwr_Set_BackupAccessActive_ExpectAndReturn( PWR_REQUEST_OK );
    Pwr_Set_BackupAccessInactive_ExpectAndReturn( PWR_REQUEST_OK );

    TEST_ASSERT_EQUAL( TAMP_REQUEST_OK, Tamp_Set_BackupAreas( &areas ) );
    TEST_ASSERT_EQUAL( UT_TAMP_AREAS_ZONE1_CNT, ( TAMP->SECCFGR & TAMP_SECCFGR_BKPRWSEC ) );
    TEST_ASSERT_EQUAL( ( UT_TAMP_AREAS_ZONE2_CNT << TAMP_SECCFGR_BKPWSEC_Pos ),
                       ( TAMP->SECCFGR & TAMP_SECCFGR_BKPWSEC ) );
}


/**
 * \brief An invalid area configuration (zone 2 smaller than zone 1) is rejected.
 * \par Expected results
 * \ref TAMP_REQUEST_ERROR is returned, TAMP_SECCFGR is left at its reset value.
 */
void Ut_Tamp_Set_BackupAreas_Zone2SmallerThanZone1_ReturnsError( void )
{
    tamp_BackupAreas_t areas;

    areas.ReadWriteSecureCount = UT_TAMP_AREAS_ZONE2_CNT;
    areas.WriteSecureCount     = UT_TAMP_AREAS_ZONE1_CNT;

    TEST_ASSERT_EQUAL( TAMP_REQUEST_ERROR, Tamp_Set_BackupAreas( &areas ) );
    TEST_ASSERT_EQUAL( 0u, TAMP->SECCFGR );
}

/* ========================== STATUS ========================================= */

/**
 * \brief The tamper status mirrors TAMP_SR.
 * \par Expected results
 * \ref Tamp_Get_TamperStatus returns the exact TAMP_SR value preset by the test, the interface
 * clock is requested once (module not initialized).
 */
void Ut_Tamp_Get_TamperStatus_PresetFlag_ReturnsFlag( void )
{
    tamp_TamperMask_t tamperStatus = 0u;

    TAMP->SR = TAMP_SR_TAMP1F;
    Rcc_Set_PeriphActive_ExpectAndReturn( RCC_PERIPH_RTC_APB, RCC_REQUEST_OK );

    TEST_ASSERT_EQUAL( TAMP_REQUEST_OK, Tamp_Get_TamperStatus( &tamperStatus ) );
    TEST_ASSERT_EQUAL( TAMP_SR_TAMP1F, tamperStatus );
}


/**
 * \brief Reading the status through a null pointer is rejected.
 * \par Expected results
 * \ref TAMP_REQUEST_ERROR is returned.
 */
void Ut_Tamp_Get_TamperStatus_NullPtr_ReturnsError( void )
{
    TEST_ASSERT_EQUAL( TAMP_REQUEST_ERROR, Tamp_Get_TamperStatus( TAMP_NULL_PTR ) );
}


/**
 * \brief Clearing a tamper event writes the matching bit of TAMP_SCR.
 * \par Expected results
 * TAMP_SCR holds exactly the cleared mask after the call.
 */
void Ut_Tamp_Set_TamperStatusClear_Flag_WritesScr( void )
{
    Rcc_Set_PeriphActive_ExpectAndReturn( RCC_PERIPH_RTC_APB, RCC_REQUEST_OK );
    Pwr_Set_BackupAccessActive_ExpectAndReturn( PWR_REQUEST_OK );
    Pwr_Set_BackupAccessInactive_ExpectAndReturn( PWR_REQUEST_OK );

    TEST_ASSERT_EQUAL( TAMP_REQUEST_OK, Tamp_Set_TamperStatusClear( TAMP_SCR_CTAMP2F ) );
    TEST_ASSERT_EQUAL( TAMP_SCR_CTAMP2F, TAMP->SCR );
}

/* ======================== BACKUP DOMAIN ACCESS ============================== */

/**
 * \brief The Init enables the backup domain write access through the Pwr module.
 * \par Expected results
 * \ref Pwr_Set_BackupAccessActive is called once (the mock verifies it), \ref TAMP_REQUEST_OK.
 */
void Ut_Tamp_Init_DefaultConfig_CallsPwrBackupAccessActive( void )
{
    UtTamp_Init_Default();
}


/**
 * \brief The Pwr module refuses the backup domain write access.
 * \par Expected results
 * \ref TAMP_REQUEST_ERROR is returned, the module is deinitialized (a setter that needs the
 * initialized module is rejected), no TAMP register but the deinitialization is written and
 * no access is released (none was granted).
 */
void Ut_Tamp_Init_PwrBackupAccessFails_ReturnsErrorAndDeinitializes( void )
{
    tamp_Config_t tampConfig;

    TEST_ASSERT_EQUAL( TAMP_REQUEST_OK, Tamp_Get_DefaultConfig( &tampConfig ) );
    Rcc_Set_PeriphActive_ExpectAndReturn( RCC_PERIPH_RTC_APB, RCC_REQUEST_OK );
    Pwr_Set_BackupAccessActive_ExpectAndReturn( PWR_REQUEST_ERROR );

    TEST_ASSERT_EQUAL( TAMP_REQUEST_ERROR, Tamp_Init( &tampConfig ) );
    TEST_ASSERT_EQUAL( TAMP_REQUEST_ERROR, Tamp_Set_InternalTamper( TAMP_INT_BACKUP_VOLTAGE, TAMP_MODE_POTENTIAL ) );
    TEST_ASSERT_EQUAL( 0u, TAMP->SECCFGR );
}


/**
 * \brief A second Init of an initialized module does not take a second Pwr reference; the
 *        Deinit releases the access once.
 * \par Expected results
 * \ref Pwr_Set_BackupAccessActive once, \ref Pwr_Set_BackupAccessInactive once.
 */
void Ut_Tamp_Init_CalledTwice_TakesOnePwrReference( void )
{
    tamp_Config_t tampConfig;

    UtTamp_Init_Default();
    TEST_ASSERT_EQUAL( TAMP_REQUEST_OK, Tamp_Get_DefaultConfig( &tampConfig ) );
    TEST_ASSERT_EQUAL( TAMP_REQUEST_OK, Tamp_Init( &tampConfig ) );

    Pwr_Set_BackupAccessInactive_ExpectAndReturn( PWR_REQUEST_OK );
    TEST_ASSERT_EQUAL( TAMP_REQUEST_OK, Tamp_Deinit() );
}


/**
 * \brief The Deinit releases the backup domain write access taken by the Init.
 * \par Expected results
 * \ref Pwr_Set_BackupAccessInactive once, \ref TAMP_REQUEST_OK; a second Deinit releases nothing.
 */
void Ut_Tamp_Deinit_AfterInit_ReleasesPwrBackupAccess( void )
{
    UtTamp_Init_Default();
    Pwr_Set_BackupAccessInactive_ExpectAndReturn( PWR_REQUEST_OK );

    TEST_ASSERT_EQUAL( TAMP_REQUEST_OK, Tamp_Deinit() );
    TEST_ASSERT_EQUAL( TAMP_REQUEST_OK, Tamp_Deinit() );
}


/**
 * \brief The Pwr module refuses the release of the access.
 * \par Expected results
 * \ref TAMP_REQUEST_ERROR is returned, the module is deinitialized anyway.
 */
void Ut_Tamp_Deinit_PwrReleaseFails_ReturnsError( void )
{
    UtTamp_Init_Default();
    Pwr_Set_BackupAccessInactive_ExpectAndReturn( PWR_REQUEST_ERROR );

    TEST_ASSERT_EQUAL( TAMP_REQUEST_ERROR, Tamp_Deinit() );
    TEST_ASSERT_EQUAL( TAMP_REQUEST_ERROR, Tamp_Set_InternalTamper( TAMP_INT_BACKUP_VOLTAGE, TAMP_MODE_POTENTIAL ) );
}


/**
 * \brief The Pwr module refuses the access for a backup register write.
 * \par Expected results
 * \ref TAMP_REQUEST_ERROR is returned and the register is not written.
 */
void Ut_Tamp_Set_BackupRegister_PwrAccessFails_ReturnsErrorNoWrite( void )
{
    Rcc_Set_PeriphActive_ExpectAndReturn( RCC_PERIPH_RTC_APB, RCC_REQUEST_OK );
    Pwr_Set_BackupAccessActive_ExpectAndReturn( PWR_REQUEST_ERROR );

    TEST_ASSERT_EQUAL( TAMP_REQUEST_ERROR,
                       Tamp_Set_BackupRegister( UT_TAMP_BACKUP_INDEX, UT_TAMP_BACKUP_DATA ) );
    TEST_ASSERT_EQUAL( 0u, TAMP->BKP5R );
}


/**
 * \brief An initialized module already holds the access: a backup register write calls no Pwr function.
 * \par Expected results
 * \ref TAMP_REQUEST_OK, the register holds the value, the mock sees no further call.
 */
void Ut_Tamp_Set_BackupRegister_Initialized_NoPwrCall( void )
{
    UtTamp_Init_Default();

    TEST_ASSERT_EQUAL( TAMP_REQUEST_OK,
                       Tamp_Set_BackupRegister( UT_TAMP_BACKUP_INDEX, UT_TAMP_BACKUP_DATA ) );
    TEST_ASSERT_EQUAL( UT_TAMP_BACKUP_DATA, TAMP->BKP5R );
}

/* ========================== CONFIG LOCK ===================================== */

/**
 * \brief After \ref Tamp_Set_ConfigLock, a new external tamper configuration is rejected.
 * \par Expected results
 * The first call (before the lock) succeeds, the second call (after the lock) is rejected
 * with \ref TAMP_REQUEST_ERROR and TAMP_CR2 is unchanged by the rejected call (mutation
 * check: removing the lock check would make the second assertion fail, since the second
 * call would overwrite TAMP_CR2 with the new trigger).
 */
void Ut_Tamp_Set_ExternalTamper_AfterConfigLock_ReturnsError( void )
{
    tamp_ExternalConfig_t tamperConfig;

    UtTamp_Init_Default();

    tamperConfig.Trigger       = TAMP_TRIGGER_FALLING;
    tamperConfig.FilterSamples = TAMP_FILTER_1_SAMPLE;
    tamperConfig.FilterFreq    = TAMP_FILTER_FREQ_DIV_32768;
    tamperConfig.Precharge     = TAMP_PRECHARGE_1_CYCLE;
    tamperConfig.Active        = TAMP_FUNCTION_INACTIVE;
    tamperConfig.InputPin      = TAMP_IN1_PIN_PC13;
    tamperConfig.OutputPin     = TAMP_OUT_PIN_UNUSED;

    UtTamp_Expect_AnalogPin( GPIO_PORT_C, GPIO_PIN_ID_13 );

    TEST_ASSERT_EQUAL( TAMP_REQUEST_OK, Tamp_Set_ExternalTamper( TAMP_EXT_1, &tamperConfig ) );

    TEST_ASSERT_EQUAL( TAMP_REQUEST_OK, Tamp_Set_ConfigLock() );

    TEST_ASSERT_EQUAL( TAMP_REQUEST_ERROR, Tamp_Set_ExternalTamper( TAMP_EXT_1, &tamperConfig ) );
}

/* ========================== PIN GPIO INITIALIZATION ========================= */

/**
 * \brief A fixed input pin (no TAMP_OR field) configures the GPIO in analog mode, no pull.
 * \par Expected results
 * \ref Gpio_Set_PinMode and \ref Gpio_Set_PinPull are called with the pin table's
 * (port, pin), \ref TAMP_REQUEST_OK is returned, TAMP_OR is left untouched.
 */
void Ut_Tamp_InitInputGpio_FixedPin_ConfiguresGpio( void )
{
    UtTamp_Expect_AnalogPin( GPIO_PORT_C, GPIO_PIN_ID_13 );

    TEST_ASSERT_EQUAL( TAMP_REQUEST_OK, Tamp_InitInputGpio( TAMP_IN1_PIN_PC13 ) );
    TEST_ASSERT_EQUAL( 0u, TAMP->OR );
}


/**
 * \brief A remappable input pin alternate writes its TAMP_OR field to the matching code.
 * \par Expected results
 * \ref Gpio_Set_PinMode / \ref Gpio_Set_PinPull are called for PA0, TAMP_OR stays at its
 * reset value (\ref TAMP_IN2_PIN_PA0 is the default alternative, code 0). Asserted on the
 * whole register, not the IN2_RMP bit mask, because STM32H503 has no TAMP_OR section at
 * all (RM0492) and its CMSIS header defines no IN2_RMP macro. The Rcc and Pwr calls of the
 * TAMP_OR write exist only on the lines with a remap field, so the mocks accept zero or more calls.
 */
void Ut_Tamp_InitInputGpio_RemappablePinDefault_WritesOrField( void )
{
    Rcc_Set_PeriphActive_IgnoreAndReturn( RCC_REQUEST_OK );
    Pwr_Set_BackupAccessActive_IgnoreAndReturn( PWR_REQUEST_OK );
    Pwr_Set_BackupAccessInactive_IgnoreAndReturn( PWR_REQUEST_OK );
    UtTamp_Expect_AnalogPin( GPIO_PORT_A, GPIO_PIN_ID_0 );

    TEST_ASSERT_EQUAL( TAMP_REQUEST_OK, Tamp_InitInputGpio( TAMP_IN2_PIN_PA0 ) );
    TEST_ASSERT_EQUAL( 0u, TAMP->OR );
}


/**
 * \brief \ref TAMP_IN_PIN_UNUSED is rejected (never configured as a GPIO).
 * \par Expected results
 * \ref TAMP_REQUEST_ERROR is returned, no GPIO call is made (CMock fails the test if an
 * unexpected call happens).
 */
void Ut_Tamp_InitInputGpio_UnusedPin_ReturnsError( void )
{
    TEST_ASSERT_EQUAL( TAMP_REQUEST_ERROR, Tamp_InitInputGpio( TAMP_IN_PIN_UNUSED ) );
}


/**
 * \brief \ref TAMP_OUT_PIN_UNUSED is rejected (never configured as a GPIO).
 * \par Expected results
 * \ref TAMP_REQUEST_ERROR is returned, no GPIO call is made.
 */
void Ut_Tamp_InitOutputGpio_UnusedPin_ReturnsError( void )
{
    TEST_ASSERT_EQUAL( TAMP_REQUEST_ERROR, Tamp_InitOutputGpio( TAMP_OUT_PIN_UNUSED ) );
}

/* ========================== INTERFACE CLOCK ================================ */

/**
 * \brief The Rcc module refuses the TAMP register interface clock for the Init.
 * \par Expected results
 * \ref TAMP_REQUEST_ERROR is returned, the backup domain access is not requested (the Pwr mock
 * sees no call), the module stays deinitialized.
 */
void Ut_Tamp_Init_RccInterfaceClockFails_ReturnsErrorNoPwrCall( void )
{
    tamp_Config_t tampConfig;

    TEST_ASSERT_EQUAL( TAMP_REQUEST_OK, Tamp_Get_DefaultConfig( &tampConfig ) );
    Rcc_Set_PeriphActive_ExpectAndReturn( RCC_PERIPH_RTC_APB, RCC_REQUEST_ERROR );

    TEST_ASSERT_EQUAL( TAMP_REQUEST_ERROR, Tamp_Init( &tampConfig ) );
    TEST_ASSERT_EQUAL( TAMP_REQUEST_ERROR, Tamp_Set_InternalTamper( TAMP_INT_BACKUP_VOLTAGE, TAMP_MODE_POTENTIAL ) );
    TEST_ASSERT_EQUAL( 0u, TAMP->SECCFGR );
}


/**
 * \brief The Rcc module refuses the interface clock for a backup register write.
 * \par Expected results
 * \ref TAMP_REQUEST_ERROR is returned, the register is not written, the Pwr mock sees no call.
 */
void Ut_Tamp_Set_BackupRegister_RccInterfaceClockFails_ReturnsErrorNoWrite( void )
{
    Rcc_Set_PeriphActive_ExpectAndReturn( RCC_PERIPH_RTC_APB, RCC_REQUEST_ERROR );

    TEST_ASSERT_EQUAL( TAMP_REQUEST_ERROR,
                       Tamp_Set_BackupRegister( UT_TAMP_BACKUP_INDEX, UT_TAMP_BACKUP_DATA ) );
    TEST_ASSERT_EQUAL( 0u, TAMP->BKP5R );
}


/**
 * \brief A backup register read of an uninitialized module requests the interface clock, so a
 *        value kept over a reset is readable.
 * \par Expected results
 * \ref Rcc_Set_PeriphActive is called once with \c RCC_PERIPH_RTC_APB, the preset value is
 * returned, the Pwr mock sees no call.
 */
void Ut_Tamp_Get_BackupRegister_Uninitialized_RequestsInterfaceClock( void )
{
    tamp_BackupData_t readBack = 0u;

    TAMP->BKP5R = UT_TAMP_BACKUP_DATA;
    Rcc_Set_PeriphActive_ExpectAndReturn( RCC_PERIPH_RTC_APB, RCC_REQUEST_OK );

    TEST_ASSERT_EQUAL( TAMP_REQUEST_OK, Tamp_Get_BackupRegister( UT_TAMP_BACKUP_INDEX, &readBack ) );
    TEST_ASSERT_EQUAL( UT_TAMP_BACKUP_DATA, readBack );
}


/**
 * \brief An initialized module already runs the interface clock: a backup register read and a
 *        status read request nothing.
 * \par Expected results
 * \ref TAMP_REQUEST_OK, the preset value is returned, the Rcc mock sees no further call.
 */
void Ut_Tamp_Get_BackupRegister_Initialized_NoRccCall( void )
{
    tamp_BackupData_t readBack     = 0u;
    tamp_TamperMask_t tamperStatus = 0u;

    UtTamp_Init_Default();
    TAMP->BKP5R = UT_TAMP_BACKUP_DATA;

    TEST_ASSERT_EQUAL( TAMP_REQUEST_OK, Tamp_Get_BackupRegister( UT_TAMP_BACKUP_INDEX, &readBack ) );
    TEST_ASSERT_EQUAL( UT_TAMP_BACKUP_DATA, readBack );
    TEST_ASSERT_EQUAL( TAMP_REQUEST_OK, Tamp_Get_TamperStatus( &tamperStatus ) );
}


/**
 * \brief The Rcc module refuses the interface clock for a backup register read.
 * \par Expected results
 * \ref TAMP_REQUEST_ERROR is returned and the output stays untouched.
 */
void Ut_Tamp_Get_BackupRegister_RccInterfaceClockFails_ReturnsErrorNoRead( void )
{
    tamp_BackupData_t readBack = UT_TAMP_BACKUP_DATA;

    TAMP->BKP5R = 0u;
    Rcc_Set_PeriphActive_ExpectAndReturn( RCC_PERIPH_RTC_APB, RCC_REQUEST_ERROR );

    TEST_ASSERT_EQUAL( TAMP_REQUEST_ERROR, Tamp_Get_BackupRegister( UT_TAMP_BACKUP_INDEX, &readBack ) );
    TEST_ASSERT_EQUAL( UT_TAMP_BACKUP_DATA, readBack );
}


/**
 * \brief The Rcc module refuses the interface clock for a status read.
 * \par Expected results
 * \ref TAMP_REQUEST_ERROR is returned and the output stays untouched.
 */
void Ut_Tamp_Get_TamperStatus_RccInterfaceClockFails_ReturnsErrorNoRead( void )
{
    tamp_TamperMask_t tamperStatus = (tamp_TamperMask_t)0u;

    TAMP->SR = TAMP_SR_TAMP1F;
    Rcc_Set_PeriphActive_ExpectAndReturn( RCC_PERIPH_RTC_APB, RCC_REQUEST_ERROR );

    TEST_ASSERT_EQUAL( TAMP_REQUEST_ERROR, Tamp_Get_TamperStatus( &tamperStatus ) );
    TEST_ASSERT_EQUAL( 0u, tamperStatus );
}

/* ========================== PIN GPIO (Gpio_Init) ============================ */

/**
 * \brief An output pin configures the GPIO through \ref Gpio_Init (port clock, analog mode, no pull).
 * \par Expected results
 * \ref Gpio_Init is called once with the pin table's port / pin and the analog configuration,
 * \ref TAMP_REQUEST_OK is returned (fixed pin: no TAMP_OR write, no Pwr / Rcc call).
 */
void Ut_Tamp_InitOutputGpio_FixedPin_ConfiguresGpioInit( void )
{
    UtTamp_Expect_AnalogPin( GPIO_PORT_A, GPIO_PIN_ID_0 );

    TEST_ASSERT_EQUAL( TAMP_REQUEST_OK, Tamp_InitOutputGpio( TAMP_OUT1_PIN_PA0 ) );
}


/**
 * \brief The Gpio module refuses the pin configuration.
 * \par Expected results
 * \ref TAMP_REQUEST_ERROR is returned for an input pin and for an output pin.
 */
void Ut_Tamp_InitGpio_GpioInitFails_ReturnsError( void )
{
    Gpio_Init_ExpectAnyArgsAndReturn( GPIO_REQUEST_ERROR );
    TEST_ASSERT_EQUAL( TAMP_REQUEST_ERROR, Tamp_InitInputGpio( TAMP_IN1_PIN_PC13 ) );

    Gpio_Init_ExpectAnyArgsAndReturn( GPIO_REQUEST_ERROR );
    TEST_ASSERT_EQUAL( TAMP_REQUEST_ERROR, Tamp_InitOutputGpio( TAMP_OUT1_PIN_PA0 ) );
}

/* ========================== INTERNAL TAMPER REACTION ======================== */

/**
 * \brief The default configuration does not enable any internal tamper.
 * \par Expected results
 * Every internal tamper enable bit (TAMP_CR1.ITAMPxE) is cleared by \ref Tamp_Init, also the
 * ones preset (a tamper left enabled by an earlier run must not stay armed), and the default
 * mode of every internal tamper is \ref TAMP_MODE_DISABLED.
 */
void Ut_Tamp_Init_DefaultConfig_InternalTampersDisabled_EnableBitsCleared( void )
{
    tamp_Config_t tampConfig;

    TAMP->CR1 = ( UT_TAMP_ITAMP_NOER_ALL << TAMP_CR1_ITAMP1E_Pos );

    TEST_ASSERT_EQUAL( TAMP_REQUEST_OK, Tamp_Get_DefaultConfig( &tampConfig ) );
    TEST_ASSERT_EQUAL( TAMP_MODE_DISABLED, tampConfig.InternalMode[ TAMP_INT_DEBUG_ACCESS ] );

    Rcc_Set_PeriphActive_ExpectAndReturn( RCC_PERIPH_RTC_APB, RCC_REQUEST_OK );
    Pwr_Set_BackupAccessActive_ExpectAndReturn( PWR_REQUEST_OK );
    TEST_ASSERT_EQUAL( TAMP_REQUEST_OK, Tamp_Init( &tampConfig ) );

    TEST_ASSERT_EQUAL_HEX32( 0u, TAMP->CR1 & ( UT_TAMP_ITAMP_NOER_ALL << TAMP_CR1_ITAMP1E_Pos ) );
}


/**
 * \brief Internal tampers configured in the potential mode are enabled with the no-erase bit set.
 * \par Expected results
 * Every internal tamper enable bit (TAMP_CR1.ITAMPxE) is set together with its no-erase bit
 * (TAMP_CR3.ITAMPxNOER), so a tamper that is pending right after the enable does not erase the
 * secrets (a confirmed tamper erases the backup registers and the SRAM2).
 */
void Ut_Tamp_Init_InternalTampersPotential_EnableAndNoEraseBitsSet( void )
{
    tamp_Config_t tampConfig;

    TEST_ASSERT_EQUAL( TAMP_REQUEST_OK, Tamp_Get_DefaultConfig( &tampConfig ) );

    for( uint32_t intIdx = 0u; (uint32_t)TAMP_INT_CNT > intIdx; intIdx++ )
    {
        tampConfig.InternalMode[ intIdx ] = TAMP_MODE_POTENTIAL;
    }

    Rcc_Set_PeriphActive_ExpectAndReturn( RCC_PERIPH_RTC_APB, RCC_REQUEST_OK );
    Pwr_Set_BackupAccessActive_ExpectAndReturn( PWR_REQUEST_OK );
    TEST_ASSERT_EQUAL( TAMP_REQUEST_OK, Tamp_Init( &tampConfig ) );

    TEST_ASSERT_EQUAL_HEX32( UT_TAMP_ITAMP_NOER_ALL, TAMP->CR3 & UT_TAMP_ITAMP_NOER_ALL );
    TEST_ASSERT_EQUAL_HEX32( ( UT_TAMP_ITAMP_NOER_ALL << TAMP_CR1_ITAMP1E_Pos ),
                             ( TAMP->CR1 & ( UT_TAMP_ITAMP_NOER_ALL << TAMP_CR1_ITAMP1E_Pos ) ) );
}


/**
 * \brief The mode of an internal tamper selects the enable and the no-erase bit, only its own ones.
 * \par Expected results
 * Potential: ITAMP6E and ITAMP6NOER set. Confirmed: ITAMP6NOER cleared, ITAMP6E stays set.
 * Disabled: ITAMP6E cleared. No other bit of TAMP_CR1 / TAMP_CR3 changes.
 */
void Ut_Tamp_Set_InternalTamper_Modes_SelectEnableAndNoEraseBits( void )
{
    UtTamp_Init_Default();

    TEST_ASSERT_EQUAL( TAMP_REQUEST_OK, Tamp_Set_InternalTamper( TAMP_INT_DEBUG_ACCESS, TAMP_MODE_POTENTIAL ) );
    TEST_ASSERT_EQUAL_HEX32( UT_TAMP_ITAMP6_NOER, TAMP->CR3 );
    TEST_ASSERT_EQUAL_HEX32( TAMP_CR1_ITAMP6E, TAMP->CR1 );

    TEST_ASSERT_EQUAL( TAMP_REQUEST_OK, Tamp_Set_InternalTamper( TAMP_INT_DEBUG_ACCESS, TAMP_MODE_CONFIRMED ) );
    TEST_ASSERT_EQUAL_HEX32( 0u, TAMP->CR3 );
    TEST_ASSERT_EQUAL_HEX32( TAMP_CR1_ITAMP6E, TAMP->CR1 );

    TEST_ASSERT_EQUAL( TAMP_REQUEST_OK, Tamp_Set_InternalTamper( TAMP_INT_DEBUG_ACCESS, TAMP_MODE_DISABLED ) );
    TEST_ASSERT_EQUAL_HEX32( 0u, TAMP->CR1 );
}


/**
 * \brief The reaction of an internal tamper writes the no-erase bit of its own ITAMP number.
 * \par Expected results
 * The potential reaction of ITAMP15 sets bit 14 only (ITAMP15NOER = ITAMP number - 1), the
 * confirmed reaction clears it.
 */
void Ut_Tamp_Set_Reaction_InternalSystemFault_WritesItamp15NoEraseBit( void )
{
    tamp_Reaction_t reaction;
    tamp_TamperId_t tamperId;

    reaction.Mode       = TAMP_MODE_POTENTIAL;
    reaction.EraseScope = TAMP_ERASE_NONE;
    reaction.Timestamp  = TAMP_FUNCTION_INACTIVE;
    tamperId.Kind       = TAMP_ID_INTERNAL;
    tamperId.Index      = (tamp_TamperIndex_t)TAMP_INT_SYSTEM_FAULT;

    UtTamp_Init_Default();

    TEST_ASSERT_EQUAL( TAMP_REQUEST_OK, Tamp_Set_Reaction( tamperId, &reaction ) );
    TEST_ASSERT_EQUAL_HEX32( UT_TAMP_ITAMP15_NOER, TAMP->CR3 );

    reaction.Mode = TAMP_MODE_CONFIRMED;

    TEST_ASSERT_EQUAL( TAMP_REQUEST_OK, Tamp_Set_Reaction( tamperId, &reaction ) );
    TEST_ASSERT_EQUAL_HEX32( 0u, TAMP->CR3 );
}

/* ========================== TAMPER ACTIVATION =============================== */

/**
 * \brief Activating an external tamper sets its TAMP_CR1 enable bit.
 * \par Expected results
 * TAMP_CR1 bit 0 (TAMP1E) is set after \ref Tamp_Set_TamperActive is called with TAMP_EXT_1.
 */
void Ut_Tamp_Set_TamperActive_ExtId1_SetsCr1Bit( void )
{
    UtTamp_Init_Default();

    TEST_ASSERT_EQUAL( TAMP_REQUEST_OK, Tamp_Set_TamperActive( TAMP_EXT_1 ) );
    TEST_ASSERT_EQUAL( TAMP_CR1_TAMP1E, ( TAMP->CR1 & TAMP_CR1_TAMP1E ) );
}


/**
 * \brief Deactivating an external tamper clears its TAMP_CR1 enable bit.
 * \par Expected results
 * TAMP_CR1 bit 0 is clear after \ref Tamp_Set_TamperInactive is called with TAMP_EXT_1, even though it
 * was set by \ref Tamp_Set_TamperActive right before (mutation check: swapping Set/Reset in
 * \ref Tamp_Set_TamperInactive would leave the bit set and fail this assertion).
 */
void Ut_Tamp_Set_TamperInactive_ExtId1_ClearsCr1Bit( void )
{
    UtTamp_Init_Default();

    TEST_ASSERT_EQUAL( TAMP_REQUEST_OK, Tamp_Set_TamperActive( TAMP_EXT_1 ) );
    TEST_ASSERT_EQUAL( TAMP_REQUEST_OK, Tamp_Set_TamperInactive( TAMP_EXT_1 ) );
    TEST_ASSERT_EQUAL( 0u, ( TAMP->CR1 & TAMP_CR1_TAMP1E ) );
}

/* ========================== TRIGGER MAPPING ================================= */

/**
 * \brief TAMP_TRIGGER_RISING maps to TAMPxTRG = 0 (edge mode, TAMPFLT = 0 required).
 * \par Expected results
 * TAMP_CR2.TAMP1TRG (bit 24) reads 0 after \ref Tamp_Set_ExternalTamper with
 * \ref TAMP_TRIGGER_RISING and \ref TAMP_FILTER_1_SAMPLE (TAMPFLT = 0).
 */
void Ut_Tamp_Set_ExternalTamper_TriggerRising_SetsTrgBitZero( void )
{
    tamp_ExternalConfig_t tamperConfig;

    UtTamp_Init_Default();

    tamperConfig.Trigger       = TAMP_TRIGGER_RISING;
    tamperConfig.FilterSamples = TAMP_FILTER_1_SAMPLE;
    tamperConfig.FilterFreq    = TAMP_FILTER_FREQ_DIV_32768;
    tamperConfig.Precharge     = TAMP_PRECHARGE_1_CYCLE;
    tamperConfig.Active        = TAMP_FUNCTION_INACTIVE;
    tamperConfig.InputPin      = TAMP_IN1_PIN_PC13;
    tamperConfig.OutputPin     = TAMP_OUT_PIN_UNUSED;

    UtTamp_Expect_AnalogPin( GPIO_PORT_C, GPIO_PIN_ID_13 );

    TEST_ASSERT_EQUAL( TAMP_REQUEST_OK, Tamp_Set_ExternalTamper( TAMP_EXT_1, &tamperConfig ) );
    TEST_ASSERT_EQUAL( 0u, ( TAMP->CR2 & TAMP_CR2_TAMP1TRG ) );
}


/**
 * \brief TAMP_TRIGGER_FALLING maps to TAMPxTRG = 1 (edge mode, TAMPFLT = 0 required).
 * \par Expected results
 * TAMP_CR2.TAMP1TRG (bit 24) reads 1 after \ref Tamp_Set_ExternalTamper with
 * \ref TAMP_TRIGGER_FALLING and \ref TAMP_FILTER_1_SAMPLE.
 */
void Ut_Tamp_Set_ExternalTamper_TriggerFalling_SetsTrgBitOne( void )
{
    tamp_ExternalConfig_t tamperConfig;

    UtTamp_Init_Default();

    tamperConfig.Trigger       = TAMP_TRIGGER_FALLING;
    tamperConfig.FilterSamples = TAMP_FILTER_1_SAMPLE;
    tamperConfig.FilterFreq    = TAMP_FILTER_FREQ_DIV_32768;
    tamperConfig.Precharge     = TAMP_PRECHARGE_1_CYCLE;
    tamperConfig.Active        = TAMP_FUNCTION_INACTIVE;
    tamperConfig.InputPin      = TAMP_IN1_PIN_PC13;
    tamperConfig.OutputPin     = TAMP_OUT_PIN_UNUSED;

    UtTamp_Expect_AnalogPin( GPIO_PORT_C, GPIO_PIN_ID_13 );

    TEST_ASSERT_EQUAL( TAMP_REQUEST_OK, Tamp_Set_ExternalTamper( TAMP_EXT_1, &tamperConfig ) );
    TEST_ASSERT_EQUAL( TAMP_CR2_TAMP1TRG, ( TAMP->CR2 & TAMP_CR2_TAMP1TRG ) );
}


/**
 * \brief TAMP_TRIGGER_LOW_LEVEL maps to TAMPxTRG = 0 (level mode, needs TAMPFLT != 0).
 *
 * \par Expected results
 * TAMP_CR2.TAMP1TRG (bit 24) reads 0 with \ref TAMP_FILTER_4_SAMPLES (TAMPFLT != 0). This is
 * the inversion check: a `( Trigger & 1u )` shortcut on \ref tamp_Trigger_t's raw enumerator
 * values (RISING=0, FALLING=1, LOW_LEVEL=2, HIGH_LEVEL=3) would give the same result as this
 * assertion only by the coincidence of that particular enumerator order - \ref
 * Ut_Tamp_Set_ExternalTamper_TriggerHighLevel_SetsTrgBitOne is the pair that would catch a
 * genuinely inverted mapping.
 */
void Ut_Tamp_Set_ExternalTamper_TriggerLowLevel_SetsTrgBitZero( void )
{
    tamp_ExternalConfig_t tamperConfig;

    UtTamp_Init_Default();

    tamperConfig.Trigger       = TAMP_TRIGGER_LOW_LEVEL;
    tamperConfig.FilterSamples = TAMP_FILTER_4_SAMPLES;
    tamperConfig.FilterFreq    = TAMP_FILTER_FREQ_DIV_32768;
    tamperConfig.Precharge     = TAMP_PRECHARGE_1_CYCLE;
    tamperConfig.Active        = TAMP_FUNCTION_INACTIVE;
    tamperConfig.InputPin      = TAMP_IN1_PIN_PC13;
    tamperConfig.OutputPin     = TAMP_OUT_PIN_UNUSED;

    UtTamp_Expect_AnalogPin( GPIO_PORT_C, GPIO_PIN_ID_13 );

    TEST_ASSERT_EQUAL( TAMP_REQUEST_OK, Tamp_Set_ExternalTamper( TAMP_EXT_1, &tamperConfig ) );
    TEST_ASSERT_EQUAL( 0u, ( TAMP->CR2 & TAMP_CR2_TAMP1TRG ) );
}


/**
 * \brief TAMP_TRIGGER_HIGH_LEVEL maps to TAMPxTRG = 1 (level mode, needs TAMPFLT != 0).
 * \par Expected results
 * TAMP_CR2.TAMP1TRG (bit 24) reads 1 with \ref TAMP_FILTER_4_SAMPLES.
 */
void Ut_Tamp_Set_ExternalTamper_TriggerHighLevel_SetsTrgBitOne( void )
{
    tamp_ExternalConfig_t tamperConfig;

    UtTamp_Init_Default();

    tamperConfig.Trigger       = TAMP_TRIGGER_HIGH_LEVEL;
    tamperConfig.FilterSamples = TAMP_FILTER_4_SAMPLES;
    tamperConfig.FilterFreq    = TAMP_FILTER_FREQ_DIV_32768;
    tamperConfig.Precharge     = TAMP_PRECHARGE_1_CYCLE;
    tamperConfig.Active        = TAMP_FUNCTION_INACTIVE;
    tamperConfig.InputPin      = TAMP_IN1_PIN_PC13;
    tamperConfig.OutputPin     = TAMP_OUT_PIN_UNUSED;

    UtTamp_Expect_AnalogPin( GPIO_PORT_C, GPIO_PIN_ID_13 );

    TEST_ASSERT_EQUAL( TAMP_REQUEST_OK, Tamp_Set_ExternalTamper( TAMP_EXT_1, &tamperConfig ) );
    TEST_ASSERT_EQUAL( TAMP_CR2_TAMP1TRG, ( TAMP->CR2 & TAMP_CR2_TAMP1TRG ) );
}


/**
 * \brief A level trigger requested with no active digital filter is rejected.
 * \par Expected results
 * \ref TAMP_REQUEST_ERROR is returned for \ref TAMP_TRIGGER_LOW_LEVEL with
 * \ref TAMP_FILTER_1_SAMPLE (TAMPFLT = 0), TAMP_CR2 is left unwritten and no GPIO call is
 * made (CMock fails the test if an unexpected call happens).
 */
void Ut_Tamp_Set_ExternalTamper_LevelTriggerNoFilter_ReturnsError( void )
{
    tamp_ExternalConfig_t tamperConfig;

    UtTamp_Init_Default();

    tamperConfig.Trigger       = TAMP_TRIGGER_LOW_LEVEL;
    tamperConfig.FilterSamples = TAMP_FILTER_1_SAMPLE;
    tamperConfig.FilterFreq    = TAMP_FILTER_FREQ_DIV_32768;
    tamperConfig.Precharge     = TAMP_PRECHARGE_1_CYCLE;
    tamperConfig.Active        = TAMP_FUNCTION_INACTIVE;
    tamperConfig.InputPin      = TAMP_IN1_PIN_PC13;
    tamperConfig.OutputPin     = TAMP_OUT_PIN_UNUSED;

    TEST_ASSERT_EQUAL( TAMP_REQUEST_ERROR, Tamp_Set_ExternalTamper( TAMP_EXT_1, &tamperConfig ) );
    TEST_ASSERT_EQUAL( 0u, TAMP->CR2 );
}

/* =========================== LOCAL FUNCTIONS ============================== */

/** Initializes the module with the all-inactive default configuration (no GPIO calls). */
static void UtTamp_Init_Default( void )
{
    tamp_Config_t tampConfig;

    TEST_ASSERT_EQUAL( TAMP_REQUEST_OK, Tamp_Get_DefaultConfig( &tampConfig ) );
    Rcc_Set_PeriphActive_ExpectAndReturn( RCC_PERIPH_RTC_APB, RCC_REQUEST_OK );
    Pwr_Set_BackupAccessActive_ExpectAndReturn( PWR_REQUEST_OK );
    TEST_ASSERT_EQUAL( TAMP_REQUEST_OK, Tamp_Init( &tampConfig ) );
}


/**
 * \brief Expects the analog configuration of a tamper pin through \ref Gpio_Init (port clock,
 *        analog mode, no pull).
 *
 * \param gpioPort [in]: Expected GPIO port
 * \param gpioPin  [in]: Expected GPIO pin
 */
static void UtTamp_Expect_AnalogPin( gpio_PortId_t gpioPort, gpio_PinId_t gpioPin )
{
    utTamp_ExpectedGpio                = (gpio_Config_t){ 0 };
    utTamp_ExpectedGpio.PortId         = gpioPort;
    utTamp_ExpectedGpio.PinId          = gpioPin;
    utTamp_ExpectedGpio.PinMode        = GPIO_PIN_MODE_ANALOG;
    utTamp_ExpectedGpio.PinPull        = GPIO_PIN_PULL_NONE;
    utTamp_ExpectedGpio.PinSpeed       = GPIO_PIN_SPEED_LOW;
    utTamp_ExpectedGpio.PinOutType     = GPIO_PIN_OUTPUT_PUSHPULL;
    utTamp_ExpectedGpio.PinAltFunction = GPIO_ALT_FUNC_0;
    utTamp_ExpectedGpio.PinActiveLevel = GPIO_PIN_LEVEL_HIGH;

    Gpio_Init_ExpectAndReturn( &utTamp_ExpectedGpio, GPIO_REQUEST_OK );
}
