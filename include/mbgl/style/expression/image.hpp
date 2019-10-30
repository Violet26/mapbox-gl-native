#pragma once

#include <mbgl/style/conversion.hpp>
#include <mbgl/util/color.hpp>
#include <mbgl/util/optional.hpp>

#include <vector>
#include <string>

namespace mbgl {
namespace style {
namespace expression {

class Image {
public:
Image(std::string imageID, bool available);
    bool operator==(const Image& ) const;
    std::string toString() const;
    const std::string& id() const;

private:
    std::string imageID;
    bool available;
};

} // namespace expression

namespace conversion {
using mbgl::style::expression::Image;

template <>
struct Converter<Image> {
public:
    optional<Image> operator()(const Convertible& value, Error& error) const;
};

} // namespace conversion

} // namespace style
} // namespace mbgl
