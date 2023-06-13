/*
 * Copyright (c) 2023, Jelle Raaijmakers <jelle@gmta.nl>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibSPIRV/Instruction.h>

namespace SPIRV {

ErrorOr<String> Instruction::typed_value_to_string(u64 value, TypeRegistry::RegisteredType variable_type)
{
    if (variable_type.type == VariableType::Int) {
        if (variable_type.width == 32) {
            if (variable_type.is_signed)
                return String::number(*bit_cast<i32*>(&value));
            return String::number(*bit_cast<u32*>(&value));
        } else if (variable_type.width == 64) {
            if (variable_type.is_signed)
                return String::number(*bit_cast<i64*>(&value));
            return String::number(value);
        }
        return Error::from_string_view("Unsupported integer width"sv);
    } else if (variable_type.type == VariableType::Float) {
        if (variable_type.width == 32)
            return String::number(*bit_cast<float*>(&value));
        else if (variable_type.width == 64)
            return String::number(*bit_cast<double*>(&value));
        return Error::from_string_view("Unsupported float width"sv);
    }

    // FIXME: implement other types
    return String::number(value);
}

}
