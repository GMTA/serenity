set(SOURCES
    Grammar.cpp
)

lagom_tool(GenerateSPIRVEnums SOURCES GenerateSPIRVEnums.cpp LIBS LibMain)
lagom_tool(GenerateSPIRVInstructions SOURCES GenerateSPIRVInstructions.cpp LIBS LibMain)
