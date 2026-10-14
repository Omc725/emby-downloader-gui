#pragma once

#include <gtk/gtk.h>
#include <emby/engine.hpp>
#include <memory>
#include "search_view.hpp"
#include "queue_view.hpp"

class AppWindow {
public:
    static GtkWidget* create(GtkApplication* app);

private:
    explicit AppWindow(GtkApplication* app);
    ~AppWindow() = default;

    void buildUi(GtkApplication* app);
    void populateProfiles();
    void updateConnectionStatus();

    Emby::EmbyEngine m_engine;
    GtkWidget* m_window = nullptr;
    GtkWidget* m_profileCombo = nullptr;
    GtkWidget* m_statusLabel = nullptr;
    GtkWidget* m_notebook = nullptr;

    std::unique_ptr<SearchView> m_searchView;
    std::unique_ptr<QueueView> m_queueView;
};
