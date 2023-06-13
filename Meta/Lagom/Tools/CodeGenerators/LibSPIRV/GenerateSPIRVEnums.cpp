/*
 * Copyright (c) 2023, Jelle Raaijmakers <jelle@gmta.nl>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/HashTable.h>
#include <AK/JsonObject.h>
#include <AK/SourceGenerator.h>
#include <AK/StringBuilder.h>
#include <AK/StringView.h>
#include <LibCore/ArgsParser.h>
#include <LibCore/File.h>
#include <LibMain/Main.h>
#include <Meta/Lagom/Tools/CodeGenerators/LibSPIRV/Grammar.h>

namespace SPIRV {

static ErrorOr<void> generate_header_file(Grammar const& grammar, Core::File& file)
{
    StringBuilder builder;
    SourceGenerator generator { builder };

    TRY(generator.try_appendln("#pragma once"));
    TRY(generator.try_append("\n"));
    TRY(generator.try_appendln("#include <AK/EnumBits.h>"));
    TRY(generator.try_appendln("#include <AK/String.h>"));
    TRY(generator.try_appendln("#include <AK/StringView.h>"));
    TRY(generator.try_appendln("#include <AK/Types.h>"));
    TRY(generator.try_append("\n"));
    TRY(generator.try_appendln("namespace SPIRV {"));

    // Opcodes
    TRY(generator.try_append("\n"));
    TRY(generator.try_appendln("enum class Opcode : u16 {"));
    for (auto const& instruction : grammar.instructions) {
        generator.set("opname"sv, instruction.opname);
        generator.set("opcode"sv, instruction.cpp_opcode);

        TRY(generator.try_appendln("    @opname@ = @opcode@,"));
    }
    TRY(generator.try_appendln("};"));

    // Variable types
    TRY(generator.try_append("\n"));
    TRY(generator.try_appendln("enum class VariableType {"));
    for (auto const& instruction : grammar.instructions) {
        if (!instruction.opname.starts_with("Type"sv))
            continue;

        auto type_name = instruction.opname.substring(4);
        generator.set("type_name"sv, type_name);
        TRY(generator.try_appendln("    @type_name@,"));
    }
    TRY(generator.try_appendln("};"));

    // Value enums and bitwise enums
    for (auto const& enumeration : grammar.enumerations) {
        generator.set("name"sv, enumeration.name);

        TRY(generator.try_append("\n"));
        TRY(generator.try_appendln("enum class @name@ : u32 {"));
        for (auto const& enumeration_value : enumeration.values) {
            generator.set("enumerant"sv, enumeration_value.name);
            generator.set("value"sv, enumeration_value.cpp_value);

            TRY(generator.try_appendln("    @enumerant@ = @value@,"));
        }
        TRY(generator.try_appendln("};"));

        TRY(generator.try_append("\n"));
        if (enumeration.type == EnumerationType::BitEnum) {
            TRY(generator.try_appendln("AK_ENUM_BITWISE_OPERATORS(@name@);"));
            TRY(generator.try_appendln("ErrorOr<String> enumerant_to_string(@name@);"));
        } else {
            TRY(generator.try_appendln("StringView enumerant_to_string(@name@);"));
        }
    }

    // Composite structs
    for (auto const& composite_struct : grammar.composite_structs) {
        generator.set("name"sv, composite_struct.name);

        TRY(generator.try_append("\n"));
        TRY(generator.try_appendln("struct @name@ {"));
        for (auto const& argument : composite_struct.arguments) {
            generator.set("cpp_type"sv, argument.cpp_type);
            generator.set("argument_name"sv, argument.name);

            TRY(generator.try_appendln("    @cpp_type@ @argument_name@;"));
        }
        TRY(generator.try_appendln("};"));
    }

    TRY(generator.try_append("\n"));
    TRY(generator.try_appendln("}"));

    // Formatters
    for (auto const& composite_struct : grammar.composite_structs) {
        generator.set("name"sv, composite_struct.name);

        TRY(generator.try_append("\n"));
        TRY(generator.try_appendln("template<>"));
        TRY(generator.try_appendln("struct AK::Formatter<SPIRV::@name@> : AK::Formatter<FormatString> {"));
        TRY(generator.try_appendln("    ErrorOr<void> format(FormatBuilder& builder, SPIRV::@name@ const& value)"));
        TRY(generator.try_appendln("    {"));
        TRY(generator.try_append("        return Formatter<FormatString>::format(builder, \""));
        bool first = true;
        for (size_t i = 0; i < composite_struct.arguments.size(); ++i) {
            if (!first)
                TRY(generator.try_append(" "));
            first = false;
            TRY(generator.try_append("{}"));
        }
        TRY(generator.try_append("\"sv"));
        for (auto const& argument : composite_struct.arguments) {
            generator.set("argument_name"sv, argument.name);
            TRY(generator.try_append(", value.@argument_name@"));
        }
        TRY(generator.try_appendln(");"));
        TRY(generator.try_appendln("    }"));
        TRY(generator.try_appendln("};"));
    }

    TRY(file.write_until_depleted(generator.as_string_view().bytes()));
    return {};
}

static ErrorOr<void> generate_implementation_file(Grammar const& grammar, Core::File& file)
{
    StringBuilder builder;
    SourceGenerator generator { builder };

    TRY(generator.try_appendln("#include <LibSPIRV/CoreEnums.h>"));
    TRY(generator.try_appendln("#include <AK/StringBuilder.h>"));
    TRY(generator.try_appendln("#include <AK/Vector.h>"));
    TRY(generator.try_append("\n"));
    TRY(generator.try_appendln("namespace SPIRV {"));

    for (auto const& enumeration : grammar.enumerations) {
        generator.set("name"sv, enumeration.name);

        HashTable<DeprecatedString> unique_enum_values;
        TRY(generator.try_append("\n"));
        if (enumeration.type == EnumerationType::ValueEnum) {
            TRY(generator.try_appendln("StringView enumerant_to_string(@name@ value) {"));
            TRY(generator.try_appendln("    switch (value) {"));
            for (auto const& value : enumeration.values) {
                // Because some enumerants have identical values, we can only return a string for the first one
                if (unique_enum_values.set(value.cpp_value) != HashSetResult::InsertedNewEntry)
                    continue;

                generator.set("value"sv, value.name);

                TRY(generator.try_appendln("    case @name@::@value@:"));
                TRY(generator.try_appendln("        return \"@value@\"sv;"));
            }
            TRY(generator.try_appendln("    }"));
            TRY(generator.try_appendln("    VERIFY_NOT_REACHED();"));
            TRY(generator.try_appendln("}"));
        } else {
            TRY(generator.try_appendln("ErrorOr<String> enumerant_to_string(@name@ value) {"));
            TRY(generator.try_appendln("    Vector<DeprecatedString> parts;"));
            for (auto const& value : enumeration.values) {
                // Because some enumerants have identical values, we can only return a string for the first one
                if (unique_enum_values.set(value.cpp_value) != HashSetResult::InsertedNewEntry)
                    continue;

                generator.set("value"sv, value.name);

                TRY(generator.try_appendln("    if (has_flag(value, @name@::@value@))"));
                TRY(generator.try_appendln("        TRY(parts.try_append(\"@value@\"));"));
            }
            TRY(generator.try_appendln("    StringBuilder builder;"));
            TRY(generator.try_appendln("    TRY(builder.try_join(\",\"sv, parts));"));
            TRY(generator.try_appendln("    return builder.to_string();"));
            TRY(generator.try_appendln("}"));
        }
    }

    TRY(generator.try_append("\n"));
    TRY(generator.try_appendln("}"));

    TRY(file.write_until_depleted(generator.as_string_view().bytes()));
    return {};
}

}

ErrorOr<int> serenity_main(Main::Arguments arguments)
{
    StringView generated_header_path;
    StringView generated_implementation_path;
    StringView spirv_grammar_json_path;

    Core::ArgsParser args_parser;
    args_parser.add_option(generated_header_path, "Path to the enum header file to generate", "generated-header-path", 'h', "generated-header-path");
    args_parser.add_option(generated_implementation_path, "Path to the enum implementation file to generate", "generated-implementation-path", 'c', "generated-implementation-path");
    args_parser.add_option(spirv_grammar_json_path, "Path to the SPIR-V grammar JSON file", "json-path", 'j', "json-path");
    args_parser.parse(arguments);

    auto json = TRY(SPIRV::read_entire_file_as_json(spirv_grammar_json_path));
    auto grammar = SPIRV::parse_grammar(json.as_object());

    auto generated_header_file = TRY(Core::File::open(generated_header_path, Core::File::OpenMode::Write));
    auto generated_implementation_file = TRY(Core::File::open(generated_implementation_path, Core::File::OpenMode::Write));

    TRY(SPIRV::generate_header_file(grammar, *generated_header_file));
    TRY(SPIRV::generate_implementation_file(grammar, *generated_implementation_file));

    return 0;
}
