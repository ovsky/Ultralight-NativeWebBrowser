// sidecar_main.cpp
// Minimal sidecar process entry point. This file is a mocked/stubbed CEF wrapper
// that demonstrates how the child process would parse command-line arguments,
// accept a parent window handle, and embed a renderer into that parent.

#include <iostream>
#include <string>
#include <vector>
#include <cstring>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#else
#include <unistd.h>
#ifdef __linux__
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#endif
#endif

static void PrintUsage()
{
    std::cout << "sidecar --url=<url> --parent-window-id=<id> --width=<w> --height=<h>\n";
}

int main(int argc, char **argv)
{
    std::string url;
    uint64_t parentId = 0;
    int width = 800, height = 600;

    for (int i = 1; i < argc; ++i)
    {
        std::string a(argv[i]);
        if (a.rfind("--url=", 0) == 0)
            url = a.substr(6);
        else if (a.rfind("--parent-window-id=", 0) == 0)
            parentId = std::stoull(a.substr(19));
        else if (a.rfind("--width=", 0) == 0)
            width = std::stoi(a.substr(8));
        else if (a.rfind("--height=", 0) == 0)
            height = std::stoi(a.substr(9));
    }

    std::cout << "Sidecar starting. url=" << url << " parentId=" << parentId << " " << width << "x" << height << std::endl;

    // If built with CEF support, initialize and run CEF; otherwise fall back
    // to the lightweight platform stubs that simulate an embedded renderer.
#if defined(HAVE_CEF)

    // Include CEF headers (CEF include directory must be on the compiler include path)
    #include <cef_app.h>
    #include <cef_client.h>
    #include <cef_browser.h>

    // Minimal CefApp implementation for the browser process.
    class SimpleCefApp : public CefApp, public CefBrowserProcessHandler {
    public:
        SimpleCefApp() {}

        CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override { return this; }

        void OnContextInitialized() override {
            // Intentionally empty: browser creation is performed below in main()
        }

    private:
        IMPLEMENT_REFCOUNTING(SimpleCefApp);
    };

    // Minimal CefClient handling lifespan events and shutting down the loop.
    class SimpleClient : public CefClient, public CefLifeSpanHandler {
    public:
        SimpleClient() : browser_(nullptr) {}

        CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }

        void OnAfterCreated(CefRefPtr<CefBrowser> browser) override {
            CEF_REQUIRE_UI_THREAD();
            browser_ = browser;
        }

        bool DoClose(CefRefPtr<CefBrowser> browser) override { return false; }

        void OnBeforeClose(CefRefPtr<CefBrowser> browser) override {
            CEF_REQUIRE_UI_THREAD();
            CefQuitMessageLoop();
        }

    private:
        CefRefPtr<CefBrowser> browser_;
        IMPLEMENT_REFCOUNTING(SimpleClient);
    };

    // Prepare CEF main args
#if defined(_WIN32)
    CefEnableHighDPISupport();
    CefMainArgs main_args(GetModuleHandle(NULL));
#else
    CefMainArgs main_args(argc, argv);
#endif

    CefRefPtr<SimpleCefApp> app(new SimpleCefApp());

    // Run secondary CEF process if necessary
    int exit_code = CefExecuteProcess(main_args, app.get(), nullptr);
    if (exit_code >= 0)
        return exit_code;

    CefSettings settings;
    settings.no_sandbox = true;
    CefInitialize(main_args, settings, app.get(), nullptr);

    // Create a browser as a child of the provided parent window
    CefWindowInfo window_info;
#if defined(_WIN32)
    HWND parent = (HWND)(uintptr_t)parentId;
    window_info.SetAsChild(parent, CefRect(0, 0, width, height));
#else
    void* parent = reinterpret_cast<void*>(static_cast<uintptr_t>(parentId));
    window_info.SetAsChild(parent, CefRect(0, 0, width, height));
#endif

    CefBrowserSettings browser_settings;
    CefRefPtr<SimpleClient> client(new SimpleClient());
    CefBrowserHost::CreateBrowser(window_info, client.get(), url, browser_settings, nullptr);

    CefRunMessageLoop();
    CefShutdown();

    return 0;

#else

#ifdef _WIN32
    HWND parent = (HWND)(uintptr_t)parentId;

    // Create a simple child window to simulate renderer content. We create a static control that fills the parent.
    HINSTANCE hinst = GetModuleHandle(NULL);
    const char *clsName = "SidecarChildClass";

    WNDCLASSA wc = {};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = hinst;
    wc.lpszClassName = clsName;
    RegisterClassA(&wc);

    HWND hwnd = CreateWindowExA(0, clsName, "SidecarChild", WS_CHILD | WS_VISIBLE,
                                0, 0, width, height, parent, NULL, hinst, NULL);

    // In a real CEF-based sidecar, you'd tell CEF to render into `parent` using
    // CefWindowInfo::SetAsChild((HWND)parent, rect) and let CEF create and manage
    // the browser child window. The child process must run a message loop.

    // Listen for WM_COPYDATA messages to receive resize commands from the parent
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        if (msg.message == WM_COPYDATA)
        {
            COPYDATASTRUCT *cds = (COPYDATASTRUCT *)msg.lParam;
            if (cds && cds->cbData == sizeof(int) * 4)
            {
                int *p = (int *)cds->lpData;
                int x = p[0];
                int y = p[1];
                int w = p[2];
                int h = p[3];
                // Resize our child window to match parent client area
                SetWindowPos(hwnd, NULL, x, y, w, h, SWP_NOZORDER | SWP_SHOWWINDOW);
            }
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

#elif __linux__
    // On Linux/X11 we assume parentId is an X Window (Window type). We'll attempt to reparent
    // our own created window into it or set our rendering to that parent window.
    Display *dpy = XOpenDisplay(NULL);
    if (!dpy)
    {
        std::cerr << "Failed to open X display" << std::endl;
        return 1;
    }
    Window parent = (Window)parentId;
    int screen = DefaultScreen(dpy);

    Window win = XCreateSimpleWindow(dpy, RootWindow(dpy, screen), 0, 0, width, height, 0,
                                     BlackPixel(dpy, screen), WhitePixel(dpy, screen));

    // Reparent into the provided parent window
    XReparentWindow(dpy, win, parent, 0, 0);
    XMapWindow(dpy, win);
    XFlush(dpy);

    // In real CEF use: CefWindowInfo::SetAsChild((void*)parentId, CefRect(..))

    // Listen for ClientMessage resize events (the parent may send a ClientMessage)
    XSelectInput(dpy, win, StructureNotifyMask | ExposureMask);
    XEvent ev;
    while (true)
    {
        XNextEvent(dpy, &ev);
        if (ev.type == ConfigureNotify)
        {
            // Reconfigure size if needed
        }
        // Add a small sleep to avoid tight loop if no messages
    }

#else
    // macOS: Cocoa-based embedding would be used. Here we just print and exit.
    std::cout << "macOS sidecar would initialize Cocoa/CEF and embed into NSView/NSWindow with id=" << parentId << std::endl;
#endif

    return 0;
}
