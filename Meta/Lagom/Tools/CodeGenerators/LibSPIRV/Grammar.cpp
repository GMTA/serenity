/*
 * Copyright (c) 2023, Jelle Raaijmakers <jelle@gmta.nl>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/HashMap.h>
#include <AK/StringBuilder.h>
#include <LibCore/File.h>
#include <Meta/Lagom/Tools/CodeGenerators/LibSPIRV/Grammar.h>

namespace SPIRV {

DeprecatedString argument_name(StringView kind, ArgumentQuantifier quantifier, DeprecatedString name)
{
    if (kind == "IdResult"sv)
        return "result_id";
    if (kind == "IdResultType"sv)
        return "result_type_id";

    // Deal with weird *-quantifier names
    if (quantifier == ArgumentQuantifier::Multiple) {
        // Only look at part before first comma
        if (auto comma_pos = name.find(','); comma_pos.has_value())
            name = name.substring_view(0, comma_pos.value());

        // Remove quotes and freestanding numbers
        name = name.trim("'"sv);
        StringBuilder builder;
        for (size_t i = 0; i < name.length(); ++i) {
            auto name_char = name[i];

            if (name_char >= '0' && name_char <= '9') {
                while (++i < name.length()) {
                    if (name[i] != ' ')
                        break;
                }
                --i;
                continue;
            }

            builder.append(name_char);
        }
        name = builder.to_deprecated_string().trim(" "sv);
    }

    // Convert camelCase into snake_case
    StringBuilder builder;
    bool last_is_underscore = true;
    bool last_is_uppercase = false;
    for (size_t i = 0; i < name.length(); ++i) {
        auto name_char = name[i];

        // Remove formatting characters
        if (name_char == '\'' || name_char == '.' || name_char == '~')
            continue;

        if (name_char == ' ') {
            if (!last_is_underscore) {
                builder.append('_');
                last_is_underscore = true;
            }
            continue;
        }

        if (name_char >= 'A' && name_char <= 'Z') {
            if (!last_is_underscore && !last_is_uppercase)
                builder.append('_');
            name_char |= 0x20;
            last_is_uppercase = true;
        } else {
            last_is_uppercase = false;
        }

        builder.append(name_char);
        last_is_underscore = false;
    }

    if (kind == "IdRef"sv) {
        builder.append("_id"sv);
        if (quantifier == ArgumentQuantifier::Multiple)
            builder.append("s"sv);
    }

    return builder.to_deprecated_string();
}

DeprecatedString opcode_identifier(StringView opcode)
{
    // Strip the 'Op' prefix to make code a bit easier on the eyes
    if (opcode.starts_with("Op"sv))
        return opcode.substring_view(2);
    return opcode;
}

ErrorOr<JsonValue> read_entire_file_as_json(StringView filename)
{
    auto file = TRY(Core::File::open(filename, Core::File::OpenMode::Read));
    auto json_data = TRY(file->read_until_eof());
    return JsonValue::from_string(json_data);
}

DeprecatedString enumeration_name(StringView kind)
{
    if (kind == "IdMemorySemantics")
        return "MemorySemantics";
    if (kind == "IdScope")
        return "Scope";
    return kind;
}

DeprecatedString valid_enumerant_identifier(StringView enumerant_identifier)
{
    auto first_character = enumerant_identifier[0];
    if (first_character >= '0' && first_character <= '9')
        return DeprecatedString::formatted("_{}", enumerant_identifier);
    return enumerant_identifier;
}

static void deduplicate_arguments(Vector<Argument>& arguments)
{
    // Slow but fine for the small number of identifiers we expect
    for (size_t i = 0; i < arguments.size(); ++i) {
        auto renumbered = false;
        auto suffix_count = 2;
        for (size_t j = i + 1; j < arguments.size(); ++j) {
            if (arguments[i].name == arguments[j].name) {
                arguments[j].name = DeprecatedString::formatted("{}_{}", arguments[j].name, suffix_count++);
                renumbered = true;
            }
        }
        if (renumbered)
            arguments[i].name = DeprecatedString::formatted("{}_1", arguments[i].name);
    }
}

static Enumeration parse_kind(EnumerationType enumeration_type, JsonObject const& operand_kind)
{
    DeprecatedString name = operand_kind.get("kind"sv)->as_string();

    Vector<EnumerationValue> values;
    auto enumerants = operand_kind.get("enumerants"sv)->as_array();
    enumerants.for_each([&](JsonValue const& enumerant_value) {
        auto enumerant = enumerant_value.as_object();

        auto enumerant_name = valid_enumerant_identifier(enumerant.get("enumerant"sv)->as_string());
        auto cpp_value = enumerant.get("value"sv)->to_deprecated_string();

        values.append({ enumerant_name, cpp_value, {} });
    });

    return { enumeration_type, name, values };
}

static void update_enumeration_with_parameters(Enumeration&, JsonArray const&)
{
}

static CompositeStruct parse_composite(JsonObject const& operand_kind)
{
    auto name = operand_kind.get("kind"sv)->as_string();

    Vector<Argument> arguments;
    auto bases = operand_kind.get("bases"sv)->as_array();
    bases.for_each([&](JsonValue const& base_value) {
        auto base = base_value.as_string();
        if (base == "IdRef"sv)
            arguments.append({ ArgumentType::Integer, ArgumentQuantifier::Single, "u32", "ref_id", {} });
        else if (base == "LiteralInteger"sv)
            arguments.append({ ArgumentType::Integer, ArgumentQuantifier::Single, "u32", "literal", {} });
        else
            VERIFY_NOT_REACHED();
    });
    deduplicate_arguments(arguments);

    return { name, arguments };
}

static Instruction parse_instruction(Vector<Enumeration>& enumerations, Vector<CompositeStruct>& composite_structs, JsonObject const& instruction)
{
    auto opname = opcode_identifier(instruction.get("opname"sv)->as_string());
    auto opcode = instruction.get("opcode"sv)->as_integer<u16>();
    auto cpp_opcode = instruction.get("opcode"sv)->to_deprecated_string();

    if (!instruction.has("operands"sv))
        return { opname, opcode, cpp_opcode, {} };

    Vector<Argument> arguments;
    auto operands = instruction.get("operands"sv)->as_array();
    operands.for_each([&](JsonValue const& operand_value) {
        auto operand = operand_value.as_object();
        auto quantifier_string = operand.get_deprecated_string("quantifier"sv);

        // Determine qualifier
        ArgumentQuantifier quantifier = ArgumentQuantifier::Single;
        if (quantifier_string.has_value()) {
            if (quantifier_string.value() == "?"sv)
                quantifier = ArgumentQuantifier::Optional;
            else if (quantifier_string.value() == "*"sv)
                quantifier = ArgumentQuantifier::Multiple;
            else
                VERIFY_NOT_REACHED();
        }

        // Determine C++ type
        ArgumentType argument_type;
        DeprecatedString cpp_type;
        auto kind = operand.get("kind"sv)->as_string();
        if (kind == "IdRef"sv
            || kind == "IdResult"sv
            || kind == "IdResultType"sv) {
            argument_type = ArgumentType::IdReference;
            cpp_type = "u32";
        } else if (kind == "LiteralExtInstInteger"sv
            || kind == "LiteralInteger"sv
            || kind == "LiteralSpecConstantOpInteger"sv) {
            argument_type = ArgumentType::Integer;
            cpp_type = "u32";
        } else if (kind == "LiteralContextDependentNumber"sv) {
            argument_type = ArgumentType::ContextDependentNumber;
            cpp_type = "u64";
        } else if (kind == "LiteralString"sv) {
            argument_type = ArgumentType::String;
            cpp_type = "String";
        } else {
            cpp_type = enumeration_name(kind);

            // See if kind points to a composite struct
            auto composite_index = composite_structs.find_first_index_if([&kind](CompositeStruct const& composite_struct) {
                return composite_struct.name == kind;
            });
            if (composite_index.has_value())
                argument_type = ArgumentType::CompositeStruct;
            else
                argument_type = ArgumentType::Enumeration;
        }

        // Set enumeration
        Optional<Enumeration> enumeration {};
        if (argument_type == ArgumentType::Enumeration) {
            auto possible_enumeration = enumerations.find_if([&cpp_type](Enumeration const& enumeration) {
                return enumeration.name == cpp_type;
            });
            VERIFY(possible_enumeration != enumerations.end());
            enumeration = *possible_enumeration;
        }

        // Determine argument name
        DeprecatedString name;
        if (operand.has("name"sv))
            name = argument_name(kind, quantifier, operand.get("name"sv)->as_string());
        else
            name = argument_name(kind, quantifier, kind);

        arguments.append({ argument_type, quantifier, cpp_type, name, enumeration });
    });
    deduplicate_arguments(arguments);

    return { opname, opcode, cpp_opcode, arguments };
}

Grammar parse_grammar(JsonObject const& grammar)
{
    Vector<Enumeration> enumerations;
    Vector<CompositeStruct> composite_structs;
    Vector<Instruction> instructions;

    // Enumerations and composite structs
    auto operand_kinds = grammar.get("operand_kinds"sv)->as_array();
    operand_kinds.for_each([&](JsonValue const& operand_kind_value) {
        auto operand_kind = operand_kind_value.as_object();
        auto category = operand_kind.get("category"sv)->as_string();

        if (category == "Composite"sv) {
            composite_structs.append(parse_composite(operand_kind));
            return;
        }

        EnumerationType enumeration_type;
        if (category == "BitEnum"sv)
            enumeration_type = EnumerationType::BitEnum;
        else if (category == "ValueEnum"sv)
            enumeration_type = EnumerationType::ValueEnum;
        else
            return;

        enumerations.append(parse_kind(enumeration_type, operand_kind));
    });

    // Add parameters for enumerant values in a second pass, since they can point to other enumerations
    operand_kinds.for_each([&](JsonValue const& operand_kind_value) {
        auto operand_kind = operand_kind_value.as_object();

        // Only ValueEnum enumerant values can have parameters
        auto category = operand_kind.get("category"sv)->as_string();
        if (category != "ValueEnum"sv)
            return;

        auto kind = operand_kind.get("kind"sv)->as_string();
        auto enumeration = enumerations.find_if([&kind](Enumeration const& enumeration) {
            return enumeration.name == kind;
        });

        auto enumerants = operand_kind.get("enumerants"sv)->as_array();
        update_enumeration_with_parameters(*enumeration, enumerants);
    });

    // Instructions
    auto instruction_list = grammar.get("instructions"sv)->as_array();
    instruction_list.for_each([&](JsonValue const& instruction_value) {
        auto instruction = instruction_value.as_object();

        // Skip unsupported classes
        auto instruction_class = instruction.get("class"sv)->as_string();
        if (instruction_class == "@exclude"sv || instruction_class == "Reserved"sv)
            return;

        instructions.append(parse_instruction(enumerations, composite_structs, instruction));
    });

    return { enumerations, composite_structs, instructions };
}

}
