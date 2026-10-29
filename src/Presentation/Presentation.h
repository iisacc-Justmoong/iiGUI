#pragma once
#include <iiGUI/Graphics.h>
#include <iiGUI/Export.h>
#include <iiSDKCore/Core/Property/ObservableSubscription.h>
#include <iiSDKCore/Core/Result/Result.h>
#include <iiSDKCore/Runtime/Execution.h>
#include <functional>
#include <memory>

namespace ii::gui {
enum class PresentationKind { queued, closed };
struct PresentationEvent final {
    PresentationKind kind = PresentationKind::queued;
    runtime::Clock::time_point timestamp{};
    std::uint64_t sequence = 0;
    GraphicsApi api = GraphicsApi::none;
    std::string error;
    friend bool operator==(const PresentationEvent&, const PresentationEvent&) = default;
};
// Owner-thread read boundary. The lease detaches its callback; it never owns a window.
class IIGUI_EXPORT PresentationSource {
public:
    virtual ~PresentationSource() = default;
    [[nodiscard]] virtual bool closed() const = 0;
    [[nodiscard]] virtual core::Result<core::ObservableSubscription> observe(
        std::function<void(const PresentationEvent&)>) = 0;
};
// A renderer owns this producer. Host exposes only PresentationSource to consumers.
// Events describe accepted presentation submissions, not scanout or GPU completion.
class IIGUI_EXPORT PresentationStream final : public PresentationSource {
public:
    PresentationStream();
    explicit PresentationStream(std::function<runtime::Submission(runtime::Task)> deferred);
    ~PresentationStream() override;
    PresentationStream(const PresentationStream&) = delete;
    PresentationStream& operator=(const PresentationStream&) = delete;
    [[nodiscard]] bool closed() const override;
    [[nodiscard]] core::Result<core::ObservableSubscription> observe(
        std::function<void(const PresentationEvent&)>) override;
    [[nodiscard]] bool has_observers() const noexcept;
    [[nodiscard]] std::uint64_t sequence() const;
    [[nodiscard]] std::uint64_t observer_failures() const;
    [[nodiscard]] core::Result<void> queued(runtime::Clock::time_point, GraphicsApi);
    void close() noexcept;
private:
    struct Impl;
    std::shared_ptr<Impl> impl_;
};
}
