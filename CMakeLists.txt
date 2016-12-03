cmake_minimum_required(VERSION 2.8)
project(userlkl)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Werror -Wshadow -Wno-error=unused-parameter -O0 -ggdb")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread -DCONFIG_AUTO_LKL_POSIX_HOST -D_FILE_OFFSET_BITS=64")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-short-enums")

include_directories(
    include
    ${LKL_KERNEL_OUT}/install/include
    ${TOP}/uefi/lkl/tools/lkl
)

add_executable(lklfuse
    ${LKL_KERNEL_OUT}/install/lib/lkl.o

    ${TOP}/uefi/lkl/tools/lkl/lib/fs.c
    ${TOP}/uefi/lkl/tools/lkl/lib/iomem.c
    ${TOP}/uefi/lkl/tools/lkl/lib/utils.c
    ${TOP}/uefi/lkl/tools/lkl/lib/virtio_blk.c
    ${TOP}/uefi/lkl/tools/lkl/lib/virtio.c

    src/posix-host.c

    ${TOP}/uefi/lkl/tools/lkl/lklfuse.c

    src/lk/kernel/mutex.c
    src/lk/kernel/semaphore.c
    src/lk/kernel/thread.c
    src/lk/arch_thread.c
    src/lk/debug.c
    src/lk/heap.c
)
target_link_libraries(lklfuse rt fuse dl)
