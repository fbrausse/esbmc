set(CMAKE_SYSTEM_NAME               Linux)
set(CMAKE_SYSTEM_PROCESSOR          arm)

## Without that flag CMake is not able to pass test compilation check
#set(CMAKE_TRY_COMPILE_TARGET_TYPE   STATIC_LIBRARY)

# aarch64-linux-gnueabihf
set(arch                      "aarch64")
set(os                        "linux")
set(flavor                    "gnu")

set(triple                    "${arch}-${os}-${flavor}")

set(TOOLCHAIN_PREFIX          "${triple}-")
set(CMAKE_SYSROOT             "/usr/${triple}")

set(CMAKE_CROSSCOMPILING_EMULATOR qemu-${arch} -L "${CMAKE_SYSROOT}"
                                               -E LD_LIBRARY_PATH=/usr/lib/gcc/${triple}/11.2.0)

set(CMAKE_ASM_COMPILER_TARGET ${triple})
set(CMAKE_C_COMPILER_TARGET   ${triple})
set(CMAKE_CXX_COMPILER_TARGET ${triple})

set(CMAKE_AR                  ${TOOLCHAIN_PREFIX}ar${CMAKE_EXECUTABLE_SUFFIX})
set(CMAKE_ASM_COMPILER        ${TOOLCHAIN_PREFIX}gcc${CMAKE_EXECUTABLE_SUFFIX})
set(CMAKE_C_COMPILER          ${TOOLCHAIN_PREFIX}gcc${CMAKE_EXECUTABLE_SUFFIX})
set(CMAKE_CXX_COMPILER        ${TOOLCHAIN_PREFIX}g++${CMAKE_EXECUTABLE_SUFFIX})
set(CMAKE_LINKER              ${TOOLCHAIN_PREFIX}ld${CMAKE_EXECUTABLE_SUFFIX})
set(CMAKE_OBJCOPY             ${TOOLCHAIN_PREFIX}objcopy${CMAKE_EXECUTABLE_SUFFIX} CACHE INTERNAL "")
set(CMAKE_RANLIB              ${TOOLCHAIN_PREFIX}ranlib${CMAKE_EXECUTABLE_SUFFIX} CACHE INTERNAL "")
set(CMAKE_SIZE                ${TOOLCHAIN_PREFIX}size${CMAKE_EXECUTABLE_SUFFIX} CACHE INTERNAL "")
set(CMAKE_STRIP               ${TOOLCHAIN_PREFIX}strip${CMAKE_EXECUTABLE_SUFFIX} CACHE INTERNAL "")

#set(CMAKE_C_FLAGS             "-Wno-psabi --specs=nosys.specs -fdata-sections -ffunction-sections -Wl,--gc-sections" CACHE INTERNAL "")
#set(CMAKE_C_FLAGS             "-Wno-psabi -fdata-sections -ffunction-sections -Wl,--gc-sections" CACHE INTERNAL "")
#set(CMAKE_CXX_FLAGS           "${CMAKE_C_FLAGS} -fno-exceptions" CACHE INTERNAL "")

#set(CMAKE_C_FLAGS_DEBUG       "-Os -g" CACHE INTERNAL "")
#set(CMAKE_C_FLAGS_RELEASE     "-Os -DNDEBUG" CACHE INTERNAL "")
#set(CMAKE_CXX_FLAGS_DEBUG     "${CMAKE_C_FLAGS_DEBUG}" CACHE INTERNAL "")
#set(CMAKE_CXX_FLAGS_RELEASE   "${CMAKE_C_FLAGS_RELEASE}" CACHE INTERNAL "")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
