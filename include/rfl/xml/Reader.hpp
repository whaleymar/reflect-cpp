#ifndef RFL_XML_READER_HPP_
#define RFL_XML_READER_HPP_

#include <exception>
#include <optional>
#include <pugixml.hpp>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>

#include "../Result.hpp"
#include "../always_false.hpp"
#include "../parsing/is_view_reader.hpp"

namespace rfl {
namespace xml {

struct Reader {
  struct XMLInputArray {
    XMLInputArray(pugi::xml_node _node) : node_(_node) {}
    pugi::xml_node node_;
  };

  struct XMLInputObject {
    XMLInputObject(pugi::xml_node _node) : node_(_node) {}
    pugi::xml_node node_;
  };

  struct XMLInputVar {
    XMLInputVar() : node_or_attribute_(pugi::xml_node()) {}
    XMLInputVar(pugi::xml_attribute _attr) : node_or_attribute_(_attr) {}
    XMLInputVar(pugi::xml_node _node) : node_or_attribute_(_node) {}
    std::variant<pugi::xml_node, pugi::xml_attribute> node_or_attribute_;
  };

  using InputArrayType = XMLInputArray;
  using InputObjectType = XMLInputObject;
  using InputVarType = XMLInputVar;

  // TODO
  template <class T>
  static constexpr bool has_custom_constructor = false;

  rfl::Result<InputVarType> get_field_from_array(
      const size_t _idx, const InputArrayType& _arr) const noexcept;

  rfl::Result<InputVarType> get_field_from_object(
      const std::string& _name, const InputObjectType _obj) const noexcept;

  bool is_empty(const InputVarType _var) const noexcept;

  template <class T>
  rfl::Result<T> to_basic_type(const InputVarType _var) const noexcept {
    const auto get_value = [](const auto& _n) -> std::string {
      using Type = std::remove_cvref_t<decltype(_n)>;
      if constexpr (std::is_same<Type, pugi::xml_node>()) {
        return std::string(_n.child_value());
      } else {
        return std::string(_n.value());
      }
    };

    if constexpr (std::is_same<std::remove_cvref_t<T>, std::string>()) {
      return std::visit(get_value, _var.node_or_attribute_);
    } else if constexpr (std::is_same<std::remove_cvref_t<T>, bool>()) {
      return std::visit(get_value, _var.node_or_attribute_) == "true";
    } else if constexpr (std::is_floating_point<std::remove_cvref_t<T>>()) {
      const auto str = std::visit(get_value, _var.node_or_attribute_);
      return static_cast<T>(std::stod(str));
    } else if constexpr (std::is_integral<std::remove_cvref_t<T>>()) {
      const auto str = std::visit(get_value, _var.node_or_attribute_);
      return static_cast<T>(std::stoi(str));
    } else {
      static_assert(rfl::always_false_v<T>, "Unsupported type.");
    }
  }

  rfl::Result<InputArrayType> to_array(const InputVarType _var) const noexcept;

  template <class ArrayReader>
  std::optional<Error> read_array(const ArrayReader& _array_reader,
                                  const InputArrayType& _arr) const noexcept {
    const auto name = _arr.node_.name();
    for (auto node = _arr.node_; node; node = node.next_sibling(name)) {
      const auto err = _array_reader.read(InputVarType(node));
      if (err) {
        return err;
      }
    }
    return std::nullopt;
  }

  template <class ObjectReader>
  std::optional<Error> read_object(const ObjectReader& _object_reader,
                                   const InputObjectType& _obj) const noexcept {
    for (auto child = _obj.node_.first_child(); child;
         child = child.next_sibling()) {
      _object_reader.read(std::string_view(child.name()), InputVarType(child));
    }

    for (auto attr = _obj.node_.first_attribute(); attr;
         attr = attr.next_attribute()) {
      _object_reader.read(std::string_view(attr.name()), InputVarType(attr));
    }

    if constexpr (parsing::is_view_reader_v<ObjectReader>) {
      _object_reader.read(std::string_view("xml_content"),
                          InputVarType(_obj.node_));
    }

    return std::nullopt;
  }

  rfl::Result<InputObjectType> to_object(
      const InputVarType _var) const noexcept;

  template <class T>
  rfl::Result<T> use_custom_constructor(
      const InputVarType _var) const noexcept {
    return rfl::Error("TODO");
  }
};

}  // namespace xml
}  // namespace rfl

#endif
