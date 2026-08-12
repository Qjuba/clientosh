cmake_minimum_required(VERSION 3.21)
project(clientosh VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

include(GNUInstallDirs)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network Svg)

# ---- libssh (pkg-config, LIBSSH_ROOT, or system paths) ----
set(LIBSSH_ROOT "" CACHE PATH "Optional libssh install prefix (include/ + lib/)")

set(CLIENTOSH_LIBSSH_TARGET "")
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND AND NOT LIBSSH_ROOT)
    pkg_check_modules(PC_LIBSSH QUIET IMPORTED_TARGET libssh)
    if(TARGET PkgConfig::PC_LIBSSH)
        set(CLIENTOSH_LIBSSH_TARGET PkgConfig::PC_LIBSSH)
    endif()
endif()

# ---- OpenSSL (crypto) for the vault (AES-256-GCM) ----
# Prefer the OpenSSL that lives beside libssh (e.g. under LIBSSH_ROOT) so the
# bundled runtime DLLs already copied next to the exe are the ones we use.
set(OPENSSL_ROOT_DIR "" CACHE PATH "Optional OpenSSL install prefix")
if(NOT OPENSSL_ROOT_DIR AND LIBSSH_ROOT)
    set(OPENSSL_ROOT_DIR "${LIBSSH_ROOT}")
endif()
if(WIN32 AND OPENSSL_ROOT_DIR)
    set(OPENSSL_USE_STATIC_LIBS FALSE)
endif()
find_package(OpenSSL REQUIRED)
set(CLIENTOSH_OPENSSL_TARGET OpenSSL::Crypto)

if(NOT CLIENTOSH_LIBSSH_TARGET)
    set(_libssh_hints)
    if(LIBSSH_ROOT)
        list(APPEND _libssh_hints "${LIBSSH_ROOT}")
    endif()
    if(WIN32)
        list(APPEND _libssh_hints "C:/msys64/mingw64" "C:/msys64/ucrt64")
    elseif(APPLE)
        list(APPEND _libssh_hints "/opt/homebrew" "/usr/local")
    endif()

    find_path(LIBSSH_INCLUDE_DIR
        NAMES libssh/libssh.h
        HINTS ${_libssh_hints}
        PATH_SUFFIXES include
        REQUIRED
    )
    find_library(LIBSSH_LIBRARY
        NAMES ssh libssh
        HINTS ${_libssh_hints}
        PATH_SUFFIXES lib lib64
        REQUIRED
    )

    add_library(clientosh_libssh INTERFACE)
    target_include_directories(clientosh_libssh INTERFACE "${LIBSSH_INCLUDE_DIR}")
    target_link_libraries(clientosh_libssh INTERFACE "${LIBSSH_LIBRARY}")
    set(CLIENTOSH_LIBSSH_TARGET clientosh_libssh)
endif()

add_executable(clientosh
    src/main.cpp
    src/MainWindow.h
    src/MainWindow.cpp
    src/TerminalWidget.h
    src/TerminalWidget.cpp
    src/core/SshSession.h
    src/core/SshSession.cpp
    src/core/SessionProfile.h
    src/core/SessionManager.h
    src/core/SessionManager.cpp
    src/core/SftpClient.h
    src/core/SftpClient.cpp
    src/core/ServerStatsClient.h
    src/core/ServerStatsClient.cpp
    src/core/FontManager.h
    src/core/FontManager.cpp
    src/core/CryptoEngine.h
    src/core/CryptoEngine.cpp
    src/core/KeyringAdapter.h
    src/core/KeyringAdapter.cpp
    src/core/VaultManager.h
    src/core/VaultManager.cpp
    src/PanelTypes.h
    src/DropOverlay.h
    src/DropOverlay.cpp
    src/PaneFrame.h
    src/PaneFrame.cpp
    src/DashboardPage.h
    src/DashboardPage.cpp
    src/SessionWorkspace.h
    src/SessionWorkspace.cpp
    src/SessionChip.h
    src/SessionChip.cpp
    src/TopNavBar.h
    src/TopNavBar.cpp
    src/SftpWindow.h
    src/SftpWindow.cpp
    src/ui/Motion.h
    src/ui/Motion.cpp
    resources.qrc
    $<$<BOOL:${WIN32}>:clientosh.rc>
)

target_include_directories(clientosh PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
)

target_link_libraries(clientosh PRIVATE
    Qt6::Widgets
    Qt6::Network
    Qt6::Svg
    ${CLIENTOSH_LIBSSH_TARGET}
    ${CLIENTOSH_OPENSSL_TARGET}
)

if(APPLE)
    target_link_libraries(clientosh PRIVATE
        "-framework Security"
        "-framework IOKit"
        "-framework CoreFoundation"
    )
endif()

if(WIN32)
    target_link_libraries(clientosh PRIVATE ws2_32 advapi32)
    set_target_properties(clientosh PROPERTIES WIN32_EXECUTABLE TRUE)

    # Bundle runtime DLLs next to the exe when building against MSYS2/custom prefix
    if(LIBSSH_ROOT)
        set(_dll_candidates
            libssh.dll
            libcrypto-3-x64.dll
            libssl-3-x64.dll
            libcrypto-3.dll
            libssl-3.dll
            zlib1.dll
            libwinpthread-1.dll
            libgcc_s_seh-1.dll
            libstdc++-6.dll
        )
        foreach(_dll IN LISTS _dll_candidates)
            set(_dll_path "${LIBSSH_ROOT}/bin/${_dll}")
            if(EXISTS "${_dll_path}")
                add_custom_command(TARGET clientosh POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${_dll_path}"
                        "$<TARGET_FILE_DIR:clientosh>"
                    VERBATIM
                )
            endif()
        endforeach()
    endif()

    # Bundle the Qt6 runtime DLLs and the MinGW C++ runtime so the exe launches
    # from a plain shell without the Qt toolchain on PATH.
    get_target_property(_qt_bin_dir Qt6::Core IMPORTED_LOCATION)
    if(_qt_bin_dir)
        get_filename_component(_qt_bin_dir "${_qt_bin_dir}" DIRECTORY)
    endif()
    set(_qt_dlls
        Qt6Core.dll
        Qt6Gui.dll
        Qt6Network.dll
        Qt6Svg.dll
        Qt6Widgets.dll
    )
    foreach(_dll IN LISTS _qt_dlls)
        if(EXISTS "${_qt_bin_dir}/${_dll}")
            add_custom_command(TARGET clientosh POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_qt_bin_dir}/${_dll}"
                    "$<TARGET_FILE_DIR:clientosh>"
                VERBATIM
            )
        endif()
    endforeach()

    # MinGW C++ runtime (from LIBSSH_ROOT when it is an MSYS2 prefix, else from
    # the Qt toolchain) plus the Qt platform plugin.
    set(_gcc_dlls libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll)
    foreach(_dll IN LISTS _gcc_dlls)
        set(_gcc_path "")
        if(LIBSSH_ROOT AND EXISTS "${LIBSSH_ROOT}/bin/${_dll}")
            set(_gcc_path "${LIBSSH_ROOT}/bin/${_dll}")
        elseif(CMAKE_CXX_COMPILER)
            get_filename_component(_gcc_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
            if(EXISTS "${_gcc_dir}/${_dll}")
                set(_gcc_path "${_gcc_dir}/${_dll}")
            endif()
        endif()
        if(_gcc_path)
            add_custom_command(TARGET clientosh POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_gcc_path}" "$<TARGET_FILE_DIR:clientosh>"
                VERBATIM
            )
        endif()
    endforeach()

    if(_qt_bin_dir AND EXISTS "${_qt_bin_dir}/../plugins/platforms/qwindows.dll")
        add_custom_command(TARGET clientosh POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:clientosh>/platforms"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${_qt_bin_dir}/../plugins/platforms/qwindows.dll"
                "$<TARGET_FILE_DIR:clientosh>/platforms"
            VERBATIM
        )
        # Image format plugins (PNG/ICO) used by the icon and app.
        if(EXISTS "${_qt_bin_dir}/../plugins/imageformats")
            add_custom_command(TARGET clientosh POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${_qt_bin_dir}/../plugins/imageformats"
                    "$<TARGET_FILE_DIR:clientosh>/imageformats"
                VERBATIM
            )
        endif()
        if(EXISTS "${_qt_bin_dir}/../plugins/iconengines")
            add_custom_command(TARGET clientosh POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${_qt_bin_dir}/../plugins/iconengines"
                    "$<TARGET_FILE_DIR:clientosh>/iconengines"
                VERBATIM
            )
        endif()
        if(EXISTS "${_qt_bin_dir}/../plugins/tls")
            add_custom_command(TARGET clientosh POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${_qt_bin_dir}/../plugins/tls"
                    "$<TARGET_FILE_DIR:clientosh>/tls"
                VERBATIM
            )
        endif()
    endif()
elseif(APPLE)
    set_target_properties(clientosh PROPERTIES
        MACOSX_BUNDLE FALSE
        INSTALL_RPATH "@executable_path/../lib"
    )
else()
    set_target_properties(clientosh PROPERTIES
        INSTALL_RPATH "$ORIGIN"
    )
endif()

# ---- Install (used by .deb / make install) ----
install(TARGETS clientosh
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    BUNDLE DESTINATION .
)

if(UNIX AND NOT APPLE)
    install(FILES packaging/clientosh.desktop
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications
    )
    install(FILES icons/terminal.svg
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/scalable/apps
        RENAME clientosh.svg
    )
    install(FILES packaging/clientosh.metainfo.xml
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/metainfo
        RENAME io.github.clientosh.metainfo.xml
    )
endif()

# ---- CPack (.deb on Linux) ----
set(CPACK_PACKAGE_NAME "clientosh")
set(CPACK_PACKAGE_VENDOR "clientosh")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Raw dark-gray SSH client (Qt 6 + libssh)")
set(CPACK_PACKAGE_DESCRIPTION
    "clientosh is a lightweight SSH client with a dark monospace UI, session management, detachable terminals, and scrollback.")
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_PACKAGE_CONTACT "clientosh <clientosh@localhost>")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/clientosh/clientosh")
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
    set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
endif()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
    set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
endif()

if(UNIX AND NOT APPLE)
    set(CPACK_GENERATOR "DEB")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "clientosh <clientosh@localhost>")
    set(CPACK_DEBIAN_PACKAGE_SECTION "net")
    set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
    set(CPACK_DEBIAN_FILE_NAME "DEB-DEFAULT")
    # Prefer auto shlibdeps when dpkg-dev is installed; keep explicit fallback deps.
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
    set(CPACK_DEBIAN_PACKAGE_DEPENDS
        "libqt6widgets6 | libqt6widgets6t64, libqt6network6 | libqt6network6t64, libqt6svg6 | libqt6svg6t64, libssh-4"
    )
endif()

include(CPack)
