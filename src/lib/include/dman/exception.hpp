#pragma once

#include <stdexcept>

namespace common
{
class exception : public std::runtime_error
{
  public:
    using std::runtime_error::runtime_error;
};
} // namespace common
