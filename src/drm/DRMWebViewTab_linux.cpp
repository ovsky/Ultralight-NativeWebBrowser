#include "drm/DRMWebViewTab.h"

#if defined(__linux__)

#include <gtk/gtk.h>
#include <webkit2/webkit2.h>

namespace drm
{
    namespace
    {
        std::string ToStdString(const gchar *value)
        {
            if (!value)
                return {};
            return std::string(value);
        }
    }

    class DRMWebViewTabLinux : public DRMWebViewTab
    {
    public:
        DRMWebViewTabLinux(uint64_t id, const DRMWebViewConfig &config, const DRMWebViewCallbacks &callbacks)
            : DRMWebViewTab(id, config, callbacks)
        {
            parent_window_ = static_cast<GtkWindow *>(config.parent_window);
            CreateWidgets();
        }

        ~DRMWebViewTabLinux() override
        {
            Close();
        }

        void LoadURL(const std::string &url) override
        {
            current_url_ = url;
            if (!web_view_)
                return;
            webkit_web_view_load_uri(WEBKIT_WEB_VIEW(web_view_), url.c_str());
        }

        void GoBack() override
        {
            if (WEBKIT_WEB_VIEW(web_view_) && webkit_web_view_can_go_back(WEBKIT_WEB_VIEW(web_view_)))
                webkit_web_view_go_back(WEBKIT_WEB_VIEW(web_view_));
        }

        void GoForward() override
        {
            if (WEBKIT_WEB_VIEW(web_view_) && webkit_web_view_can_go_forward(WEBKIT_WEB_VIEW(web_view_)))
                webkit_web_view_go_forward(WEBKIT_WEB_VIEW(web_view_));
        }

        void Reload() override
        {
            if (WEBKIT_WEB_VIEW(web_view_))
                webkit_web_view_reload(WEBKIT_WEB_VIEW(web_view_));
        }

        void Stop() override
        {
            if (WEBKIT_WEB_VIEW(web_view_))
                webkit_web_view_stop_loading(WEBKIT_WEB_VIEW(web_view_));
        }

        void Focus() override
        {
            if (web_view_)
                gtk_widget_grab_focus(web_view_);
        }

        void Blur() override
        {
            // Remove focus from WebView by focusing parent
            if (parent_window_)
                gtk_window_present(GTK_WINDOW(parent_window_));
        }

        void Resize(uint32_t width, uint32_t height, uint32_t offset_x, uint32_t offset_y) override
        {
            if (!container_)
                return;
            gtk_fixed_move(GTK_FIXED(container_), web_view_, offset_x, offset_y);
            gtk_widget_set_size_request(web_view_, width, height);
        }

        void Show() override
        {
            if (container_)
                gtk_widget_show(container_);
            if (web_view_)
                gtk_widget_show(web_view_);
        }

        void Hide() override
        {
            if (container_)
                gtk_widget_hide(container_);
        }

        void Close() override
        {
            if (web_view_)
            {
                g_signal_handlers_disconnect_by_data(web_view_, this);
                gtk_widget_destroy(web_view_);
                web_view_ = nullptr;
            }
            if (container_)
            {
                gtk_widget_destroy(container_);
                container_ = nullptr;
            }
        }

        std::string GetTitle() const override { return current_title_; }
        std::string GetURL() const override { return current_url_; }
        bool CanGoBack() const override { return web_view_ && webkit_web_view_can_go_back(WEBKIT_WEB_VIEW(web_view_)); }
        bool CanGoForward() const override { return web_view_ && webkit_web_view_can_go_forward(WEBKIT_WEB_VIEW(web_view_)); }

        void HandleTitleChanged()
        {
            if (!web_view_)
                return;
            const gchar *title = webkit_web_view_get_title(WEBKIT_WEB_VIEW(web_view_));
            current_title_ = ToStdString(title);
            if (callbacks_.on_title_changed)
                callbacks_.on_title_changed(id_, current_title_);
        }

        void HandleURIChanged()
        {
            if (!web_view_)
                return;
            const gchar *uri = webkit_web_view_get_uri(WEBKIT_WEB_VIEW(web_view_));
            current_url_ = ToStdString(uri);
            if (callbacks_.on_url_changed)
                callbacks_.on_url_changed(id_, current_url_);
            if (callbacks_.on_navigation_state)
                callbacks_.on_navigation_state(id_, CanGoBack(), CanGoForward());
        }

        void HandleLoadChanged(WebKitLoadEvent load_event)
        {
            if (!callbacks_.on_loading_state)
                return;
            bool loading = load_event == WEBKIT_LOAD_STARTED || load_event == WEBKIT_LOAD_REDIRECTED || load_event == WEBKIT_LOAD_COMMITTED;
            if (load_event == WEBKIT_LOAD_FINISHED)
                loading = false;
            callbacks_.on_loading_state(id_, loading);
        }

    private:
        static void NotifyTitle(GtkWidget *, GParamSpec *, gpointer user_data)
        {
            reinterpret_cast<DRMWebViewTabLinux *>(user_data)->HandleTitleChanged();
        }

        static void NotifyURI(GtkWidget *, GParamSpec *, gpointer user_data)
        {
            reinterpret_cast<DRMWebViewTabLinux *>(user_data)->HandleURIChanged();
        }

        static void LoadChanged(WebKitWebView *, WebKitLoadEvent load_event, gpointer user_data)
        {
            reinterpret_cast<DRMWebViewTabLinux *>(user_data)->HandleLoadChanged(load_event);
        }

        void CreateWidgets()
        {
            if (!parent_window_)
                return;
            GtkWidget *parent_widget = GTK_WIDGET(parent_window_);
            GtkWidget *root = gtk_bin_get_child(GTK_BIN(parent_widget));
            if (!root)
                return;
            container_ = gtk_fixed_new();
            gtk_container_add(GTK_CONTAINER(root), container_);
            web_view_ = webkit_web_view_new();
            gtk_container_add(GTK_CONTAINER(container_), web_view_);
            g_signal_connect(web_view_, "notify::title", G_CALLBACK(NotifyTitle), this);
            g_signal_connect(web_view_, "notify::uri", G_CALLBACK(NotifyURI), this);
            g_signal_connect(web_view_, "load-changed", G_CALLBACK(LoadChanged), this);
            gtk_widget_show_all(container_);
        }

        GtkWindow *parent_window_ = nullptr;
        GtkWidget *container_ = nullptr;
        GtkWidget *web_view_ = nullptr;
        std::string current_title_ = "DRM WebView";
        std::string current_url_;
    };

    std::unique_ptr<DRMWebViewTab> CreatePlatformWebViewTab(uint64_t id,
                                                            const DRMWebViewConfig &config,
                                                            DRMWebViewCallbacks callbacks)
    {
        return std::make_unique<DRMWebViewTabLinux>(id, config, callbacks);
    }

    void PrewarmWebViewEnvironment()
    {
        // GTK/WebKitGTK doesn't need pre-warming - widgets are created on demand
        // and are fast to initialize
    }

} // namespace drm

#endif
