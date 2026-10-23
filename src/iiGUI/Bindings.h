#pragma once
#include "NativeWidgets.h"
#include <iiSDKCore/Core/Property/Observable.h>
#include <iiSDKCore/Core/Text/Number.h>
#include <stdexcept>
#include <type_traits>

namespace ii::gui {
template<class T>std::string binding_text(const T& value){
    if constexpr(std::convertible_to<const T&,std::string_view>)return std::string(std::string_view(value));
    else if constexpr(std::same_as<T,bool>)return value?"true":"false";
    else if constexpr(std::floating_point<T>)return core::format_decimal(value);
    else return std::to_string(value);
}
// Retain this with the widget builder (including each popup activation). Its
// subscriptions end with that widget generation, independently of backend work.
class BindingScope final {
public:
    template<class T,class Setter>
    [[nodiscard]] core::Result<void> bind(Widget widget,core::Observable<T>& source,Setter setter){
        if(auto initialized=setter(widget,source.get());!initialized)return initialized;
        subscriptions_.push_back(source.observe([widget,setter](const T& value,std::uint64_t){
            const auto changed=setter(widget,value);
            if(!changed&&changed.error().code!=core::ErrorCode::closed)
                throw std::runtime_error(changed.error().message);
        }));
        return {};
    }
private:
    std::vector<core::ObservableSubscription> subscriptions_;
};
template<class T>struct IsBindingObservable:std::false_type{};
template<class T>struct IsBindingObservable<core::Observable<T>>:std::true_type{};
// A scoped C++ member may be a plain construction value or a Core Observable.
// shared_ptr::get(), for example, must never be mistaken for an observable.
template<class T>decltype(auto) binding_value(T& source){
    if constexpr(IsBindingObservable<std::remove_cvref_t<T>>::value)return source.get();else return (source);
}
template<class T,class Setter>[[nodiscard]] core::Result<void> bind_member(BindingScope& owner,Widget widget,T& source,Setter setter){
    if constexpr(IsBindingObservable<std::remove_cvref_t<T>>::value)return owner.bind(widget,source,std::move(setter));else {(void)owner;(void)widget;(void)source;(void)setter;return {};}
}
template<class T> [[nodiscard]] core::Result<void> bind_text(BindingScope& owner,Widget widget,core::Observable<T>& source){
    return owner.bind(widget,source,[](Widget value,const T& text){return value.set_text(binding_text(text));});
}
template<class T> [[nodiscard]] core::Result<void> bind_enabled(BindingScope& owner,Widget widget,core::Observable<T>& source){
    return owner.bind(widget,source,[](Widget value,const T& enabled){return value.set_enabled(enabled);});
}
template<class T> [[nodiscard]] core::Result<void> bind_checked(BindingScope& owner,Widget widget,core::Observable<T>& source){
    return owner.bind(widget,source,[](Widget value,const T& checked){return value.set_checked(checked);});
}
template<class T> [[nodiscard]] core::Result<void> bind_value(BindingScope& owner,Widget widget,core::Observable<T>& source){
    return owner.bind(widget,source,[](Widget value,const T& number){return value.set_value(number);});
}
template<class T> [[nodiscard]] core::Result<void> bind_current_index(BindingScope& owner,Widget widget,core::Observable<T>& source){
    return owner.bind(widget,source,[](Widget value,const T& index){return value.set_current_index(int(index));});
}
template<class T> [[nodiscard]] core::Result<void> bind_visible(BindingScope& owner,Widget widget,core::Observable<T>& source){
    return owner.bind(widget,source,[](Widget value,const T& visible){return value.set_visible(visible);});
}
}
