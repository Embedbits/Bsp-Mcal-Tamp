#======================== Unit tests configuration ============================#
# Unit tests of Tamp module. Registered in module CMakeLists.txt by
# UnitTesting_AddPath() and created by UnitTesting_Generate() in "UnitTest"
# build (see EmBi_Platform/CMake/UnitTesting/README.md).

get_filename_component(TAMP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
get_filename_component(MCAL_DIR "${TAMP_DIR}/.." ABSOLUTE)

UnitTesting_Add_Test(
    NAME            Tamp
    TEST_SOURCE     Test_Tamp.c
    UUT_SOURCES     ${TAMP_DIR}/Tamp.c
                    ${TAMP_DIR}/Tamp_Reg.c
    MOCK_HEADERS    ${MCAL_DIR}/Gpio/Gpio_Port.h
                    ${MCAL_DIR}/Nvic/Nvic_Port.h
                    ${MCAL_DIR}/Pwr/Pwr_Port.h
                    ${MCAL_DIR}/Rcc/Rcc_Port.h
    INCLUDE_DIRS    ${TAMP_DIR}
                    ${MCAL_DIR}/Gpio
                    ${MCAL_DIR}/Nvic
                    ${MCAL_DIR}/Pwr
                    ${MCAL_DIR}/Rcc
    LINK_LIBS       ut_RegMem                           # Registers in host memory
                    ut_CmsisHost                        # CMSIS core intrinsics on host
    FORCE_INCLUDES  ${UNIT_TESTING_CMSIS_HOST_HEADER}   # Replaces cmsis_gcc.h
    HOST_LIBS       Ral_Lib                             # ST LL compiled for host
    COVERAGE_EXCLUDE ".*/Ral/.*"
)
