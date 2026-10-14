#pragma once

#include <gtk/gtk.h>
#include <emby/engine.hpp>
#include <string>
#include <unordered_map>

class QueueView {
public:
    explicit QueueView(Emby::EmbyEngine& engine);
    ~QueueView();

    GtkWidget* getWidget() const { return m_container; }
    void refresh();

private:
    void buildUi();
    void showContextMenu(GdkEventButton* event);
    void showTaskDetailsDialog(const Emby::QueueTask& task);
    GdkPixbuf* getPosterPixbuf(const std::string& itemId);

    Emby::EmbyEngine& m_engine;
    GtkWidget* m_container = nullptr;
    GtkWidget* m_treeView = nullptr;
    GtkListStore* m_listStore = nullptr;
    GtkWidget* m_statusLabel = nullptr;
    guint m_timerId = 0;

    std::unordered_map<std::string, GdkPixbuf*> m_posterPixbufs;
    GdkPixbuf* m_defaultPoster = nullptr;

    std::string m_contextTaskId;
    Emby::QueueTask m_contextTask;
};
