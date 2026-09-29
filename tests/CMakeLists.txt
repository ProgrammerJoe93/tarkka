find_package(Qt6 REQUIRED COMPONENTS Test)

foreach(test_name tst_money tst_csv tst_categoriser tst_store tst_jwt tst_enablebanking tst_trading212 tst_ukbanks
                 tst_settings tst_redirect tst_connectionfile tst_trends tst_budgets tst_recurring tst_rules tst_balance)
    qt_add_executable(${test_name} ${test_name}.cpp)
    target_link_libraries(${test_name} PRIVATE tarkka_core Qt6::Test)
    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()
