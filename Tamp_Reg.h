/**
 * \author Mr.Nobody
 * \file Tamp_Reg.h
 * \ingroup Tamp
 * \brief Tamp module Reg component header file.
 *
 * This component contains functionality for register operations. It is private to the
 * module (the register access is not shared with other modules). Register and bit field
 * names match Bsp/Ral/CMSIS_ST/Include/stm32h563xx.h TAMP_TypeDef (ground truth read
 * directly from the vendored CMSIS header, not from the reference manual text alone,
 * since the RM calls the erase-configuration register RPCFGR while CMSIS names it ERCFGR).
 *
 */

#ifndef TAMP_REG_TAMP_REG_H
#define TAMP_REG_TAMP_REG_H

#ifdef __cplusplus
extern "C" {
#endif

/* ============================= INCLUDES =================================== */
#include "Tamp_Types.h"                     /* Module types definition        */
#include "Gpio_Types.h"                     /* gpio_PortId_t / gpio_PinId_t   */
/* ============================= TYPEDEFS =================================== */

/** \brief List of TAMP registers accessed by the module. */
typedef enum
{
    TAMP_REG_CR1      = 0u, /**< TAMP control register 1 (per-tamper enable)        */
    TAMP_REG_CR2          , /**< TAMP control register 2 (trigger, mask, no-erase)  */
    TAMP_REG_CR3          , /**< TAMP control register 3 (internal tamper no-erase) */
    TAMP_REG_FLTCR        , /**< TAMP filter control register                       */
    TAMP_REG_ATCR1        , /**< TAMP active tamper control register 1              */
    TAMP_REG_ATOR         , /**< TAMP active tamper output register                 */
    TAMP_REG_ATCR2        , /**< TAMP active tamper control register 2              */
    TAMP_REG_SECCFGR      , /**< TAMP secure configuration register                 */
    TAMP_REG_PRIVCFGR     , /**< TAMP privilege configuration register              */
    TAMP_REG_IER          , /**< TAMP interrupt enable register                     */
    TAMP_REG_SR           , /**< TAMP status register                               */
    TAMP_REG_SCR          , /**< TAMP status clear register                         */
    TAMP_REG_OR           , /**< TAMP option register (pin remap)                   */
    TAMP_REG_ERCFGR       , /**< TAMP erase configuration register (RM: RPCFGR)     */
    TAMP_REG_CNT            /**< Count of available TAMP registers                  */
}   tamp_RegId_t;


/** \brief Which TAMP_OR bit field (if any) a pin item requires, and the value it must take. */
typedef enum
{
    TAMP_OR_FIELD_NONE     = 0u, /**< Fixed pin, no TAMP_OR field to write */
    TAMP_OR_FIELD_IN2_RMP      ,
    TAMP_OR_FIELD_IN3_RMP      ,
    TAMP_OR_FIELD_IN4_RMP      ,
    TAMP_OR_FIELD_OUT3_RMP     ,
    TAMP_OR_FIELD_OUT5_RMP
}   tamp_OrField_t;


/** \brief One row of the input pin lookup table: GPIO location + TAMP_OR consequence. */
typedef struct
{
    tamp_InputPin_t PinItem;  /**< The enum item this row describes       */
    gpio_PortId_t   GpioPort; /**< GPIO port of the pin                   */
    gpio_PinId_t    GpioPin;  /**< GPIO pin number                        */
    tamp_OrField_t  OrField;  /**< TAMP_OR field this pin choice requires */
    uint8_t         OrValue;  /**< Value the field must be set to         */
}   tamp_InputPinInfo_t;


/** \brief One row of the output pin lookup table (same shape as \ref tamp_InputPinInfo_t). */
typedef struct
{
    tamp_OutputPin_t PinItem;
    gpio_PortId_t    GpioPort;
    gpio_PinId_t     GpioPin;
    tamp_OrField_t   OrField;
    uint8_t          OrValue;
}   tamp_OutputPinInfo_t;

/* ========================= SYMBOLIC CONSTANTS ============================= */

/** Number of TAMP backup registers (TAMP_BKP0R .. TAMP_BKP31R) */
#define TAMP_REG_BACKUP_REG_CNT               ( 32u )

/* ========================= EXPORTED MACROS ================================ */

/* ========================= EXPORTED VARIABLES ============================= */

/* ======================== EXPORTED FUNCTIONS ============================== */

/* ---------------------------- Generic accessors --------------------------- */

void                        Tamp_Set_RegBit             ( tamp_RegId_t regId, uint32_t bitMask );
void                        Tamp_Reset_RegBit           ( tamp_RegId_t regId, uint32_t bitMask );
uint32_t                    Tamp_Get_RegBit             ( tamp_RegId_t regId, uint32_t bitMask );

void                        Tamp_Set_RegVal             ( tamp_RegId_t regId, uint32_t regMask, uint32_t regValue );
uint32_t                    Tamp_Get_RegVal             ( tamp_RegId_t regId, uint32_t regMask );

/* ----------------------------- Backup registers ---------------------------- */

void                        Tamp_Reg_Set_BackupReg      ( tamp_BackupIndex_t index, tamp_BackupData_t data );
tamp_BackupData_t           Tamp_Reg_Get_BackupReg      ( tamp_BackupIndex_t index );

/* ----------------------------- Pin lookup tables ---------------------------- */

tamp_RequestState_t         Tamp_Reg_Get_InputPinInfo   ( tamp_InputPin_t pinItem,
                                                          const tamp_InputPinInfo_t * * const info );
tamp_RequestState_t         Tamp_Reg_Get_OutputPinInfo  ( tamp_OutputPin_t pinItem,
                                                          const tamp_OutputPinInfo_t * * const info );

#ifdef __cplusplus
}
#endif

#endif /* TAMP_REG_TAMP_REG_H */
