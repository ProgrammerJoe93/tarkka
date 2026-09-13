cmake_minimum_required(VERSION 3.21)
project(tarkka VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Network Sql
                                   LinguistTools)

# Qt for Android ships no OpenSSL, and bundling a copy would mean shipping a
# crypto library this app then has to keep patched. The Android build signs
# through the platform's own JCA instead; only the desktop build needs OpenSSL.
if(NOT ANDROID)
    find_package(OpenSSL REQUIRED)
else()
    # Qt for Android carries TLS support but not the OpenSSL libraries
    # themselves, so without these every HTTPS request fails at runtime with
    # "Failed to load libssl/libcrypto". Qt documents pulling in KDAB's
    # prebuilts; set TARKKA_ANDROID_OPENSSL_DIR to a local clone to build
    # offline.
    set(TARKKA_ANDROID_OPENSSL_DIR "" CACHE PATH
        "Local checkout of KDAB/android_openssl; downloaded when empty")
    if(TARKKA_ANDROID_OPENSSL_DIR)
        include(${TARKKA_ANDROID_OPENSSL_DIR}/android_openssl.cmake)
    else()
        include(FetchContent)
        FetchContent_Declare(android_openssl
            DOWNLOAD_EXTRACT_TIMESTAMP true
            URL https://github.com/KDAB/android_openssl/archive/refs/heads/master.zip)
        FetchContent_MakeAvailable(android_openssl)
        include(${android_openssl_SOURCE_DIR}/android_openssl.cmake)
    endif()
endif()

qt_standard_project_setup(REQUIRES 6.5)

# The components/ subdirectory has no qmldir of its own; CMake generates the
# module-wide one, which is what this policy acknowledges.
if(QT_KNOWN_POLICY_QTP0004)
    qt_policy(SET QTP0004 NEW)
endif()

set(TARKKA_SOURCES
    src/core/Transaction.cpp
    src/core/Categoriser.cpp
    src/data/TransactionStore.cpp
    src/data/TransactionModel.cpp
    src/data/TransactionFilterModel.cpp
    src/data/CategoryTotalsModel.cpp
    src/data/SummaryModel.cpp
    src/data/TrendsModel.cpp
    src/data/AccountModel.cpp
    src/net/JwtKey.cpp
    src/net/HttpClient.cpp
    src/auth/RedirectListener.cpp
    src/auth/Settings.cpp
    src/auth/ConnectionFile.cpp
    src/i18n/Translation.cpp
    src/providers/EnableBankingProvider.cpp
    src/providers/CsvProvider.cpp
    src/providers/CsvParser.cpp
    src/providers/Trading212Csv.cpp
    src/providers/Trading212Provider.cpp
    src/app/AppController.cpp
)

set(TARKKA_HEADERS
    src/core/Money.h
    src/core/Transaction.h
    src/core/Categoriser.h
    src/data/TransactionStore.h
    src/data/TransactionModel.h
    src/data/TransactionFilterModel.h
    src/data/CategoryTotalsModel.h
    src/data/SummaryModel.h
    src/data/TrendsModel.h
    src/data/AccountModel.h
    src/net/Jwt.h
    src/net/JwtKey.h
    src/net/HttpClient.h
    src/auth/RedirectListener.h
    src/auth/Settings.h
    src/auth/LoopbackCertificate.h
    src/auth/ConnectionFile.h
    src/i18n/Translation.h
    src/providers/Provider.h
    src/providers/EnableBankingProvider.h
    src/providers/CsvProvider.h
    src/providers/CsvParser.h
    src/providers/Trading212Csv.h
    src/providers/Trading212Provider.h
    src/app/AppController.h
)

# Core logic lives in a static library so the test suite can link it directly.
if(ANDROID)
    list(APPEND TARKKA_SOURCES src/net/Jwt_android.cpp)
    list(APPEND TARKKA_SOURCES src/auth/LoopbackCertificate_android.cpp)
else()
    list(APPEND TARKKA_SOURCES src/net/Jwt_openssl.cpp)
    list(APPEND TARKKA_SOURCES src/auth/LoopbackCertificate_openssl.cpp)
endif()

qt_add_library(tarkka_core STATIC ${TARKKA_SOURCES} ${TARKKA_HEADERS})
# The generated QML type registration file includes headers by basename, so the
# directories holding QML_ELEMENT types have to be reachable directly as well as
# through the "core/..." style paths the sources use.
target_include_directories(tarkka_core PUBLIC
    src
    src/app
    src/core
    src/data
    src/i18n
    src/net
    src/providers
)
target_link_libraries(tarkka_core PUBLIC
    Qt6::Core Qt6::Gui Qt6::Qml Qt6::Network Qt6::Sql)
if(NOT ANDROID)
    target_link_libraries(tarkka_core PUBLIC OpenSSL::Crypto)
endif()

set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(tarkka_core
    URI Tarkka
    VERSION 1.0
    OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Tarkka
    QML_FILES
        qml/Main.qml
        qml/Theme.qml
        qml/DashboardView.qml
        qml/TrendsView.qml
        qml/TransactionsView.qml
        qml/ConnectView.qml
        qml/SettingsView.qml
        qml/components/NavButton.qml
        qml/components/BrandMark.qml
        qml/components/NavIcon.qml
        qml/components/SummaryCard.qml
        qml/components/TransactionDelegate.qml
        qml/components/CategoryBars.qml
        qml/components/SpendSparkline.qml
        qml/components/StatusBanner.qml
        qml/components/EmptyState.qml
        qml/components/SegmentedChoice.qml
        qml/components/CategoryChip.qml
        qml/components/ChangeBadge.qml
)

qt_add_executable(tarkka src/main.cpp)

# The window icon must be a raster the window manager can consume, so the SVG
# is pre-rendered by tools/make-icons.sh.
qt_add_resources(tarkka "tarkka_icons"
    PREFIX "/icons"
    BASE assets/icons
    FILES
        assets/icons/tarkka-16.png
        assets/icons/tarkka-24.png
        assets/icons/tarkka-32.png
        assets/icons/tarkka-48.png
        assets/icons/tarkka-64.png
        assets/icons/tarkka-128.png
        assets/icons/tarkka-256.png
)

if(ANDROID)
    add_android_openssl_libraries(tarkka)
    set_target_properties(tarkka PROPERTIES
        QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android"
        QT_ANDROID_PACKAGE_NAME "com.joegineered.tarkka"
        QT_ANDROID_APP_NAME "Tarkka"
        # Fills the icon placeholder in Qt's manifest template; without it the
        # attribute is stripped and the app ships with no launcher icon at all.
        QT_ANDROID_APP_ICON "@mipmap/ic_launcher"
        QT_ANDROID_TARGET_SDK_VERSION 35
        QT_ANDROID_MIN_SDK_VERSION 28
        QT_ANDROID_VERSION_NAME "${PROJECT_VERSION}"
        QT_ANDROID_VERSION_CODE 1
    )
endif()

# English is the source language and needs no catalogue, so Finnish is the only
# .ts file. lupdate scans tarkka_core, which is where the QML and all but one
# of the C++ sources live; the .qm is embedded in the executable so that the
# Android package carries it without a second asset to install.
qt_add_translations(tarkka
    SOURCE_TARGETS tarkka_core
    TS_FILES i18n/tarkka_fi.ts
    RESOURCE_PREFIX "/i18n"
    LUPDATE_OPTIONS -no-obsolete -locations none
)

target_compile_definitions(tarkka PRIVATE TARKKA_VERSION="${PROJECT_VERSION}")
target_link_libraries(tarkka PRIVATE tarkka_core tarkka_coreplugin Qt6::Quick Qt6::QuickControls2)

include(CTest)
if(BUILD_TESTING AND NOT ANDROID)
    add_subdirectory(tests)
endif()

# On Android the executable is built as a shared module that the APK loads, so
# it needs a library destination as well as the desktop's runtime one.
install(TARGETS tarkka
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    BUNDLE DESTINATION .)
