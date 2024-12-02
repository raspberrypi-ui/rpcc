#include <locale.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>

GtkWidget *plugin;

const char *plugin_name (void)
{
    return "Test plugin";
}

GtkWidget *get_plugin (void)
{
    GtkBuilder *builder;
    GtkWidget *window;

    setlocale (LC_ALL, "");
    bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
    textdomain (GETTEXT_PACKAGE);

    builder = gtk_builder_new_from_file (PACKAGE_DATA_DIR "/ui/test.ui");

    window = (GtkWidget *) gtk_builder_get_object (builder, "dummy_wd");
    plugin = (GtkWidget *) gtk_builder_get_object (builder, "contents");

    g_object_ref (plugin);
    gtk_container_remove (GTK_CONTAINER (window), plugin);

    g_object_unref (builder);

    return plugin;
}
