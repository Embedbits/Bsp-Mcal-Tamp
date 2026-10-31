/**
 * \author Mr.Nobody
 * \file Tamp.h
 * \ingroup Tamp
 * \brief Tamp module private header file.
 *
 * Holds the module state and the prototypes shared between Tamp.c and Tamp_Reg.c. Not
 * included by any other module.
 *
 */

#ifndef TAMP_TAMP_H
#define TAMP_TAMP_H

#ifdef __cplusplus
extern "C" {
#endif

/* ============================== INCLUDES ================================== */
#include "Tamp_Types.h"                     /* Module types definition        */
/* ============================== TYPEDEFS ================================== */

/** \brief Module runtime state, kept private to the module. */
typedef struct
{
    tamp_FunctionState_t Initialized;                     /**< TAMP_FUNCTION_ACTIVE once \ref Tamp_Init succeeded  */
    tamp_FunctionState_t BackupAccess;                    /**< ACTIVE while the Pwr backup access is held          */
    tamp_FunctionState_t ConfigLocked;                    /**< TAMP_FUNCTION_ACTIVE after \ref Tamp_Set_ConfigLock */
    tamp_FunctionState_t ExternalEnabled[ TAMP_EXT_CNT ]; /**< Per external tamper enabled state                   */
    tamp_TamperIsr_t    *TamperCallback;                  /**< Registered callback, \ref TAMP_NULL_PTR if none     */
}   tamp_State_t;

/* ========================== SYMBOLIC CONSTANTS ============================ */

/* ========================== EXPORTED MACROS ================================ */

/* ========================== EXPORTED VARIABLES ============================= */

/* ========================= EXPORTED FUNCTIONS =============================== */

#ifdef __cplusplus
}
#endif

#endif /* TAMP_TAMP_H */
