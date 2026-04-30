cmake_minimum_required(VERSION 3.20)
project(kurali LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Allow using vcpkg toolchain on Windows
# cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=C:/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows

if(UNIX)
	# Collect sources (exclude tests)
	file(GLOB_RECURSE CORAL_SOURCES
		"src/*.cpp"
	)
	# Remove test sources if any matched
	list(FILTER CORAL_SOURCES EXCLUDE REGEX "/test/.*\\.cpp$")

	add_executable(kurali ${CORAL_SOURCES})

	# pkg-config for glib/gio and other libs
	find_package(PkgConfig REQUIRED)
	pkg_check_modules(GLIB2 REQUIRED glib-2.0)
	pkg_check_modules(GIO2 REQUIRED gio-2.0)

	# Optional: portal, xkbcommon
	pkg_check_modules(PORTAL libportal QUIET)
	pkg_check_modules(XKBCOMMON xkbcommon QUIET)

	# Audio libs via pkg-config
	pkg_check_modules(PORTAUDIO portaudio-2.0 QUIET)
	if(NOT PORTAUDIO_FOUND)
		# Fallback name
		pkg_check_modules(PORTAUDIO portaudio QUIET)
	endif()
	pkg_check_modules(SNDFILE sndfile REQUIRED)

	target_include_directories(kurali PRIVATE
		${GLIB2_INCLUDE_DIRS}
		${GIO2_INCLUDE_DIRS}
		${PORTAL_INCLUDE_DIRS}
		${XKBCOMMON_INCLUDE_DIRS}
		${PORTAUDIO_INCLUDE_DIRS}
		${SNDFILE_INCLUDE_DIRS}
	)

	target_link_directories(kurali PRIVATE
		${GLIB2_LIBRARY_DIRS}
		${GIO2_LIBRARY_DIRS}
		${PORTAL_LIBRARY_DIRS}
		${XKBCOMMON_LIBRARY_DIRS}
		${PORTAUDIO_LIBRARY_DIRS}
		${SNDFILE_LIBRARY_DIRS}
	)

	target_compile_options(kurali PRIVATE
		${GLIB2_CFLAGS_OTHER}
		${GIO2_CFLAGS_OTHER}
		${PORTAL_CFLAGS_OTHER}
		${XKBCOMMON_CFLAGS_OTHER}
		${PORTAUDIO_CFLAGS_OTHER}
		${SNDFILE_CFLAGS_OTHER}
	)

	# Link Linux libraries
	target_link_libraries(kurali PRIVATE
		${GLIB2_LIBRARIES}
		${GIO2_LIBRARIES}
		${PORTAL_LIBRARIES}
		${XKBCOMMON_LIBRARIES}
		${PORTAUDIO_LIBRARIES}
		${SNDFILE_LIBRARIES}
		X11
		Xtst
		Xinerama
	)

	# Link libxdo if available as static in repo; else system -lxdo
	set(LIBXDO_STATIC "${CMAKE_CURRENT_LIST_DIR}/../libxdo/libxdo.a")
	if(EXISTS "${LIBXDO_STATIC}")
		target_link_libraries(kurali PRIVATE "${LIBXDO_STATIC}")
	else()
		find_library(LIBXDO xdo)
		if(LIBXDO)
			target_link_libraries(kurali PRIVATE ${LIBXDO})
		endif()
	endif()

	# RPATH to local lib folder if used
	set_target_properties(kurali PROPERTIES
		BUILD_RPATH "${CMAKE_CURRENT_LIST_DIR}/lib"
		INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib"
	)

	# Install rules (portable layout)
	install(TARGETS kurali RUNTIME DESTINATION .)

else() # WIN32
    # Collect sources and exclude Linux-only files
    file(GLOB_RECURSE CORAL_SOURCES "src/*.cpp")
    # Exclude Linux-only sources from Windows build
    list(FILTER CORAL_SOURCES EXCLUDE REGEX "/src/X11.*\\.cpp$")
    list(FILTER CORAL_SOURCES EXCLUDE REGEX "/src/Wayland.*\\.cpp$")
    list(FILTER CORAL_SOURCES EXCLUDE REGEX "/src/A11yInjector\\.cpp$")
    list(FILTER CORAL_SOURCES EXCLUDE REGEX "/src/X11WindowUtils\\.cpp$")
    list(FILTER CORAL_SOURCES EXCLUDE REGEX "/src/UInputInjector\\.cpp$")

    add_executable(kurali ${CORAL_SOURCES})

    # Version info: allow -D or env vars (APPVER, WHISPERLIB, GGMLLIBS from workflow)
    if(NOT DEFINED APP_VERSION)
        if(DEFINED ENV{APPVER})
            set(APP_VERSION "$ENV{APPVER}")
        else()
            set(APP_VERSION "0.0.0")
        endif()
    endif()
    if(NOT DEFINED BUILD_DATE)
        if(DEFINED ENV{BUILD_DATE})
            set(BUILD_DATE "$ENV{BUILD_DATE}")
        else()
            string(TIMESTAMP BUILD_DATE "%Y-%m-%d")
        endif()
    endif()
    if(NOT DEFINED GIT_COMMIT)
        if(DEFINED ENV{GIT_COMMIT})
            set(GIT_COMMIT "$ENV{GIT_COMMIT}")
        else()
            execute_process(COMMAND git rev-parse --short HEAD
                            WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
                            OUTPUT_VARIABLE GIT_COMMIT OUTPUT_STRIP_TRAILING_WHITESPACE
                            ERROR_QUIET)
            if(NOT GIT_COMMIT)
                set(GIT_COMMIT "unknown")
            endif()
        endif()
    endif()

    # Compute RC numeric version A,B,C,D from APP_VERSION (A.B.C[.D])
    string(REPLACE "." ";" _ver_list ${APP_VERSION})
    list(GET _ver_list 0 _v0)
    list(LENGTH _ver_list _vlen)
    if(_vlen GREATER 1)
        list(GET _ver_list 1 _v1)
    else()
        set(_v1 0)
    endif()
    if(_vlen GREATER 2)
        list(GET _ver_list 2 _v2)
    else()
        set(_v2 0)
    endif()
    if(_vlen GREATER 3)
        list(GET _ver_list 3 _v3)
    else()
        set(_v3 0)
    endif()
    set(APP_VERSION_RC "${_v0},${_v1},${_v2},${_v3}")
    set(APP_VERSION_STR ${APP_VERSION})

    # Generate version headers/resources in build dir
    configure_file(${CMAKE_CURRENT_LIST_DIR}/src/version.h.in
                   ${CMAKE_CURRENT_BINARY_DIR}/version.h @ONLY)
    configure_file(${CMAKE_CURRENT_LIST_DIR}/src/version.rc.in
                   ${CMAKE_CURRENT_BINARY_DIR}/version.rc @ONLY)
    target_include_directories(kurali PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
    target_sources(kurali PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/version.rc)

    # Dependencies via vcpkg (static recommended):
    #   vcpkg install portaudio:x64-windows-static libsndfile:x64-windows-static
    find_package(PortAudio CONFIG QUIET)
    find_package(sndfile CONFIG QUIET)
    # vcpkg sometimes exposes libsndfile as "unofficial-sndfile"
    find_package(unofficial-sndfile CONFIG QUIET)

    # Resolve PortAudio target or fallback to library path
    if(TARGET PortAudio::PortAudio)
        set(PORTAUDIO_TARGET PortAudio::PortAudio)
        message(STATUS "Using PortAudio target: PortAudio::PortAudio")
    elseif(TARGET portaudio)
        set(PORTAUDIO_TARGET portaudio)
        message(STATUS "Using PortAudio target: portaudio")
    else()
        find_library(PORTAUDIO_LIB NAMES portaudio portaudio_static)
        if(PORTAUDIO_LIB)
            set(PORTAUDIO_TARGET ${PORTAUDIO_LIB})
            find_path(PORTAUDIO_INCLUDE_DIR portaudio.h)
            if(PORTAUDIO_INCLUDE_DIR)
                target_include_directories(kurali PRIVATE ${PORTAUDIO_INCLUDE_DIR})
            endif()
            message(STATUS "Using PortAudio library: ${PORTAUDIO_LIB}")
        else()
            message(FATAL_ERROR "PortAudio not found. Ensure vcpkg toolchain is set and portaudio installed.")
        endif()
    endif()

    # Resolve libsndfile imported target (static brings codec deps transitively)
    if(TARGET sndfile::sndfile)
        set(SNDFILE_TARGET sndfile::sndfile)
        message(STATUS "Using sndfile target: sndfile::sndfile")
    elseif(TARGET SndFile::sndfile)
        set(SNDFILE_TARGET SndFile::sndfile)
        message(STATUS "Using sndfile target: SndFile::sndfile")
    elseif(TARGET unofficial::sndfile::sndfile)
        set(SNDFILE_TARGET unofficial::sndfile::sndfile)
        message(STATUS "Using sndfile target: unofficial::sndfile::sndfile")
    elseif(TARGET unofficial::libsndfile::libsndfile)
        set(SNDFILE_TARGET unofficial::libsndfile::libsndfile)
        message(STATUS "Using sndfile target: unofficial::libsndfile::libsndfile")
    elseif(TARGET sndfile)
        set(SNDFILE_TARGET sndfile)
        message(STATUS "Using sndfile target: sndfile")
    else()
        # Fallback: try to find raw library; may require codec libs separately
        find_library(SNDFILE_LIB NAMES sndfile libsndfile-1)
        if(SNDFILE_LIB)
            set(SNDFILE_TARGET ${SNDFILE_LIB})
            message(STATUS "Using sndfile library: ${SNDFILE_LIB}")
        else()
            message(FATAL_ERROR "libsndfile imported target not found. Ensure vcpkg toolchain is set and libsndfile installed.")
        endif()
    endif()

    # Optional: Whisper library on Windows (user-provided or env WHISPERLIB)
    set(WHISPER_HINT "${CMAKE_CURRENT_LIST_DIR}/lib")
    if(NOT WHISPER_LIBRARY AND DEFINED ENV{WHISPERLIB})
        set(WHISPER_LIBRARY "$ENV{WHISPERLIB}")
    endif()
    if(NOT WHISPER_LIBRARY)
        find_library(WHISPER_LIBRARY NAMES whisper whisper.dll.lib HINTS ${WHISPER_HINT})
    endif()
    if(WHISPER_LIBRARY)
        target_link_libraries(kurali PRIVATE ${WHISPER_LIBRARY})
        message(STATUS "Using Whisper library: ${WHISPER_LIBRARY}")
    else()
        message(WARNING "Whisper library not found. Ensure whisper(.lib) is available and set via -DWHISPER_LIBRARY.")
    endif()

    # GGML deps (may consist of multiple libs; env GGMLLIBS from workflow)
    if(NOT GGML_LIBRARIES AND DEFINED ENV{GGMLLIBS})
        set(GGML_LIBRARIES "$ENV{GGMLLIBS}")
    endif()
    if(GGML_LIBRARIES)
        string(REPLACE ";" "\n  - " _ggml_list "${GGML_LIBRARIES}")
        message(STATUS "Using GGML libraries:\n  - ${_ggml_list}")
        separate_arguments(GGML_LIBRARIES)
        target_link_libraries(kurali PRIVATE ${GGML_LIBRARIES})
    else()
        find_library(GGML_LIBRARY NAMES ggml ggml-static ggml.lib HINTS ${WHISPER_HINT})
        if(GGML_LIBRARY)
            target_link_libraries(kurali PRIVATE ${GGML_LIBRARY})
            message(STATUS "Using GGML library: ${GGML_LIBRARY}")
        else()
            message(WARNING "GGML libraries not provided and not found; whisper may fail to link.")
        endif()
    endif()

    # whisper/ggml headers (optional if using system installs)
    target_include_directories(kurali PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/../whispercpp/include
        ${CMAKE_CURRENT_LIST_DIR}/../whispercpp/ggml/include
    )

    target_link_libraries(kurali PRIVATE ${PORTAUDIO_TARGET} ${SNDFILE_TARGET} user32 winmm setupapi ole32 oleaut32 uuid)

    # Post-build: copy conf/ next to exe (backend looks for exeDir/conf, exeDir/model)
    add_custom_command(TARGET kurali POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:kurali>/conf
        COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:kurali>/model
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_LIST_DIR}/conf/config-windows.json
            $<TARGET_FILE_DIR:kurali>/conf/config.json
        COMMENT "Copying conf/ next to kurali.exe for Windows"
    )

    # Install rules (portable layout)
    install(TARGETS kurali RUNTIME DESTINATION .)
endif()

# CPack (Windows NSIS) – safe to include always; only generates when packaging
set(CPACK_PACKAGE_NAME "Kurali")
set(CPACK_PACKAGE_VENDOR "Kurali Project")
set(CPACK_PACKAGE_VERSION "0.5.0")
set(CPACK_PACKAGE_CONTACT "")
if(WIN32)
	set(CPACK_GENERATOR NSIS)
	set(CPACK_NSIS_DISPLAY_NAME "Kurali")
	set(CPACK_NSIS_PACKAGE_NAME "Kurali")
	set(CPACK_NSIS_CONTACT "")
	set(CPACK_NSIS_MUI_ICON "${CMAKE_CURRENT_LIST_DIR}/logo/coral.ico")
endif()
include(CPack) 