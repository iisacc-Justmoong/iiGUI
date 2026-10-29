#pragma once
#include <iiGUI/Export.h>
#include <iiSDKCore/Core/Property/ObservableSubscription.h>
#include <iiSDKCore/Core/Result/Result.h>
#include <functional>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ii::gui {
struct FontFace final {
    std::string family;
    int weight=400;
    std::string style="Regular";
    friend bool operator==(const FontFace&,const FontFace&)=default;
};
struct FontResource final {std::string_view file,family,style;int weight=400;};
struct FontLoad final {std::string file;bool loaded=false;std::string error;};
struct FontLoadReport final {std::vector<FontLoad> resources;};
// Platform font discovery and registration, independent of windows/controls.
// A fake provider can replace the OS boundary without replacing policy logic.
class IIGUI_EXPORT FontProvider {
public:
    virtual ~FontProvider()=default;
    [[nodiscard]] virtual std::vector<std::string> families()const=0;
    [[nodiscard]] virtual std::string system_family()const=0;
    [[nodiscard]] virtual std::string resolve(const FontFace&)const=0;
    [[nodiscard]] virtual FontLoadReport load(std::span<const FontResource>)=0;
};
// Owner-thread application font state. Native glyph/layout implementations use
// the same registry as policy consumers. Queries do not create a window.
class IIGUI_EXPORT FontDatabase final {
    struct Impl;
public:
    FontDatabase(std::shared_ptr<FontProvider>,std::optional<FontFace> application);
    ~FontDatabase();
    FontDatabase(const FontDatabase&)=delete;
    FontDatabase& operator=(const FontDatabase&)=delete;
    [[nodiscard]] static std::shared_ptr<FontDatabase> application();
    [[nodiscard]] static std::shared_ptr<FontDatabase> native(std::string resource_directory={});
    [[nodiscard]] static std::span<const FontResource> bundled_fonts()noexcept;
    [[nodiscard]] static std::string fold_name(std::string_view);
    [[nodiscard]] std::vector<std::string> families()const;
    [[nodiscard]] std::string system_family()const;
    [[nodiscard]] std::optional<FontFace> application_font()const;
    [[nodiscard]] bool family_available(std::string_view)const;
    [[nodiscard]] std::string resolve(const FontFace&)const;
    [[nodiscard]] std::string resolve_family(std::string_view)const;
    [[nodiscard]] std::vector<std::string> family_stack(std::string_view)const;
    [[nodiscard]] std::uint64_t revision()const;
    [[nodiscard]] core::Result<bool> refresh();
    [[nodiscard]] core::Result<FontLoadReport> load_bundled_fonts();
    [[nodiscard]] core::Result<bool> set_application_font(FontFace);
    [[nodiscard]] core::Result<bool> install_substitution(std::string target,std::string replacement);
    [[nodiscard]] std::vector<std::string> substitutions(std::string_view)const;
    [[nodiscard]] core::ObservableSubscription observe(std::function<void()>)const;
private:
    std::shared_ptr<Impl> impl_;
};
}
