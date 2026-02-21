cmake_minimum_required(VERSION 3.16)

project(OSCRouter LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_SUPPRESS_REGENERATION ON)

if(MSVC)
  add_compile_definitions(__WINDOWS_MM__)
  add_compile_options(/W4 /MP /wd4996)
  set(ENV{QTDIR} "C:/Qt/6.10.2/msvc2022_64")
elseif(XCODE)
  add_compile_definitions(__MACOSX_CORE__)
  set(CMAKE_XCODE_ATTRIBUTE_DEBUG_INFORMATION_FORMAT "dwarf-with-dsym")
  set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO")
  set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGN_STYLE "Manual")
  set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "")
  set(CMAKE_OSX_DEPLOYMENT_TARGET "13.3")
  set(ENV{QTDIR} "$ENV{HOME}/Qt/6.10.2/macos")
endif()

set(CMAKE_PREFIX_PATH "$ENV{QTDIR}")
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Gui Network Qml)
qt_standard_project_setup()

include_directories(
  "../EosSyncLib/EosSyncLib"
  "."
  "OSCRouter"  
  "psn"
  "sACN/ACN/StreamingACN/Client"
  "sACN/ACN/StreamingACN/Client/Src"
  "sACN/ACN/StreamingACN/Common"
  "sACN/ACN/StreamingACN/Server"
  "sACN/ACN/StreamingACN/Server/Src"
  "sACN/ACN/VHD"
  "sACN/Network_Common/ACNTypes"
  "sACN/Network_Common/ACNTypes/Src"
  "sACN/Network_Common/AsyncSocket"
  "sACN/Network_Common/AsyncSocket/Src"
  "sACN/Network_Common/Tock"
  "rtmidi"
)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

file(GLOB HEADERS
  "OSCRouter/*.h"  
  "artnet/*.h"
  "psn/*.hpp"
  "otp/otp.h"
  "sACN/ACN/StreamingACN/Client/*.h"
  "sACN/ACN/StreamingACN/Client/Src/*.h"
  "sACN/ACN/StreamingACN/Common/*.h"
  "sACN/ACN/StreamingACN/Server/*.h"
  "sACN/ACN/StreamingACN/Server/Src/*.h"
  "sACN/ACN/VHD/*.h"
  "sACN/Network_Common/ACNTypes/*.h"
  "sACN/Network_Common/AsyncSocket/*.h"
  "sACN/Network_Common/AsyncSocket/Src/*.h"
  "sACN/Network_Common/Tock/*.h"
  "rtmidi/*.h"
)

file(GLOB SOURCES
  "OSCRouter/*.cpp"
  "artnet/*.c"
  "otp/otp.cpp"
  "sACN/ACN/StreamingACN/Client/Src/*.cpp"
  "sACN/ACN/StreamingACN/Common/*.cpp"
  "sACN/ACN/StreamingACN/Server/Src/*.cpp"
  "sACN/ACN/VHD/*.cpp"
  "sACN/Network_Common/ACNTypes/Src/*.cpp"
  "sACN/Network_Common/AsyncSocket/Src/*.cpp"
  "rtmidi/*.c"
  "rtmidi/*.cpp"
)

set(EOS_SYNC_LIBS_SOURCES
  "../EosSyncLib/EosSyncLib/EosLog.cpp"
  "../EosSyncLib/EosSyncLib/EosTcp.cpp"
  "../EosSyncLib/EosSyncLib/EosTimer.cpp"
  "../EosSyncLib/EosSyncLib/EosUdp.cpp"
  "../EosSyncLib/EosSyncLib/OSCParser.cpp"
)

if(WIN32)
  include_directories(
    "sACN/ACN/StreamingACN/Client/Windows"
    "sACN/ACN/StreamingACN/Server/Windows"
    "sACN/Network_Common/AsyncSocket/Windows"
    "sACN/Network_Common/defpack/Windows"
    "sACN/Network_Common/ObjectSync/Windows"
    "sACN/Network_Common/Tock/Windows"
    "sACN/Onyx_Common/Windows"
  )

  file(GLOB WIN_HEADERS
    "sACN/ACN/StreamingACN/Client/Windows/*.h"
    "sACN/ACN/StreamingACN/Server/Windows/*.h"
    "sACN/Network_Common/AsyncSocket/Windows/*.h"
    "sACN/Network_Common/defpack/Windows/*.h"
    "sACN/Network_Common/ObjectSync/Windows/*.h"
  )

  set(HEADERS ${HEADERS} ${WIN_HEADERS})

  file(GLOB WIN_SOURCES
    "sACN/ACN/StreamingACN/Client/Windows/*.cpp"
    "sACN/ACN/StreamingACN/Server/Windows/*.cpp"
    "sACN/Network_Common/AsyncSocket/Windows/*.cpp"
    "sACN/Network_Common/ObjectSync/Windows/*.cpp"
    "sACN/Network_Common/Tock/Windows/*.cpp"
  )

  set(SOURCES ${SOURCES} ${WIN_SOURCES})

  set(EOS_SYNC_LIBS_SOURCES ${EOS_SYNC_LIBS_SOURCES}
    "../EosSyncLib/EosSyncLib/EosTcp_Win.cpp"
    "../EosSyncLib/EosSyncLib/EosUdp_Win.cpp"
  )

  set(RESOURCE_FILES
    "OSCRouter/OSCRouter.rc"
    "OSCRouter/icon1.ico"
    "OSCRouter/qt.conf"
    "OSCRouter/qt.qrc"
  )
elseif(APPLE)
  include_directories(
    "OSCRouter/Mac"
    "sACN/ACN/StreamingACN/Client/OSX"
    "sACN/ACN/StreamingACN/Server/OSX"
    "sACN/Network_Common/AsyncSocket/OSX"
    "sACN/Network_Common/defpack/OSX"
    "sACN/Network_Common/ObjectSync/OSX"
    "sACN/Network_Common/Tock/OSX"
    "sACN/Onyx_Common/OSX"
  )

  file(GLOB MAC_HEADERS
    "OSCRouter/Mac/*.h"
    "sACN/ACN/StreamingACN/Client/OSX/*.h"
    "sACN/ACN/StreamingACN/Server/OSX/*.h"
    "sACN/Network_Common/AsyncSocket/OSX/*.h"
    "sACN/Network_Common/defpack/OSX/*.h"
    "sACN/Network_Common/ObjectSync/OSX/*.h"
  )

  set(HEADERS ${HEADERS} ${MAC_HEADERS})

  file(GLOB MAC_SOURCES
    "OSCRouter/Mac/*.cpp"
    "OSCRouter/Mac/*.mm"
    "sACN/ACN/StreamingACN/Client/OSX/*.cpp"
    "sACN/ACN/StreamingACN/Server/OSX/*.cpp"
    "sACN/Network_Common/ObjectSync/OSX/*.cpp"
    "sACN/Network_Common/Tock/OSX/*.cpp"
  )

  set(SOURCES ${SOURCES} ${MAC_SOURCES}
    "sACN/Network_Common/AsyncSocket/OSX/AsyncSocketServ_select.cpp"
    "sACN/Network_Common/AsyncSocket/OSX/IfaceSupport.cpp"
  )

  set(EOS_SYNC_LIBS_SOURCES ${EOS_SYNC_LIBS_SOURCES}
    "../EosSyncLib/EosSyncLib/EosTcp_Mac.cpp"
    "../EosSyncLib/EosSyncLib/EosUdp_Mac.cpp"
  )

  set(RESOURCE_FILES
    "OSCRouter/qt.conf"
    "OSCRouter/qt.qrc"
  )
  file(GLOB BUNDLE_RESOURCE_FILES
    "icon.icns"
  )
  set_source_files_properties(${BUNDLE_RESOURCE_FILES} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
  source_group("Resources" FILES ${BUNDLE_RESOURCE_FILES})

  add_compile_definitions(HAVE_SOCKADDR_SA_LEN)
endif()

add_compile_definitions(NOMINMAX)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${HEADERS} ${SOURCES})
source_group("Resource Files" FILES ${RESOURCE_FILES})
qt_add_executable(${PROJECT_NAME} ${SOURCES} ${EOS_SYNC_LIBS_SOURCES} ${HEADERS} ${RESOURCE_FILES} ${BUNDLE_RESOURCE_FILES})
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})

target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Core Qt6::Widgets Qt6::Gui Qt6::Network Qt6::Qml)

if(WIN32)
  target_link_libraries(${PROJECT_NAME} PRIVATE winmm iphlpapi)
elseif(APPLE)
  target_link_libraries(${PROJECT_NAME} PRIVATE "-framework CoreMIDI -framework CoreAudio")
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES
  WIN32_EXECUTABLE ON
  MACOSX_BUNDLE ON
  MACOSX_BUNDLE_GUI_IDENTIFIER org.etc.${PROJECT_NAME}
  MACOSX_BUNDLE_ICON_FILE "icon.icns"
)

if(WIN32)
  add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND cmd /C \"\"$ENV{QTDIR}\\bin\\windeployqt\" --force --no-translations --no-opengl-sw --no-compiler-runtime \"$<TARGET_FILE:${PROJECT_NAME}>\"\"
  )
elseif(APPLE)
  add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND rm -rf \"$<TARGET_FILE_DIR:${PROJECT_NAME}>/../../../${PROJECT_NAME}.dmg\"    
    COMMAND "$ENV{QTDIR}/bin/macdeployqt" \"$<TARGET_FILE_DIR:${PROJECT_NAME}>/../..\" -dmg
  )
endif()
