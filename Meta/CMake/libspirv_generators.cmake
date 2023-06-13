function (generate_spirv_implementation)
    set(LIBSPIRV_INPUT_FOLDER "${CMAKE_CURRENT_SOURCE_DIR}")

    invoke_generator(
        "CoreEnums.cpp"
        Lagom::GenerateSPIRVEnums
        "${LIBSPIRV_INPUT_FOLDER}/spirv.core.grammar.json"
        "CoreEnums.h"
        "CoreEnums.cpp"
        arguments -j "${LIBSPIRV_INPUT_FOLDER}/spirv.core.grammar.json"
    )

    invoke_generator(
        "CoreInstructions.cpp"
        Lagom::GenerateSPIRVInstructions
        "${LIBSPIRV_INPUT_FOLDER}/spirv.core.grammar.json"
        "CoreInstructions.h"
        "CoreInstructions.cpp"
        arguments -j "${LIBSPIRV_INPUT_FOLDER}/spirv.core.grammar.json"
    )
endfunction()
