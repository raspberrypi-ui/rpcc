/*============================================================================
Copyright (c) 2024 Raspberry Pi Holdings Ltd.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
============================================================================*/

#include <locale.h>
#include <dlfcn.h>
#include <dirent.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include "rpcc.h"

/*----------------------------------------------------------------------------*/
/* Typedefs and macros */
/*----------------------------------------------------------------------------*/

typedef enum {
    WM_OPENBOX,
    WM_WAYFIRE,
    WM_LABWC } wm_type;

/*----------------------------------------------------------------------------*/
/* Global data */
/*----------------------------------------------------------------------------*/

static wm_type wm;
static GList *plugin_handles = NULL;
static GtkWidget *dlg, *msg_dlg;
static gulong draw_id;
static gboolean reboot = FALSE;

/*----------------------------------------------------------------------------*/
/* Function prototypes */
/*----------------------------------------------------------------------------*/

static int (*plugin_tabs) (void);
static void (*init_plugin) (void);
static const char *(*tab_name) (int tab);
static GtkWidget *(*get_tab) (int tab);
static gboolean (*reboot_needed) (void);
static void (*free_plugin) (void);

static void load_plugin (GtkWidget *nb, const char *filename);
static void free_plugins (void *phandle, gpointer);
static void call_func (void *phandle, gpointer data);
static void call_plugin_func (char *name);
static void reboot_check (void *phandle, gpointer);
static void close_with_prompt (void);
static gboolean close_app (GtkButton *button, gpointer);
static gboolean close_app_reboot (GtkButton *button, gpointer);
static void message (char *msg);
static gboolean ok_main (GtkButton *button, gpointer data);
static gboolean close_prog (GtkWidget *widget, GdkEvent *event, gpointer data);
static gboolean init_window (gpointer);
static gboolean event (GtkWidget *wid, GdkEventWindowState *ev, gpointer data);
static gboolean draw (GtkWidget *wid, cairo_t *cr, gpointer data);

/*----------------------------------------------------------------------------*/
/* Plugin management */
/*----------------------------------------------------------------------------*/

static void load_plugin (GtkWidget *nb, const char *filename)
{
    GtkWidget *label, *page;
    void *phandle;
    char *path;
    int count, tab;
    const char *name, *tablabel;

    if (!strstr (filename, ".so")) return;
    path = g_build_filename (PLUGIN_PATH, filename, NULL);
    phandle = dlopen (path, RTLD_LAZY);
    g_free (path);

    init_plugin = dlsym (phandle, "init_plugin");
    plugin_tabs = dlsym (phandle, "plugin_tabs");
    tab_name = dlsym (phandle, "tab_name");
    get_tab = dlsym (phandle, "get_tab");

    init_plugin ();

    for (tab = 0; tab < plugin_tabs (); tab++)
    {
        name = tab_name (tab);
        label = gtk_label_new (name);
        page = get_tab (tab);
        for (count = 0; count < gtk_notebook_get_n_pages (GTK_NOTEBOOK (nb)); count++)
        {
            tablabel = gtk_notebook_get_tab_label_text (GTK_NOTEBOOK (nb), gtk_notebook_get_nth_page (GTK_NOTEBOOK (nb), count));
            if (strcmp (tablabel, name) > 0) break;
        }
        gtk_notebook_insert_page (GTK_NOTEBOOK (nb), page, label, count);
    }

    plugin_handles = g_list_append (plugin_handles, phandle);
}

static void free_plugins (void *phandle, gpointer)
{
    free_plugin = dlsym (phandle, "free_plugin");
    free_plugin ();
    dlclose (phandle);
}

static void call_func (void *phandle, gpointer data)
{
    void (*func) (void) = dlsym (phandle, (char *) data);
    if (func) func ();
}

static void call_plugin_func (char *name)
{
    g_list_foreach (plugin_handles, call_func, name);
}

/*----------------------------------------------------------------------------*/
/* Reboot prompt */
/*----------------------------------------------------------------------------*/

static void reboot_check (void *phandle, gpointer)
{
    reboot_needed = dlsym (phandle, "reboot_needed");
    if (reboot_needed ()) reboot = TRUE;
}

static void close_with_prompt (void)
{
    g_list_foreach (plugin_handles, reboot_check, NULL);

    gtk_widget_destroy (dlg);
    if (reboot)
    {
        GtkWidget *wid;
        GtkBuilder *builder = gtk_builder_new_from_file (PACKAGE_DATA_DIR "/ui/rpcc.ui");

        msg_dlg = (GtkWidget *) gtk_builder_get_object (builder, "modal");

        wid = (GtkWidget *) gtk_builder_get_object (builder, "modal_msg");
        gtk_label_set_text (GTK_LABEL (wid), _("The changes you have made require the Raspberry Pi to be rebooted to take effect.\n\nWould you like to reboot now? "));

        wid = (GtkWidget *) gtk_builder_get_object (builder, "modal_cancel");
        gtk_button_set_label (GTK_BUTTON (wid), _("_No"));
        g_signal_connect (wid, "clicked", G_CALLBACK (close_app), NULL);
        gtk_widget_show (wid);

        wid = (GtkWidget *) gtk_builder_get_object (builder, "modal_ok");
        gtk_button_set_label (GTK_BUTTON (wid), _("_Yes"));
        g_signal_connect (wid, "clicked", G_CALLBACK (close_app_reboot), NULL);
        gtk_widget_show (wid);

        wid = (GtkWidget *) gtk_builder_get_object (builder, "modal_buttons");
        gtk_widget_show (wid);

        gtk_widget_show (msg_dlg);

        g_object_unref (builder);
    }
    else gtk_main_quit ();
}

static gboolean close_app (GtkButton *button, gpointer)
{
    gtk_widget_destroy (msg_dlg);
    gtk_main_quit ();
    return FALSE;
}

static gboolean close_app_reboot (GtkButton *button, gpointer)
{
    gtk_widget_destroy (msg_dlg);
    gtk_main_quit ();
    system ("reboot");
    return FALSE;
}

/*----------------------------------------------------------------------------*/
/* Message box */
/*----------------------------------------------------------------------------*/

static void message (char *msg)
{
    GtkWidget *wid;
    GtkBuilder *builder = gtk_builder_new_from_file (PACKAGE_DATA_DIR "/ui/rpcc.ui");

    msg_dlg = (GtkWidget *) gtk_builder_get_object (builder, "modal");
    if (dlg) gtk_window_set_transient_for (GTK_WINDOW (msg_dlg), GTK_WINDOW (dlg));

    wid = (GtkWidget *) gtk_builder_get_object (builder, "modal_msg");
    gtk_label_set_text (GTK_LABEL (wid), msg);

    gtk_widget_show (msg_dlg);

    g_object_unref (builder);
}

/*----------------------------------------------------------------------------*/
/* Button handlers */
/*----------------------------------------------------------------------------*/

static gboolean ok_main (GtkButton *button, gpointer data)
{
    close_with_prompt ();
    return FALSE;
}

static gboolean close_prog (GtkWidget *widget, GdkEvent *event, gpointer data)
{
    close_with_prompt ();
    return TRUE;
}

/*----------------------------------------------------------------------------*/
/* Startup */
/*----------------------------------------------------------------------------*/

static gboolean init_window (gpointer)
{
    GtkBuilder *builder;
    GtkWidget *nb;
    DIR *d;
    struct dirent *dir;

    /* create the dialog */
    builder = gtk_builder_new_from_file (PACKAGE_DATA_DIR "/ui/rpcc.ui");

    dlg = (GtkWidget *) gtk_builder_get_object (builder, "dlg");
    nb = (GtkWidget *) gtk_builder_get_object (builder, "notebook");
    gtk_window_set_default_size (GTK_WINDOW (dlg), 500, 400);

    g_signal_connect (dlg, "delete_event", G_CALLBACK (close_prog), NULL);
    g_signal_connect (gtk_builder_get_object (builder, "btn_close"), "clicked", G_CALLBACK (ok_main), NULL);

    g_object_unref (builder);

    /* loop thorough plugins */
    if ((d = opendir (PLUGIN_PATH)))
    {
        while ((dir = readdir (d)))
        {
            load_plugin (nb, dir->d_name);
        }
    }
    closedir (d);

    gtk_widget_show (dlg);
    gtk_widget_destroy (msg_dlg);

    return FALSE;
}

static gboolean event (GtkWidget *wid, GdkEventWindowState *ev, gpointer data)
{
    if (ev->type == GDK_WINDOW_STATE)
    {
        if (ev->changed_mask == GDK_WINDOW_STATE_FOCUSED
            && ev->new_window_state & GDK_WINDOW_STATE_FOCUSED)
                g_idle_add (init_window, NULL);
    }
    return FALSE;
}

static gboolean draw (GtkWidget *wid, cairo_t *cr, gpointer data)
{
    g_signal_handler_disconnect (wid, draw_id);
    g_idle_add (init_window, NULL);
    return FALSE;
}

/*----------------------------------------------------------------------------*/
/* Main function */
/*----------------------------------------------------------------------------*/

int main (int argc, char* argv[])
{
    setlocale (LC_ALL, "");
    bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
    textdomain (GETTEXT_PACKAGE);

    if (getenv ("WAYLAND_DISPLAY"))
    {
        if (getenv ("WAYFIRE_CONFIG_FILE")) wm = WM_WAYFIRE;
        else wm = WM_LABWC;
    }
    else wm = WM_OPENBOX;

    gtk_init (&argc, &argv);

    /* show wait message */
    message (_("Loading configuration - please wait..."));
    if (wm != WM_OPENBOX) draw_id = g_signal_connect (msg_dlg, "event", G_CALLBACK (event), NULL);
    else draw_id = g_signal_connect (msg_dlg, "draw", G_CALLBACK (draw), NULL);

    gtk_main ();

    /* close the plugins cleanly */
    g_list_foreach (plugin_handles, free_plugins, NULL);

    return 0;
}

/* End of file */
/*============================================================================*/
