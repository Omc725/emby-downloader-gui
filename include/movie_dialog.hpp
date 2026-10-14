#pragma once

#include <gtk/gtk.h>
#include <emby/engine.hpp>
#include <functional>

class MovieDialog {
public:
    using OnEnqueuedCallback = std::function<void(int count, bool directDownload)>;

    static void show(
        GtkWindow* parent,
        Emby::EmbyEngine& engine,
        const Emby::MediaItem& movieItem,
        OnEnqueuedCallback onEnqueued = nullptr
    );

private:
    MovieDialog(
        GtkWindow* parent,
        Emby::EmbyEngine& engine,
        const Emby::MediaItem& movieItem,
        OnEnqueuedCallback onEnqueued
    );
    ~MovieDialog() = default;

    void buildUi();
    void populateSourcesList();
    void updateStreamsForSelectedSource(int srcIndex);
    void enqueueSelected(bool directDownload);

    GtkWindow* m_parent;
    Emby::EmbyEngine& m_engine;
    Emby::MediaItem m_movieItem;
    OnEnqueuedCallback m_onEnqueued;

    int m_selectedSourceIndex = 0;

    GtkWidget* m_dialog = nullptr;
    GtkWidget* m_treeView = nullptr;
    GtkListStore* m_listStore = nullptr;
    GtkWidget* m_checkVideo = nullptr;
    GtkWidget* m_checkAudio = nullptr;
    GtkWidget* m_audioCombo = nullptr;
    GtkWidget* m_checkSubtitle = nullptr;
    GtkWidget* m_subCombo = nullptr;
    GtkWidget* m_lblDetails = nullptr;
};
