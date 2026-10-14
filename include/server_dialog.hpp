#pragma once

#include <gtk/gtk.h>
#include <emby/engine.hpp>

class ServerDialog {
public:
    static bool show(GtkWindow* parent, Emby::EmbyEngine& engine);

private:
    ServerDialog(GtkWindow* parent, Emby::EmbyEngine& engine);
    ~ServerDialog() = default;

    bool run();

    GtkWindow* m_parent;
    Emby::EmbyEngine& m_engine;

    GtkWidget* m_dialog = nullptr;
    GtkWidget* m_urlEntry = nullptr;
    GtkWidget* m_portEntry = nullptr;
    GtkWidget* m_prefixEntry = nullptr;
    GtkWidget* m_userEntry = nullptr;
    GtkWidget* m_passEntry = nullptr;
    GtkWidget* m_errorLabel = nullptr;
};
