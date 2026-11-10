#pragma once
#include <iiGUI/NativeWidgets.h>
#include <algorithm>
#include <cstddef>
#include <string_view>
#include <utility>
namespace ii::gui {
// An editor composes a replaceable field; text, selection and rendering remain
// owned by that field and its model. Configuration is supplied by the app.
template<class Policy>
class BasicNativeTextEditor final {
    using Field=typename Policy::Field;
    using Configuration=typename Policy::Configuration;
    using Callbacks=typename Policy::Callbacks;
    using Clipboard=typename Policy::Clipboard;
    using Snapshot=typename Policy::Snapshot;
    Field field_;
    explicit BasicNativeTextEditor(Field field):field_(std::move(field)){}
    [[nodiscard]] static auto field_options(Configuration value){
        value.field.height=(std::max)(value.field.height,value.minimum_height);
        return std::move(value.field);
    }
public:
    [[nodiscard]] static core::Result<BasicNativeTextEditor> create(WidgetBuilder& builder,Widget parent,
            std::string_view id,Configuration options={},Callbacks callbacks={},Clipboard clipboard={}){
        if(!Policy::valid_configuration(options))
            return core::fail(core::ErrorCode::invalid_argument,"Invalid text editor configuration");
        auto field=Field::create(builder,parent,id,field_options(std::move(options)),std::move(callbacks),std::move(clipboard));
        if(!field)return std::unexpected(field.error());
        return BasicNativeTextEditor(std::move(*field));
    }
    [[nodiscard]] Widget root()const{return field_.root();}
    [[nodiscard]] core::Result<Snapshot> snapshot()const{return field_.snapshot();}
    [[nodiscard]] core::Result<void> set_selection(std::size_t anchor,std::size_t caret)const{return field_.set_selection(anchor,caret);}
    [[nodiscard]] core::Result<void> set_options(Configuration options)const{
        if(!Policy::valid_configuration(options))
            return core::fail(core::ErrorCode::invalid_argument,"Invalid text editor configuration");
        return field_.set_options(field_options(std::move(options)));
    }
};
} // namespace ii::gui
