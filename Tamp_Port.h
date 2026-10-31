/**
 * \author Mr.Nobody
 * \file Tamp_Port.h
 * \ingroup Tamp
 * \brief Tamp module public functionality
 *
 * This file contains all available public functionality, any other files shall
 * not be used outside of the module.
 *
 */

#ifndef TAMP_TAMP_PORT_H
#define TAMP_TAMP_PORT_H

#ifdef __cplusplus
extern "C" {
#endif

/* ============================== INCLUDES ================================== */
#include "Tamp_Types.h"                     /* Module types definition        */
/* ============================== TYPEDEFS ================================== */

/* ========================== SYMBOLIC CONSTANTS ============================ */

/* ========================== EXPORTED MACROS =============================== */

/* ========================== EXPORTED VARIABLES ============================ */

/* ========================= EXPORTED FUNCTIONS ============================= */

tamp_ModuleVersion_t        Tamp_Get_ModuleVersion      ( void );

tamp_RequestState_t         Tamp_Init                   ( const tamp_Config_t * const tampConfig );
tamp_RequestState_t         Tamp_Deinit                 ( void );
void                        Tamp_Task                   ( void );
tamp_RequestState_t         Tamp_Get_DefaultConfig      ( tamp_Config_t * const tampConfig );

/* ----------------------------- Tamper inputs ------------------------------- */

tamp_RequestState_t         Tamp_Set_ExternalTamper     ( tamp_ExternalId_t tamperId,
                                                          const tamp_ExternalConfig_t * const tamperConfig );
tamp_RequestState_t         Tamp_Set_InternalTamper     ( tamp_InternalId_t tamperId, tamp_Mode_t mode );
tamp_RequestState_t         Tamp_Set_TamperActive       ( tamp_ExternalId_t tamperId );
tamp_RequestState_t         Tamp_Set_TamperInactive     ( tamp_ExternalId_t tamperId );
tamp_RequestState_t         Tamp_Set_ActiveTamperPairs  ( const tamp_ExternalConfig_t * const pairs,
                                                          tamp_PairCnt_t pairCnt );

/* ----------------------------- Reaction and status ------------------------- */

tamp_RequestState_t         Tamp_Set_Reaction           ( tamp_TamperId_t tamperId,
                                                          const tamp_Reaction_t * const reaction );
tamp_RequestState_t         Tamp_Get_TamperStatus        ( tamp_TamperMask_t * const tamperStatus );
tamp_RequestState_t         Tamp_Set_TamperStatusClear   ( tamp_TamperMask_t tamperMask );
tamp_RequestState_t         Tamp_Set_ConfigLock          ( void );

/* ----------------------------- Backup registers ----------------------------- */

tamp_RequestState_t         Tamp_Get_BackupRegisterCount ( tamp_BackupCnt_t * const registerCnt );
tamp_RequestState_t         Tamp_Set_BackupRegister       ( tamp_BackupIndex_t index, tamp_BackupData_t data );
tamp_RequestState_t         Tamp_Get_BackupRegister       ( tamp_BackupIndex_t index,
                                                            tamp_BackupData_t * const data );
tamp_RequestState_t         Tamp_Set_BackupAreas          ( const tamp_BackupAreas_t * const areas );

/* ----------------------------- Interrupts ------------------------------------ */

tamp_RequestState_t         Tamp_Set_IrqPriority          ( tamp_IrqPrio_t irqPriority );
tamp_RequestState_t         Tamp_Get_IrqPriority          ( tamp_IrqPrio_t * const irqPriority );

/* ----------------------------- Pins ------------------------------------------ */

tamp_RequestState_t         Tamp_InitInputGpio            ( tamp_InputPin_t inputPin );
tamp_RequestState_t         Tamp_InitOutputGpio           ( tamp_OutputPin_t outputPin );

#ifdef __cplusplus
}
#endif

#endif /* TAMP_TAMP_PORT_H */
