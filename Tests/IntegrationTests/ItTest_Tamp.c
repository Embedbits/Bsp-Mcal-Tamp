/**
 * \author Mr.Nobody
 * \file ItTest_Tamp.c
 * \ingroup Tamp
 * \brief Integration tests of the Tamp module on target.
 *
 * Tamp module runs on the MCU together with real GPIO and NVIC modules and hardware
 * (TAMP peripheral, backup domain). Tests verify behavior which cannot be verified by unit
 * tests (emulated registers): a real backup register write/read cycle, GPIO pin
 * configuration not faulting, and the default configuration applied against the real
 * peripheral reset state.
 *
 * Used resources:
 * - TAMP (backup domain)
 * - GPIO port C pin 13 (TAMP_IN1, not externally wired - only the GPIO mode is checked)
 *
 * \note The Tamp module requests the TAMP register interface clock (Rcc) and the backup domain
 *       write access (Pwr) itself. A pending tamper flag keeps the backup registers (and the SRAM2)
 *       reading as zero and ignoring writes until it is cleared, so every test clears the tamper
 *       flags first (\ref setUp, a flag can be left by an earlier run).
 * \note No external tamper event is generated (it would need a wired mesh / probe): the
 *       tests exercise the register-level API, not a physical tamper detection.
 */

/* ============================= INCLUDES =================================== */
#include "unity.h"                          /* Unity testing framework        */
#include "IntegrationTesting.h"             /* Integration testing on target  */
#include "Tamp_Port.h"                       /* Module under test              */
#include "Gpio_Port.h"                       /* Pin read-back                  */
/* ============================= TYPEDEFS =================================== */

/* ======================= FORWARD DECLARATIONS ============================= */

/* ========================= SYMBOLIC CONSTANTS ============================= */

/** Backup register index used by the tests */
#define IT_TAMP_BACKUP_INDEX                ( 3u )

/** Backup register data pattern used by the tests */
#define IT_TAMP_BACKUP_DATA                 ( 0x12345678u )

/** Hardware backup register count (TAMP_BKP0R .. TAMP_BKP31R) */
#define IT_TAMP_BACKUP_REG_CNT               ( 32u )

/** Mask of every tamper event flag (clear request of the tamper status) */
#define IT_TAMP_ALL_FLAGS                    ( 0xFFFFFFFFu )
/* ========================== LOCAL VARIABLES =============================== */

/* ============================= TEST SETUP ================================= */

void setUp( void )
{
    /* Flags left by the debug access of the test host would block the backup registers */
    TEST_ASSERT_EQUAL( TAMP_REQUEST_OK, Tamp_Set_TamperStatusClear( IT_TAMP_ALL_FLAGS ) );
}


void tearDown( void )
{
    /* Peripheral state is cleared by system reset after the test case */
}

/* =============================== TESTS ==================================== */

/**
 * \brief A backup register written on the real peripheral reads back the same value.
 * \par Expected results
 * \ref Tamp_Set_BackupRegister / \ref Tamp_Get_BackupRegister round-trip the pattern.
 */
void It_Tamp_BackupRegister_WriteThenRead_RoundTrips( void )
{
    tamp_BackupData_t readBack = 0u;

    TEST_ASSERT_EQUAL( TAMP_REQUEST_OK,
                       Tamp_Set_BackupRegister( IT_TAMP_BACKUP_INDEX, IT_TAMP_BACKUP_DATA ) );
    TEST_ASSERT_EQUAL( TAMP_REQUEST_OK,
                       Tamp_Get_BackupRegister( IT_TAMP_BACKUP_INDEX, &readBack ) );
    TEST_ASSERT_EQUAL_UINT32( IT_TAMP_BACKUP_DATA, readBack );
}


/**
 * \brief The backup register count reads back the hardware value.
 * \par Expected results
 * \ref Tamp_Get_BackupRegisterCount returns 32 on the real peripheral.
 */
void It_Tamp_BackupRegisterCount_Call_Returns32( void )
{
    tamp_BackupCnt_t registerCnt = 0u;

    TEST_ASSERT_EQUAL( TAMP_REQUEST_OK, Tamp_Get_BackupRegisterCount( &registerCnt ) );
    TEST_ASSERT_EQUAL_UINT32( IT_TAMP_BACKUP_REG_CNT, registerCnt );
}


/**
 * \brief Configuring the external tamper 1 input pin on real GPIO succeeds and reads back
 *        analog mode (no pull).
 * \par Expected results
 * \ref Tamp_InitInputGpio returns \ref TAMP_REQUEST_OK, \ref Gpio_Get_PinMode reads back
 * \ref GPIO_PIN_MODE_ANALOG on the pin the pin table assigns to \ref TAMP_IN1_PIN_PC13.
 */
void It_Tamp_InitInputGpio_Tamp1Pin_ConfiguresAnalogMode( void )
{
    gpio_PinMode_t pinMode = GPIO_PIN_MODE_OUTPUT;

    TEST_ASSERT_EQUAL( TAMP_REQUEST_OK, Tamp_InitInputGpio( TAMP_IN1_PIN_PC13 ) );
    TEST_ASSERT_EQUAL( GPIO_REQUEST_OK, Gpio_Get_PinMode( GPIO_PORT_C, GPIO_PIN_ID_13, &pinMode ) );
    TEST_ASSERT_EQUAL( GPIO_PIN_MODE_ANALOG, pinMode );
}


/**
 * \brief The default configuration initializes the module without touching any pin.
 * \par Expected results
 * \ref Tamp_Init with the all-inactive default configuration returns \ref TAMP_REQUEST_OK,
 * the status reads no pending tamper event right after init (the default configuration enables
 * no internal tamper, so the debug access of the test host raises no flag).
 */
void It_Tamp_Init_DefaultConfig_Succeeds( void )
{
    tamp_Config_t      tampConfig;
    tamp_TamperMask_t  tamperStatus = (tamp_TamperMask_t)1u;

    TEST_ASSERT_EQUAL( TAMP_REQUEST_OK, Tamp_Get_DefaultConfig( &tampConfig ) );
    TEST_ASSERT_EQUAL( TAMP_REQUEST_OK, Tamp_Init( &tampConfig ) );

    TEST_ASSERT_EQUAL( TAMP_REQUEST_OK, Tamp_Get_TamperStatus( &tamperStatus ) );
    TEST_ASSERT_EQUAL_UINT32( 0u, tamperStatus );

    TEST_ASSERT_EQUAL( TAMP_REQUEST_OK, Tamp_Deinit() );
}

/* ========================== LOCAL FUNCTIONS ============================== */
