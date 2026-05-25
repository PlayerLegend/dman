#pragma once

#include <stdexcept>

namespace exception
{
class exception : public std::runtime_error
{
  public:
    using std::runtime_error::runtime_error;
};
} // namespace exception
