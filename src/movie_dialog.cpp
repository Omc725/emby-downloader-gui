#include "movie_dialog.hpp"
#include "poster_cache.hpp"

enum {
    COL_SRC_NAME = 0,
    COL_SRC_RES,
    COL_SRC_SIZE,
    COL_SRC_CONTAINER,
    COL_SRC_INDEX,
    NUM_SRC_COLS
};

void MovieDialog::show(
    GtkWindow* parent,
    Emby::EmbyEngine& engine,
    const Emby::MediaItem& movieItem,
    OnEnqueuedCallback onEnqueued
) {
    MovieDialog dialog(parent, engine, movieItem, onEnqueued);
}

MovieDialog::MovieDialog(
    GtkWindow* parent,
    Emby::EmbyEngine& engine,
    const Emby::MediaItem& movieItem,
    OnEnqueuedCallback onEnqueued
) : m_parent(parent),
    m_engine(engine),
    m_movieItem(movieItem),
    m_onEnqueued(onEnqueued)
{
    // Emby arama sonuclari yalnizca tek bir varsayilan kaynak dondurur.
    // Tum alternatif surumlerin (2160p, Remux, 1080p, 720p vb.) eksiksiz
    // listelenebilmesi icin her zaman sunucudan ogenin tam detayini cekiyoruz.
    Emby::MediaItem fullItem = m_engine.getItem(m_movieItem.id);
    if (!fullItem.id.empty() && !fullItem.sources.empty()) {
        m_movieItem = std::move(fullItem);
    }

    buildUi();
    populateSourcesList();

    gint res = gtk_dialog_run(GTK_DIALOG(m_dialog));
    if (res == GTK_RESPONSE_ACCEPT) {
        enqueueSelected(false);
    } else if (res == GTK_RESPONSE_APPLY) {
        enqueueSelected(true);
    }

    gtk_widget_destroy(m_dialog);
}

void MovieDialog::buildUi() {
    std::string title = m_movieItem.name + " - Surum ve Indirme Secenekleri";
    m_dialog = gtk_dialog_new_with_buttons(
        title.c_str(),
        m_parent,
        static_cast<GtkDialogFlags>(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
        "_Iptal", GTK_RESPONSE_CANCEL,
        "_Kuyruga Ekle", GTK_RESPONSE_ACCEPT,
        "_Direkt Indir", GTK_RESPONSE_APPLY,
        NULL
    );

    gtk_window_set_default_size(GTK_WINDOW(m_dialog), 740, 520);
    gtk_container_set_border_width(GTK_CONTAINER(m_dialog), 12);

    GtkWidget* contentArea = gtk_dialog_get_content_area(GTK_DIALOG(m_dialog));
    GtkWidget* mainVBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_add(GTK_CONTAINER(contentArea), mainVBox);

    // Üst Bölüm: Poster + Başlık / Özet
    GtkWidget* topHBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_box_pack_start(GTK_BOX(mainVBox), topHBox, FALSE, FALSE, 0);

    // Sol: Poster Görseli
    GtkWidget* imgPoster = gtk_image_new();
    gtk_widget_set_size_request(imgPoster, 90, 135);

    std::string serverUrl = m_engine.getClient().getServerUrl();
    std::string token = m_engine.getClient().getToken();
    std::string prefix = m_engine.getClient().getPrefix();

    GdkPixbuf* initialPb = PosterCache::instance().getOrFetch(
        m_movieItem.id,
        serverUrl,
        token,
        prefix,
        90,
        135,
        [imgPoster](GdkPixbuf* pb) {
            gtk_image_set_from_pixbuf(GTK_IMAGE(imgPoster), pb);
        }
    );
    if (initialPb) {
        gtk_image_set_from_pixbuf(GTK_IMAGE(imgPoster), initialPb);
        g_object_unref(initialPb);
    }
    gtk_box_pack_start(GTK_BOX(topHBox), imgPoster, FALSE, FALSE, 0);

    // Sağ: Başlık ve Bilgiler
    GtkWidget* topInfoBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_box_pack_start(GTK_BOX(topHBox), topInfoBox, TRUE, TRUE, 0);

    std::string titleMarkup = "<span size='x-large' weight='bold'>" + m_movieItem.name + "</span>";
    if (m_movieItem.year > 0) {
        titleMarkup += " (" + std::to_string(m_movieItem.year) + ")";
    }
    GtkWidget* lblTitle = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(lblTitle), titleMarkup.c_str());
    gtk_label_set_xalign(GTK_LABEL(lblTitle), 0.0);
    gtk_box_pack_start(GTK_BOX(topInfoBox), lblTitle, FALSE, FALSE, 0);

    size_t srcCount = m_movieItem.sources.size();
    std::string countText = "Sunucuda toplam <b>" + std::to_string(srcCount) + "</b> farkli surum tespit edildi. Asagidaki listeden indirmek istediginiz surumu seciniz:";
    GtkWidget* lblCount = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(lblCount), countText.c_str());
    gtk_label_set_xalign(GTK_LABEL(lblCount), 0.0);
    gtk_label_set_line_wrap(GTK_LABEL(lblCount), TRUE);
    gtk_box_pack_start(GTK_BOX(topInfoBox), lblCount, FALSE, FALSE, 0);

    // Orta Bölüm: Sürümler Tablosu (GtkTreeView)
    m_listStore = gtk_list_store_new(
        NUM_SRC_COLS,
        G_TYPE_STRING, // Sürüm Adı
        G_TYPE_STRING, // Çözünürlük
        G_TYPE_STRING, // Boyut
        G_TYPE_STRING, // Biçim
        G_TYPE_INT     // Orijinal İndeks
    );

    m_treeView = gtk_tree_view_new_with_model(GTK_TREE_MODEL(m_listStore));
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(m_treeView), TRUE);

    GtkCellRenderer* renderer = gtk_cell_renderer_text_new();

    // Sütun 1: Sürüm / Yayın Adı
    GtkTreeViewColumn* colName = gtk_tree_view_column_new_with_attributes(
        "Surum / Yayin Adi", renderer, "text", COL_SRC_NAME, NULL
    );
    gtk_tree_view_column_set_expand(colName, TRUE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(m_treeView), colName);

    // Sütun 2: Çözünürlük
    GtkTreeViewColumn* colRes = gtk_tree_view_column_new_with_attributes(
        "Cozunurluk", renderer, "text", COL_SRC_RES, NULL
    );
    gtk_tree_view_append_column(GTK_TREE_VIEW(m_treeView), colRes);

    // Sütun 3: Boyut
    GtkTreeViewColumn* colSize = gtk_tree_view_column_new_with_attributes(
        "Boyut", renderer, "text", COL_SRC_SIZE, NULL
    );
    gtk_tree_view_append_column(GTK_TREE_VIEW(m_treeView), colSize);

    // Sütun 4: Biçim
    GtkTreeViewColumn* colCont = gtk_tree_view_column_new_with_attributes(
        "Format", renderer, "text", COL_SRC_CONTAINER, NULL
    );
    gtk_tree_view_append_column(GTK_TREE_VIEW(m_treeView), colCont);

    GtkTreeSelection* selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(m_treeView));
    gtk_tree_selection_set_mode(selection, GTK_SELECTION_BROWSE);

    g_signal_connect(m_treeView, "cursor-changed", G_CALLBACK(+[](GtkTreeView* tv, gpointer userData) {
        auto* self = static_cast<MovieDialog*>(userData);
        GtkTreeSelection* sel = gtk_tree_view_get_selection(tv);
        GtkTreeModel* model;
        GtkTreeIter iter;
        if (gtk_tree_selection_get_selected(sel, &model, &iter)) {
            gint idx = 0;
            gtk_tree_model_get(model, &iter, COL_SRC_INDEX, &idx, -1);
            self->updateStreamsForSelectedSource(idx);
        }
    }), this);

    GtkWidget* scrollWindow = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrollWindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(scrollWindow, -1, 160);
    gtk_container_add(GTK_CONTAINER(scrollWindow), m_treeView);
    gtk_box_pack_start(GTK_BOX(mainVBox), scrollWindow, TRUE, TRUE, 0);

    // Alt Bölüm: Seçili Sürüme Ait İndirme ve Akış Seçenekleri
    GtkWidget* optFrame = gtk_frame_new("Secili Surum Icin Indirme Secenekleri");
    gtk_box_pack_start(GTK_BOX(mainVBox), optFrame, FALSE, FALSE, 0);

    GtkWidget* optBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(optBox), 8);
    gtk_container_add(GTK_CONTAINER(optFrame), optBox);

    // Video Onay Kutusu
    m_checkVideo = gtk_check_button_new_with_label("Tam Video Dosyasi (Dondurmesiz Birebir Kopya)");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_checkVideo), TRUE);
    gtk_box_pack_start(GTK_BOX(optBox), m_checkVideo, FALSE, FALSE, 0);

    // Ses ve Altyazı Seçim Izgarası
    GtkWidget* streamGrid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(streamGrid), 6);
    gtk_grid_set_column_spacing(GTK_GRID(streamGrid), 10);
    gtk_box_pack_start(GTK_BOX(optBox), streamGrid, FALSE, FALSE, 0);

    // Ses Parçası
    m_checkAudio = gtk_check_button_new_with_label("Ses Parcasini Ayri Indir:");
    gtk_grid_attach(GTK_GRID(streamGrid), m_checkAudio, 0, 0, 1, 1);

    m_audioCombo = gtk_combo_box_text_new();
    gtk_widget_set_hexpand(m_audioCombo, TRUE);
    gtk_grid_attach(GTK_GRID(streamGrid), m_audioCombo, 1, 0, 1, 1);

    g_signal_connect(m_checkAudio, "toggled", G_CALLBACK(+[](GtkToggleButton* btn, gpointer userData) {
        auto* self = static_cast<MovieDialog*>(userData);
        gboolean active = gtk_toggle_button_get_active(btn);
        gtk_widget_set_sensitive(self->m_audioCombo, active);
    }), this);

    // Altyazı Parçası
    m_checkSubtitle = gtk_check_button_new_with_label("Altyaziyi Ayri Indir:");
    gtk_grid_attach(GTK_GRID(streamGrid), m_checkSubtitle, 0, 1, 1, 1);

    m_subCombo = gtk_combo_box_text_new();
    gtk_widget_set_hexpand(m_subCombo, TRUE);
    gtk_grid_attach(GTK_GRID(streamGrid), m_subCombo, 1, 1, 1, 1);

    g_signal_connect(m_checkSubtitle, "toggled", G_CALLBACK(+[](GtkToggleButton* btn, gpointer userData) {
        auto* self = static_cast<MovieDialog*>(userData);
        gboolean active = gtk_toggle_button_get_active(btn);
        gtk_widget_set_sensitive(self->m_subCombo, active);
    }), this);

    // Detay Bilgisi Etiketi
    m_lblDetails = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(m_lblDetails), 0.0);
    gtk_box_pack_start(GTK_BOX(optBox), m_lblDetails, FALSE, FALSE, 0);

    gtk_widget_show_all(contentArea);
}

void MovieDialog::populateSourcesList() {
    gtk_list_store_clear(m_listStore);

    for (size_t i = 0; i < m_movieItem.sources.size(); ++i) {
        const auto& s = m_movieItem.sources[i];

        std::string name = s.name.empty() ? ("Surum #" + std::to_string(i + 1)) : s.name;
        std::string res = (s.width > 0 && s.height > 0) ? (std::to_string(s.width) + "x" + std::to_string(s.height)) : "-";
        std::string size = s.size > 0 ? Emby::Platform::formatBytes(s.size) : "-";
        std::string cont = s.container.empty() ? "mkv" : s.container;

        GtkTreeIter iter;
        gtk_list_store_append(m_listStore, &iter);
        gtk_list_store_set(
            m_listStore, &iter,
            COL_SRC_NAME, name.c_str(),
            COL_SRC_RES, res.c_str(),
            COL_SRC_SIZE, size.c_str(),
            COL_SRC_CONTAINER, cont.c_str(),
            COL_SRC_INDEX, static_cast<gint>(i),
            -1
        );
    }

    if (!m_movieItem.sources.empty()) {
        GtkTreeSelection* sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(m_treeView));
        GtkTreePath* path = gtk_tree_path_new_first();
        gtk_tree_selection_select_path(sel, path);
        gtk_tree_view_set_cursor(GTK_TREE_VIEW(m_treeView), path, NULL, FALSE);
        gtk_tree_path_free(path);
        updateStreamsForSelectedSource(0);
    }
}

void MovieDialog::updateStreamsForSelectedSource(int srcIndex) {
    if (srcIndex < 0 || srcIndex >= static_cast<int>(m_movieItem.sources.size())) {
        return;
    }
    m_selectedSourceIndex = srcIndex;
    const auto& source = m_movieItem.sources[srcIndex];

    // Ses akışlarını güncelle
    gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(m_audioCombo));
    if (source.audioStreams.empty()) {
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(m_audioCombo), "Ses akisi bulunamadi");
        gtk_widget_set_sensitive(m_checkAudio, FALSE);
        gtk_widget_set_sensitive(m_audioCombo, FALSE);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_checkAudio), FALSE);
    } else {
        gtk_widget_set_sensitive(m_checkAudio, TRUE);
        for (const auto& a : source.audioStreams) {
            std::string label = a.title;
            if (label.empty()) {
                label = a.language.empty() ? "Ses" : a.language;
                if (!a.codec.empty()) label += " (" + a.codec + ")";
            }
            gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(m_audioCombo), label.c_str());
        }
        gtk_combo_box_set_active(GTK_COMBO_BOX(m_audioCombo), 0);
        gtk_widget_set_sensitive(m_audioCombo, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(m_checkAudio)));
    }

    // Altyazı akışlarını güncelle
    gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(m_subCombo));
    if (source.subtitleStreams.empty()) {
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(m_subCombo), "Altyazi bulunamadi");
        gtk_widget_set_sensitive(m_checkSubtitle, FALSE);
        gtk_widget_set_sensitive(m_subCombo, FALSE);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_checkSubtitle), FALSE);
    } else {
        gtk_widget_set_sensitive(m_checkSubtitle, TRUE);
        for (const auto& s : source.subtitleStreams) {
            std::string label = s.title;
            if (label.empty()) {
                label = s.language.empty() ? "Altyazi" : s.language;
                if (!s.codec.empty()) label += " (" + s.codec + ")";
            }
            gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(m_subCombo), label.c_str());
        }
        gtk_combo_box_set_active(GTK_COMBO_BOX(m_subCombo), 0);
        gtk_widget_set_sensitive(m_subCombo, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(m_checkSubtitle)));
    }

    std::string details = "Secili Surum: " + source.name;
    if (source.width > 0 && source.height > 0) {
        details += " | Cozunurluk: " + std::to_string(source.width) + "x" + std::to_string(source.height);
    }
    details += " | Boyut: " + Emby::Platform::formatBytes(source.size) + " | Kapsayici: " + source.container;
    details += " | Ses Akislari: " + std::to_string(source.audioStreams.size());
    details += " | Altyazilar: " + std::to_string(source.subtitleStreams.size());
    gtk_label_set_text(GTK_LABEL(m_lblDetails), details.c_str());
}

void MovieDialog::enqueueSelected(bool directDownload) {
    if (m_selectedSourceIndex < 0 || m_selectedSourceIndex >= static_cast<int>(m_movieItem.sources.size())) {
        return;
    }

    const auto& source = m_movieItem.sources[m_selectedSourceIndex];
    int count = 0;

    bool dlVideo = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(m_checkVideo));
    bool dlAudio = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(m_checkAudio));
    bool dlSub = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(m_checkSubtitle));

    if (dlVideo) {
        m_engine.enqueueVideo(m_movieItem, source);
        count++;
    }

    if (dlAudio && !source.audioStreams.empty()) {
        int aIdx = gtk_combo_box_get_active(GTK_COMBO_BOX(m_audioCombo));
        if (aIdx >= 0 && aIdx < static_cast<int>(source.audioStreams.size())) {
            m_engine.enqueueAudio(m_movieItem, source, source.audioStreams[aIdx]);
            count++;
        }
    }

    if (dlSub && !source.subtitleStreams.empty()) {
        int sIdx = gtk_combo_box_get_active(GTK_COMBO_BOX(m_subCombo));
        if (sIdx >= 0 && sIdx < static_cast<int>(source.subtitleStreams.size())) {
            m_engine.enqueueSubtitle(m_movieItem, source, source.subtitleStreams[sIdx]);
            count++;
        }
    }

    if (count > 0) {
        m_engine.getQueue().start();
        if (m_onEnqueued) {
            m_onEnqueued(count, directDownload);
        }
    }
}

