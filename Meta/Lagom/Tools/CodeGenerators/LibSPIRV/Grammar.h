/*
 * Copyright (c) 2023, Jelle Raaijmakers <jelle@gmta.nl>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/DeprecatedString.h>
#include <AK/JsonObject.h>
#include <AK/Optional.h>
#include <AK/StringView.h>
#include <AK/Vector.h>

namespace SPIRV {

enum class EnumerationType {
    BitEnum,
    ValueEnum,
};

struct Argument;
struct EnumerationValue {
    DeprecatedString name;
    DeprecatedString cpp_value;
    Vector<Argument> parameters;
};

struct Enumeration {
    EnumerationType type;
    DeprecatedString name;
    Vector<EnumerationValue> values;
};

enum class ArgumentType {
    IdReference,
    Integer,
    String,
    ContextDependentNumber,
    Enumeration,
    CompositeStruct,
};

enum class ArgumentQuantifier {
    Optional,
    Single,
    Multiple,
};

struct Argument {
    ArgumentType type;
    ArgumentQuantifier quantifier;
    DeprecatedString cpp_type;
    DeprecatedString name;
    Optional<Enumeration> enumeration;
};

struct CompositeStruct {
    DeprecatedString name;
    Vector<Argument> arguments;
};

struct Instruction {
    DeprecatedString opname;
    u16 opcode;
    DeprecatedString cpp_opcode;
    Vector<Argument> arguments;
};

struct Grammar {
    Vector<Enumeration> enumerations;
    Vector<CompositeStruct> composite_structs;
    Vector<Instruction> instructions;
};

DeprecatedString argument_name(StringView kind, Optional<DeprecatedString> quantifier, DeprecatedString name);
DeprecatedString opcode_identifier(StringView opcode);
ErrorOr<JsonValue> read_entire_file_as_json(StringView filename);
DeprecatedString valid_enumerant_identifier(StringView enumerant_identifier);

Grammar parse_grammar(JsonObject const&);

}
