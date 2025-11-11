#include <dman/digest.hpp>

#include <gcrypt.h>
#include <stdexcept>

digest::sha256::sha256(const void *begin, size_t size)
{
    gcry_md_hash_buffer(GCRY_MD_SHA256, content, begin, size);
}

std::string digest::sha256::hex() const
{
    static const char hex_chars[] = "0123456789abcdef";
    std::string result;
    result.reserve(64);
    for (size_t i = 0; i < 32; ++i)
    {
        result.push_back(hex_chars[(content[i] >> 4) & 0x0F]);
        result.push_back(hex_chars[content[i] & 0x0F]);
    }
    return result;
}

bool digest::sha256::operator==(const digest::sha256 &other) const
{
    for (size_t i = 0; i < 32; ++i)
    {
        if (content[i] != other.content[i])
        {
            return false;
        }
    }
    return true;
}

bool digest::sha256::operator==(const std::string &hex_str) const
{
    return hex() == hex_str;
}

uint8_t hex_char_to_value(char c)
{
    c = tolower(c);

    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;

    throw std::invalid_argument("Invalid hex character");
}

digest::sha256::sha256(const std::string &input)
    : sha256(input.data(), input.size())
{
}