#pragma once

#include <gtk/gtk.h>
#include <emby/engine.hpp>

class QueueView {
public:
    explicit QueueView(Emby::EmbyEngine& engine);
    ~QueueView();

    GtkWidget* getWidget() const { return m_container; }
    void refresh();

private:
    void buildUi();
    void updateModel();

    Emby::EmbyEngine& m_engine;
    GtkWidget* m_container = nullptr;
    GtkWidget* m_treeView = nullptr;
    GtkListStore* m_listStore = nullptr;
    GtkWidget* m_statusLabel = nullptr;
    guint m_timerId = 0;
};
