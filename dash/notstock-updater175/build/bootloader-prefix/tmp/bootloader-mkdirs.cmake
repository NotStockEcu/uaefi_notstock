# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/bootloader/subproject"
  "/home/user/uaefi_notstock/dash/notstock-updater175/build/bootloader"
  "/home/user/uaefi_notstock/dash/notstock-updater175/build/bootloader-prefix"
  "/home/user/uaefi_notstock/dash/notstock-updater175/build/bootloader-prefix/tmp"
  "/home/user/uaefi_notstock/dash/notstock-updater175/build/bootloader-prefix/src/bootloader-stamp"
  "/home/user/uaefi_notstock/dash/notstock-updater175/build/bootloader-prefix/src"
  "/home/user/uaefi_notstock/dash/notstock-updater175/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/user/uaefi_notstock/dash/notstock-updater175/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/user/uaefi_notstock/dash/notstock-updater175/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
