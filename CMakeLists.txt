cmake_minimum_required(VERSION 3.21)
project(budget_app VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Network Sql)
find_package(OpenSSL REQUIRED)

qt_standard_project_setup(REQUIRES 6.5)

# The components/ subdirectory has no qmldir of its own; CMake generates the
# module-wide one, which is what this policy acknowledges.
if(QT_KNOWN_POLICY_QTP0004)
    qt_policy(SET QTP0004 NEW)
endif()

set(BUDGET_APP_SOURCES
    src/core/Transaction.cpp
    src/core/Categoriser.cpp
    src/data/TransactionStore.cpp
    src/data/TransactionModel.cpp
    src/data/TransactionFilterModel.cpp
    src/data/SummaryModel.cpp
    src/data/AccountModel.cpp
    src/net/Jwt.cpp
    src/net/HttpClient.cpp
    src/auth/RedirectListener.cpp
    src/auth/Settings.cpp
    src/providers/EnableBankingProvider.cpp
    src/providers/CsvProvider.cpp
    src/providers/CsvParser.cpp
    src/providers/Trading212Csv.cpp
    src/providers/Trading212Provider.cpp
    src/app/AppController.cpp
)

set(BUDGET_APP_HEADERS
    src/core/Money.h
    src/core/Transaction.h
    src/core/Categoriser.h
    src/data/TransactionStore.h
    src/data/TransactionModel.h
    src/data/TransactionFilterModel.h
    src/data/SummaryModel.h
    src/data/AccountModel.h
    src/net/Jwt.h
    src/net/HttpClient.h
    src/auth/RedirectListener.h
    src/auth/Settings.h
    src/providers/Provider.h
    src/providers/EnableBankingProvider.h
    src/providers/CsvProvider.h
    src/providers/CsvParser.h
    src/providers/Trading212Csv.h
    src/providers/Trading212Provider.h
    src/app/AppController.h
)

# Core logic lives in a static library so the test suite can link it directly.
qt_add_library(budget_core STATIC ${BUDGET_APP_SOURCES} ${BUDGET_APP_HEADERS})
# The generated QML type registration file includes headers by basename, so the
# directories holding QML_ELEMENT types have to be reachable directly as well as
# through the "core/..." style paths the sources use.
target_include_directories(budget_core PUBLIC
    src
    src/app
    src/core
    src/data
    src/net
    src/providers
)
target_link_libraries(budget_core PUBLIC
    Qt6::Core Qt6::Gui Qt6::Qml Qt6::Network Qt6::Sql OpenSSL::Crypto)

set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(budget_core
    URI BudgetApp
    VERSION 1.0
    OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/BudgetApp
    QML_FILES
        qml/Main.qml
        qml/Theme.qml
        qml/DashboardView.qml
        qml/TransactionsView.qml
        qml/ConnectView.qml
        qml/SettingsView.qml
        qml/components/SummaryCard.qml
        qml/components/TransactionDelegate.qml
        qml/components/CategoryBars.qml
        qml/components/SpendSparkline.qml
        qml/components/StatusBanner.qml
        qml/components/EmptyState.qml
)

qt_add_executable(budget_app src/main.cpp)
target_compile_definitions(budget_app PRIVATE BUDGET_APP_VERSION="${PROJECT_VERSION}")
target_link_libraries(budget_app PRIVATE budget_core budget_coreplugin Qt6::Quick Qt6::QuickControls2)

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

install(TARGETS budget_app RUNTIME DESTINATION bin)
