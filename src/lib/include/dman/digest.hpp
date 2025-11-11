#include <stdint.h>
#include <string>
#include <sys/types.h>

namespace digest
{
class sha256
{
    uint8_t content[32];

  public:
    sha256(const void *begin, size_t size);
    sha256() {};
    std::string hex() const;
    bool operator==(const sha256 &other) const;
    bool operator==(const std::string &hex_str) const;
    void operator=(const std::string &hex_str);
};
} // namespace digest