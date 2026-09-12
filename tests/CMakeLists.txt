find_package(Qt6 REQUIRED COMPONENTS Test)

foreach(test_name tst_money tst_csv tst_categoriser tst_store tst_jwt tst_enablebanking tst_trading212)
    qt_add_executable(${test_name} ${test_name}.cpp)
    target_link_libraries(${test_name} PRIVATE tarkka_core Qt6::Test)
    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()
