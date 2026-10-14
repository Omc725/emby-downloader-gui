#include "queue_view.hpp"
#include "poster_cache.hpp"

#include <filesystem>
#include <iomanip>
#include <sstream>
#include <cstdlib>
#include <utility>

namespace fs = std::filesystem;

enum {
    QCOL_POSTER = 0,    // GDK_TYPE_PIXBUF
    QCOL_ICON,          // G_TYPE_STRING
    QCOL_NAME,          // G_TYPE_STRING
    QCOL_TYPE,          // G_TYPE_STRING
    QCOL_STATUS,        // G_TYPE_STRING
    QCOL_PROGRESS,      // G_TYPE_INT
    QCOL_SIZE,          // G_TYPE_STRING
    QCOL_SPEED,         // G_TYPE_STRING
    QCOL_ETA,           // G_TYPE_STRING
    QCOL_PATH,          // G_TYPE_STRING
    QCOL_ID,            // G_TYPE_STRING
    QCOL_ITEM_ID,       // G_TYPE_STRING
    QNUM_COLS
};

static void openPath(const std::string& path) {
#if defined(_WIN32)
    std::string cmd = "explorer \"" + path + "\"";
#elif defined(__APPLE__)
    std::string cmd = "open \"" + path + "\"";
#else
    std::string cmd = "xdg-open \"" + path + "\" &";
#endif
    int res = std::system(cmd.c_str());
    (void)res;
}

QueueView::QueueView(Emby::EmbyEngine& engine)
    : m_engine(engine)
{
    m_defaultPoster = PosterCache::createPlaceholder(36, 54);
    buildUi();

    // 300 ms'de bir kuyruk durumunu yenile
    m_timerId = g_timeout_add(300, G_SOURCE_FUNC(+[](gpointer userData) -> gboolean {
        static_cast<QueueView*>(userData)->refresh();
        return G_SOURCE_CONTINUE;
    }), this);
}

QueueView::~QueueView() {
    if (m_timerId > 0) {
        g_source_remove(m_timerId);
        m_timerId = 0;
    }
    for (auto& [id, pb] : m_posterPixbufs) {
        if (pb) {
            g_object_unref(pb);
        }
    }
    m_posterPixbufs.clear();
    if (m_defaultPoster) {
        g_object_unref(m_defaultPoster);
        m_defaultPoster = nullptr;
    }
}

GdkPixbuf* QueueView::getPosterPixbuf(const std::string& itemId) {
    if (itemId.empty()) {
        return m_defaultPoster;
    }

    auto it = m_posterPixbufs.find(itemId);
    if (it != m_posterPixbufs.end() && it->second) {
        return it->second;
    }

    GdkPixbuf* placeholder = PosterCache::createPlaceholder(36, 54);
    m_posterPixbufs[itemId] = placeholder;

    std::string sUrl = m_engine.getClient().getServerUrl();
    std::string token = m_engine.getClient().getToken();
    std::string prefix = m_engine.getClient().getPrefix();

    if (!sUrl.empty() && !token.empty()) {
        PosterCache::instance().getOrFetch(
            itemId, sUrl, token, prefix, 36, 54,
            [this, itemId](GdkPixbuf* loadedPb) {
                if (loadedPb) {
                    auto pit = m_posterPixbufs.find(itemId);
                    if (pit != m_posterPixbufs.end() && pit->second) {
                        g_object_unref(pit->second);
                    }
                    g_object_ref(loadedPb);
                    m_posterPixbufs[itemId] = loadedPb;
                    this->refresh();
                }
            }
        );
    }

    return placeholder;
}

void QueueView::buildUi() {
    m_container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(m_container), 10);

    // Üst Araç Çubuğu
    GtkWidget* toolBar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_pack_start(GTK_BOX(m_container), toolBar, FALSE, FALSE, 0);

    GtkWidget* btnStart = gtk_button_new_with_label("Baslat");
    g_signal_connect(btnStart, "clicked", G_CALLBACK(+[](GtkButton*, gpointer userData) {
        auto* self = static_cast<QueueView*>(userData);
        self->m_engine.getQueue().start();
        self->refresh();
    }), this);
    gtk_box_pack_start(GTK_BOX(toolBar), btnStart, FALSE, FALSE, 0);

    GtkWidget* btnPause = gtk_button_new_with_label("Duraklat");
    g_signal_connect(btnPause, "clicked", G_CALLBACK(+[](GtkButton*, gpointer userData) {
        auto* self = static_cast<QueueView*>(userData);
        self->m_engine.getQueue().pause();
        self->refresh();
    }), this);
    gtk_box_pack_start(GTK_BOX(toolBar), btnPause, FALSE, FALSE, 0);

    GtkWidget* btnRetry = gtk_button_new_with_label("Secileni Yeniden Dene");
    g_signal_connect(btnRetry, "clicked", G_CALLBACK(+[](GtkButton*, gpointer userData) {
        auto* self = static_cast<QueueView*>(userData);
        GtkTreeSelection* sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(self->m_treeView));
        GtkTreeModel* model;
        GtkTreeIter iter;
        if (gtk_tree_selection_get_selected(sel, &model, &iter)) {
            gchar* idStr = nullptr;
            gtk_tree_model_get(model, &iter, QCOL_ID, &idStr, -1);
            if (idStr) {
                self->m_engine.getQueue().retryTask(idStr);
                self->m_engine.getQueue().start();
                g_free(idStr);
            }
        } else {
            self->m_engine.getQueue().retryAllFailed();
            self->m_engine.getQueue().start();
        }
        self->refresh();
    }), this);
    gtk_box_pack_start(GTK_BOX(toolBar), btnRetry, FALSE, FALSE, 0);

    GtkWidget* btnCancel = gtk_button_new_with_label("Secileni Iptal Et");
    g_signal_connect(btnCancel, "clicked", G_CALLBACK(+[](GtkButton*, gpointer userData) {
        auto* self = static_cast<QueueView*>(userData);
        GtkTreeSelection* sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(self->m_treeView));
        GtkTreeModel* model;
        GtkTreeIter iter;
        if (gtk_tree_selection_get_selected(sel, &model, &iter)) {
            gchar* idStr = nullptr;
            gtk_tree_model_get(model, &iter, QCOL_ID, &idStr, -1);
            if (idStr) {
                self->m_engine.getQueue().cancelTask(idStr);
                g_free(idStr);
            }
        }
        self->refresh();
    }), this);
    gtk_box_pack_start(GTK_BOX(toolBar), btnCancel, FALSE, FALSE, 0);

    GtkWidget* btnClear = gtk_button_new_with_label("Kuyrugu Temizle");
    g_signal_connect(btnClear, "clicked", G_CALLBACK(+[](GtkButton*, gpointer userData) {
        static_cast<QueueView*>(userData)->m_engine.getQueue().clear();
        static_cast<QueueView*>(userData)->refresh();
    }), this);
    gtk_box_pack_start(GTK_BOX(toolBar), btnClear, FALSE, FALSE, 0);

    GtkWidget* btnFolder = gtk_button_new_with_label("Indirme Klasorunu Ac");
    g_signal_connect(btnFolder, "clicked", G_CALLBACK(+[](GtkButton*, gpointer userData) {
        std::string dir = static_cast<QueueView*>(userData)->m_engine.getDefaultDownloadDir();
#ifdef _WIN32
        std::string cmd = "explorer \"" + dir + "\"";
#elif __APPLE__
        std::string cmd = "open \"" + dir + "\"";
#else
        std::string cmd = "xdg-open \"" + dir + "\" &";
#endif
        int res = std::system(cmd.c_str());
        (void)res;
    }), this);
    gtk_box_pack_start(GTK_BOX(toolBar), btnFolder, FALSE, FALSE, 0);

    m_statusLabel = gtk_label_new("Kuyruk bos.");
    gtk_label_set_xalign(GTK_LABEL(m_statusLabel), 1.0);
    gtk_box_pack_end(GTK_BOX(toolBar), m_statusLabel, TRUE, TRUE, 0);

    // Kuyruk Tablosu
    m_listStore = gtk_list_store_new(
        QNUM_COLS,
        GDK_TYPE_PIXBUF, // Afis
        G_TYPE_STRING,   // Tip ikonu
        G_TYPE_STRING,   // Ad
        G_TYPE_STRING,   // Tur
        G_TYPE_STRING,   // Durum
        G_TYPE_INT,      // Ilerleme (%)
        G_TYPE_STRING,   // Boyut
        G_TYPE_STRING,   // Hiz
        G_TYPE_STRING,   // Kalan Sure
        G_TYPE_STRING,   // Hedef Yol
        G_TYPE_STRING,   // ID
        G_TYPE_STRING    // Item ID
    );

    m_treeView = gtk_tree_view_new_with_model(GTK_TREE_MODEL(m_listStore));
    g_object_unref(m_listStore);

    // Sutun 0: Afis (36x54)
    GtkCellRenderer* posterRenderer = gtk_cell_renderer_pixbuf_new();
    GtkTreeViewColumn* colPoster = gtk_tree_view_column_new_with_attributes(
        "Afis", posterRenderer, "pixbuf", QCOL_POSTER, NULL
    );
    gtk_tree_view_column_set_fixed_width(colPoster, 44);
    gtk_tree_view_column_set_sizing(colPoster, GTK_TREE_VIEW_COLUMN_FIXED);
    gtk_tree_view_append_column(GTK_TREE_VIEW(m_treeView), colPoster);

    // Sutun 1: Tip Ikonu
    GtkCellRenderer* iconRenderer = gtk_cell_renderer_pixbuf_new();
    GtkTreeViewColumn* colIcon = gtk_tree_view_column_new_with_attributes(
        "Tip", iconRenderer, "icon-name", QCOL_ICON, NULL
    );
    gtk_tree_view_column_set_fixed_width(colIcon, 36);
    gtk_tree_view_column_set_sizing(colIcon, GTK_TREE_VIEW_COLUMN_FIXED);
    gtk_tree_view_append_column(GTK_TREE_VIEW(m_treeView), colIcon);

    // Sutun 2: Gorev / Dosya Adi
    GtkCellRenderer* nameRenderer = gtk_cell_renderer_text_new();
    g_object_set(nameRenderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
    GtkTreeViewColumn* colName = gtk_tree_view_column_new_with_attributes(
        "Gorev / Dosya Adi", nameRenderer, "text", QCOL_NAME, NULL
    );
    gtk_tree_view_column_set_expand(colName, TRUE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(m_treeView), colName);

    // Sutun 3: Tur Detayi
    GtkCellRenderer* typeRenderer = gtk_cell_renderer_text_new();
    GtkTreeViewColumn* colType = gtk_tree_view_column_new_with_attributes(
        "Tur", typeRenderer, "text", QCOL_TYPE, NULL
    );
    gtk_tree_view_append_column(GTK_TREE_VIEW(m_treeView), colType);

    // Sutun 4: Durum
    GtkCellRenderer* statusRenderer = gtk_cell_renderer_text_new();
    GtkTreeViewColumn* colStatus = gtk_tree_view_column_new_with_attributes(
        "Durum", statusRenderer, "text", QCOL_STATUS, NULL
    );
    gtk_tree_view_append_column(GTK_TREE_VIEW(m_treeView), colStatus);

    // Sutun 5: Ilerleme Cubugu
    GtkCellRenderer* progressRenderer = gtk_cell_renderer_progress_new();
    GtkTreeViewColumn* colProgress = gtk_tree_view_column_new_with_attributes(
        "Ilerleme", progressRenderer, "value", QCOL_PROGRESS, NULL
    );
    gtk_tree_view_column_set_min_width(colProgress, 120);
    gtk_tree_view_append_column(GTK_TREE_VIEW(m_treeView), colProgress);

    // Sutun 6: Boyut
    GtkCellRenderer* sizeRenderer = gtk_cell_renderer_text_new();
    GtkTreeViewColumn* colSize = gtk_tree_view_column_new_with_attributes(
        "Boyut", sizeRenderer, "text", QCOL_SIZE, NULL
    );
    gtk_tree_view_append_column(GTK_TREE_VIEW(m_treeView), colSize);

    // Sutun 7: Hiz
    GtkCellRenderer* speedRenderer = gtk_cell_renderer_text_new();
    GtkTreeViewColumn* colSpeed = gtk_tree_view_column_new_with_attributes(
        "Hiz", speedRenderer, "text", QCOL_SPEED, NULL
    );
    gtk_tree_view_append_column(GTK_TREE_VIEW(m_treeView), colSpeed);

    // Sutun 8: Kalan Sure (ETA)
    GtkCellRenderer* etaRenderer = gtk_cell_renderer_text_new();
    GtkTreeViewColumn* colEta = gtk_tree_view_column_new_with_attributes(
        "Kalan Sure", etaRenderer, "text", QCOL_ETA, NULL
    );
    gtk_tree_view_append_column(GTK_TREE_VIEW(m_treeView), colEta);

    // Sag Tik Olaylari (Context Menu)
    g_signal_connect(m_treeView, "button-press-event", G_CALLBACK(+[](GtkWidget* widget, GdkEventButton* event, gpointer userData) -> gboolean {
        if (event->type == GDK_BUTTON_PRESS && event->button == GDK_BUTTON_SECONDARY) {
            GtkTreeSelection* selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));
            GtkTreePath* path = nullptr;
            if (gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(widget), (gint)event->x, (gint)event->y, &path, nullptr, nullptr, nullptr)) {
                gtk_tree_selection_unselect_all(selection);
                gtk_tree_selection_select_path(selection, path);
                gtk_tree_path_free(path);
            }
            static_cast<QueueView*>(userData)->showContextMenu(event);
            return GDK_EVENT_STOP;
        }
        return GDK_EVENT_PROPAGATE;
    }), this);

    g_signal_connect(m_treeView, "popup-menu", G_CALLBACK(+[](GtkWidget*, gpointer userData) -> gboolean {
        static_cast<QueueView*>(userData)->showContextMenu(nullptr);
        return GDK_EVENT_STOP;
    }), this);

    GtkWidget* scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scroll), m_treeView);
    gtk_box_pack_start(GTK_BOX(m_container), scroll, TRUE, TRUE, 0);
}

void QueueView::showContextMenu(GdkEventButton* event) {
    GtkTreeSelection* sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(m_treeView));
    GtkTreeModel* model = nullptr;
    GtkTreeIter iter;
    if (!gtk_tree_selection_get_selected(sel, &model, &iter)) {
        return;
    }

    gchar* idCStr = nullptr;
    gtk_tree_model_get(model, &iter, QCOL_ID, &idCStr, -1);
    if (!idCStr) return;
    m_contextTaskId = idCStr;
    g_free(idCStr);

    m_contextTask = m_engine.getQueue().getTask(m_contextTaskId);
    if (m_contextTask.id.empty()) return;

    GtkWidget* menu = gtk_menu_new();
    g_signal_connect(menu, "selection-done", G_CALLBACK(gtk_widget_destroy), NULL);

    // 1. Baslat / Devam Et
    GtkWidget* itemResume = gtk_menu_item_new_with_label("Baslat / Devam Et");
    g_signal_connect(itemResume, "activate", G_CALLBACK(+[](GtkMenuItem*, gpointer userData) {
        auto* self = static_cast<QueueView*>(userData);
        self->m_engine.getQueue().resumeTask(self->m_contextTaskId);
        self->refresh();
    }), this);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), itemResume);
    if (m_contextTask.status == Emby::TaskStatus::Downloading) {
        gtk_widget_set_sensitive(itemResume, FALSE);
    }

    // 2. Duraklat
    GtkWidget* itemPause = gtk_menu_item_new_with_label("Duraklat");
    g_signal_connect(itemPause, "activate", G_CALLBACK(+[](GtkMenuItem*, gpointer userData) {
        auto* self = static_cast<QueueView*>(userData);
        self->m_engine.getQueue().pauseTask(self->m_contextTaskId);
        self->refresh();
    }), this);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), itemPause);
    if (m_contextTask.status != Emby::TaskStatus::Downloading) {
        gtk_widget_set_sensitive(itemPause, FALSE);
    }

    // 3. Yeniden Dene
    GtkWidget* itemRetry = gtk_menu_item_new_with_label("Yeniden Dene");
    g_signal_connect(itemRetry, "activate", G_CALLBACK(+[](GtkMenuItem*, gpointer userData) {
        auto* self = static_cast<QueueView*>(userData);
        self->m_engine.getQueue().retryTask(self->m_contextTaskId);
        self->m_engine.getQueue().start();
        self->refresh();
    }), this);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), itemRetry);

    // 4. Iptal Et
    GtkWidget* itemCancel = gtk_menu_item_new_with_label("Iptal Et");
    g_signal_connect(itemCancel, "activate", G_CALLBACK(+[](GtkMenuItem*, gpointer userData) {
        auto* self = static_cast<QueueView*>(userData);
        self->m_engine.getQueue().cancelTask(self->m_contextTaskId);
        self->refresh();
    }), this);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), itemCancel);
    if (m_contextTask.status == Emby::TaskStatus::Completed || m_contextTask.status == Emby::TaskStatus::Cancelled) {
        gtk_widget_set_sensitive(itemCancel, FALSE);
    }

    // 5. Kuyruktan Kaldir
    GtkWidget* itemRemove = gtk_menu_item_new_with_label("Kuyruktan Kaldir");
    g_signal_connect(itemRemove, "activate", G_CALLBACK(+[](GtkMenuItem*, gpointer userData) {
        auto* self = static_cast<QueueView*>(userData);
        self->m_engine.getQueue().removeTask(self->m_contextTaskId);
        self->refresh();
    }), this);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), itemRemove);

    // Ayirici
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());

    // 6. Dosyayi Ac / Yurut
    GtkWidget* itemOpen = gtk_menu_item_new_with_label("Dosyayi Ac / Yurut");
    g_signal_connect(itemOpen, "activate", G_CALLBACK(+[](GtkMenuItem*, gpointer userData) {
        auto* self = static_cast<QueueView*>(userData);
        openPath(self->m_contextTask.outputPath);
    }), this);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), itemOpen);
    std::error_code ec;
    if (!fs::exists(m_contextTask.outputPath, ec)) {
        gtk_widget_set_sensitive(itemOpen, FALSE);
    }

    // 7. Iceren Klasoru Ac
    GtkWidget* itemFolder = gtk_menu_item_new_with_label("Iceren Klasoru Ac");
    g_signal_connect(itemFolder, "activate", G_CALLBACK(+[](GtkMenuItem*, gpointer userData) {
        auto* self = static_cast<QueueView*>(userData);
        fs::path outP(self->m_contextTask.outputPath);
        std::string parentDir = outP.has_parent_path() ? outP.parent_path().string() : ".";
        openPath(parentDir);
    }), this);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), itemFolder);

    // 8. Dosyayi Diskten Sil
    GtkWidget* itemDelete = gtk_menu_item_new_with_label("Dosyayi Diskten Sil");
    g_signal_connect(itemDelete, "activate", G_CALLBACK(+[](GtkMenuItem*, gpointer userData) {
        auto* self = static_cast<QueueView*>(userData);
        std::string taskId = self->m_contextTaskId;
        std::string taskName = self->m_contextTask.name;

        GtkWidget* toplevel = gtk_widget_get_toplevel(self->m_container);
        GtkWidget* dlg = gtk_message_dialog_new(
            GTK_IS_WINDOW(toplevel) ? GTK_WINDOW(toplevel) : nullptr,
            GTK_DIALOG_MODAL,
            GTK_MESSAGE_QUESTION,
            GTK_BUTTONS_YES_NO,
            "\"%s\" gorevi ve varsa indirilen dosyalari diskten tamamen silinecektir.\nEmin misiniz?",
            taskName.c_str()
        );
        gtk_window_set_title(GTK_WINDOW(dlg), "Dosyayi Diskten Sil");
        gint response = gtk_dialog_run(GTK_DIALOG(dlg));
        gtk_widget_destroy(dlg);

        if (response == GTK_RESPONSE_YES) {
            self->m_engine.getQueue().deleteTaskFiles(taskId, false);
            self->m_engine.getQueue().removeTask(taskId);
            self->refresh();
        }
    }), this);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), itemDelete);

    // Ayirici
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());

    // 9. Indirme Baglantisini Kopyala
    GtkWidget* itemCopyUrl = gtk_menu_item_new_with_label("Indirme Baglantisini Kopyala");
    g_signal_connect(itemCopyUrl, "activate", G_CALLBACK(+[](GtkMenuItem*, gpointer userData) {
        auto* self = static_cast<QueueView*>(userData);
        std::string url = self->m_engine.getQueue().getTaskDownloadUrl(self->m_contextTaskId);
        if (!url.empty()) {
            GtkClipboard* cb = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
            gtk_clipboard_set_text(cb, url.c_str(), -1);
        }
    }), this);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), itemCopyUrl);

    // 10. Ayrintilar...
    GtkWidget* itemDetails = gtk_menu_item_new_with_label("Ayrintilar...");
    g_signal_connect(itemDetails, "activate", G_CALLBACK(+[](GtkMenuItem*, gpointer userData) {
        auto* self = static_cast<QueueView*>(userData);
        self->showTaskDetailsDialog(self->m_contextTask);
    }), this);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), itemDetails);

    gtk_widget_show_all(menu);
#if GTK_CHECK_VERSION(3, 22, 0)
    if (event) {
        gtk_menu_popup_at_pointer(GTK_MENU(menu), reinterpret_cast<const GdkEvent*>(event));
    } else {
        gtk_menu_popup_at_widget(GTK_MENU(menu), m_treeView, GDK_GRAVITY_CENTER, GDK_GRAVITY_CENTER, nullptr);
    }
#else
    gtk_menu_popup(GTK_MENU(menu), nullptr, nullptr, nullptr, nullptr,
                   event ? event->button : 0,
                   event ? event->time : gtk_get_current_event_time());
#endif
}

void QueueView::showTaskDetailsDialog(const Emby::QueueTask& task) {
    GtkWidget* toplevel = gtk_widget_get_toplevel(m_container);
    GtkWidget* dlg = gtk_dialog_new_with_buttons(
        "Gorev Ayrintilari",
        GTK_IS_WINDOW(toplevel) ? GTK_WINDOW(toplevel) : nullptr,
        GTK_DIALOG_MODAL,
        "Kapat", GTK_RESPONSE_CLOSE,
        NULL
    );
    gtk_window_set_default_size(GTK_WINDOW(dlg), 560, 420);

    GtkWidget* contentArea = gtk_dialog_get_content_area(GTK_DIALOG(dlg));
    gtk_container_set_border_width(GTK_CONTAINER(contentArea), 14);

    GtkWidget* grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
    gtk_box_pack_start(GTK_BOX(contentArea), grid, TRUE, TRUE, 0);

    int row = 0;
    auto addRow = [&](const std::string& labelText, const std::string& valText, bool isEntry = false) {
        GtkWidget* lbl = gtk_label_new((labelText + ":").c_str());
        gtk_label_set_xalign(GTK_LABEL(lbl), 0.0);
        gtk_grid_attach(GTK_GRID(grid), lbl, 0, row, 1, 1);

        if (isEntry) {
            GtkWidget* entry = gtk_entry_new();
            gtk_entry_set_text(GTK_ENTRY(entry), valText.c_str());
            gtk_editable_set_editable(GTK_EDITABLE(entry), FALSE);
            gtk_widget_set_hexpand(entry, TRUE);
            gtk_grid_attach(GTK_GRID(grid), entry, 1, row, 1, 1);
        } else {
            GtkWidget* val = gtk_label_new(valText.c_str());
            gtk_label_set_xalign(GTK_LABEL(val), 0.0);
            gtk_label_set_selectable(GTK_LABEL(val), TRUE);
            gtk_widget_set_hexpand(val, TRUE);
            gtk_grid_attach(GTK_GRID(grid), val, 1, row, 1, 1);
        }
        row++;
    };

    addRow("Gorev Adi", task.name);
    addRow("Gorev ID", task.id);
    addRow("Icerik ID (Emby)", task.itemId);
    
    std::string typeStr = "Video";
    if (task.type == Emby::TaskType::Audio) typeStr = "Ses";
    if (task.type == Emby::TaskType::Subtitle) typeStr = "Altyazi";
    addRow("Tur", typeStr);

    std::string statusStr = "Bekliyor";
    if (task.status == Emby::TaskStatus::Downloading) statusStr = "Indiriliyor";
    else if (task.status == Emby::TaskStatus::Paused) statusStr = "Duraklatildi";
    else if (task.status == Emby::TaskStatus::Completed) statusStr = "Tamamlandi";
    else if (task.status == Emby::TaskStatus::Error) statusStr = "Hata";
    else if (task.status == Emby::TaskStatus::Cancelled) statusStr = "Iptal Edildi";
    addRow("Durum", statusStr);

    std::string codecInfo = "Konteyner: " + task.container;
    if (!task.streamCodec.empty()) codecInfo += ", Codec: " + task.streamCodec;
    if (!task.streamLanguage.empty()) codecInfo += ", Dil: " + task.streamLanguage;
    if (task.streamBitrate > 0) codecInfo += ", Bitrate: " + std::to_string(task.streamBitrate / 1000) + " kbps";
    addRow("Format / Akis", codecInfo);

    std::string sizeInfo = Emby::Platform::formatBytes(task.progress.currentBytes) + " / " +
                           Emby::Platform::formatBytes(task.expectedSize > 0 ? task.expectedSize : task.progress.totalBytes) +
                           " (%" + std::to_string(static_cast<int>(task.progress.percent)) + ")";
    addRow("Ilerleme & Boyut", sizeInfo);

    addRow("Hedef Dosya Yolu", task.outputPath, true);

    std::string directUrl = m_engine.getQueue().getTaskDownloadUrl(task.id);
    if (!directUrl.empty()) {
        addRow("Indirme Baglantisi", directUrl, true);
    }

    if (!task.error.empty()) {
        addRow("Hata Aciklamasi", task.error);
    }

    gtk_widget_show_all(dlg);
    gtk_dialog_run(GTK_DIALOG(dlg));
    gtk_widget_destroy(dlg);
}

void QueueView::refresh() {
    auto tasks = m_engine.getQueue().getTasks();

    size_t pendingCount = 0;
    size_t activeCount = 0;
    size_t pausedCount = 0;
    size_t completedCount = 0;

    gint storeRows = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(m_listStore), NULL);
    bool canUpdateInPlace = (storeRows == static_cast<gint>(tasks.size()));

    if (canUpdateInPlace) {
        GtkTreeIter checkIter;
        gboolean valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(m_listStore), &checkIter);
        for (size_t i = 0; i < tasks.size() && valid; ++i) {
            gchar* idStr = nullptr;
            gtk_tree_model_get(GTK_TREE_MODEL(m_listStore), &checkIter, QCOL_ID, &idStr, -1);
            if (!idStr || tasks[i].id != idStr) {
                if (idStr) g_free(idStr);
                canUpdateInPlace = false;
                break;
            }
            g_free(idStr);
            valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(m_listStore), &checkIter);
        }
    }

    if (!canUpdateInPlace) {
        gtk_list_store_clear(m_listStore);
    }

    GtkTreeIter iter;
    gboolean validIter = canUpdateInPlace ? gtk_tree_model_get_iter_first(GTK_TREE_MODEL(m_listStore), &iter) : FALSE;

    for (size_t i = 0; i < tasks.size(); ++i) {
        const auto& t = tasks[i];

        std::string iconName = "video-x-generic";
        std::string typeStr = "Video [" + t.container + "]";
        if (t.type == Emby::TaskType::Audio) {
            iconName = "audio-x-generic";
            typeStr = "Ses [" + (t.streamCodec.empty() ? t.container : t.streamCodec);
            if (!t.streamLanguage.empty()) typeStr += " / " + t.streamLanguage;
            typeStr += "]";
        } else if (t.type == Emby::TaskType::Subtitle) {
            iconName = "text-x-generic";
            typeStr = "Altyazi [" + (t.streamCodec.empty() ? t.container : t.streamCodec);
            if (!t.streamLanguage.empty()) typeStr += " / " + t.streamLanguage;
            typeStr += "]";
        }

        std::string statusStr = "Bekliyor";
        if (t.status == Emby::TaskStatus::Downloading) {
            statusStr = "Indiriliyor";
            activeCount++;
        } else if (t.status == Emby::TaskStatus::Paused) {
            statusStr = "Duraklatildi";
            pausedCount++;
        } else if (t.status == Emby::TaskStatus::Completed) {
            statusStr = "Tamamlandi";
            completedCount++;
        } else if (t.status == Emby::TaskStatus::Error) {
            statusStr = "Hata: " + t.error;
        } else if (t.status == Emby::TaskStatus::Cancelled) {
            statusStr = "Iptal Edildi";
        } else {
            pendingCount++;
        }

        int percent = static_cast<int>(t.progress.percent);
        std::string sizeStr = Emby::Platform::formatBytes(t.progress.currentBytes) + " / " + 
                              Emby::Platform::formatBytes(t.expectedSize > 0 ? t.expectedSize : t.progress.totalBytes);

        std::string speedStr = "-";
        std::string etaStr = "-";
        if (t.status == Emby::TaskStatus::Downloading && t.progress.speedBps > 0) {
            speedStr = Emby::Platform::formatBytes(static_cast<uint64_t>(t.progress.speedBps)) + "/s";
            if (t.progress.remainingSeconds > 0) {
                uint64_t m = t.progress.remainingSeconds / 60;
                uint64_t s = t.progress.remainingSeconds % 60;
                std::ostringstream ss;
                ss << std::setfill('0') << std::setw(2) << m << ":"
                   << std::setfill('0') << std::setw(2) << s;
                etaStr = ss.str();
            }
        }

        GdkPixbuf* posterPb = getPosterPixbuf(t.itemId);

        if (canUpdateInPlace && validIter) {
            gtk_list_store_set(
                m_listStore, &iter,
                QCOL_POSTER, posterPb,
                QCOL_ICON, iconName.c_str(),
                QCOL_NAME, t.name.c_str(),
                QCOL_TYPE, typeStr.c_str(),
                QCOL_STATUS, statusStr.c_str(),
                QCOL_PROGRESS, percent,
                QCOL_SIZE, sizeStr.c_str(),
                QCOL_SPEED, speedStr.c_str(),
                QCOL_ETA, etaStr.c_str(),
                QCOL_PATH, t.outputPath.c_str(),
                QCOL_ID, t.id.c_str(),
                QCOL_ITEM_ID, t.itemId.c_str(),
                -1
            );
            validIter = gtk_tree_model_iter_next(GTK_TREE_MODEL(m_listStore), &iter);
        } else {
            GtkTreeIter newIter;
            gtk_list_store_append(m_listStore, &newIter);
            gtk_list_store_set(
                m_listStore, &newIter,
                QCOL_POSTER, posterPb,
                QCOL_ICON, iconName.c_str(),
                QCOL_NAME, t.name.c_str(),
                QCOL_TYPE, typeStr.c_str(),
                QCOL_STATUS, statusStr.c_str(),
                QCOL_PROGRESS, percent,
                QCOL_SIZE, sizeStr.c_str(),
                QCOL_SPEED, speedStr.c_str(),
                QCOL_ETA, etaStr.c_str(),
                QCOL_PATH, t.outputPath.c_str(),
                QCOL_ID, t.id.c_str(),
                QCOL_ITEM_ID, t.itemId.c_str(),
                -1
            );
        }
    }

    std::string summary = "Gorevler: " + std::to_string(tasks.size()) + 
                          " (Aktif: " + std::to_string(activeCount);
    if (pausedCount > 0) {
        summary += ", Duraklatildi: " + std::to_string(pausedCount);
    }
    summary += ", Bekleyen: " + std::to_string(pendingCount) + 
               ", Tamamlanan: " + std::to_string(completedCount) + ")";
    gtk_label_set_text(GTK_LABEL(m_statusLabel), summary.c_str());
}
