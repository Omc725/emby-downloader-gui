#include "server_dialog.hpp"

bool ServerDialog::show(GtkWindow* parent, Emby::EmbyEngine& engine) {
    ServerDialog dlg(parent, engine);
    return dlg.run();
}

ServerDialog::ServerDialog(GtkWindow* parent, Emby::EmbyEngine& engine)
    : m_parent(parent), m_engine(engine)
{
    m_dialog = gtk_dialog_new_with_buttons(
        "Yeni Emby Sunucusu Bagla",
        m_parent,
        static_cast<GtkDialogFlags>(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
        "_Iptal", GTK_RESPONSE_CANCEL,
        "_Baglan", GTK_RESPONSE_ACCEPT,
        NULL
    );

    gtk_window_set_default_size(GTK_WINDOW(m_dialog), 450, 320);
    gtk_container_set_border_width(GTK_CONTAINER(m_dialog), 12);

    GtkWidget* contentArea = gtk_dialog_get_content_area(GTK_DIALOG(m_dialog));
    GtkWidget* grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    gtk_container_add(GTK_CONTAINER(contentArea), grid);

    // Sunucu Adresi
    GtkWidget* lblUrl = gtk_label_new("Sunucu Adresi (URL):");
    gtk_label_set_xalign(GTK_LABEL(lblUrl), 0.0);
    gtk_grid_attach(GTK_GRID(grid), lblUrl, 0, 0, 1, 1);

    m_urlEntry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(m_urlEntry), "https://emby.ornek.com");
    gtk_grid_attach(GTK_GRID(grid), m_urlEntry, 1, 0, 1, 1);

    // Port
    GtkWidget* lblPort = gtk_label_new("Port:");
    gtk_label_set_xalign(GTK_LABEL(lblPort), 0.0);
    gtk_grid_attach(GTK_GRID(grid), lblPort, 0, 1, 1, 1);

    m_portEntry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(m_portEntry), "443");
    gtk_grid_attach(GTK_GRID(grid), m_portEntry, 1, 1, 1, 1);

    // Prefix
    GtkWidget* lblPrefix = gtk_label_new("Yol On Eki (Prefix):");
    gtk_label_set_xalign(GTK_LABEL(lblPrefix), 0.0);
    gtk_grid_attach(GTK_GRID(grid), lblPrefix, 0, 2, 1, 1);

    m_prefixEntry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(m_prefixEntry), "/emby");
    gtk_grid_attach(GTK_GRID(grid), m_prefixEntry, 1, 2, 1, 1);

    // Kullanıcı Adı
    GtkWidget* lblUser = gtk_label_new("Kullanici Adi:");
    gtk_label_set_xalign(GTK_LABEL(lblUser), 0.0);
    gtk_grid_attach(GTK_GRID(grid), lblUser, 0, 3, 1, 1);

    m_userEntry = gtk_entry_new();
    gtk_grid_attach(GTK_GRID(grid), m_userEntry, 1, 3, 1, 1);

    // Parola
    GtkWidget* lblPass = gtk_label_new("Parola:");
    gtk_label_set_xalign(GTK_LABEL(lblPass), 0.0);
    gtk_grid_attach(GTK_GRID(grid), lblPass, 0, 4, 1, 1);

    m_passEntry = gtk_entry_new();
    gtk_entry_set_visibility(GTK_ENTRY(m_passEntry), FALSE);
    gtk_grid_attach(GTK_GRID(grid), m_passEntry, 1, 4, 1, 1);

    // Hata Mesajı
    m_errorLabel = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(m_errorLabel), 0.0);
    gtk_grid_attach(GTK_GRID(grid), m_errorLabel, 0, 5, 2, 1);

    gtk_widget_show_all(contentArea);
}

bool ServerDialog::run() {
    while (true) {
        gint res = gtk_dialog_run(GTK_DIALOG(m_dialog));
        if (res != GTK_RESPONSE_ACCEPT) {
            gtk_widget_destroy(m_dialog);
            return false;
        }

        std::string url = gtk_entry_get_text(GTK_ENTRY(m_urlEntry));
        std::string port = gtk_entry_get_text(GTK_ENTRY(m_portEntry));
        std::string prefix = gtk_entry_get_text(GTK_ENTRY(m_prefixEntry));
        std::string user = gtk_entry_get_text(GTK_ENTRY(m_userEntry));
        std::string pass = gtk_entry_get_text(GTK_ENTRY(m_passEntry));

        std::string err;
        bool ok = m_engine.connect(url, port, user, pass, err, prefix);
        if (ok) {
            gtk_widget_destroy(m_dialog);
            return true;
        }

        std::string markup = "<span color='red'>" + err + "</span>";
        gtk_label_set_markup(GTK_LABEL(m_errorLabel), markup.c_str());
    }
}
