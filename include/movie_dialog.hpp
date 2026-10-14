#pragma once

#include <gtk/gtk.h>
#include <emby/engine.hpp>
#include <functional>

class MovieDialog {
public:
    using OnEnqueuedCallback = std::function<void(int count)>;

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
    void updateStreamsForSelectedSource();
    void enqueueSelected();

    GtkWindow* m_parent;
    Emby::EmbyEngine& m_engine;
    Emby::MediaItem m_movieItem;
    OnEnqueuedCallback m_onEnqueued;

    GtkWidget* m_dialog = nullptr;
    GtkWidget* m_sourceCombo = nullptr;
    GtkWidget* m_checkVideo = nullptr;
    GtkWidget* m_checkAudio = nullptr;
    GtkWidget* m_audioCombo = nullptr;
    GtkWidget* m_checkSubtitle = nullptr;
    GtkWidget* m_subCombo = nullptr;
    GtkWidget* m_lblDetails = nullptr;
};
