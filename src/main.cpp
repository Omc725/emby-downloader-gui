#include <gtk/gtk.h>
#include "app_window.hpp"

static void onAppActivate(GtkApplication* app, gpointer /*userData*/) {
    GtkWidget* win = AppWindow::create(app);
    gtk_window_present(GTK_WINDOW(win));
}

int main(int argc, char* argv[]) {
    GtkApplication* app = gtk_application_new(
        "io.github.omc725.emby_downloader_gui",
        G_APPLICATION_DEFAULT_FLAGS
    );

    g_signal_connect(app, "activate", G_CALLBACK(onAppActivate), NULL);

    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);

    return status;
}
