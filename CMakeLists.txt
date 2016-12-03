cmake_minimum_required(VERSION 2.8)
project(userlkl)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Werror -Wshadow -Wno-error=unused-parameter -O0 -ggdb")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread -DCONFIG_AUTO_LKL_POSIX_HOST -D_FILE_OFFSET_BITS=64")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-short-enums")

include_directories( "/media/Data/repositories/git/efidroid/modules/dynfilefs/include")

include_directories(
    include
    ${LKL_KERNEL_OUT}/install/include
)

add_executable(lklfuse
    ${LKL_KERNEL_OUT}/install/lib/lkl.o

    ${TOP}/uefi/lkl/tools/lkl/lib/fs.c
    ${TOP}/uefi/lkl/tools/lkl/lib/iomem.c
    ${TOP}/uefi/lkl/tools/lkl/lib/utils.c
    ${TOP}/uefi/lkl/tools/lkl/lib/virtio_blk.c
    ${TOP}/uefi/lkl/tools/lkl/lib/virtio.c

    ${TOP}/uefi/lkl/tools/lkl/lib/posix-host.c

    ${TOP}/uefi/lkl/tools/lkl/lklfuse.c
)
target_link_libraries(lklfuse rt fuse dl)
