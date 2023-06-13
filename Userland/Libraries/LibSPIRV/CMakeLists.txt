include(libspirv_generators)

set(SOURCES
    Instruction.cpp
    Module.cpp
)

generate_spirv_implementation()

set(GENERATED_SOURCES
    CoreEnums.cpp
    CoreInstructions.cpp
)

serenity_lib(LibSPIRV spirv)
