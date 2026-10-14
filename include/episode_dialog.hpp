#pragma once

#include <gtk/gtk.h>
#include <emby/engine.hpp>
#include <vector>
#include "episode_filter.hpp"

class EpisodeDialog {
public:
    static void show(
        GtkWindow* parent,
        Emby::EmbyEngine& engine,
        const Emby::MediaItem& seriesItem
    );

private:
    struct EpisodeRow {
        bool selected = true;
        Emby::MediaItem item;
    };

    EpisodeDialog(GtkWindow* parent, Emby::EmbyEngine& engine, const Emby::MediaItem& seriesItem);
    ~EpisodeDialog() = default;

    void buildUi();
    void loadEpisodes();
    void applyFilter();
    void updatePreviewModel();
    void enqueueSelected();

    GtkWindow* m_parent;
    Emby::EmbyEngine& m_engine;
    Emby::MediaItem m_seriesItem;

    GtkWidget* m_dialog = nullptr;
    GtkWidget* m_seasonCombo = nullptr;
    GtkWidget* m_ruleCombo = nullptr;
    GtkWidget* m_indicesEntry = nullptr;
    GtkWidget* m_checkVideo = nullptr;
    GtkWidget* m_checkAudio = nullptr;
    GtkWidget* m_audioLangCombo = nullptr;
    GtkWidget* m_checkSubtitle = nullptr;
    GtkWidget* m_subLangCombo = nullptr;
    GtkWidget* m_treeView = nullptr;
    GtkListStore* m_listStore = nullptr;
    GtkWidget* m_countLabel = nullptr;
    GtkWidget* m_btnEnqueue = nullptr;

    std::vector<Emby::MediaItem> m_allEpisodes;
    std::vector<EpisodeRow> m_displayedRows;
};
