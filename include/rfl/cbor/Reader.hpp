#ifndef RFL_CBOR_READER_HPP_
#define RFL_CBOR_READER_HPP_

#include <cbor.h>

#include <cstddef>
#include <exception>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include "../Bytestring.hpp"
#include "../Result.hpp"
#include "../always_false.hpp"

namespace rfl {
namespace cbor {

/// Please refer to https://intel.github.io/tinycbor/current/index.html
struct Reader {
  struct CBORInputArray {
    CborValue val_;
  };

  struct CBORInputObject {
    CborValue val_;
  };

  struct CBORInputVar {
    CborValue val_;
  };

  using InputArrayType = CBORInputArray;
  using InputObjectType = CBORInputObject;
  using InputVarType = CBORInputVar;

  template <class T>
  static constexpr bool has_custom_constructor =
      (requires(InputVarType var) { T::from_cbor_obj(var); });

  rfl::Result<InputVarType> get_field_from_array(
      const size_t _idx, const InputArrayType& _arr) const noexcept;

  rfl::Result<InputVarType> get_field_from_object(
      const std::string& _name, const InputObjectType& _obj) const noexcept;

  bool is_empty(const InputVarType& _var) const noexcept;

  template <class T>
  rfl::Result<T> to_basic_type(const InputVarType& _var) const noexcept {
    if constexpr (std::is_same<std::remove_cvref_t<T>, std::string>()) {
      if (!cbor_value_is_text_string(&_var.val_)) {
        return Error("Could not cast to string.");
      }
      std::string str;
      const auto err = get_string(&_var.val_, &str);
      if (err != CborNoError) {
        return Error(cbor_error_string(err));
      }
      return str;
    } else if constexpr (std::is_same<std::remove_cvref_t<T>,
                                      rfl::Bytestring>()) {
      if (!cbor_value_is_byte_string(&_var.val_)) {
        return Error("Could not cast to bytestring.");
      }
      rfl::Bytestring bstr;
      const auto err = get_bytestring(&_var.val_, &bstr);
      if (err != CborNoError) {
        return Error(cbor_error_string(err));
      }
      return bstr;
    } else if constexpr (std::is_same<std::remove_cvref_t<T>, bool>()) {
      if (!cbor_value_is_boolean(&_var.val_)) {
        return rfl::Error("Could not cast to boolean.");
      }
      bool result = false;
      const auto err = cbor_value_get_boolean(&_var.val_, &result);
      if (err != CborNoError) {
        return Error(cbor_error_string(err));
      }
      return result;
    } else if constexpr (std::is_floating_point<std::remove_cvref_t<T>>() ||
                         std::is_integral<std::remove_cvref_t<T>>()) {
      if (cbor_value_is_integer(&_var.val_)) {
        std::int64_t result = 0;
        const auto err = cbor_value_get_int64(&_var.val_, &result);
        if (err != CborNoError) {
          return Error(cbor_error_string(err));
        }
        return static_cast<T>(result);
      } else if (cbor_value_is_float(&_var.val_)) {
        float result = 0.0;
        const auto err = cbor_value_get_float(&_var.val_, &result);
        if (err != CborNoError) {
          return Error(cbor_error_string(err));
        }
        return static_cast<T>(result);
      } else if (cbor_value_is_double(&_var.val_)) {
        double result = 0.0;
        const auto err = cbor_value_get_double(&_var.val_, &result);
        if (err != CborNoError) {
          return Error(cbor_error_string(err));
        }
        return static_cast<T>(result);
      }
      return rfl::Error(
          "Could not cast to numeric value. The type must be integral, float "
          "or double.");

    } else {
      static_assert(rfl::always_false_v<T>, "Unsupported type.");
    }
  }

  rfl::Result<InputArrayType> to_array(const InputVarType& _var) const noexcept;

  rfl::Result<InputObjectType> to_object(
      const InputVarType& _var) const noexcept;

  template <class ArrayReader>
  std::optional<Error> read_array(const ArrayReader& _array_reader,
                                  const InputArrayType& _arr) const noexcept {
    InputVarType var;
    auto buffer = std::vector<char>();
    auto err = cbor_value_enter_container(&_arr.val_, &var.val_);
    if (err != CborNoError && err != CborErrorOutOfMemory) {
      return Error(cbor_error_string(err));
    }
    size_t length = 0;
    err = cbor_value_get_array_length(&_arr.val_, &length);
    if (err != CborNoError && err != CborErrorOutOfMemory) {
      return Error(cbor_error_string(err));
    }
    for (size_t i = 0; i < length; ++i) {
      const auto err2 = _array_reader.read(var);
      if (err2) {
        return err2;
      }
      err = cbor_value_advance(&var.val_);
      if (err != CborNoError && err != CborErrorOutOfMemory) {
        return Error(cbor_error_string(err));
      }
    }
    return std::nullopt;
  }

  template <class ObjectReader>
  std::optional<Error> read_object(const ObjectReader& _object_reader,
                                   const InputObjectType& _obj) const noexcept {
    size_t length = 0;
    auto err = cbor_value_get_map_length(&_obj.val_, &length);
    if (err != CborNoError) {
      return Error(cbor_error_string(err));
    }

    InputVarType var;
    err = cbor_value_enter_container(&_obj.val_, &var.val_);
    if (err != CborNoError) {
      return Error(cbor_error_string(err));
    }

    auto buffer = std::string();

    for (size_t i = 0; i < length; ++i) {
      err = get_string(&var.val_, &buffer);
      if (err != CborNoError) {
        return Error(cbor_error_string(err));
      }
      err = cbor_value_advance(&var.val_);
      if (err != CborNoError) {
        return Error(cbor_error_string(err));
      }
      const auto name = std::string_view(buffer);
      _object_reader.read(name, var);
      cbor_value_advance(&var.val_);
    }

    return std::nullopt;
  }

  template <class T>
  rfl::Result<T> use_custom_constructor(
      const InputVarType& _var) const noexcept {
    return T::from_cbor_obj(_var);
  }

 private:
  CborError get_bytestring(const CborValue* _ptr,
                           rfl::Bytestring* _str) const noexcept;

  CborError get_string(const CborValue* _ptr, std::string* _str) const noexcept;
};

}  // namespace cbor
}  // namespace rfl

#endif
