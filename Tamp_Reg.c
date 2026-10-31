/**
 * \author Mr.Nobody
 * \file Tamp_Reg.c
 * \ingroup Tamp
 * \brief Tamp module Reg component functionality.
 *
 * Register access of the module: generic accessors (bounds checked register ID),
 * the backup register accessors and the pin lookup tables (GPIO location and the
 * TAMP_OR consequence of every \ref tamp_InputPin_t / \ref tamp_OutputPin_t item).
 * The masks come from the CMSIS device header (Bsp/Ral/CMSIS_ST/Include/stm32h563xx.h).
 *
 */
/* ============================== INCLUDES ================================== */
#include "Tamp_Reg.h"                       /* Self include                   */
#include "Tamp_Types.h"                     /* Module types definitions       */
#include "Stm32.h"                          /* TAMP registers definition      */
/* ============================== TYPEDEFS ================================== */

/** \brief Physical address of a TAMP register, referenced through its enumeration id. */
typedef struct
{
    tamp_RegId_t         RegId;   /**< Peripheral register ID      */
    volatile uint32_t * RegAddr; /**< Peripheral register address */
}   tamp_RegList_t;

/* ========================== SYMBOLIC CONSTANTS ============================ */

/** \brief Register value without any bit set */
#define TAMP_REG_BITS_CLEAR             ( 0u )

/** \brief TAMP_OR.OUT3_RMP codes (RM0481 52.7.17 / RM0517 58.6.17) for the pins that are
 *  neither the reset default (code 0) nor the single-bit alternate (code 1, PI8). */
#define TAMP_OR_OUT3_CODE_PE3            ( 2u )
#define TAMP_OR_OUT3_CODE_PA2            ( 3u )

/* ========================== LOCAL VARIABLES ============================== */

/** \brief TAMP register addresses, referenced through their enumeration id. */
static const tamp_RegList_t tamp_RegList[] =
{
    { .RegId = TAMP_REG_CR1     , .RegAddr = &(TAMP->CR1     ) },
    { .RegId = TAMP_REG_CR2     , .RegAddr = &(TAMP->CR2     ) },
    { .RegId = TAMP_REG_CR3     , .RegAddr = &(TAMP->CR3     ) },
    { .RegId = TAMP_REG_FLTCR   , .RegAddr = &(TAMP->FLTCR   ) },
    { .RegId = TAMP_REG_ATCR1   , .RegAddr = &(TAMP->ATCR1   ) },
    { .RegId = TAMP_REG_ATOR    , .RegAddr = &(TAMP->ATOR    ) },
    { .RegId = TAMP_REG_ATCR2   , .RegAddr = &(TAMP->ATCR2   ) },
    { .RegId = TAMP_REG_SECCFGR , .RegAddr = &(TAMP->SECCFGR ) },
    { .RegId = TAMP_REG_PRIVCFGR, .RegAddr = &(TAMP->PRIVCFGR) },
    { .RegId = TAMP_REG_IER     , .RegAddr = &(TAMP->IER     ) },
    { .RegId = TAMP_REG_SR      , .RegAddr = &(TAMP->SR      ) },
    { .RegId = TAMP_REG_SCR     , .RegAddr = &(TAMP->SCR     ) },
    { .RegId = TAMP_REG_OR      , .RegAddr = &(TAMP->OR      ) },
    { .RegId = TAMP_REG_ERCFGR  , .RegAddr = &(TAMP->ERCFGR  ) },
};

_Static_assert( (sizeof(tamp_RegList) / sizeof(tamp_RegList_t)) == TAMP_REG_CNT,
                "Tamp_Reg: tamp_RegList has incorrect size." );

/** \brief Backup register addresses (TAMP_BKP0R .. TAMP_BKP31R), indexed 0..31. */
static volatile uint32_t * const tamp_BackupRegList[ TAMP_REG_BACKUP_REG_CNT ] =
{
    &(TAMP->BKP0R ), &(TAMP->BKP1R ), &(TAMP->BKP2R ), &(TAMP->BKP3R ),
    &(TAMP->BKP4R ), &(TAMP->BKP5R ), &(TAMP->BKP6R ), &(TAMP->BKP7R ),
    &(TAMP->BKP8R ), &(TAMP->BKP9R ), &(TAMP->BKP10R), &(TAMP->BKP11R),
    &(TAMP->BKP12R), &(TAMP->BKP13R), &(TAMP->BKP14R), &(TAMP->BKP15R),
    &(TAMP->BKP16R), &(TAMP->BKP17R), &(TAMP->BKP18R), &(TAMP->BKP19R),
    &(TAMP->BKP20R), &(TAMP->BKP21R), &(TAMP->BKP22R), &(TAMP->BKP23R),
    &(TAMP->BKP24R), &(TAMP->BKP25R), &(TAMP->BKP26R), &(TAMP->BKP27R),
    &(TAMP->BKP28R), &(TAMP->BKP29R), &(TAMP->BKP30R), &(TAMP->BKP31R),
};

/** \brief Input pin lookup table, one row per \ref tamp_InputPin_t enumerator (see
 *  Tamp_Types.h header comment for the two sources this was built from). */
static const tamp_InputPinInfo_t tamp_InputPinList[] =
{
    { TAMP_IN1_PIN_PC13, GPIO_PORT_C, GPIO_PIN_ID_13, TAMP_OR_FIELD_NONE   , 0u },
    { TAMP_IN2_PIN_PA0 , GPIO_PORT_A, GPIO_PIN_ID_0 , TAMP_OR_FIELD_IN2_RMP, 0u },
#if defined(TAMP_LINE_HAS_EXT_6_7_8)
    { TAMP_IN2_PIN_PI8 , GPIO_PORT_I, GPIO_PIN_ID_8 , TAMP_OR_FIELD_IN2_RMP, 1u },
#endif
#if defined(TAMP_LINE_HAS_EXT_6_7_8)
    { TAMP_IN3_PIN_PC1 , GPIO_PORT_C, GPIO_PIN_ID_1 , TAMP_OR_FIELD_IN3_RMP, 0u },
    { TAMP_IN3_PIN_PE6 , GPIO_PORT_E, GPIO_PIN_ID_6 , TAMP_OR_FIELD_IN3_RMP, 1u },
#endif
#if defined(TAMP_LINE_HAS_EXT_4_5)
    { TAMP_IN4_PIN_PA2 , GPIO_PORT_A, GPIO_PIN_ID_2 , TAMP_OR_FIELD_IN4_RMP, 0u },
#if defined(TAMP_LINE_HAS_EXT_6_7_8)
    { TAMP_IN4_PIN_PI11, GPIO_PORT_I, GPIO_PIN_ID_11, TAMP_OR_FIELD_IN4_RMP, 1u },
#endif
    { TAMP_IN5_PIN_PA1 , GPIO_PORT_A, GPIO_PIN_ID_1 , TAMP_OR_FIELD_NONE   , 0u },
#endif
#if defined(TAMP_LINE_HAS_EXT_6_7_8)
    { TAMP_IN6_PIN_PE3 , GPIO_PORT_E, GPIO_PIN_ID_3 , TAMP_OR_FIELD_NONE   , 0u },
    { TAMP_IN7_PIN_PE4 , GPIO_PORT_E, GPIO_PIN_ID_4 , TAMP_OR_FIELD_NONE   , 0u },
    { TAMP_IN8_PIN_PE5 , GPIO_PORT_E, GPIO_PIN_ID_5 , TAMP_OR_FIELD_NONE   , 0u },
#endif
};

/** \brief Output pin lookup table, one row per \ref tamp_OutputPin_t enumerator. */
static const tamp_OutputPinInfo_t tamp_OutputPinList[] =
{
    { TAMP_OUT1_PIN_PA0 , GPIO_PORT_A, GPIO_PIN_ID_0 , TAMP_OR_FIELD_NONE    , 0u },
    { TAMP_OUT2_PIN_PC13, GPIO_PORT_C, GPIO_PIN_ID_13, TAMP_OR_FIELD_NONE    , 0u },
#if defined(TAMP_LINE_HAS_EXT_4_5)
    { TAMP_OUT3_PIN_PC13, GPIO_PORT_C, GPIO_PIN_ID_13, TAMP_OR_FIELD_OUT3_RMP, 0u },
#if defined(TAMP_LINE_HAS_EXT_6_7_8)
    { TAMP_OUT3_PIN_PI8 , GPIO_PORT_I, GPIO_PIN_ID_8 , TAMP_OR_FIELD_OUT3_RMP, 1u },
#endif
    { TAMP_OUT3_PIN_PE3 , GPIO_PORT_E, GPIO_PIN_ID_3 , TAMP_OR_FIELD_OUT3_RMP, TAMP_OR_OUT3_CODE_PE3 },
    { TAMP_OUT3_PIN_PA2 , GPIO_PORT_A, GPIO_PIN_ID_2 , TAMP_OR_FIELD_OUT3_RMP, TAMP_OR_OUT3_CODE_PA2 },
    { TAMP_OUT4_PIN_PA1 , GPIO_PORT_A, GPIO_PIN_ID_1 , TAMP_OR_FIELD_NONE    , 0u },
#endif
#if defined(TAMP_LINE_HAS_EXT_6_7_8)
    { TAMP_OUT5_PIN_PI11, GPIO_PORT_I, GPIO_PIN_ID_11, TAMP_OR_FIELD_OUT5_RMP, 0u },
    { TAMP_OUT5_PIN_PC1 , GPIO_PORT_C, GPIO_PIN_ID_1 , TAMP_OR_FIELD_OUT5_RMP, 1u },
    { TAMP_OUT6_PIN_PE6 , GPIO_PORT_E, GPIO_PIN_ID_6 , TAMP_OR_FIELD_NONE    , 0u },
    { TAMP_OUT7_PIN_PE5 , GPIO_PORT_E, GPIO_PIN_ID_5 , TAMP_OR_FIELD_NONE    , 0u },
    { TAMP_OUT8_PIN_PE4 , GPIO_PORT_E, GPIO_PIN_ID_4 , TAMP_OR_FIELD_NONE    , 0u },
#endif
};

/* ========================= EXPORTED FUNCTIONS ============================= */

/**
 * \brief Sets the register bit with given mask.
 *
 * \param regId   [in]: TAMP register ID
 * \param bitMask [in]: Mask to apply to the register
 */
void Tamp_Set_RegBit( tamp_RegId_t regId, uint32_t bitMask )
{
    if( TAMP_REG_CNT > regId )
    {
        *tamp_RegList[regId].RegAddr |= bitMask;
    }
    else
    {
        /* Invalid register ID, no action required */
    }
}


/**
 * \brief Resets the register bit with given mask.
 *
 * \param regId   [in]: TAMP register ID
 * \param bitMask [in]: Mask to apply to the register
 */
void Tamp_Reset_RegBit( tamp_RegId_t regId, uint32_t bitMask )
{
    if( TAMP_REG_CNT > regId )
    {
        *tamp_RegList[regId].RegAddr &= ~bitMask;
    }
    else
    {
        /* Invalid register ID, no action required */
    }
}


/**
 * \brief Gets the register bit value with given mask.
 *
 * \param regId   [in]: TAMP register ID
 * \param bitMask [in]: Mask to apply to the register
 * \return Register bit value with applied mask, no bit set for invalid register ID
 */
uint32_t Tamp_Get_RegBit( tamp_RegId_t regId, uint32_t bitMask )
{
    uint32_t retValue = TAMP_REG_BITS_CLEAR;

    if( TAMP_REG_CNT > regId )
    {
        retValue = ( *tamp_RegList[regId].RegAddr & bitMask );
    }
    else
    {
        /* Invalid register ID, no bit is set */
        retValue = TAMP_REG_BITS_CLEAR;
    }

    return ( retValue );
}


/**
 * \brief Updates the register value with given mask and value.
 *
 * \param regId    [in]: TAMP register ID
 * \param regMask  [in]: Mask to apply to the register
 * \param regValue [in]: Value to set in the register
 */
void Tamp_Set_RegVal( tamp_RegId_t regId, uint32_t regMask, uint32_t regValue )
{
    if( TAMP_REG_CNT > regId )
    {
        *tamp_RegList[regId].RegAddr = ( ( *tamp_RegList[regId].RegAddr & ~regMask ) |
                                         ( regValue & regMask ) );
    }
    else
    {
        /* Invalid register ID, no action required */
    }
}


/**
 * \brief Gets the register value with given mask.
 *
 * \param regId   [in]: TAMP register ID
 * \param regMask [in]: Mask to apply to the register
 * \return Register value with applied mask, no bit set for invalid register ID
 */
uint32_t Tamp_Get_RegVal( tamp_RegId_t regId, uint32_t regMask )
{
    uint32_t retValue = TAMP_REG_BITS_CLEAR;

    if( TAMP_REG_CNT > regId )
    {
        retValue = ( *tamp_RegList[regId].RegAddr & regMask );
    }
    else
    {
        /* Invalid register ID, no bit is set */
        retValue = TAMP_REG_BITS_CLEAR;
    }

    return ( retValue );
}


/**
 * \brief Writes a backup register.
 *
 * \param index [in]: Backup register index, must be lower than \ref TAMP_REG_BACKUP_REG_CNT
 * \param data  [in]: Value to write
 */
void Tamp_Reg_Set_BackupReg( tamp_BackupIndex_t index, tamp_BackupData_t data )
{
    if( TAMP_REG_BACKUP_REG_CNT > index )
    {
        *tamp_BackupRegList[index] = data;
    }
    else
    {
        /* Invalid backup register index, no action required */
    }
}


/**
 * \brief Reads a backup register.
 *
 * \param index [in]: Backup register index, must be lower than \ref TAMP_REG_BACKUP_REG_CNT
 * \return The backup register value, 0 for an invalid index
 */
tamp_BackupData_t Tamp_Reg_Get_BackupReg( tamp_BackupIndex_t index )
{
    tamp_BackupData_t retValue = (tamp_BackupData_t)TAMP_REG_BITS_CLEAR;

    if( TAMP_REG_BACKUP_REG_CNT > index )
    {
        retValue = *tamp_BackupRegList[index];
    }
    else
    {
        /* Invalid backup register index, 0 is returned */
        retValue = (tamp_BackupData_t)TAMP_REG_BITS_CLEAR;
    }

    return ( retValue );
}


/**
 * \brief Looks up the GPIO location and the TAMP_OR consequence of an input pin item.
 *
 * \param pinItem [in]:  Input pin enumerator (never \ref TAMP_IN_PIN_UNUSED)
 * \param info    [out]: Set to point at the matching table row
 * \return \ref TAMP_REQUEST_OK if found, \ref TAMP_REQUEST_ERROR otherwise (info untouched)
 */
tamp_RequestState_t Tamp_Reg_Get_InputPinInfo( tamp_InputPin_t pinItem, const tamp_InputPinInfo_t * * const info )
{
    tamp_RequestState_t retState  = TAMP_REQUEST_ERROR;
    const uint32_t       rowCnt   = (uint32_t)( sizeof(tamp_InputPinList) / sizeof(tamp_InputPinInfo_t) );

    if( TAMP_NULL_PTR != info )
    {
        for( uint32_t rowIdx = 0u; rowCnt > rowIdx; rowIdx++ )
        {
            if( pinItem == tamp_InputPinList[rowIdx].PinItem )
            {
                *info    = &tamp_InputPinList[rowIdx];
                retState = TAMP_REQUEST_OK;
                break;
            }
            else
            {
                /* Not this row, keep searching */
            }
        }
    }
    else
    {
        /* Invalid output pointer, no action required */
    }

    return ( retState );
}


/**
 * \brief Looks up the GPIO location and the TAMP_OR consequence of an output pin item.
 *
 * \param pinItem [in]:  Output pin enumerator (never \ref TAMP_OUT_PIN_UNUSED)
 * \param info    [out]: Set to point at the matching table row
 * \return \ref TAMP_REQUEST_OK if found, \ref TAMP_REQUEST_ERROR otherwise (info untouched)
 */
tamp_RequestState_t Tamp_Reg_Get_OutputPinInfo( tamp_OutputPin_t pinItem,
                                                const tamp_OutputPinInfo_t * * const info )
{
    tamp_RequestState_t retState = TAMP_REQUEST_ERROR;
    const uint32_t       rowCnt  = (uint32_t)( sizeof(tamp_OutputPinList) / sizeof(tamp_OutputPinInfo_t) );

    if( TAMP_NULL_PTR != info )
    {
        for( uint32_t rowIdx = 0u; rowCnt > rowIdx; rowIdx++ )
        {
            if( pinItem == tamp_OutputPinList[rowIdx].PinItem )
            {
                *info    = &tamp_OutputPinList[rowIdx];
                retState = TAMP_REQUEST_OK;
                break;
            }
            else
            {
                /* Not this row, keep searching */
            }
        }
    }
    else
    {
        /* Invalid output pointer, no action required */
    }

    return ( retState );
}

/* =========================== LOCAL FUNCTIONS ============================== */

/* =========================== INTERRUPT HANDLERS =========================== */

/* ================================ TASKS =================================== */
