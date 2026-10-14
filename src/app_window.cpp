#include "app_window.hpp"
#include "server_dialog.hpp"

GtkWidget* AppWindow::create(GtkApplication* app) {
    auto* window = new AppWindow(app);
    return window->m_window;
}

AppWindow::AppWindow(GtkApplication* app) {
    buildUi(app);
    populateProfiles();
    updateConnectionStatus();
}

void AppWindow::buildUi(GtkApplication* app) {
    m_window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(m_window), "Emby Downloader");
    gtk_window_set_default_size(GTK_WINDOW(m_window), 1080, 720);
    gtk_container_set_border_width(GTK_CONTAINER(m_window), 6);

    GtkWidget* mainVBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_add(GTK_CONTAINER(m_window), mainVBox);

    // Üst Bar (Profil ve Bağlantı)
    GtkWidget* topBar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(topBar), 4);
    gtk_box_pack_start(GTK_BOX(mainVBox), topBar, FALSE, FALSE, 0);

    GtkWidget* lblServer = gtk_label_new("<b>Sunucu:</b>");
    gtk_label_set_use_markup(GTK_LABEL(lblServer), TRUE);
    gtk_box_pack_start(GTK_BOX(topBar), lblServer, FALSE, FALSE, 0);

    m_profileCombo = gtk_combo_box_text_new();
    gtk_widget_set_size_request(m_profileCombo, 220, -1);
    g_signal_connect(m_profileCombo, "changed", G_CALLBACK(+[](GtkComboBoxText* combo, gpointer userData) {
        auto* self = static_cast<AppWindow*>(userData);
        const char* activeId = gtk_combo_box_get_active_id(GTK_COMBO_BOX(combo));
        if (activeId) {
            self->m_engine.loadProfile(activeId);
            self->updateConnectionStatus();
        }
    }), this);
    gtk_box_pack_start(GTK_BOX(topBar), m_profileCombo, FALSE, FALSE, 0);

    GtkWidget* btnAddServer = gtk_button_new_with_label("Sunucu Bagla...");
    g_signal_connect(btnAddServer, "clicked", G_CALLBACK(+[](GtkButton*, gpointer userData) {
        auto* self = static_cast<AppWindow*>(userData);
        if (ServerDialog::show(GTK_WINDOW(self->m_window), self->m_engine)) {
            self->populateProfiles();
            self->updateConnectionStatus();
        }
    }), this);
    gtk_box_pack_start(GTK_BOX(topBar), btnAddServer, FALSE, FALSE, 0);

    m_statusLabel = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(m_statusLabel), 1.0);
    gtk_box_pack_end(GTK_BOX(topBar), m_statusLabel, TRUE, TRUE, 0);

    // Sekmeler (Notebook)
    m_notebook = gtk_notebook_new();
    gtk_box_pack_start(GTK_BOX(mainVBox), m_notebook, TRUE, TRUE, 0);

    m_searchView = std::make_unique<SearchView>(GTK_WINDOW(m_window), m_engine);
    GtkWidget* tabLabel1 = gtk_label_new("Medya Arama");
    gtk_notebook_append_page(GTK_NOTEBOOK(m_notebook), m_searchView->getWidget(), tabLabel1);

    m_queueView = std::make_unique<QueueView>(m_engine);
    GtkWidget* tabLabel2 = gtk_label_new("Indirme Kuyrugu");
    gtk_notebook_append_page(GTK_NOTEBOOK(m_notebook), m_queueView->getWidget(), tabLabel2);

    // Alt Durum Çubuğu
    GtkWidget* bottomBar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 15);
    gtk_container_set_border_width(GTK_CONTAINER(bottomBar), 4);
    gtk_box_pack_start(GTK_BOX(mainVBox), bottomBar, FALSE, FALSE, 0);

    std::string dirInfo = "Indirme Dizini: " + m_engine.getDefaultDownloadDir();
    GtkWidget* lblDir = gtk_label_new(dirInfo.c_str());
    gtk_label_set_xalign(GTK_LABEL(lblDir), 0.0);
    gtk_box_pack_start(GTK_BOX(bottomBar), lblDir, TRUE, TRUE, 0);

    std::string ffmpegInfo = m_engine.isFfmpegAvailable() ? "FFmpeg: Hazir" : "FFmpeg: Bulunamadi";
    GtkWidget* lblFfmpeg = gtk_label_new(ffmpegInfo.c_str());
    gtk_box_pack_end(GTK_BOX(bottomBar), lblFfmpeg, FALSE, FALSE, 0);

    gtk_widget_show_all(m_window);
}

void AppWindow::populateProfiles() {
    g_signal_handlers_block_by_func(m_profileCombo, (gpointer)+[](GtkComboBoxText*, gpointer) {}, this);
    gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(m_profileCombo));

    auto profiles = m_engine.getConfig().getProfiles();
    std::string lastActive = m_engine.getConfig().getLastActiveProfileId();

    int activeIdx = -1;
    for (size_t i = 0; i < profiles.size(); ++i) {
        const auto& p = profiles[i];
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(m_profileCombo), p.id.c_str(), p.name.c_str());
        if (p.id == lastActive) {
            activeIdx = static_cast<int>(i);
        }
    }

    if (activeIdx >= 0) {
        gtk_combo_box_set_active(GTK_COMBO_BOX(m_profileCombo), activeIdx);
    } else if (!profiles.empty()) {
        gtk_combo_box_set_active(GTK_COMBO_BOX(m_profileCombo), 0);
    }

    g_signal_handlers_unblock_by_func(m_profileCombo, (gpointer)+[](GtkComboBoxText*, gpointer) {}, this);
}

void AppWindow::updateConnectionStatus() {
    if (m_engine.isAuthenticated()) {
        std::string server = m_engine.getClient().getServerUrl();
        std::string user = m_engine.getClient().getUsername();
        std::string markup = "<span color='#4caf50'>● Bagli: " + user + " (" + server + ")</span>";
        gtk_label_set_markup(GTK_LABEL(m_statusLabel), markup.c_str());
    } else {
        std::string markup = "<span color='#f44336'>● Baglanti Yok</span>";
        gtk_label_set_markup(GTK_LABEL(m_statusLabel), markup.c_str());
    }
}
