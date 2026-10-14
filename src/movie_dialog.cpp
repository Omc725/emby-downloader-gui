#include "movie_dialog.hpp"
#include "poster_cache.hpp"

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
    // Detayları ve kaynakları garantiye al
    if (m_movieItem.sources.empty()) {
        m_movieItem = m_engine.getItem(m_movieItem.id);
    }

    buildUi();
    updateStreamsForSelectedSource();

    gint res = gtk_dialog_run(GTK_DIALOG(m_dialog));
    if (res == GTK_RESPONSE_ACCEPT) {
        enqueueSelected();
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
        NULL
    );

    gtk_window_set_default_size(GTK_WINDOW(m_dialog), 640, 420);
    gtk_container_set_border_width(GTK_CONTAINER(m_dialog), 12);

    GtkWidget* contentArea = gtk_dialog_get_content_area(GTK_DIALOG(m_dialog));
    GtkWidget* mainHBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 15);
    gtk_container_add(GTK_CONTAINER(contentArea), mainHBox);

    // Sol: Poster Görseli
    GtkWidget* imgPoster = gtk_image_new();
    gtk_widget_set_size_request(imgPoster, 110, 165);

    std::string serverUrl = m_engine.getClient().getServerUrl();
    std::string token = m_engine.getClient().getToken();
    std::string prefix = m_engine.getClient().getPrefix();

    GdkPixbuf* initialPb = PosterCache::instance().getOrFetch(
        m_movieItem.id,
        serverUrl,
        token,
        prefix,
        110,
        165,
        [imgPoster](GdkPixbuf* pb) {
            gtk_image_set_from_pixbuf(GTK_IMAGE(imgPoster), pb);
        }
    );
    if (initialPb) {
        gtk_image_set_from_pixbuf(GTK_IMAGE(imgPoster), initialPb);
        g_object_unref(initialPb);
    }
    gtk_box_pack_start(GTK_BOX(mainHBox), imgPoster, FALSE, FALSE, 0);

    // Sağ: Ayarlar ve Akış Seçimleri
    GtkWidget* rightVBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_box_pack_start(GTK_BOX(mainHBox), rightVBox, TRUE, TRUE, 0);

    // Başlık
    std::string titleMarkup = "<span size='large' weight='bold'>" + m_movieItem.name + "</span>";
    if (m_movieItem.year > 0) {
        titleMarkup += " (" + std::to_string(m_movieItem.year) + ")";
    }
    GtkWidget* lblTitle = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(lblTitle), titleMarkup.c_str());
    gtk_label_set_xalign(GTK_LABEL(lblTitle), 0.0);
    gtk_box_pack_start(GTK_BOX(rightVBox), lblTitle, FALSE, FALSE, 0);

    // Grid formu
    GtkWidget* grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    gtk_box_pack_start(GTK_BOX(rightVBox), grid, FALSE, FALSE, 0);

    // 1. Sürüm Seçimi
    GtkWidget* lblSource = gtk_label_new("Surum (Kaynak):");
    gtk_label_set_xalign(GTK_LABEL(lblSource), 0.0);
    gtk_grid_attach(GTK_GRID(grid), lblSource, 0, 0, 1, 1);

    m_sourceCombo = gtk_combo_box_text_new();
    for (size_t i = 0; i < m_movieItem.sources.size(); ++i) {
        const auto& s = m_movieItem.sources[i];
        std::string sText = s.name;
        if (sText.empty()) sText = "Surum #" + std::to_string(i + 1);
        if (s.size > 0) {
            sText += " (" + Emby::Platform::formatBytes(s.size) + " - " + s.container + ")";
        }
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(m_sourceCombo), sText.c_str());
    }
    if (!m_movieItem.sources.empty()) {
        gtk_combo_box_set_active(GTK_COMBO_BOX(m_sourceCombo), 0);
    }
    g_signal_connect(m_sourceCombo, "changed", G_CALLBACK(+[](GtkComboBox*, gpointer userData) {
        static_cast<MovieDialog*>(userData)->updateStreamsForSelectedSource();
    }), this);
    gtk_grid_attach(GTK_GRID(grid), m_sourceCombo, 1, 0, 2, 1);

    // 2. Video İndirme Onayı
    m_checkVideo = gtk_check_button_new_with_label("Tam Video Dosyasi (Dondurmesiz Birebir Kopya)");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_checkVideo), TRUE);
    gtk_grid_attach(GTK_GRID(grid), m_checkVideo, 0, 1, 3, 1);

    // 3. Ses Parçası İndirme
    m_checkAudio = gtk_check_button_new_with_label("Ses Parcasini Ayri Indir:");
    gtk_grid_attach(GTK_GRID(grid), m_checkAudio, 0, 2, 1, 1);

    m_audioCombo = gtk_combo_box_text_new();
    gtk_grid_attach(GTK_GRID(grid), m_audioCombo, 1, 2, 2, 1);

    g_signal_connect(m_checkAudio, "toggled", G_CALLBACK(+[](GtkToggleButton* btn, gpointer userData) {
        auto* self = static_cast<MovieDialog*>(userData);
        gboolean active = gtk_toggle_button_get_active(btn);
        gtk_widget_set_sensitive(self->m_audioCombo, active);
    }), this);

    // 4. Altyazı İndirme
    m_checkSubtitle = gtk_check_button_new_with_label("Altyaziyi Ayri Indir:");
    gtk_grid_attach(GTK_GRID(grid), m_checkSubtitle, 0, 3, 1, 1);

    m_subCombo = gtk_combo_box_text_new();
    gtk_grid_attach(GTK_GRID(grid), m_subCombo, 1, 3, 2, 1);

    g_signal_connect(m_checkSubtitle, "toggled", G_CALLBACK(+[](GtkToggleButton* btn, gpointer userData) {
        auto* self = static_cast<MovieDialog*>(userData);
        gboolean active = gtk_toggle_button_get_active(btn);
        gtk_widget_set_sensitive(self->m_subCombo, active);
    }), this);

    // 5. Bilgi Etiketi
    m_lblDetails = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(m_lblDetails), 0.0);
    gtk_box_pack_start(GTK_BOX(rightVBox), m_lblDetails, FALSE, FALSE, 0);

    gtk_widget_show_all(contentArea);
}

void MovieDialog::updateStreamsForSelectedSource() {
    int srcIdx = gtk_combo_box_get_active(GTK_COMBO_BOX(m_sourceCombo));
    if (srcIdx < 0 || srcIdx >= static_cast<int>(m_movieItem.sources.size())) {
        return;
    }

    const auto& source = m_movieItem.sources[srcIdx];

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

    std::string details = "Secili Kaynak: " + source.name + " | Boyut: " + 
                          Emby::Platform::formatBytes(source.size) + " | Kapsayici: " + source.container;
    gtk_label_set_text(GTK_LABEL(m_lblDetails), details.c_str());
}

void MovieDialog::enqueueSelected() {
    int srcIdx = gtk_combo_box_get_active(GTK_COMBO_BOX(m_sourceCombo));
    if (srcIdx < 0 || srcIdx >= static_cast<int>(m_movieItem.sources.size())) {
        return;
    }

    const auto& source = m_movieItem.sources[srcIdx];
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
            m_onEnqueued(count);
        }
    }
}
