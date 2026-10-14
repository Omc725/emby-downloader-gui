#pragma once

#include <gtk/gtk.h>
#include <emby/engine.hpp>
#include <vector>
#include <functional>

class SearchView {
public:
    using OnTaskEnqueuedCallback = std::function<void(int count, bool directDownload)>;

    explicit SearchView(GtkWindow* parentWindow, Emby::EmbyEngine& engine);
    ~SearchView() = default;

    GtkWidget* getWidget() const { return m_container; }
    void setOnTaskEnqueued(OnTaskEnqueuedCallback cb) { m_onTaskEnqueued = cb; }

private:
    struct SearchItemRow {
        Emby::MediaItem item;
        GtkWidget* checkSelect = nullptr;
        GtkWidget* btnAction = nullptr;
        GtkWidget* rowBox = nullptr;
    };

    void buildUi();
    void performSearch();
    void renderResults(const std::vector<Emby::MediaItem>& items);
    void enqueueBulkSelected();

    GtkWindow* m_parentWindow;
    Emby::EmbyEngine& m_engine;
    OnTaskEnqueuedCallback m_onTaskEnqueued;

    GtkWidget* m_container = nullptr;
    GtkWidget* m_entrySearch = nullptr;
    GtkWidget* m_btnSearch = nullptr;
    GtkWidget* m_spinner = nullptr;
    GtkWidget* m_listBox = nullptr;
    GtkWidget* m_btnBulkEnqueue = nullptr;
    GtkWidget* m_lblResultsCount = nullptr;

    std::vector<SearchItemRow> m_currentRows;
};
