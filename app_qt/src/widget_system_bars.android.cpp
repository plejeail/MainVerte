#include "widget_system_bars.android.h"

#include <QCoreApplication>
#include <QDebug>
#include <QGuiApplication>
#include <QJniObject>
#include <QPointer>
#include <QWindow>

#include "app_settings.h"
#include "mvlog/mvlog.h"


namespace {

void use_light_system_icons(bool dark_background) {
    // View access belongs to the Android UI thread; only the chosen appearance crosses threads.
    QNativeInterface::QAndroidApplication::runOnAndroidMainThread([dark_background] {
        const QJniObject activity = QNativeInterface::QAndroidApplication::context();
        const auto window = activity.callObjectMethod("getWindow", "()Landroid/view/Window;");
        if (!window.isValid()) {
            mvlog::wrn("Cannot update system bars: no Android window");
            return;
        }

        const bool modern = QNativeInterface::QAndroidApplication::sdkVersion() >= 30;
        const auto target = modern
            ? window.callObjectMethod("getInsetsController", "()Landroid/view/WindowInsetsController;")
            : window.callObjectMethod("getDecorView", "()Landroid/view/View;");
        if (!target.isValid()) {
            mvlog::wrn("Cannot update system bars: no appearance controller");
            return;
        }

        // Android's LIGHT flags describe the background and select DARK icons.
        // Navigation bar appearance is supported from API 26 onward.
        const jint navigation_mask = QNativeInterface::QAndroidApplication::sdkVersion() >= 26 ? 16 : 0;
        const jint mask = (modern ? 8 : 8192) | navigation_mask;
        const jint current = target.callMethod<jint>(modern ? "getSystemBarsAppearance" : "getSystemUiVisibility");
        const jint appearance = dark_background ? 0 : mask;
        if (modern) {
            target.callMethod<void>("setSystemBarsAppearance", "(II)V", appearance, mask);
        } else {
            target.callMethod<void>("setSystemUiVisibility", "(I)V", (current & ~mask) | appearance);
        }
    });
}

}  // namespace

void configure_app_system_bars(AppSettings& settings, QWindow* setup_window) {
    const QPointer<QWindow> wizard(setup_window);
    const auto update = [&settings, wizard] {
        use_light_system_icons(settings.darkTheme() || (wizard && wizard->isVisible()));
    };
    QObject::connect(&settings, &AppSettings::themeChanged, &settings, update);
    // Queued so Qt finishes applying the new window's system UI flags first.
    QObject::connect(qGuiApp, &QGuiApplication::focusWindowChanged, &settings, update, Qt::QueuedConnection);
    QObject::connect(qGuiApp, &QGuiApplication::applicationStateChanged, &settings,
                     [update](Qt::ApplicationState state) {
        if (state == Qt::ApplicationActive) {
            update();
        }
    });
    if (setup_window) {
        QObject::connect(setup_window, &QWindow::visibleChanged, &settings, update, Qt::QueuedConnection);
        QObject::connect(setup_window, &QObject::destroyed, &settings, update, Qt::QueuedConnection);
    }

    update();
}
