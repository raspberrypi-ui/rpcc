/*============================================================================
Copyright (c) 2024 Raspberry Pi Holdings Ltd.
Some code based on lxinput from the LXDE project :
Copyright (c) 2009-2014 PCMan, martyj19, Julien Lavergne, Andri Grytsenko
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

/*----------------------------------------------------------------------------*/
/* Global data */
/*----------------------------------------------------------------------------*/

GList *plugin_handles = NULL;

/*----------------------------------------------------------------------------*/
/* Function prototypes */
/*----------------------------------------------------------------------------*/

static int (*plugin_tabs) (void);
static void (*init_plugin) (void);
static const char *(*plugin_name) (int tab);
static GtkWidget *(*get_plugin) (int tab);
static void load_plugin (GtkWidget *nb, const char *filename);
static void (*free_plugin) (void);

/*----------------------------------------------------------------------------*/
/* Helpers */
/*----------------------------------------------------------------------------*/

static void load_plugin (GtkWidget *nb, const char *filename)
{
    GtkWidget *label, *page;
    void *phandle;
    char *path;
    int tab;

    if (!strstr (filename, ".so")) return;
    path = g_build_filename (PLUGIN_PATH, filename, NULL);
    phandle = dlopen (path, RTLD_LAZY);
    g_free (path);

    init_plugin = dlsym (phandle, "init_plugin");
    plugin_tabs = dlsym (phandle, "plugin_tabs");
    plugin_name = dlsym (phandle, "plugin_name");
    get_plugin = dlsym (phandle, "get_plugin");

    init_plugin ();

    for (tab = 0; tab < plugin_tabs (); tab++)
    {
        label = gtk_label_new (plugin_name (tab));
        page = get_plugin (tab);
        gtk_notebook_insert_page (GTK_NOTEBOOK (nb), page, label, -1);
    }

    plugin_handles = g_list_append (plugin_handles, phandle);
}

static void free_plugins (void *phandle, gpointer)
{
    free_plugin = dlsym (phandle, "free_plugin");
    free_plugin ();
    dlclose (phandle);
}

/*----------------------------------------------------------------------------*/
/* Main function */
/*----------------------------------------------------------------------------*/

int main (int argc, char* argv[])
{
    GtkBuilder *builder;
    GtkWidget *dlg, *nb;
    DIR *d;
    struct dirent *dir;

    setlocale (LC_ALL, "");
    bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
    textdomain (GETTEXT_PACKAGE);

    gtk_init (&argc, &argv);

    /* create the dialog */
    builder = gtk_builder_new_from_file (PACKAGE_DATA_DIR "/ui/rpcc.ui");

    dlg = (GtkWidget *) gtk_builder_get_object (builder, "dlg");
    nb = (GtkWidget *) gtk_builder_get_object (builder, "notebook");
    gtk_window_set_default_size (GTK_WINDOW (dlg), 500, 400);

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

    /* run the dialog */
    gtk_dialog_run (GTK_DIALOG (dlg));

    gtk_widget_destroy (dlg);

    /* close the plugins cleanly */
    g_list_foreach (plugin_handles, free_plugins, NULL);

    return 0;
}

/* End of file */
/*============================================================================*/
