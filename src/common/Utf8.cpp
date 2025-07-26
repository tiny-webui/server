#include "Utf8.h"

using namespace TUI::Common;

bool Utf8::IsValid(const std::string& str) noexcept
{
    size_t i = 0;
    while (i < str.length()) {
        unsigned char c = str[i];
        
        /** Single byte character (0xxxxxxx) */
        if ((c & 0x80) == 0) {
            i++;
        }
        /** Two byte character (110xxxxx 10xxxxxx) */
        else if ((c & 0xE0) == 0xC0) {
            if (i + 1 >= str.length() || (str[i + 1] & 0xC0) != 0x80) {
                return false;
            }
            i += 2;
        }
        /** Three byte character (1110xxxx 10xxxxxx 10xxxxxx) */ 
        else if ((c & 0xF0) == 0xE0) {
            if (i + 2 >= str.length() || 
                (str[i + 1] & 0xC0) != 0x80 || 
                (str[i + 2] & 0xC0) != 0x80) {
                return false;
            }
            i += 3;
        }
        /** Four byte character (11110xxx 10xxxxxx 10xxxxxx 10xxxxxx) */
        else if ((c & 0xF8) == 0xF0) {
            if (i + 3 >= str.length() || 
                (str[i + 1] & 0xC0) != 0x80 || 
                (str[i + 2] & 0xC0) != 0x80 || 
                (str[i + 3] & 0xC0) != 0x80) {
                return false;
            }
            i += 4;
        }
        else {
            return false;
        }
    }
    return true;
}
