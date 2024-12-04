#include <locale.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>

GtkBuilder *builder;

void init_plugin (void)
{
    setlocale (LC_ALL, "");
    bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
    textdomain (GETTEXT_PACKAGE);

    builder = gtk_builder_new_from_file (PACKAGE_DATA_DIR "/ui/test.ui");
}

int plugin_tabs (void)
{
    return 1;
}

const char *tab_name (int tab)
{
    return "Test plugin";
}

GtkWidget *get_tab (int tab)
{
    GtkWidget *window, *plugin;

    window = (GtkWidget *) gtk_builder_get_object (builder, "dummy_wd");
    plugin = (GtkWidget *) gtk_builder_get_object (builder, "contents");

    gtk_container_remove (GTK_CONTAINER (window), plugin);

    return plugin;
}

void free_plugin (void)
{
    g_object_unref (builder);
}
