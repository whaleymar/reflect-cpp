#ifndef RFL_IO_SAVE_BYTES_HPP_
#define RFL_IO_SAVE_BYTES_HPP_

#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "../Result.hpp"

namespace rfl {
namespace io {

template <class T, class WriteFunction>
Result<Nothing> save_bytes(const std::string& _fname, const T& _obj,
                           const WriteFunction& _write) {
  std::ofstream output(_fname, std::ios::out | std::ios::binary);
  _write(_obj, output);
  output.close();
  return Nothing{};
}

}  // namespace io
}  // namespace rfl

#endif
