# TAMP Peripheral Driver

This module provides an abstraction layer for the **tamper detection** (`TAMP`) of STM32 MCUs: the external tamper pins (passive
and active), the internal tamper events, the reaction to a tamper (the erase of the secrets, the timestamp, the interrupt) and the
**backup registers** that survive the reset, the Standby mode and the loss of the main supply. It is a low-level MCAL module: it
owns the tamper hardware, the application and the other modules never touch the registers.

The public interface (`Tamp_Port.h`, `Tamp_Types.h`) is **identical for all supported families**. The implementation
for a family lives in its own branch (`Dev/<Family>` for development, `Releases/<Family>` for the released versions), so
a module or an application written against the interface does not change when the MCU family changes.

This README is the landing page and the **specification of the interface** (master). The family branches contain their
own README with everything that is specific to the family: the instances of the device, their settings and the status of
the implementation.

> **Status: STM32H5 implemented (branch `Dev/STM32H5`).** External tamper trigger/filter/precharge
> configuration, the pin tables, internal tamper enable, reaction (mode, timestamp), status read/clear,
> config lock and the backup registers with their protected areas are implemented and unit tested on
> all 12 STM32H5 lines. Integration tests run on NUCLEO-H563ZI, NUCLEO-H503RB and NUCLEO-H533RE. Not
> implemented yet: the active tamper pattern generator (multi-pair output sharing), per-scope erase
> selection narrower than the hardware default, and RTC/TrustZone forwarding - see Tamp.c for the
> exact list. The module enables the backup domain write access itself through the Pwr module
> (`Pwr_Set_BackupAccessActive`) in `Tamp_Init`.

---

## Intent and purpose

- A product that stores a key, a license or a counter must know when somebody **opens the case, glitches the supply or heats the
  chip**. The tamper peripheral watches the pins and the internal sensors in the backup domain (also on the battery) and reacts in hardware: it **erases the backup
  registers, the backup SRAM, the SRAM2 and the cryptographic peripherals** and flags the event. The module configures
  what is watched and how the system reacts, and reports the event after the wake-up.
- The external tamper inputs can be **passive** (a level or an edge on a pin with a digital filter, ultra-low-power) or **active**
  (the chip drives a pattern on an output and compares it with the input - an open or a short on a mesh is detected). The module
  describes the pins and the meshes as tables and checks that the pin plan is valid.
- Each tamper can be in the **confirmed** mode (the secrets are erased at once, in hardware) or in the **potential** mode (the
  hardware only raises the flag and the software decides - an event that may be a false alarm, for example a temperature excursion).
- The **backup registers** (32-bit words in the backup domain) are the place where the application keeps a small non-volatile state over the
  reset and the Standby mode. In some families they are a part of the RTC peripheral, in the newest ones of the tamper peripheral;
  the module holds them where they belong to the tamper (and the Rtc module offers the same functions as a facade).
- The module hides the differences between the families: the number of the tamper pins and of the internal tampers, the active
  mode, the backup register count and the areas, the security rules and the erase scope differ, the interface stays the same.

What the module is **not**: it does not decide the policy after a tamper (the application, the callback), it does not
provide the secure storage of keys (the Saes module) and it does not erase the flash.

---

## Concept

The module keeps three things apart:

| Item | Meaning |
|---|---|
| **Tamper inputs** | External: `TAMP_EXT_1` ... (the pins of the device) with the trigger (`TAMP_TRIGGER_RISING` / `FALLING` / `LOW_LEVEL` / `HIGH_LEVEL`), the digital filter (the number of samples and the sampling frequency) and the pull-up precharge; active tampers with the output pin for each input. Internal: `TAMP_INT_*` (the temperature, the voltage and the clock monitors, the RTC / crypto faults, the backup domain voltage) - only the ones of the device. |
| **Reaction** | For every tamper: `TAMP_MODE_CONFIRMED` (hardware erase of the secrets), `TAMP_MODE_POTENTIAL` (flag and interrupt only), `TAMP_MODE_DISABLED` (internal tampers only: not enabled, the default), whether it makes a timestamp in the RTC, whether it wakes the MCU, and the erase scope (`TAMP_ERASE_BACKUP_REGS`, `_BACKUP_SRAM`, `_SRAM2`, `_CACHES`, `_CRYPTO`). |
| **Backup registers** | `Tamp_Get_BackupRegisterCount`, `Tamp_Set_BackupRegister`, `Tamp_Get_BackupRegister` - and the protected areas: a read / write secure area, a write secure / read non-secure area and a non-secure area, configured by the size of the areas. |

Rules:

1. `Tamp_Init` requests the write access to the backup domain from the Pwr module (an error of the Pwr module deinitializes the
   module and returns an error) and configures the inputs, the filters, the reaction and the interrupt. The configuration of a tamper input that is
   **already enabled** is rejected (the hardware rule); the configuration may be locked until the reset by `Tamp_Set_ConfigLock`.
2. After a **confirmed** tamper the registers of the secrets are erased by the hardware, the module reads the event with
   `Tamp_Get_TamperStatus` (a bit mask of the events that happened, also while the MCU was in Standby) and clears it with
   `Tamp_Set_TamperStatusClear`. A new write to the secrets is blocked until the flag is cleared.
3. The **active tamper** needs a pair of the pins (an output driving the pattern and an input); the module checks the pairs against the
   table of the device and starts the pattern generator of the hardware.
4. The backup registers are untouched by `Tamp_Init`; their content is lost by the reset of the backup domain and by a confirmed tamper.
5. The callbacks (a tamper, an internal tamper) are called from the interrupt of the tamper peripheral (and after the wake-up
   from Standby they are replaced by the read of the status); the flags are cleared by the module.
6. All parameters are checked before a register is written; the TrustZone / privilege rules of the family are applied to the caller.

---

## Supported families

Every family gets its own story, its own tasks and its own branches `Dev/<Family>` and `Releases/<Family>` cut from this
master.

| Family | Branch | Peripheral | Status |
|---|---|---|---|
| STM32H5 | `Dev/STM32H5` | `TAMP`: 32 backup registers in three protected areas, up to 8 external tamper events on up to 11 pins (passive and active, from 5 to 8 meshes), 13 internal tampers, confirmed and potential mode, erase of the backup registers, the backup SRAM, the SRAM2, the caches and the crypto peripherals, the timestamp, TrustZone | implemented, unit tested on 12 lines, integration tests run on NUCLEO-H563ZI, NUCLEO-H503RB and NUCLEO-H533RE |
| STM32U5 | `Dev/STM32U5` | `TAMP` of the same generation | planned |
| STM32U3 | `Dev/STM32U3` | `TAMP` | planned |
| STM32G4, STM32L4 | `Dev/STM32G4`, `Dev/STM32L4` | the tamper pins and the backup registers are a part of the `RTC` (the functions are provided by the Rtc module and the module forwards to them) | planned |
| STM32H7, STM32F7 | `Dev/STM32H7`, `Dev/STM32F7` | tamper pins and backup registers in the `RTC` (the module forwards to the Rtc module) | planned |
| STM32F4 | `Dev/STM32F4` | `RTC` tamper pins and 20 backup registers (the module forwards to the Rtc module) | planned |

A function without a meaning in a family stays in the interface, returns `TAMP_REQUEST_ERROR` and suppresses the unused
parameters (for example the active tamper or the internal tampers on a family without them, the protected areas of the
backup registers, the erase scope the device does not offer).

---

## Public interface in short (proposal)

| Group | Functions |
|---|---|
| Module management | `Tamp_Get_ModuleVersion`, `Tamp_Init`, `Tamp_Deinit`, `Tamp_Task`, `Tamp_Get_DefaultConfig` |
| Tamper inputs | `Tamp_Set_ExternalTamper`, `Tamp_Set_InternalTamper`, `Tamp_Set_TamperActive`, `Tamp_Set_TamperInactive`, `Tamp_Set_ActiveTamperPairs` |
| Reaction and status | `Tamp_Set_Reaction`, `Tamp_Get_TamperStatus`, `Tamp_Set_TamperStatusClear`, `Tamp_Set_ConfigLock` |
| Backup registers | `Tamp_Get_BackupRegisterCount`, `Tamp_Set_BackupRegister`, `Tamp_Get_BackupRegister`, `Tamp_Set_BackupAreas` |
| Interrupts | `Tamp_Set_IrqPriority` / `Get`, the callbacks are a part of the configuration |
| Pins | `Tamp_InitInputGpio`, `Tamp_InitOutputGpio` |

```c
tamp_ModuleVersion_t Tamp_Get_ModuleVersion        ( void );

tamp_RequestState_t  Tamp_Init                     ( const tamp_Config_t * const tampConfig );
tamp_RequestState_t  Tamp_Deinit                   ( void );
void                 Tamp_Task                     ( void );
tamp_RequestState_t  Tamp_Get_DefaultConfig        ( tamp_Config_t * const tampConfig );

tamp_RequestState_t  Tamp_Set_ExternalTamper       ( tamp_ExternalId_t tamperId, const tamp_ExternalConfig_t * const tamperConfig );
tamp_RequestState_t  Tamp_Set_InternalTamper       ( tamp_InternalId_t tamperId, tamp_Mode_t mode );
tamp_RequestState_t  Tamp_Set_Reaction             ( tamp_TamperId_t tamperId, const tamp_Reaction_t * const reaction );

tamp_RequestState_t  Tamp_Get_TamperStatus         ( tamp_TamperMask_t * const tamperStatus );
tamp_RequestState_t  Tamp_Set_TamperStatusClear    ( tamp_TamperMask_t tamperMask );
tamp_RequestState_t  Tamp_Set_ConfigLock           ( void );

tamp_RequestState_t  Tamp_Get_BackupRegisterCount  ( tamp_BackupCnt_t * const registerCnt );
tamp_RequestState_t  Tamp_Set_BackupRegister       ( tamp_BackupIndex_t index, tamp_BackupData_t data );
tamp_RequestState_t  Tamp_Get_BackupRegister       ( tamp_BackupIndex_t index, tamp_BackupData_t * const data );
```

Every function returns `TAMP_REQUEST_OK` / `TAMP_REQUEST_ERROR` and checks its parameters and the state of the module
before it touches a register.

| Type | Meaning |
|---|---|
| `tamp_Config_t` | The external and internal tamper tables, the reaction, the backup areas, the lock request, callbacks, interrupt priority |
| `tamp_ExternalConfig_t` | Trigger, filter, precharge, active pair (the output), timestamp, wake-up |
| `tamp_Reaction_t` | Mode (confirmed / potential), the erase scope, the timestamp |
| `tamp_TamperMask_t` | Bit mask of the external and the internal tamper events |
| `tamp_BackupData_t` | 32-bit data of a backup register |

---

## Usage

### A case switch that erases the keys

```c
#include "Tamp_Port.h"

static void App_Tamper( tamp_TamperMask_t tamperMask )
{
    /* the secrets are already erased by the hardware; log the event, go to the safe state */
}

tamp_Config_t tampConfig;

(void)Tamp_Get_DefaultConfig( &tampConfig );

tampConfig.External[ TAMP_EXT_1 ].Trigger        = TAMP_TRIGGER_FALLING;
tampConfig.External[ TAMP_EXT_1 ].FilterSamples  = TAMP_FILTER_4_SAMPLES;
tampConfig.Reaction[ TAMP_EXT_1 ].Mode           = TAMP_MODE_CONFIRMED;
tampConfig.Reaction[ TAMP_EXT_1 ].EraseScope     = TAMP_ERASE_BACKUP_REGS | TAMP_ERASE_BACKUP_SRAM;
tampConfig.TamperCallback                        = App_Tamper;

(void)Tamp_Init( &tampConfig );
(void)Tamp_Set_TamperActive( TAMP_EXT_1 );
(void)Tamp_Set_ConfigLock();
```

### After a restart: was it a tamper?

```c
tamp_TamperMask_t tamperStatus = 0u;

(void)Tamp_Get_TamperStatus( &tamperStatus );

if( 0u != tamperStatus )
{
    /* a tamper happened while the MCU was off: handle it, then clear the flags */
    (void)Tamp_Set_TamperStatusClear( tamperStatus );
}
else
{
    /* No tamper event */
}
```

---

## Behaviour and limits

- **Backup domain write access.** `Tamp_Init` takes one reference of the access through `Pwr_Set_BackupAccessActive` and
  holds it while the module is initialized; a repeated `Tamp_Init` does not take a second one. `Tamp_Deinit` releases it
  through `Pwr_Set_BackupAccessInactive`. `Tamp_Set_BackupRegister`, `Tamp_Set_BackupAreas` and `Tamp_Set_TamperStatusClear`
  work also without `Tamp_Init`: when the module does not hold the access they take a reference only for the duration of the
  write. The access of the Pwr module is reference counted, so the reference of the Tamp module and the one of the Rtc module do
  not cancel each other; the protection is enabled again when the last holder releases it. The Rcc module writes the same bit
  by itself (the LSE), which the Pwr module accepts by design.
- **Register interface clock.** The TAMP registers (also the backup registers) need the register interface clock
  (`RCC_APB3ENR.RTCAPBEN`), without it they read as zero and ignore the writes. The module requests it from the Rcc module
  with `Rcc_Set_PeriphActive( RCC_PERIPH_RTC_APB )` (the identifier enables the interface clock alone, no RTC clock selection)
  in `Tamp_Init`, in the setters that work without `Tamp_Init`, and in `Tamp_Get_BackupRegister` / `Tamp_Get_TamperStatus`
  of a module that is not initialized (a value kept over a reset is readable). The module never disables the clock: it is shared
  with the Rtc module and the backup registers must stay accessible.
- **Internal tampers.** The default configuration (`Tamp_Get_DefaultConfig`) leaves every internal tamper disabled
  (`TAMP_MODE_DISABLED`): an enabled tamper with a pending flag blocks the backup registers and the SRAM2 (read as zero, writes
  ignored) until the flag is cleared, for example the debug access tamper while a probe is attached. `TAMP_MODE_POTENTIAL`
  enables the tamper without the erase, `TAMP_MODE_CONFIRMED` with the erase of the secrets.
- **Tamper pins.** `Tamp_InitInputGpio` / `Tamp_InitOutputGpio` configure the pin in the analog mode through `Gpio_Init`
  (it also enables the clock of the GPIO port).
- **Erase is destructive.** A confirmed tamper erases the backup registers: do not keep the data that must survive a tamper there.
- **Battery.** The tamper watch works from the battery in the backup domain; the passive inputs are ultra-low-power, the active
  mode consumes more.
- **Filter and false alarms.** A tamper input floating or noisy produces events; use the digital filter and the pull-up
  precharge. The internal temperature / voltage monitors have the datasheet limits as thresholds.
- **Clock.** The sampling of the tamper inputs follows the RTC clock; the LSE failure is an internal tamper source on the families that have it.
- **Not part of the first version:** the policy after a tamper, the secure counters, the monotonic counters of the backup
  domain on the families that have them, the tamper with the clock input of the timestamp trigger.

---

## Dependencies

- **Rcc** - peripheral clock (the backup domain, the RTC clock source)
- **Pwr** - the backup domain write access (reference counted)
- **Rtc** - the timestamp of a tamper, the RTC clock source; on the families where the tamper is a part of the RTC the module forwards to it
- **Nvic** and **Exti** - the interrupt and the wake-up line
- **Gpio** - the tamper input and output pins
- **RAL (Register Abstraction Layer)** - used internally to access the registers

All mandatory RCC, PWR, NVIC, EXTI and GPIO configurations are handled internally.

---

## Testing

- **Unit tests** (host, Unity + CMock + RegMem): the pin table and the active pair validation, the rejected changes of an enabled or a locked tamper, the
  reaction and erase scope mapping, the status read and clear, the callbacks, the backup register access and the areas, the
  forwarding to the Rtc module on the families where the tamper is in the RTC.
- **Integration tests** (target, probe-rs): a tamper on a pin with the erase of the backup registers, the status after the Standby, the
  active tamper with a mesh open, the potential mode with the software decision.

---

## Interface conventions

The module follows the common rules of the MCAL layer, so that it behaves like the other modules:

- **Lists instead of free members.** Everything that the hardware restricts (the pins, the kernel clock sources, the DMA
  channels, the triggers and the other selectable sources) is a `typedef enum` list with exactly the items that are valid
  for the device line; a pin is an item `TAMP_<SIGNAL>_PIN_<INSTANCE>_P<port><pin>` and there is an `*_UNUSED` item. A choice
  that is not in the list cannot be written.
- **Own peripheral only.** The module calls only the registers / LL functions of its own peripheral. The clocks, the
  interrupts, the pins, the DMA and the EXTI lines go through the API of the Rcc, Nvic, Gpio, Gpdma and Exti modules; a
  missing function is added to that module, it is not bypassed.
- **Init verifies itself.** `Tamp_Init` checks the result at the end; when it is not as required, the module is
  deinitialized and an error is returned. The waiting for the hardware is a loop on the register state with a timeout,
  never a blind delay.
- **Transfer modes.** Where the module moves data, the POLL (served by `Tamp_Task`), ISR and DMA modes have the same
  configuration structure, the same request and the same callbacks (error, half transfer, transfer complete).
- **Flags and states are enums of the module** (`tamp_FunctionState_t`, `tamp_FlagState_t`), the limits and the constants are
  named, there are no magic numbers in the interface.
- **Unsupported functions stay.** A function without a meaning in a family keeps its signature, suppresses the unused
  parameters and returns `TAMP_REQUEST_ERROR`.
- **Documentation.** Every function and type has a Doxygen description, the main header has the `\defgroup`.

---

## Repository layout and branches

The master holds this README and the license. The family branches (`Dev/<Family>`, `Releases/<Family>`) hold:

| Path | Content |
|---|---|
| `README.md` | README of the family: the instances of the device, specifics, errata, status |
| `Tamp_Port.h`, `Tamp_Types.h` | Public interface (identical in all families) |
| `Tamp*.c`, `Tamp.h` | Implementation of the family |
| `CMakeLists.txt` | Library `Tamp_Lib` of the EmBi platform |
| `Tests/UnitTests` | Unit tests (Unity + CMock + RegMem) |
| `Tests/IntegrationTests` | Integration tests on the target (probe-rs) |

A change of the public interface is made in all families.

---

## CMake integration

```cmake
target_link_libraries( User_Lib PRIVATE Tamp_Lib )
```

1. Link `Tamp_Lib` to your CMake library.
2. Include `Tamp_Port.h` in your project.
3. Configure the module as needed for your hardware.

---

## License

This project is licensed under the **Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)**.

You are free to use, modify, and share this work for **non-commercial purposes**, provided appropriate credit is given.

See [LICENSE.md](LICENSE.md) for full terms or visit [creativecommons.org/licenses/by-nc/4.0](https://creativecommons.org/licenses/by-nc/4.0/).

---

## Authors

- **Mr.Nobody** - [embedbits.com](https://embedbits.com)

Contributions are welcome! Please open a pull request.
