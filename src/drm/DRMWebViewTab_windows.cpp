#include "drm/DRMWebViewTab.h"

#if defined(_WIN32)

#include <windows.h>
#include <string>

#if defined(__has_include)
#if __has_include(<WebView2.h>)
#define ULTRALIGHT_HAS_WEBVIEW2 1
#else
#define ULTRALIGHT_HAS_WEBVIEW2 0
#endif
#else
#define ULTRALIGHT_HAS_WEBVIEW2 0
#endif

#if ULTRALIGHT_HAS_WEBVIEW2
#include <wrl.h>
#include <wrl/event.h>
#include <combaseapi.h>
#include <WebView2.h>
#endif

namespace drm
{
#if ULTRALIGHT_HAS_WEBVIEW2
    namespace
    {
        std::wstring ToWide(const std::string &value)
        {
            if (value.empty())
                return std::wstring();
            int len = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, nullptr, 0);
            if (len <= 0)
                return std::wstring();
            std::wstring result(static_cast<size_t>(len), L'\0');
            MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, result.data(), len);
            if (!result.empty() && result.back() == L'\0')
                result.pop_back();
            return result;
        }

        std::string FromWide(const std::wstring &value)
        {
            if (value.empty())
                return std::string();
            int len = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
            if (len <= 0)
                return std::string();
            std::string result(static_cast<size_t>(len), '\0');
            WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, result.data(), len, nullptr, nullptr);
            if (!result.empty() && result.back() == '\0')
                result.pop_back();
            return result;
        }
    }

    class DRMWebViewTabWindows : public DRMWebViewTab
    {
    public:
        DRMWebViewTabWindows(uint64_t id, const DRMWebViewConfig &config, const DRMWebViewCallbacks &callbacks)
            : DRMWebViewTab(id, config, callbacks)
        {
            parent_hwnd_ = static_cast<HWND>(config.parent_window);
            Initialize();
        }

        ~DRMWebViewTabWindows() override
        {
            Close();
        }

        void LoadURL(const std::string &url) override
        {
            current_url_ = url;
            if (!webview_)
                return;
            webview_->Navigate(ToWide(url).c_str());
        }

        void GoBack() override
        {
            if (webview_ && webview_->get_CanGoBack(&can_go_back_) == S_OK && can_go_back_)
                webview_->GoBack();
        }

        void GoForward() override
        {
            if (webview_ && webview_->get_CanGoForward(&can_go_forward_) == S_OK && can_go_forward_)
                webview_->GoForward();
        }

        void Reload() override
        {
            if (webview_)
                webview_->Reload();
        }

        void Stop() override
        {
            if (webview_)
                webview_->Stop();
        }

        void Focus() override
        {
            if (controller_)
                controller_->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
        }

        void Resize(uint32_t width, uint32_t height, uint32_t offset_x, uint32_t offset_y) override
        {
            if (!controller_)
                return;
            RECT bounds = {static_cast<LONG>(offset_x), static_cast<LONG>(offset_y), static_cast<LONG>(offset_x + width), static_cast<LONG>(offset_y + height)};
            controller_->put_Bounds(bounds);
        }

        void Show() override
        {
            if (controller_)
                controller_->put_IsVisible(TRUE);
        }

        void Hide() override
        {
            if (controller_)
                controller_->put_IsVisible(FALSE);
        }

        void Close() override
        {
            if (webview_)
            {
                if (title_handler_registered_)
                    webview_->remove_DocumentTitleChanged(title_token_);
                if (source_handler_registered_)
                    webview_->remove_SourceChanged(source_token_);
                if (nav_completed_registered_)
                    webview_->remove_NavigationCompleted(nav_completed_token_);
                if (nav_starting_registered_)
                    webview_->remove_NavigationStarting(nav_starting_token_);
            }
            if (controller_)
            {
                controller_->Close();
                controller_.Reset();
            }
            webview_.Reset();
            environment_.Reset();
        }

        std::string GetTitle() const override { return current_title_; }
        std::string GetURL() const override { return current_url_; }
        bool CanGoBack() const override { return can_go_back_; }
        bool CanGoForward() const override { return can_go_forward_; }

    private:
        void Initialize()
        {
            if (!parent_hwnd_)
                return;

            HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(nullptr, nullptr, nullptr,
                                                                  Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
                                                                      [this](HRESULT result, ICoreWebView2Environment *env) -> HRESULT
                                                                      {
                                                                          if (FAILED(result) || !env)
                                                                              return result;
                                                                          environment_ = env;
                                                                          return env->CreateCoreWebView2Controller(parent_hwnd_,
                                                                                                                   Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                                                                                                                       [this](HRESULT controller_result, ICoreWebView2Controller *controller) -> HRESULT
                                                                                                                       {
                                                                                                                           if (FAILED(controller_result) || !controller)
                                                                                                                               return controller_result;
                                                                                                                           controller_ = controller;
                                                                                                                           controller_->get_CoreWebView2(&webview_);
                                                                                                                           controller_->put_IsVisible(TRUE);
                                                                                                                           SetupEvents();
                                                                                                                           if (!current_url_.empty())
                                                                                                                               webview_->Navigate(ToWide(current_url_).c_str());
                                                                                                                           return S_OK;
                                                                                                                       })
                                                                                                                       .Get());
                                                                      })
                                                                      .Get());
            (void)hr;
        }

        void SetupEvents()
        {
            if (!webview_)
                return;

            if (SUCCEEDED(webview_->add_DocumentTitleChanged(
                    Microsoft::WRL::Callback<ICoreWebView2DocumentTitleChangedEventHandler>(
                        [this](ICoreWebView2 *sender, IUnknown *) -> HRESULT
                        {
                            LPWSTR title = nullptr;
                            if (SUCCEEDED(sender->get_DocumentTitle(&title)) && title)
                            {
                                current_title_ = FromWide(title);
                                CoTaskMemFree(title);
                                if (callbacks_.on_title_changed)
                                    callbacks_.on_title_changed(id_, current_title_);
                            }
                            return S_OK;
                        })
                        .Get(),
                    &title_token_)))
            {
                title_handler_registered_ = true;
            }

            if (SUCCEEDED(webview_->add_SourceChanged(
                    Microsoft::WRL::Callback<ICoreWebView2SourceChangedEventHandler>(
                        [this](ICoreWebView2 *sender, ICoreWebView2SourceChangedEventArgs *) -> HRESULT
                        {
                            LPWSTR source = nullptr;
                            if (SUCCEEDED(sender->get_Source(&source)) && source)
                            {
                                current_url_ = FromWide(source);
                                CoTaskMemFree(source);
                                if (callbacks_.on_url_changed)
                                    callbacks_.on_url_changed(id_, current_url_);
                            }
                            sender->get_CanGoBack(&can_go_back_);
                            sender->get_CanGoForward(&can_go_forward_);
                            if (callbacks_.on_navigation_state)
                                callbacks_.on_navigation_state(id_, can_go_back_, can_go_forward_);
                            return S_OK;
                        })
                        .Get(),
                    &source_token_)))
            {
                source_handler_registered_ = true;
            }

            if (SUCCEEDED(webview_->add_NavigationStarting(
                    Microsoft::WRL::Callback<ICoreWebView2NavigationStartingEventHandler>(
                        [this](ICoreWebView2 *, ICoreWebView2NavigationStartingEventArgs *) -> HRESULT
                        {
                            if (callbacks_.on_loading_state)
                                callbacks_.on_loading_state(id_, true);
                            return S_OK;
                        })
                        .Get(),
                    &nav_starting_token_)))
            {
                nav_starting_registered_ = true;
            }

            if (SUCCEEDED(webview_->add_NavigationCompleted(
                    Microsoft::WRL::Callback<ICoreWebView2NavigationCompletedEventHandler>(
                        [this](ICoreWebView2 *sender, ICoreWebView2NavigationCompletedEventArgs *args) -> HRESULT
                        {
                            BOOL is_success = FALSE;
                            if (args)
                                args->get_IsSuccess(&is_success);
                            sender->get_CanGoBack(&can_go_back_);
                            sender->get_CanGoForward(&can_go_forward_);
                            if (callbacks_.on_navigation_state)
                                callbacks_.on_navigation_state(id_, can_go_back_, can_go_forward_);
                            if (callbacks_.on_loading_state)
                                callbacks_.on_loading_state(id_, false);
                            return S_OK;
                        })
                        .Get(),
                    &nav_completed_token_)))
            {
                nav_completed_registered_ = true;
            }
        }

        HWND parent_hwnd_ = nullptr;
        Microsoft::WRL::ComPtr<ICoreWebView2Environment> environment_;
        Microsoft::WRL::ComPtr<ICoreWebView2Controller> controller_;
        Microsoft::WRL::ComPtr<ICoreWebView2> webview_;
        EventRegistrationToken title_token_{};
        EventRegistrationToken source_token_{};
        EventRegistrationToken nav_starting_token_{};
        EventRegistrationToken nav_completed_token_{};
        bool title_handler_registered_ = false;
        bool source_handler_registered_ = false;
        bool nav_starting_registered_ = false;
        bool nav_completed_registered_ = false;
        BOOL can_go_back_ = FALSE;
        BOOL can_go_forward_ = FALSE;
        std::string current_title_ = "DRM WebView";
        std::string current_url_;
    };
#else

    class DRMWebViewTabWindows : public DRMWebViewTab
    {
    public:
        DRMWebViewTabWindows(uint64_t id, const DRMWebViewConfig &config, const DRMWebViewCallbacks &callbacks)
            : DRMWebViewTab(id, config, callbacks) {}

        void LoadURL(const std::string &) override {}
        void GoBack() override {}
        void GoForward() override {}
        void Reload() override {}
        void Stop() override {}
        void Focus() override {}
        void Resize(uint32_t, uint32_t, uint32_t, uint32_t) override {}
        void Show() override {}
        void Hide() override {}
        void Close() override {}
        std::string GetTitle() const override { return "DRM WebView"; }
        std::string GetURL() const override { return std::string(); }
        bool CanGoBack() const override { return false; }
        bool CanGoForward() const override { return false; }
    };

#endif // ULTRALIGHT_HAS_WEBVIEW2

    std::unique_ptr<DRMWebViewTab> CreatePlatformWebViewTab(uint64_t id,
                                                            const DRMWebViewConfig &config,
                                                            DRMWebViewCallbacks callbacks)
    {
#if ULTRALIGHT_HAS_WEBVIEW2
        return std::make_unique<DRMWebViewTabWindows>(id, config, callbacks);
#else
        (void)id;
        (void)config;
        (void)callbacks;
        return nullptr;
#endif
    }

} // namespace drm

#endif
