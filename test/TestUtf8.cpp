#include <string>
#include <iostream>
#include "common/Utf8.h"
#include "Utility.h"

using namespace TUI::Common;

/**
 * Store this file in utf8 or the test will fail.
 */

int main(int argc, char const *argv[])
{
    (void)argc;
    (void)argv;

    AssertWithMessage(Utf8::IsValid("Hello, World!"), "Valid UTF-8 string should return true");
    AssertWithMessage(Utf8::IsValid("你好，世界！"), "Valid UTF-8 string with Chinese characters should return true");
    AssertWithMessage(Utf8::IsValid("😂"), "Valid UTF-8 string with emoji should return true");
    AssertWithMessage(!Utf8::IsValid("\xFF"), "Invalid UTF-8 string with invalid byte should return false");
    AssertWithMessage(!Utf8::IsValid("\xC0\xFF"), "Invalid UTF-8 string with invalid continuation byte should return false");
    AssertWithMessage(!Utf8::IsValid("\xE0\x80\xFF"), "Invalid UTF-8 string with invalid continuation byte in three-byte sequence should return false");
    AssertWithMessage(!Utf8::IsValid("\xF0\x80\x80\xFF"), "Invalid UTF-8 string with invalid continuation byte in four-byte sequence should return false");

    return 0;
}
