set(CMAKE_SYSTEM_NAME Generic)
cmake_minimum_required(VERSION 3.10)
project(mh1903_boot)

option(COMPILE_BOOT "compile on BOOT target" ON)
option(COMPILE_MAIN "compile on MAIN target" OFF)

set(CROSS_COMPILE_PREFIX arm-none-eabi)
set(CMAKE_C_COMPILER ${CROSS_COMPILE_PREFIX}-gcc)
set(CMAKE_ASM_COMPILER ${CROSS_COMPILE_PREFIX}-gcc)
set(CMAKE_OBJCOPY ${CROSS_COMPILE_PREFIX}-objcopy)
set(CMAKE_OBJDUMP ${CROSS_COMPILE_PREFIX}-objdump)
set(CMAKE_SIZE ${CROSS_COMPILE_PREFIX}-size)
set(MCU cortex-m4)
set(LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/gcc/mh1903b.ld)
set(ARCH_FLAGS "-mcpu=${MCU} -mthumb -mlittle-endian")
set(MCU_FLAGS "${ARCH_FLAGS} -O3 -mfloat-abi=hard -mfpu=fpv4-sp-d16")
set(CMAKE_C_FLAGS "${MCU_FLAGS} -Wall -Wno-unknown-pragmas -Wno-format -g")
set(CMAKE_CXX_FLAGS "${MCU_FLAGS} -Wall -Wno-unknown-pragmas -Wno-format -g")
set(CMAKE_C_FLAGS_DEBUG "-O0 -g2 -ggdb")
set(CMAKE_C_FLAGS_RELEASE "-O3")

if(CMAKE_HOST_SYSTEM_NAME MATCHES "Linux")
    set(HOST_ARCH LINUX_ARCH)
elseif(CMAKE_HOST_SYSTEM_NAME MATCHES "Windows")
    set(HOST_ARCH WINDOWS_ARCH)
else()
    set(HOST_ARCH DARWIN_ARCH)
endif()

if(CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "aarch64")
    message("this is aarch64 cpu")
elseif(CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "x86_64")
    message("this is x86_64 cpu")
endif()

function (print_firmware_info)
    message(STATUS "\n\n=============================================")
    message(STATUS "            - General -")
    message(STATUS "Firmware version:       ${FIRMWARE_VERSION_FULL}")
    message(STATUS "Firmware v. (git):      ${GIT_FIRMWARE_VERSION_STRING}")
    message(STATUS "Firmware-btc version:   ${FIRMWARE_BTC_ONLY_VERSION_FULL}")
    message(STATUS "Firmware-btc v. (git):  ${GIT_FIRMWARE_BTC_ONLY_VERSION_STRING}")
    message(STATUS "Bootloader version:     ${BOOTLOADER_VERSION_FULL}")
    message(STATUS "Bootloader v. (git):    ${GIT_BOOTLOADER_VERSION_STRING}")
    message(STATUS "CMake version:          ${CMAKE_VERSION}")
    message(STATUS "System:                 ${CMAKE_SYSTEM}")
    message(STATUS "Processor:              ${CMAKE_SYSTEM_PROCESSOR}")
    message(STATUS "             - Build -")
    message(STATUS "Compiler version:       ${CMAKE_C_COMPILER_ID} ${C_COMPILER_VERSION}")
    message(STATUS "Compiler:               ${CMAKE_C_COMPILER}")
    message(STATUS "Linker:                 ${CMAKE_LINKER}")
    message(STATUS "Archiver:               ${CMAKE_AR}")
    message(STATUS "Default CFLAGS:         ${CMAKE_C_FLAGS} ${CMAKE_C_FLAGS_${CMAKE_BUILD_TYPE}}")
    message(STATUS "Default EXE LDFLAGS:    ${CMAKE_EXE_LINKER_FLAGS}")
    message(STATUS "            - Options -")
    message(STATUS "Build type:             ${CMAKE_BUILD_TYPE}")
    message(STATUS "Hardware version:       ${HARDWARE}")
    message(STATUS "Verbose:                ${CMAKE_VERBOSE_MAKEFILE}")
    message(STATUS "Coverage flags:         ${COVERAGE}")
    message(STATUS "\n=============================================\n\n")
endfunction()

if(COMPILE_BOOT)
    set(FIRMWARE_VERSION "0.0.1")
    message(STATUS "compile main firmware")
    add_subdirectory(rust)
    add_definitions(
    -DUSE_STDPERIPH_DRIVER 
    -DUSE_FULL_ASSERT 
    -DLV_CONF_INCLUDE_SIMPLE 
    -DLV_LVGL_H_INCLUDE_SIMPLE 
    -DARM_MATH_CM4 
    -D__TARGET_FPU_VFP 
    -DNDEBUG -DATCA_HAL_I2C
    -DCONFIG_USB_DEVICE_MSC
#    -DRUST_MEMORY_DEBUG
    )
    print_firmware_info()

    include_directories(
        Inc
        core
        driver
        driver/usb
        mh1903_lib/SCPU_USB_Lib/SCPU_USB_Device_Library/Class/msc/inc
        mh1903_lib/SCPU_USB_Lib/SCPU_USB_Device_Library/Class/cdc/inc
        mh1903_lib/SCPU_USB_Lib/SCPU_USB_OTG_Driver/inc
        mh1903_lib/SCPU_USB_Lib/SCPU_USB_Device_Library/Core/inc
        mh1903_lib/USB-ESL
        cm_backtrace
        mid/ram
        mid/fatfs
        mid/log
        mid/msg
        mid/param
        mid/checksum
        mid/utils
        mid/quicklz
        mid/ram
        mid/reduced_gl
        app
        crypto
        external/ctaes
        mh1903_lib/MHSCPU_Driver/inc 
        mh1903_lib/Device/MegaHunt/mhscpu/Include 
        mh1903_lib/CMSIS/Include 
        mh1903_lib/MHSCPU_Driver/inc/cryptlib 
        mh1903_lib/MHSCPU_Driver/inc/emvlib
        build/rust-builds
        ${CMAKE_CURRENT_BINARY_DIR}
    )

    set_property(SOURCE mh1903_lib/Device/MegaHunt/mhscpu/Source/GCC/startup_mhscpu.s PROPERTY LANGUAGE C)
    set_property(SOURCE cm_backtrace/fault_handler/gcc/cmb_fault.s PROPERTY LANGUAGE C)
    
    aux_source_directory(mh1903_lib/MHSCPU_Driver/src/ mh1903_driver)
    aux_source_directory(driver/ driver)
    aux_source_directory(cm_backtrace/ cm_backtrace)
    aux_source_directory(app/ app)
    aux_source_directory(core/ core)
    aux_source_directory(crypto/ crypto)

    file(GLOB_RECURSE usb
        "driver/usb/*.c"
        "mh1903_lib/SCPU_USB_Lib/SCPU_USB_Device_Library/Core/src/*.c"
        "mh1903_lib/SCPU_USB_Lib/SCPU_USB_Device_Library/Class/msc/src/*.c"
        "mh1903_lib/SCPU_USB_Lib/SCPU_USB_Device_Library/Class/cdc/src/*.c"
        "mh1903_lib/SCPU_USB_Lib/SCPU_USB_OTG_Driver/src/*.c"
        "mh1903_lib/USB-ESL/CircularBuffer.c"    
    )

    file (GLOB mid
        "mid/fatfs/*.c"
        "mid/log/*.c"
        "mid/msg/*.c"
        "mid/param/*.c"
        "mid/ram/*.c"
        "mid/utils/*.c"
        "mid/checksum/*.c"
        "mid/quicklz/*.c"
        "mid/reduced_gl/*.c"
    )

    file(GLOB_RECURSE ctaes
        "external/ctaes/ctaes.c"
    )

    set(objsrc ${mh1903_driver} 
        ${mh1903_driver} 
        ${driver} 
        ${cm_backtrace}
        ${app} 
        ${crypto} 
        ${usb}
        ${mid}
        ${core}
        ${ctaes}
        mh1903_lib/Device/MegaHunt/mhscpu/Source/GCC/startup_mhscpu.s
        cm_backtrace/fault_handler/gcc/cmb_fault.s)

    set(CMAKE_EXE_LINKER_FLAGS " -T ${LINKER_SCRIPT} -Wl,-Map=mh1903.map -lm -mcpu=${MCU} --specs=nano.specs -specs=nosys.specs -nostartfiles -Wl,-Map=${PROJECT_BINARY_DIR}/${PROJECT_NAME}.map,--cref -Wl,--gc-sections ")

    add_executable(${PROJECT_NAME}.elf ${objsrc})
    add_dependencies(${PROJECT_NAME}.elf rust_c)

    target_link_libraries(${PROJECT_NAME}.elf 
        m
        ${CMAKE_BINARY_DIR}/rust-builds/librust_c.a
        ${CMAKE_CURRENT_SOURCE_DIR}/mh1903_lib/MHSCPU_Driver/lib/libcrypt_s_gnu9.a
    )

    set(ELF_FILE ${PROJECT_BINARY_DIR}/${PROJECT_NAME}.elf)
    set(HEX_FILE ${PROJECT_BINARY_DIR}/${PROJECT_NAME}.hex)
    set(BIN_FILE ${PROJECT_BINARY_DIR}/${PROJECT_NAME}.bin)

    add_custom_command(TARGET "${PROJECT_NAME}.elf" POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -Obinary ${ELF_FILE} ${BIN_FILE}
        COMMAND ${CMAKE_OBJCOPY} -Oihex  ${ELF_FILE} ${HEX_FILE}
        COMMENT "Building ${PROJECT_NAME}.bin and ${PROJECT_NAME}.hex"

        COMMAND ${CMAKE_COMMAND} -E copy ${HEX_FILE} "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.hex"
        COMMAND ${CMAKE_COMMAND} -E copy ${BIN_FILE} "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.bin"

        COMMAND ${CMAKE_SIZE} --format=berkeley ${PROJECT_NAME}.elf ${PROJECT_NAME}.hex
        COMMENT "Invoking: Cross ARM GNU Print Size"
    )
elseif(COMPILE_MAIN)
    set(FIRMWARE_BOOT_VERSION "0.0.1")
    message(FATAL_ERROR "Unknow..")
endif(COMPILE_MAIN)
