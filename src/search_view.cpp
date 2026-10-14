#include "search_view.hpp"
#include "poster_cache.hpp"
#include "episode_dialog.hpp"
#include <thread>

SearchView::SearchView(GtkWindow* parentWindow, Emby::EmbyEngine& engine)
    : m_parentWindow(parentWindow), m_engine(engine)
{
    buildUi();
}

void SearchView::buildUi() {
    m_container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(m_container), 10);

    // Arama Çubuğu
    GtkWidget* searchBar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_pack_start(GTK_BOX(m_container), searchBar, FALSE, FALSE, 0);

    m_entrySearch = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(m_entrySearch), "Film veya dizi adi, IMDb ID (tt...) veya TMDb ID girin...");
    gtk_box_pack_start(GTK_BOX(searchBar), m_entrySearch, TRUE, TRUE, 0);

    m_btnSearch = gtk_button_new_with_label("Ara");
    gtk_box_pack_start(GTK_BOX(searchBar), m_btnSearch, FALSE, FALSE, 0);

    m_spinner = gtk_spinner_new();
    gtk_box_pack_start(GTK_BOX(searchBar), m_spinner, FALSE, FALSE, 0);

    // Toplu Eylem Çubuğu
    GtkWidget* bulkBar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_pack_start(GTK_BOX(m_container), bulkBar, FALSE, FALSE, 0);

    GtkWidget* btnSelectAll = gtk_button_new_with_label("Tumunu Sec");
    g_signal_connect(btnSelectAll, "clicked", G_CALLBACK(+[](GtkButton*, gpointer userData) {
        auto* self = static_cast<SearchView*>(userData);
        for (auto& row : self->m_currentRows) {
            if (row.checkSelect) {
                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(row.checkSelect), TRUE);
            }
        }
    }), this);
    gtk_box_pack_start(GTK_BOX(bulkBar), btnSelectAll, FALSE, FALSE, 0);

    GtkWidget* btnDeselectAll = gtk_button_new_with_label("Secimi Kaldir");
    g_signal_connect(btnDeselectAll, "clicked", G_CALLBACK(+[](GtkButton*, gpointer userData) {
        auto* self = static_cast<SearchView*>(userData);
        for (auto& row : self->m_currentRows) {
            if (row.checkSelect) {
                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(row.checkSelect), FALSE);
            }
        }
    }), this);
    gtk_box_pack_start(GTK_BOX(bulkBar), btnDeselectAll, FALSE, FALSE, 0);

    m_btnBulkEnqueue = gtk_button_new_with_label("Secilenleri Kuyruga Ekle");
    g_signal_connect(m_btnBulkEnqueue, "clicked", G_CALLBACK(+[](GtkButton*, gpointer userData) {
        static_cast<SearchView*>(userData)->enqueueBulkSelected();
    }), this);
    gtk_box_pack_start(GTK_BOX(bulkBar), m_btnBulkEnqueue, FALSE, FALSE, 0);

    m_lblResultsCount = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(m_lblResultsCount), 1.0);
    gtk_box_pack_end(GTK_BOX(bulkBar), m_lblResultsCount, TRUE, TRUE, 0);

    // Sonuç Listesi (GtkListBox)
    m_listBox = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(m_listBox), GTK_SELECTION_SINGLE);

    // Satıra tıklandığında kutucuğu işaretle/kaldır
    g_signal_connect(m_listBox, "row-activated", G_CALLBACK(+[](GtkListBox*, GtkListBoxRow* row, gpointer userData) {
        auto* self = static_cast<SearchView*>(userData);
        int idx = gtk_list_box_row_get_index(row);
        if (idx >= 0 && idx < static_cast<int>(self->m_currentRows.size())) {
            GtkWidget* cb = self->m_currentRows[idx].checkSelect;
            if (cb) {
                gboolean active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cb));
                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cb), !active);
            }
        }
    }), this);

    GtkWidget* scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scroll), m_listBox);
    gtk_box_pack_start(GTK_BOX(m_container), scroll, TRUE, TRUE, 0);

    // Arama Sinyalleri
    g_signal_connect(m_btnSearch, "clicked", G_CALLBACK(+[](GtkButton*, gpointer userData) {
        static_cast<SearchView*>(userData)->performSearch();
    }), this);

    g_signal_connect(m_entrySearch, "activate", G_CALLBACK(+[](GtkEntry*, gpointer userData) {
        static_cast<SearchView*>(userData)->performSearch();
    }), this);
}

void SearchView::performSearch() {
    const char* text = gtk_entry_get_text(GTK_ENTRY(m_entrySearch));
    if (!text || std::string(text).empty()) return;

    std::string query = text;

    gtk_spinner_start(GTK_SPINNER(m_spinner));
    gtk_widget_set_sensitive(m_btnSearch, FALSE);
    gtk_widget_set_sensitive(m_entrySearch, FALSE);

    std::thread([this, query]() {
        auto results = m_engine.search(query);

        struct CbData {
            SearchView* self;
            std::vector<Emby::MediaItem> items;
        };
        auto* data = new CbData{this, std::move(results)};

        g_idle_add(+[](gpointer p) -> gboolean {
            auto* d = static_cast<CbData*>(p);
            d->self->renderResults(d->items);
            gtk_spinner_stop(GTK_SPINNER(d->self->m_spinner));
            gtk_widget_set_sensitive(d->self->m_btnSearch, TRUE);
            gtk_widget_set_sensitive(d->self->m_entrySearch, TRUE);
            delete d;
            return G_SOURCE_REMOVE;
        }, data);
    }).detach();
}

void SearchView::renderResults(const std::vector<Emby::MediaItem>& items) {
    GList* children = gtk_container_get_children(GTK_CONTAINER(m_listBox));
    for (GList* iter = children; iter != NULL; iter = g_list_next(iter)) {
        gtk_widget_destroy(GTK_WIDGET(iter->data));
    }
    g_list_free(children);
    m_currentRows.clear();

    std::string countStr = std::to_string(items.size()) + " sonuc bulundu.";
    gtk_label_set_text(GTK_LABEL(m_lblResultsCount), countStr.c_str());

    std::string serverUrl = m_engine.getClient().getServerUrl();
    std::string token = m_engine.getClient().getToken();
    std::string prefix = m_engine.getClient().getPrefix();

    for (size_t i = 0; i < items.size(); ++i) {
        const auto& item = items[i];
        SearchItemRow row;
        row.item = item;

        GtkWidget* rowBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
        gtk_container_set_border_width(GTK_CONTAINER(rowBox), 8);

        // Onay kutusu
        row.checkSelect = gtk_check_button_new();
        gtk_box_pack_start(GTK_BOX(rowBox), row.checkSelect, FALSE, FALSE, 0);

        // Poster Görseli
        GtkWidget* imgWidget = gtk_image_new();
        gtk_widget_set_size_request(imgWidget, 80, 120);

        GdkPixbuf* initialPb = PosterCache::instance().getOrFetch(
            item.id,
            serverUrl,
            token,
            prefix,
            80,
            120,
            [imgWidget](GdkPixbuf* pb) {
                gtk_image_set_from_pixbuf(GTK_IMAGE(imgWidget), pb);
            }
        );

        if (initialPb) {
            gtk_image_set_from_pixbuf(GTK_IMAGE(imgWidget), initialPb);
            g_object_unref(initialPb);
        }
        gtk_box_pack_start(GTK_BOX(rowBox), imgWidget, FALSE, FALSE, 0);

        // Bilgi Kutusu
        GtkWidget* infoBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
        gtk_box_pack_start(GTK_BOX(rowBox), infoBox, TRUE, TRUE, 0);

        std::string titleMarkup = "<b>" + item.name + "</b>";
        if (item.year > 0) {
            titleMarkup += " (" + std::to_string(item.year) + ")";
        }
        GtkWidget* lblTitle = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(lblTitle), titleMarkup.c_str());
        gtk_label_set_xalign(GTK_LABEL(lblTitle), 0.0);
        gtk_box_pack_start(GTK_BOX(infoBox), lblTitle, FALSE, FALSE, 0);

        std::string typeBadge = (item.type == Emby::MediaType::Series) ? "Dizi" : "Film";
        GtkWidget* lblType = gtk_label_new(("<span color='#4a90e2'>" + typeBadge + "</span>").c_str());
        gtk_label_set_use_markup(GTK_LABEL(lblType), TRUE);
        gtk_label_set_xalign(GTK_LABEL(lblType), 0.0);
        gtk_box_pack_start(GTK_BOX(infoBox), lblType, FALSE, FALSE, 0);

        if (!item.sources.empty()) {
            std::string srcInfo = "Kaynak: " + item.sources[0].name + " (" + 
                                  Emby::Platform::formatBytes(item.sources[0].size) + ")";
            GtkWidget* lblSrc = gtk_label_new(srcInfo.c_str());
            gtk_label_set_xalign(GTK_LABEL(lblSrc), 0.0);
            gtk_box_pack_start(GTK_BOX(infoBox), lblSrc, FALSE, FALSE, 0);
        }

        // Eylem Butonları
        GtkWidget* actionBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
        gtk_box_pack_start(GTK_BOX(rowBox), actionBox, FALSE, FALSE, 0);

        if (item.type == Emby::MediaType::Series) {
            GtkWidget* btnEpisodes = gtk_button_new_with_label("Bolumleri Sec...");
            row.btnAction = btnEpisodes;

            struct EpData {
                SearchView* view;
                Emby::MediaItem it;
            };
            auto* ed = new EpData{this, item};
            g_signal_connect_data(btnEpisodes, "clicked", G_CALLBACK(+[](GtkButton*, gpointer p) {
                auto* d = static_cast<EpData*>(p);
                EpisodeDialog::show(d->view->m_parentWindow, d->view->m_engine, d->it);
                if (d->view->m_onTaskEnqueued) {
                    d->view->m_onTaskEnqueued(1);
                }
            }), ed, [](gpointer p, GClosure*) { delete static_cast<EpData*>(p); }, static_cast<GConnectFlags>(0));

            gtk_box_pack_start(GTK_BOX(actionBox), btnEpisodes, TRUE, FALSE, 0);
        } else {
            GtkWidget* btnEnqueue = gtk_button_new_with_label("Kuyruga Ekle");
            row.btnAction = btnEnqueue;

            struct MvData {
                SearchView* view;
                Emby::MediaItem it;
                size_t rowIndex;
            };
            auto* md = new MvData{this, item, i};
            g_signal_connect_data(btnEnqueue, "clicked", G_CALLBACK(+[](GtkButton* btn, gpointer p) {
                auto* d = static_cast<MvData*>(p);

                // Eğer sources boşsa getItem ile detayını çek
                if (d->it.sources.empty()) {
                    d->it = d->view->m_engine.getItem(d->it.id);
                }

                if (!d->it.sources.empty()) {
                    d->view->m_engine.enqueueVideo(d->it, d->it.sources[0]);
                } else {
                    Emby::MediaSource defaultSrc;
                    defaultSrc.id = d->it.id;
                    defaultSrc.container = "mkv";
                    d->view->m_engine.enqueueVideo(d->it, defaultSrc);
                }

                d->view->m_engine.getQueue().start();

                gtk_button_set_label(btn, "Kuyrukta");
                gtk_widget_set_sensitive(GTK_WIDGET(btn), FALSE);

                if (d->rowIndex < d->view->m_currentRows.size() && d->view->m_currentRows[d->rowIndex].checkSelect) {
                    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(d->view->m_currentRows[d->rowIndex].checkSelect), TRUE);
                }

                if (d->view->m_onTaskEnqueued) {
                    d->view->m_onTaskEnqueued(1);
                }
            }), md, [](gpointer p, GClosure*) { delete static_cast<MvData*>(p); }, static_cast<GConnectFlags>(0));

            gtk_box_pack_start(GTK_BOX(actionBox), btnEnqueue, TRUE, FALSE, 0);
        }

        row.rowBox = rowBox;
        m_currentRows.push_back(row);

        gtk_list_box_insert(GTK_LIST_BOX(m_listBox), rowBox, -1);
    }

    gtk_widget_show_all(m_listBox);
}

void SearchView::enqueueBulkSelected() {
    int totalEnqueued = 0;

    for (auto& row : m_currentRows) {
        if (!row.checkSelect || !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(row.checkSelect))) {
            continue;
        }

        if (row.item.type == Emby::MediaType::Movie) {
            if (row.item.sources.empty()) {
                row.item = m_engine.getItem(row.item.id);
            }

            if (!row.item.sources.empty()) {
                m_engine.enqueueVideo(row.item, row.item.sources[0]);
                totalEnqueued++;
            } else {
                Emby::MediaSource defaultSrc;
                defaultSrc.id = row.item.id;
                defaultSrc.container = "mkv";
                m_engine.enqueueVideo(row.item, defaultSrc);
                totalEnqueued++;
            }

            if (row.btnAction) {
                gtk_button_set_label(GTK_BUTTON(row.btnAction), "Kuyrukta");
                gtk_widget_set_sensitive(row.btnAction, FALSE);
            }
        } else if (row.item.type == Emby::MediaType::Series) {
            auto episodes = m_engine.getClient().getEpisodes(row.item.id);
            for (auto& ep : episodes) {
                if (ep.sources.empty()) {
                    ep = m_engine.getItem(ep.id);
                }
                if (!ep.sources.empty()) {
                    m_engine.enqueueVideo(ep, ep.sources[0]);
                    totalEnqueued++;
                } else {
                    Emby::MediaSource defaultSrc;
                    defaultSrc.id = ep.id;
                    defaultSrc.container = "mkv";
                    m_engine.enqueueVideo(ep, defaultSrc);
                    totalEnqueued++;
                }
            }
            if (row.btnAction) {
                gtk_button_set_label(GTK_BUTTON(row.btnAction), "Kuyrukta");
            }
        }
    }

    if (totalEnqueued > 0) {
        m_engine.getQueue().start();
        std::string msg = std::to_string(totalEnqueued) + " oge basariyla kuyruga eklendi!";
        gtk_label_set_text(GTK_LABEL(m_lblResultsCount), msg.c_str());

        if (m_onTaskEnqueued) {
            m_onTaskEnqueued(totalEnqueued);
        }
    } else {
        gtk_label_set_text(GTK_LABEL(m_lblResultsCount), "Lutfen once kuyruga eklenecek ogeleri secin.");
    }
}
