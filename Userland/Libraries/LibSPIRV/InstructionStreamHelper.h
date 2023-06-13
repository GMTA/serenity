/*
 * Copyright (c) 2023, Jelle Raaijmakers <jelle@gmta.nl>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Span.h>
#include <AK/Stream.h>
#include <AK/String.h>
#include <AK/Vector.h>

namespace SPIRV {

class InstructionStreamHelper {
public:
    InstructionStreamHelper(Stream& stream, u16 word_count)
        : m_stream(stream)
        , m_word_count(word_count)
    {
    }

    ErrorOr<u32> read_u32()
    {
        if (m_word_count == 0)
            return Error::from_string_view("Instruction stream has ended but another word was requested"sv);
        --m_word_count;
        return m_stream.read_value<u32>();
    }

    template<typename T>
    ErrorOr<T> read_enumeration()
    {
        return static_cast<T>(TRY(read_u32()));
    }

    ErrorOr<String> read_string()
    {
        Vector<u8> utf8_bytes;
        u8* operand_bytes;
        do {
            auto operand = TRY(read_u32());
            operand_bytes = bit_cast<u8*>(&operand);
            TRY(utf8_bytes.try_append(operand_bytes, sizeof(u32) / sizeof(u8)));
        } while (operand_bytes[3] != 0);
        auto utf8_length_in_bytes = utf8_bytes.find_first_index(0).release_value();
        return String::from_utf8(ReadonlyBytes { utf8_bytes.data(), utf8_length_in_bytes });
    }

    u32 remaining_word_count() const { return m_word_count; }

private:
    Stream& m_stream;
    u32 m_word_count;
};

}
