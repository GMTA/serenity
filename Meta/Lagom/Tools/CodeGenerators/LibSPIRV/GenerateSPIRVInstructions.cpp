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
    TRY(generator.try_appendln("#include <AK/Error.h>"));
    TRY(generator.try_appendln("#include <AK/Optional.h>"));
    TRY(generator.try_appendln("#include <AK/Stream.h>"));
    TRY(generator.try_appendln("#include <AK/String.h>"));
    TRY(generator.try_appendln("#include <AK/Types.h>"));
    TRY(generator.try_appendln("#include <AK/Variant.h>"));
    TRY(generator.try_appendln("#include <LibSPIRV/CoreEnums.h>"));

    TRY(generator.try_append("\n"));
    TRY(generator.try_appendln("namespace SPIRV {"));

    TRY(generator.try_append("\n"));
    TRY(generator.try_appendln("struct InstructionArgumentsBase {"));
    TRY(generator.try_appendln("    virtual ~InstructionArgumentsBase() = default;"));
    TRY(generator.try_appendln("};"));

    // Generate argument structs for all opcodes
    for (auto const& instruction : grammar.instructions) {
        // Skip opcodes without arguments
        if (instruction.arguments.is_empty())
            continue;

        generator.set("opname"sv, instruction.opname);

        TRY(generator.try_append("\n"));
        TRY(generator.try_appendln("struct @opname@Arguments : InstructionArgumentsBase {"));
        for (auto const& argument : instruction.arguments) {
            auto argument_type = argument.cpp_type;
            if (argument.quantifier == ArgumentQuantifier::Optional)
                argument_type = DeprecatedString::formatted("Optional<{}>", argument_type);
            else if (argument.quantifier == ArgumentQuantifier::Multiple)
                argument_type = DeprecatedString::formatted("Vector<{}>", argument_type);

            generator.set("argument_type"sv, argument_type);
            generator.set("argument"sv, argument.name);

            TRY(generator.try_appendln("    @argument_type@ @argument@;"));
        }
        TRY(generator.try_appendln("};"));
    }

    TRY(generator.try_append("\n"));
    TRY(generator.try_appendln("}"));

    TRY(file.write_until_depleted(generator.as_string_view().bytes()));
    return {};
}

static ErrorOr<void> generate_implementation_file(Grammar const& grammar, Core::File& file)
{
    StringBuilder builder;
    SourceGenerator generator { builder };

    TRY(generator.try_appendln("#include <LibSPIRV/CoreInstructions.h>"));
    TRY(generator.try_appendln("#include <AK/StringBuilder.h>"));
    TRY(generator.try_appendln("#include <LibSPIRV/Instruction.h>"));
    TRY(generator.try_appendln("#include <LibSPIRV/InstructionStreamHelper.h>"));
    TRY(generator.try_appendln("#include <LibSPIRV/TypeRegistry.h>"));
    TRY(generator.try_append("\n"));
    TRY(generator.try_appendln("namespace SPIRV {"));

    TRY(generator.try_append("\n"));
    TRY(generator.try_appendln("template<typename T>"));
    TRY(generator.try_appendln("ErrorOr<T> read_composite_struct(InstructionStreamHelper&);"));

    for (auto const& composite_struct : grammar.composite_structs) {
        generator.set("composite_struct"sv, composite_struct.name);

        TRY(generator.try_append("\n"));
        TRY(generator.try_appendln("template<>"));
        TRY(generator.try_appendln("ErrorOr<@composite_struct@> read_composite_struct(InstructionStreamHelper& helper)"));
        TRY(generator.try_appendln("{"));
        TRY(generator.try_appendln("    return @composite_struct@ {"));
        for (auto const& argument : composite_struct.arguments) {
            generator.set("argument"sv, argument.name);
            if (argument.type == ArgumentType::Integer)
                TRY(generator.try_appendln("        .@argument@ = TRY(helper.read_u32()),"));
            else
                VERIFY_NOT_REACHED();
        }
        TRY(generator.try_appendln("    };"));
        TRY(generator.try_appendln("}"));
    }

    TRY(generator.try_append("\n"));
    TRY(generator.try_appendln("ErrorOr<Instruction> Instruction::create_from_stream(Stream& stream, TypeRegistry& type_registry)"));
    TRY(generator.try_appendln("{"));
    TRY(generator.try_appendln("    auto opcode_and_word_count = TRY(stream.read_value<u32>());"));
    TRY(generator.try_appendln("    u16 word_count = opcode_and_word_count >> 16;"));
    TRY(generator.try_appendln("    auto opcode = static_cast<Opcode>(opcode_and_word_count & 0xffff);"));
    TRY(generator.try_append("\n"));
    TRY(generator.try_appendln("    if (word_count == 0)"));
    TRY(generator.try_appendln("        return Error::from_string_view(\"Invalid word count for instruction\"sv);"));
    TRY(generator.try_appendln("    --word_count;"));
    TRY(generator.try_append("\n"));
    TRY(generator.try_appendln("    auto helper = InstructionStreamHelper { stream, word_count };"));
    TRY(generator.try_appendln("    switch (opcode) {"));
    HashTable<u32> unique_opcodes;
    for (auto const& instruction : grammar.instructions) {
        // Some instructions share opcodes - we only need a single case for them
        if (TRY(unique_opcodes.try_set(instruction.opcode)) != HashSetResult::InsertedNewEntry)
            continue;

        generator.set("opname"sv, instruction.opname);

        if (instruction.arguments.is_empty()) {
            TRY(generator.try_appendln("    case Opcode::@opname@:"));
            TRY(generator.try_appendln("        return Instruction { opcode, {} };"));
            continue;
        }

        TRY(generator.try_appendln("    case Opcode::@opname@: {"));
        TRY(generator.try_appendln("        auto arguments = TRY(adopt_nonnull_own_or_enomem(new (nothrow) @opname@Arguments()));"));
        for (auto const& argument : instruction.arguments) {
            generator.set("argument"sv, argument.name);

            DeprecatedString helper_expression;
            switch (argument.type) {
            case ArgumentType::ContextDependentNumber:
            case ArgumentType::IdReference:
            case ArgumentType::Integer:
                helper_expression = "TRY(helper.read_u32())";
                break;
            case ArgumentType::String:
                helper_expression = "TRY(helper.read_string())";
                break;
            case ArgumentType::Enumeration:
                helper_expression = DeprecatedString::formatted("TRY(helper.read_enumeration<{}>())", argument.cpp_type);
                break;
            case ArgumentType::CompositeStruct:
                helper_expression = DeprecatedString::formatted("TRY(read_composite_struct<{}>(helper))", argument.cpp_type);
                break;
            }
            generator.set("helper_expression"sv, helper_expression);

            if (argument.type == ArgumentType::ContextDependentNumber) {
                VERIFY(argument.quantifier == ArgumentQuantifier::Single);
                TRY(generator.try_appendln("        auto registered_type = type_registry.registered_type(arguments->result_type_id);"));
                TRY(generator.try_appendln("        if (!registered_type.has_value())"));
                TRY(generator.try_appendln("            return Error::from_string_view(\"Result type is unknown\"sv);"));
                TRY(generator.try_appendln("        if (registered_type->width > 32)"));
                TRY(generator.try_appendln("            arguments->@argument@ = @helper_expression@ | (static_cast<u64>(@helper_expression@) << 32);"));
                TRY(generator.try_appendln("        else"));
                TRY(generator.try_appendln("            arguments->@argument@ = @helper_expression@;"));
            } else if (argument.quantifier == ArgumentQuantifier::Optional) {
                TRY(generator.try_appendln("        if (helper.remaining_word_count() > 0)"));
                TRY(generator.try_appendln("            arguments->@argument@ = @helper_expression@;"));
            } else if (argument.quantifier == ArgumentQuantifier::Multiple) {
                TRY(generator.try_appendln("        while (helper.remaining_word_count() > 0)"));
                TRY(generator.try_appendln("            TRY(arguments->@argument@.try_append(@helper_expression@));"));
            } else {
                TRY(generator.try_appendln("        arguments->@argument@ = @helper_expression@;"));
            }
        }

        // Update type registry for all OpType* instructions
        if (instruction.opname.starts_with("Type"sv)) {
            auto result_id_attribute = instruction.arguments.find_first_index_if([](auto const& argument) {
                return argument.name == "result_id";
            });
            auto width_attribute = instruction.arguments.find_if([](auto const& argument) {
                return argument.name == "width";
            });
            auto signedness_attribute = instruction.arguments.find_if([](auto const& argument) {
                return argument.name == "signedness";
            });

            if (result_id_attribute.has_value()) {
                generator.set("type_name"sv, instruction.opname.substring(4));
                TRY(generator.try_append("        TRY(type_registry.set_registered_type(arguments->result_id, { .type = VariableType::@type_name@"));
                if (!width_attribute.is_end())
                    TRY(generator.try_appendln(", .width = arguments->width"));
                if (!signedness_attribute.is_end())
                    TRY(generator.try_appendln(", .is_signed = (arguments->signedness == 1)"));
                TRY(generator.try_appendln(" }));"));
            }
        }

        TRY(generator.try_appendln("        return Instruction { opcode, arguments.release_nonnull<InstructionArgumentsBase>() };"));
        TRY(generator.try_appendln("    }"));
    }
    TRY(generator.try_appendln("    }"));
    TRY(generator.try_appendln("    VERIFY_NOT_REACHED();"));
    TRY(generator.try_appendln("}"));

    TRY(generator.try_append("\n"));
    TRY(generator.try_appendln("ErrorOr<String> Instruction::to_string(TypeRegistry& type_registry) const"));
    TRY(generator.try_appendln("{"));
    TRY(generator.try_appendln("    StringBuilder sb;"));
    TRY(generator.try_appendln("    switch (m_opcode) {"));
    unique_opcodes.clear_with_capacity();
    for (auto const& instruction : grammar.instructions) {
        // Some instructions share opcodes - we only need a single case for them
        if (TRY(unique_opcodes.try_set(instruction.opcode)) != HashSetResult::InsertedNewEntry)
            continue;

        generator.set("opname"sv, instruction.opname);

        TRY(generator.try_appendln("    case Opcode::@opname@: {"));

        if (instruction.arguments.is_empty()) {
            TRY(generator.try_appendln("        TRY(sb.try_appendff(\"{:>15}Op@opname@\", \"\"sv));"));
            TRY(generator.try_appendln("        break;"));
            TRY(generator.try_appendln("    }"));
            continue;
        }

        TRY(generator.try_appendln("        auto args = arguments<@opname@Arguments>();"));
        TRY(generator.try_append("        TRY(sb.try_appendff(\"{:>15}Op@opname@\", "));

        // Result ID first
        auto result_id_argument = instruction.arguments.find_first_index_if([](Argument const& argument) {
            return argument.name == "result_id";
        });
        if (result_id_argument.has_value())
            TRY(generator.try_appendln("TRY(String::formatted(\"%{} = \", args.result_id))));"));
        else
            TRY(generator.try_appendln("\"\"sv));"));

        // Then all other arguments
        for (auto const& argument : instruction.arguments) {
            if (argument.name == "result_id")
                continue;

            DeprecatedString argument_expression = DeprecatedString::formatted("args.{}", argument.name);

            DeprecatedString value_expression = argument_expression;
            if (argument.quantifier == ArgumentQuantifier::Optional)
                value_expression = DeprecatedString::formatted("{}.value()", argument_expression);

            DeprecatedString stringify_expression;
            if (argument.type == ArgumentType::Enumeration) {
                VERIFY(argument.enumeration.has_value());
                if (argument.enumeration->type == EnumerationType::BitEnum)
                    stringify_expression = DeprecatedString::formatted("TRY(enumerant_to_string({}))", value_expression);
                else if (argument.enumeration->type == EnumerationType::ValueEnum)
                    stringify_expression = DeprecatedString::formatted("enumerant_to_string({})", value_expression);
                else
                    VERIFY_NOT_REACHED();
            } else if (argument.type == ArgumentType::ContextDependentNumber) {
                stringify_expression = DeprecatedString::formatted("TRY(typed_value_to_string({}, registered_type.value()))", value_expression);
            } else if (argument.type == ArgumentType::Integer) {
                stringify_expression = DeprecatedString::formatted("TRY(String::number({}))", value_expression);
            } else {
                stringify_expression = value_expression;
            }

            DeprecatedString format_expression = "{}";
            if (argument.type == ArgumentType::IdReference)
                format_expression = "%{}";
            else if (argument.type == ArgumentType::String)
                format_expression = "\\\"{}\\\"";

            generator.set("argument_expression"sv, argument_expression);
            generator.set("stringify_expression"sv, stringify_expression);
            generator.set("format_expression"sv, format_expression);
            if (argument.type == ArgumentType::ContextDependentNumber) {
                VERIFY(argument.quantifier == ArgumentQuantifier::Single);
                TRY(generator.try_appendln("        auto registered_type = type_registry.registered_type(args.result_type_id);"));
                TRY(generator.try_appendln("        if (!registered_type.has_value())"));
                TRY(generator.try_appendln("            return Error::from_string_view(\"Result type is unknown\"sv);"));
                TRY(generator.try_appendln("        TRY(sb.try_appendff(\" @format_expression@\", @stringify_expression@));"));
            } else if (argument.quantifier == ArgumentQuantifier::Single) {
                TRY(generator.try_appendln("        TRY(sb.try_appendff(\" @format_expression@\", @stringify_expression@));"));
            } else if (argument.quantifier == ArgumentQuantifier::Optional) {
                TRY(generator.try_appendln("        if (@argument_expression@.has_value())"));
                TRY(generator.try_appendln("            TRY(sb.try_appendff(\" @format_expression@\", @stringify_expression@));"));
            } else if (argument.quantifier == ArgumentQuantifier::Multiple) {
                TRY(generator.try_appendln("        for (auto value : @argument_expression@)"));
                TRY(generator.try_appendln("            TRY(sb.try_appendff(\" @format_expression@\", value));"));
            }
        }

        TRY(generator.try_appendln("        break;"));
        TRY(generator.try_appendln("    }"));
    }
    TRY(generator.try_appendln("    default:"));
    TRY(generator.try_appendln("        VERIFY_NOT_REACHED();"));
    TRY(generator.try_appendln("    }"));
    TRY(generator.try_appendln("    return sb.to_string();"));
    TRY(generator.try_appendln("}"));

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
    args_parser.add_option(generated_header_path, "Path to the instructions header file to generate", "generated-header-path", 'h', "generated-header-path");
    args_parser.add_option(generated_implementation_path, "Path to the instructions implementation file to generate", "generated-implementation-path", 'c', "generated-implementation-path");
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
