/*
 * Copyright (c) 2023, Jelle Raaijmakers <jelle@gmta.nl>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Error.h>
#include <AK/OwnPtr.h>
#include <AK/Stream.h>
#include <AK/String.h>
#include <AK/Types.h>
#include <AK/Variant.h>
#include <LibSPIRV/CoreEnums.h>
#include <LibSPIRV/CoreInstructions.h>
#include <LibSPIRV/TypeRegistry.h>

namespace SPIRV {

class Instruction {
public:
    static ErrorOr<Instruction> create_from_stream(Stream&, TypeRegistry&);

    Opcode opcode() const { return m_opcode; }
    ErrorOr<String> to_string(TypeRegistry&) const;
    static ErrorOr<String> typed_value_to_string(u64 value, TypeRegistry::RegisteredType);

    template<typename T>
    T arguments() const { return *dynamic_cast<T*>(m_arguments.ptr()); }

private:
    Instruction(Opcode opcode, OwnPtr<InstructionArgumentsBase> arguments)
        : m_opcode(opcode)
        , m_arguments(move(arguments))
    {
    }

    Opcode m_opcode;
    OwnPtr<InstructionArgumentsBase> m_arguments;
};

}
