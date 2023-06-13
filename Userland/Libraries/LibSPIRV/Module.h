/*
 * Copyright (c) 2023, Jelle Raaijmakers <jelle@gmta.nl>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Error.h>
#include <AK/Stream.h>
#include <AK/String.h>
#include <AK/Types.h>
#include <AK/Vector.h>
#include <LibSPIRV/Instruction.h>
#include <LibSPIRV/TypeRegistry.h>

namespace SPIRV {

class Module {
public:
    static ErrorOr<Module> create_from_stream(SeekableStream&);

    u8 version_major() const { return m_version_major; }
    u8 version_minor() const { return m_version_minor; }
    u32 generator_magic() const { return m_generator_magic; }
    u32 bound() const { return m_bound; }
    u32 schema() const { return m_schema; }
    Vector<Instruction> const& instructions() const { return m_instructions; }

    ErrorOr<String> to_string();

private:
    static constexpr u32 MAGIC = 0x07230203;

    Module(u8 version_major, u8 version_minor, u32 generator_magic, u32 bound, u32 schema,
        Vector<Instruction>&& instructions, TypeRegistry&& type_registry)
        : m_version_major(version_major)
        , m_version_minor(version_minor)
        , m_generator_magic(generator_magic)
        , m_bound(bound)
        , m_schema(schema)
        , m_instructions(move(instructions))
        , m_type_registry(move(type_registry))
    {
    }

    u8 m_version_major;
    u8 m_version_minor;
    u32 m_generator_magic;
    u32 m_bound;
    u32 m_schema;
    Vector<Instruction> m_instructions;
    TypeRegistry m_type_registry;
};

}
