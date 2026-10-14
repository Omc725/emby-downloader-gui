#include "episode_dialog.hpp"
#include <iomanip>
#include <sstream>
#include <set>

enum {
    COL_SELECTED = 0,
    COL_CODE,
    COL_TITLE,
    COL_SIZE,
    COL_INDEX,
    NUM_COLS
};

void EpisodeDialog::show(
    GtkWindow* parent,
    Emby::EmbyEngine& engine,
    const Emby::MediaItem& seriesItem
) {
    EpisodeDialog dialog(parent, engine, seriesItem);
}

EpisodeDialog::EpisodeDialog(
    GtkWindow* parent,
    Emby::EmbyEngine& engine,
    const Emby::MediaItem& seriesItem
) : m_parent(parent),
    m_engine(engine),
    m_seriesItem(seriesItem)
{
    buildUi();
    loadEpisodes();
    applyFilter();

    gint res = gtk_dialog_run(GTK_DIALOG(m_dialog));
    if (res == GTK_RESPONSE_ACCEPT) {
        enqueueSelected();
    }

    gtk_widget_destroy(m_dialog);
}

void EpisodeDialog::buildUi() {
    std::string title = m_seriesItem.name + " - Bolum ve Sezon Secimi";
    m_dialog = gtk_dialog_new_with_buttons(
        title.c_str(),
        m_parent,
        static_cast<GtkDialogFlags>(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
        "_Iptal", GTK_RESPONSE_CANCEL,
        "_Kuyruga Ekle", GTK_RESPONSE_ACCEPT,
        NULL
    );

    gtk_window_set_default_size(GTK_WINDOW(m_dialog), 800, 620);
    gtk_container_set_border_width(GTK_CONTAINER(m_dialog), 10);

    GtkWidget* contentArea = gtk_dialog_get_content_area(GTK_DIALOG(m_dialog));
    GtkWidget* mainBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_add(GTK_CONTAINER(contentArea), mainBox);

    // Başlık ve Açıklama
    std::string headerText = "<b>" + m_seriesItem.name + "</b> (" + std::to_string(m_seriesItem.year) + ")";
    GtkWidget* lblHeader = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(lblHeader), headerText.c_str());
    gtk_label_set_xalign(GTK_LABEL(lblHeader), 0.0);
    gtk_box_pack_start(GTK_BOX(mainBox), lblHeader, FALSE, FALSE, 0);

    // Filtre Izgarası
    GtkWidget* grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
    gtk_box_pack_start(GTK_BOX(mainBox), grid, FALSE, FALSE, 0);

    // Sezon Kapsamı
    GtkWidget* lblSeason = gtk_label_new("Sezon:");
    gtk_label_set_xalign(GTK_LABEL(lblSeason), 0.0);
    gtk_grid_attach(GTK_GRID(grid), lblSeason, 0, 0, 1, 1);

    m_seasonCombo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(m_seasonCombo), "Tum Sezonlar");
    gtk_combo_box_set_active(GTK_COMBO_BOX(m_seasonCombo), 0);
    gtk_grid_attach(GTK_GRID(grid), m_seasonCombo, 1, 0, 1, 1);

    // Bölüm Kuralı
    GtkWidget* lblRule = gtk_label_new("Bolum Kurali:");
    gtk_label_set_xalign(GTK_LABEL(lblRule), 0.0);
    gtk_grid_attach(GTK_GRID(grid), lblRule, 0, 1, 1, 1);

    m_ruleCombo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(m_ruleCombo), "Tum Bolumler");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(m_ruleCombo), "Her Sezonun Ilk Bolumu (S*E01)");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(m_ruleCombo), "Her Sezonun Final Bolumu");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(m_ruleCombo), "Her Sezonun Belirlenen Bolumleri");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(m_ruleCombo), "Ozel Bolum Numaralari");
    gtk_combo_box_set_active(GTK_COMBO_BOX(m_ruleCombo), 0);
    gtk_grid_attach(GTK_GRID(grid), m_ruleCombo, 1, 1, 1, 1);

    m_indicesEntry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(m_indicesEntry), "orn: 3, 4 veya 1-3");
    gtk_widget_set_sensitive(m_indicesEntry, FALSE);
    gtk_grid_attach(GTK_GRID(grid), m_indicesEntry, 2, 1, 1, 1);

    // İndirme Türleri
    GtkWidget* lblTypes = gtk_label_new("Indirme Turleri:");
    gtk_label_set_xalign(GTK_LABEL(lblTypes), 0.0);
    gtk_grid_attach(GTK_GRID(grid), lblTypes, 0, 2, 1, 1);

    GtkWidget* typesBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 15);
    gtk_grid_attach(GTK_GRID(grid), typesBox, 1, 2, 2, 1);

    m_checkVideo = gtk_check_button_new_with_label("Video (Birebir Kopya)");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_checkVideo), TRUE);
    gtk_box_pack_start(GTK_BOX(typesBox), m_checkVideo, FALSE, FALSE, 0);

    m_checkAudio = gtk_check_button_new_with_label("Ses");
    gtk_box_pack_start(GTK_BOX(typesBox), m_checkAudio, FALSE, FALSE, 0);

    m_audioLangCombo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(m_audioLangCombo), "Varsayilan Ses");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(m_audioLangCombo), "Turkce (tr)");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(m_audioLangCombo), "Ingilizce (en)");
    gtk_combo_box_set_active(GTK_COMBO_BOX(m_audioLangCombo), 0);
    gtk_box_pack_start(GTK_BOX(typesBox), m_audioLangCombo, FALSE, FALSE, 0);

    m_checkSubtitle = gtk_check_button_new_with_label("Altyazi");
    gtk_box_pack_start(GTK_BOX(typesBox), m_checkSubtitle, FALSE, FALSE, 0);

    m_subLangCombo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(m_subLangCombo), "Varsayilan Altyazi");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(m_subLangCombo), "Turkce (tr)");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(m_subLangCombo), "Ingilizce (en)");
    gtk_combo_box_set_active(GTK_COMBO_BOX(m_subLangCombo), 0);
    gtk_box_pack_start(GTK_BOX(typesBox), m_subLangCombo, FALSE, FALSE, 0);

    // Önizleme Tablosu (Tree View)
    m_listStore = gtk_list_store_new(NUM_COLS, G_TYPE_BOOLEAN, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT);
    m_treeView = gtk_tree_view_new_with_model(GTK_TREE_MODEL(m_listStore));
    g_object_unref(m_listStore);

    // Sütun 0: Seçim onay kutusu
    GtkCellRenderer* toggleRenderer = gtk_cell_renderer_toggle_new();
    g_signal_connect(toggleRenderer, "toggled", G_CALLBACK(+[](GtkCellRendererToggle* /*cell*/, gchar* pathStr, gpointer userData) {
        auto* self = static_cast<EpisodeDialog*>(userData);
        GtkTreePath* path = gtk_tree_path_new_from_string(pathStr);
        GtkTreeIter iter;
        if (gtk_tree_model_get_iter(GTK_TREE_MODEL(self->m_listStore), &iter, path)) {
            gint idx = 0;
            gboolean current = FALSE;
            gtk_tree_model_get(GTK_TREE_MODEL(self->m_listStore), &iter, COL_SELECTED, &current, COL_INDEX, &idx, -1);
            gboolean next = !current;
            gtk_list_store_set(self->m_listStore, &iter, COL_SELECTED, next, -1);
            if (idx >= 0 && idx < static_cast<int>(self->m_displayedRows.size())) {
                self->m_displayedRows[idx].selected = next;
            }
            self->applyFilter();
        }
        gtk_tree_path_free(path);
    }), this);

    GtkTreeViewColumn* colToggle = gtk_tree_view_column_new_with_attributes(
        "Sec", toggleRenderer, "active", COL_SELECTED, NULL
    );
    gtk_tree_view_append_column(GTK_TREE_VIEW(m_treeView), colToggle);

    // Sütun 1: Sezon/Bölüm Kodu
    GtkCellRenderer* textRenderer = gtk_cell_renderer_text_new();
    GtkTreeViewColumn* colCode = gtk_tree_view_column_new_with_attributes(
        "Kod", textRenderer, "text", COL_CODE, NULL
    );
    gtk_tree_view_append_column(GTK_TREE_VIEW(m_treeView), colCode);

    // Sütun 2: Başlık
    GtkTreeViewColumn* colTitle = gtk_tree_view_column_new_with_attributes(
        "Bolum Adi", textRenderer, "text", COL_TITLE, NULL
    );
    gtk_tree_view_column_set_expand(colTitle, TRUE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(m_treeView), colTitle);

    // Sütun 3: Boyut
    GtkTreeViewColumn* colSize = gtk_tree_view_column_new_with_attributes(
        "Boyut", textRenderer, "text", COL_SIZE, NULL
    );
    gtk_tree_view_append_column(GTK_TREE_VIEW(m_treeView), colSize);

    GtkWidget* scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scroll), m_treeView);
    gtk_box_pack_start(GTK_BOX(mainBox), scroll, TRUE, TRUE, 0);

    // Alt sayaç ve butonlar
    GtkWidget* bottomBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(mainBox), bottomBox, FALSE, FALSE, 0);

    m_countLabel = gtk_label_new("0 bolum secildi.");
    gtk_label_set_xalign(GTK_LABEL(m_countLabel), 0.0);
    gtk_box_pack_start(GTK_BOX(bottomBox), m_countLabel, TRUE, TRUE, 0);

    GtkWidget* btnSelectAll = gtk_button_new_with_label("Tumunu Sec");
    g_signal_connect(btnSelectAll, "clicked", G_CALLBACK(+[](GtkButton*, gpointer userData) {
        auto* self = static_cast<EpisodeDialog*>(userData);
        for (auto& row : self->m_displayedRows) row.selected = true;
        self->updatePreviewModel();
    }), this);
    gtk_box_pack_start(GTK_BOX(bottomBox), btnSelectAll, FALSE, FALSE, 0);

    GtkWidget* btnDeselectAll = gtk_button_new_with_label("Secimi Temizle");
    g_signal_connect(btnDeselectAll, "clicked", G_CALLBACK(+[](GtkButton*, gpointer userData) {
        auto* self = static_cast<EpisodeDialog*>(userData);
        for (auto& row : self->m_displayedRows) row.selected = false;
        self->updatePreviewModel();
    }), this);
    gtk_box_pack_start(GTK_BOX(bottomBox), btnDeselectAll, FALSE, FALSE, 0);

    // Sinyal Bağlantıları
    g_signal_connect(m_seasonCombo, "changed", G_CALLBACK(+[](GtkComboBox*, gpointer userData) {
        static_cast<EpisodeDialog*>(userData)->applyFilter();
    }), this);

    g_signal_connect(m_ruleCombo, "changed", G_CALLBACK(+[](GtkComboBox*, gpointer userData) {
        auto* self = static_cast<EpisodeDialog*>(userData);
        int active = gtk_combo_box_get_active(GTK_COMBO_BOX(self->m_ruleCombo));
        gtk_widget_set_sensitive(self->m_indicesEntry, (active == 3 || active == 4));
        self->applyFilter();
    }), this);

    g_signal_connect(m_indicesEntry, "changed", G_CALLBACK(+[](GtkEditable*, gpointer userData) {
        static_cast<EpisodeDialog*>(userData)->applyFilter();
    }), this);

    gtk_widget_show_all(contentArea);
}

void EpisodeDialog::loadEpisodes() {
    m_allEpisodes = m_engine.getClient().getEpisodes(m_seriesItem.id);

    // Sezonları topla
    std::set<int> seasons;
    for (const auto& ep : m_allEpisodes) {
        if (ep.seasonNumber > 0) {
            seasons.insert(ep.seasonNumber);
        }
    }

    for (int s : seasons) {
        std::string sText = "Sezon " + std::to_string(s);
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(m_seasonCombo), sText.c_str());
    }
}

void EpisodeDialog::applyFilter() {
    SeriesFilterOptions opts;

    int seasonIdx = gtk_combo_box_get_active(GTK_COMBO_BOX(m_seasonCombo));
    if (seasonIdx > 0) {
        // İlk seçenek "Tum Sezonlar"
        opts.targetSeasonNumber = seasonIdx;
    } else {
        opts.targetSeasonNumber = 0;
    }

    int ruleIdx = gtk_combo_box_get_active(GTK_COMBO_BOX(m_ruleCombo));
    switch (ruleIdx) {
        case 1: opts.mode = EpisodeFilterMode::FirstEpisodeOfEachSeason; break;
        case 2: opts.mode = EpisodeFilterMode::FinalEpisodeOfEachSeason; break;
        case 3: opts.mode = EpisodeFilterMode::SpecificEpisodesOfEachSeason; break;
        case 4: opts.mode = EpisodeFilterMode::CustomRange; break;
        default: opts.mode = EpisodeFilterMode::AllEpisodes; break;
    }

    if (ruleIdx == 3 || ruleIdx == 4) {
        const char* text = gtk_entry_get_text(GTK_ENTRY(m_indicesEntry));
        opts.specificEpisodeIndices = EpisodeFilter::parseIndicesString(text ? text : "");
    }

    auto matched = EpisodeFilter::filter(m_allEpisodes, opts);

    m_displayedRows.clear();
    for (const auto& ep : matched) {
        EpisodeRow row;
        row.selected = true;
        row.item = ep;
        m_displayedRows.push_back(row);
    }

    updatePreviewModel();
}

void EpisodeDialog::updatePreviewModel() {
    gtk_list_store_clear(m_listStore);

    size_t selectedCount = 0;
    for (size_t i = 0; i < m_displayedRows.size(); ++i) {
        const auto& r = m_displayedRows[i];
        if (r.selected) selectedCount++;

        std::ostringstream ss;
        ss << "S" << std::setfill('0') << std::setw(2) << r.item.seasonNumber
           << "E" << std::setfill('0') << std::setw(2) << r.item.episodeNumber;

        uint64_t bytes = 0;
        if (!r.item.sources.empty()) {
            bytes = r.item.sources[0].size;
        }

        GtkTreeIter iter;
        gtk_list_store_append(m_listStore, &iter);
        gtk_list_store_set(
            m_listStore, &iter,
            COL_SELECTED, r.selected ? TRUE : FALSE,
            COL_CODE, ss.str().c_str(),
            COL_TITLE, r.item.name.c_str(),
            COL_SIZE, Emby::Platform::formatBytes(bytes).c_str(),
            COL_INDEX, static_cast<gint>(i),
            -1
        );
    }

    std::string countStr = "Secilen: " + std::to_string(selectedCount) + " / " + 
                           std::to_string(m_displayedRows.size()) + " bolum";
    gtk_label_set_text(GTK_LABEL(m_countLabel), countStr.c_str());
}

void EpisodeDialog::enqueueSelected() {
    bool dlVideo = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(m_checkVideo));
    bool dlAudio = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(m_checkAudio));
    bool dlSub = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(m_checkSubtitle));

    int audioLangIdx = gtk_combo_box_get_active(GTK_COMBO_BOX(m_audioLangCombo));
    std::string targetAudioLang = (audioLangIdx == 1) ? "tur" : ((audioLangIdx == 2) ? "eng" : "");

    int subLangIdx = gtk_combo_box_get_active(GTK_COMBO_BOX(m_subLangCombo));
    std::string targetSubLang = (subLangIdx == 1) ? "tur" : ((subLangIdx == 2) ? "eng" : "");

    for (const auto& row : m_displayedRows) {
        if (!row.selected) continue;
        if (row.item.sources.empty()) continue;

        const auto& src = row.item.sources[0];

        if (dlVideo) {
            m_engine.enqueueVideo(row.item, src);
        }

        if (dlAudio && !src.audioStreams.empty()) {
            const Emby::StreamInfo* chosenAudio = &src.audioStreams[0];
            if (!targetAudioLang.empty()) {
                for (const auto& a : src.audioStreams) {
                    if (a.language.find(targetAudioLang) != std::string::npos) {
                        chosenAudio = &a;
                        break;
                    }
                }
            }
            m_engine.enqueueAudio(row.item, src, *chosenAudio);
        }

        if (dlSub && !src.subtitleStreams.empty()) {
            const Emby::StreamInfo* chosenSub = &src.subtitleStreams[0];
            if (!targetSubLang.empty()) {
                for (const auto& s : src.subtitleStreams) {
                    if (s.language.find(targetSubLang) != std::string::npos) {
                        chosenSub = &s;
                        break;
                    }
                }
            }
            m_engine.enqueueSubtitle(row.item, src, *chosenSub);
        }
    }

    m_engine.getQueue().start();
}
