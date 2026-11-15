#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <optional>
namespace ii::gui {
template<class Policy>
class BasicListItemQuantity final {
    using ListItemQuantityConfiguration=typename Policy::ListItemQuantityConfiguration;
public:
    [[nodiscard]] static bool validate(const ListItemQuantityConfiguration& value)noexcept{
        return std::isfinite(value.value)&&std::isfinite(value.minimum)&&
            (std::isfinite(value.maximum)||value.maximum==std::numeric_limits<double>::infinity())&&
            std::isfinite(value.step)&&value.step>0&&value.minimum<=value.maximum&&
            value.value>=value.minimum&&value.value<=value.maximum;
    }
    bool set_configuration(ListItemQuantityConfiguration value)noexcept{
        if(!validate(value))return false;configuration_=value;return true;
    }
    [[nodiscard]] const ListItemQuantityConfiguration& configuration()const noexcept{return configuration_;}
    [[nodiscard]] std::optional<double> step(int direction)noexcept{
        auto& c=configuration_;if(!c.enabled||(direction!=1&&direction!=-1))return std::nullopt;
        const auto target=static_cast<long double>(c.value)+static_cast<long double>(direction)*static_cast<long double>(c.step);
        const auto low=static_cast<long double>(c.minimum);
        const auto high=(std::min)(static_cast<long double>(c.maximum),static_cast<long double>((std::numeric_limits<double>::max)()));
        const auto next=static_cast<double>(std::clamp(target,low,high));if(next==c.value)return std::nullopt;
        c.value=next;return next;
    }
private:
    ListItemQuantityConfiguration configuration_;
};

} // namespace ii::gui
