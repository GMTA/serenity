/*
* Copyright (c) 2023, Jelle Raaijmakers <jelle@gmta.nl>
*
* SPDX-License-Identifier: BSD-2-Clause
*/

#pragma once

#include <AK/Error.h>
#include <AK/HashMap.h>
#include <AK/Types.h>
#include <LibSPIRV/CoreEnums.h>

namespace SPIRV {

class TypeRegistry {
public:
    struct RegisteredType {
        VariableType type;
        u32 width { 0 };
        bool is_signed { false };
    };

    ErrorOr<void> set_registered_type(u32 id, RegisteredType registered_type)
    {
        TRY(m_registered_types.try_set(id, registered_type));
        return {};
    }
    Optional<RegisteredType> registered_type(u32 id) const { return m_registered_types.get(id); }

private:
    HashMap<u32, RegisteredType> m_registered_types;
};

}
