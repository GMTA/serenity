/*
 * Copyright (c) 2023, Jelle Raaijmakers <jelle@gmta.nl>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/StringBuilder.h>
#include <LibSPIRV/Instruction.h>
#include <LibSPIRV/Module.h>

namespace SPIRV {

ErrorOr<Module> Module::create_from_stream(SeekableStream& stream)
{
    auto magic = TRY(stream.read_value<u32>());
    // FIXME: use magic header to determine endianness (see 3.1. Magic Number)
    if (magic != MAGIC)
        return Error::from_string_view("File does not start with the SPIR-V magic header"sv);

    auto version = TRY(stream.read_value<u32>());
    auto version_major = (version >> 16) & 0xff;
    auto version_minor = (version >> 8) & 0xff;

    auto generator_magic = TRY(stream.read_value<u32>());
    auto bound = TRY(stream.read_value<u32>());
    auto schema = TRY(stream.read_value<u32>());

    Vector<Instruction> instructions;
    TypeRegistry type_registry;
    for (;;) {
        auto instruction_or_error = Instruction::create_from_stream(stream, type_registry);
        if (instruction_or_error.is_error()) {
            if (stream.is_eof())
                break;
            return instruction_or_error.release_error();
        }

        TRY(instructions.try_append(instruction_or_error.release_value()));
    }

    return Module(version_major, version_minor, generator_magic, bound, schema, move(instructions), move(type_registry));
}

ErrorOr<String> Module::to_string()
{
    StringBuilder sb;
    TRY(sb.try_append("; SPIR-V\n"sv));
    TRY(sb.try_appendff("; Version: {}.{}\n", m_version_major, m_version_minor));
    TRY(sb.try_appendff("; Generator: {}\n", m_generator_magic));
    TRY(sb.try_appendff("; Bound: {}\n", m_bound));
    TRY(sb.try_appendff("; Schema: {}\n", m_schema));

    for (auto const& instruction : m_instructions) {
        TRY(sb.try_append(TRY(instruction.to_string(m_type_registry))));
        TRY(sb.try_append('\n'));
    }

    return sb.to_string();
}

}
