set(TEST_SOURCES
    TestSPIRVModule.cpp
)

foreach(source IN LISTS TEST_SOURCES)
    serenity_test("${source}" LibSPIRV LIBS LibSPIRV)
endforeach()
