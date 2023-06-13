/*
 * Copyright (c) 2023, Jelle Raaijmakers <jelle@gmta.nl>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibCore/File.h>
#include <LibSPIRV/Module.h>
#include <LibTest/TestCase.h>

static ErrorOr<SPIRV::Module> create_module_from_file(StringView path)
{
    auto file = MUST(Core::File::open(path, Core::File::OpenMode::Read));
    return SPIRV::Module::create_from_stream(*file);
}

static void compare_input_module_with_expected_disassembly(StringView test_name)
{
    auto input_path = MUST(String::formatted("input/{}.spv", test_name));
    auto module = TRY_OR_FAIL(create_module_from_file(input_path));
    auto disassembly = MUST(module.to_string());

    auto expected_path = MUST(String::formatted("expected/{}.txt", test_name));
    auto expected_file = MUST(Core::File::open(expected_path, Core::File::OpenMode::Read));
    auto expected_buffer = MUST(expected_file->read_until_eof());
    auto expected_disassembly = MUST(String::from_utf8({ expected_buffer.bytes() }));

    EXPECT_EQ(disassembly, expected_disassembly);
}

TEST_CASE(simple_fragment_color)
{
    compare_input_module_with_expected_disassembly("simple-fragment-color"sv);
}

TEST_CASE(texture_lighting)
{
    compare_input_module_with_expected_disassembly("texture-lighting"sv);
}