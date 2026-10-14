#include "queue_view.hpp"
#include <iomanip>
#include <sstream>
#include <cstdlib>

enum {
    QCOL_NAME = 0,
    QCOL_TYPE,
    QCOL_STATUS,
    QCOL_PROGRESS,
    QCOL_SIZE,
    QCOL_SPEED,
    QCOL_ID,
    QNUM_COLS
};

QueueView::QueueView(Emby::EmbyEngine& engine)
    : m_engine(engine)
{
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
    }
}

void QueueView::buildUi() {
    m_container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(m_container), 10);

    // Üst Araç Çubuğu
    GtkWidget* toolBar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_pack_start(GTK_BOX(m_container), toolBar, FALSE, FALSE, 0);

    GtkWidget* btnStart = gtk_button_new_with_label("Baslat");
    g_signal_connect(btnStart, "clicked", G_CALLBACK(+[](GtkButton*, gpointer userData) {
        static_cast<QueueView*>(userData)->m_engine.getQueue().start();
    }), this);
    gtk_box_pack_start(GTK_BOX(toolBar), btnStart, FALSE, FALSE, 0);

    GtkWidget* btnPause = gtk_button_new_with_label("Duraklat");
    g_signal_connect(btnPause, "clicked", G_CALLBACK(+[](GtkButton*, gpointer userData) {
        static_cast<QueueView*>(userData)->m_engine.getQueue().pause();
    }), this);
    gtk_box_pack_start(GTK_BOX(toolBar), btnPause, FALSE, FALSE, 0);

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
        G_TYPE_STRING,  // Adı
        G_TYPE_STRING,  // Tür
        G_TYPE_STRING,  // Durum
        G_TYPE_INT,     // İlerleme (%)
        G_TYPE_STRING,  // Boyut
        G_TYPE_STRING,  // Hız
        G_TYPE_STRING   // ID
    );

    m_treeView = gtk_tree_view_new_with_model(GTK_TREE_MODEL(m_listStore));
    g_object_unref(m_listStore);

    GtkCellRenderer* textRenderer = gtk_cell_renderer_text_new();

    // Sütun: Görev Adı
    GtkTreeViewColumn* colName = gtk_tree_view_column_new_with_attributes(
        "Gorev / Dosya Adi", textRenderer, "text", QCOL_NAME, NULL
    );
    gtk_tree_view_column_set_expand(colName, TRUE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(m_treeView), colName);

    // Sütun: Tür
    GtkTreeViewColumn* colType = gtk_tree_view_column_new_with_attributes(
        "Tur", textRenderer, "text", QCOL_TYPE, NULL
    );
    gtk_tree_view_append_column(GTK_TREE_VIEW(m_treeView), colType);

    // Sütun: Durum
    GtkTreeViewColumn* colStatus = gtk_tree_view_column_new_with_attributes(
        "Durum", textRenderer, "text", QCOL_STATUS, NULL
    );
    gtk_tree_view_append_column(GTK_TREE_VIEW(m_treeView), colStatus);

    // Sütun: İlerleme Çubuğu
    GtkCellRenderer* progressRenderer = gtk_cell_renderer_progress_new();
    GtkTreeViewColumn* colProgress = gtk_tree_view_column_new_with_attributes(
        "Ilerleme", progressRenderer, "value", QCOL_PROGRESS, NULL
    );
    gtk_tree_view_column_set_min_width(colProgress, 120);
    gtk_tree_view_append_column(GTK_TREE_VIEW(m_treeView), colProgress);

    // Sütun: Boyut
    GtkTreeViewColumn* colSize = gtk_tree_view_column_new_with_attributes(
        "Boyut", textRenderer, "text", QCOL_SIZE, NULL
    );
    gtk_tree_view_append_column(GTK_TREE_VIEW(m_treeView), colSize);

    // Sütun: Hız & Kalan Süre
    GtkTreeViewColumn* colSpeed = gtk_tree_view_column_new_with_attributes(
        "Hiz / ETA", textRenderer, "text", QCOL_SPEED, NULL
    );
    gtk_tree_view_append_column(GTK_TREE_VIEW(m_treeView), colSpeed);

    GtkWidget* scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scroll), m_treeView);
    gtk_box_pack_start(GTK_BOX(m_container), scroll, TRUE, TRUE, 0);
}

void QueueView::refresh() {
    auto tasks = m_engine.getQueue().getTasks();

    // Tabloyu güncelle
    gtk_list_store_clear(m_listStore);

    size_t pendingCount = 0;
    size_t activeCount = 0;
    size_t completedCount = 0;

    for (const auto& t : tasks) {
        std::string typeStr = "Video";
        if (t.type == Emby::TaskType::Audio) typeStr = "Ses (" + t.streamLanguage + ")";
        if (t.type == Emby::TaskType::Subtitle) typeStr = "Altyazi (" + t.streamLanguage + ")";

        std::string statusStr = "Bekliyor";
        if (t.status == Emby::TaskStatus::Downloading) {
            statusStr = "Indiriliyor";
            activeCount++;
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
        if (t.status == Emby::TaskStatus::Downloading && t.progress.speedBps > 0) {
            speedStr = Emby::Platform::formatBytes(static_cast<uint64_t>(t.progress.speedBps)) + "/s";
            if (t.progress.remainingSeconds > 0) {
                uint64_t m = t.progress.remainingSeconds / 60;
                uint64_t s = t.progress.remainingSeconds % 60;
                std::ostringstream ss;
                ss << " (" << std::setfill('0') << std::setw(2) << m << ":"
                   << std::setfill('0') << std::setw(2) << s << ")";
                speedStr += ss.str();
            }
        }

        GtkTreeIter iter;
        gtk_list_store_append(m_listStore, &iter);
        gtk_list_store_set(
            m_listStore, &iter,
            QCOL_NAME, t.name.c_str(),
            QCOL_TYPE, typeStr.c_str(),
            QCOL_STATUS, statusStr.c_str(),
            QCOL_PROGRESS, percent,
            QCOL_SIZE, sizeStr.c_str(),
            QCOL_SPEED, speedStr.c_str(),
            QCOL_ID, t.id.c_str(),
            -1
        );
    }

    std::string summary = "Gorevler: " + std::to_string(tasks.size()) + 
                          " (Aktif: " + std::to_string(activeCount) + 
                          ", Bekleyen: " + std::to_string(pendingCount) + 
                          ", Tamamlanan: " + std::to_string(completedCount) + ")";
    gtk_label_set_text(GTK_LABEL(m_statusLabel), summary.c_str());
}
