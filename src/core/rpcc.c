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
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include "rpcc.h"

/*----------------------------------------------------------------------------*/
/* Typedefs and macros */
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/* Global data */
/*----------------------------------------------------------------------------*/

GtkWidget *dlg, *nb;

/*----------------------------------------------------------------------------*/
/* Function prototypes */
/*----------------------------------------------------------------------------*/

const char *(*plugin_name) (void);
GtkWidget *(*get_plugin) (void);

/*----------------------------------------------------------------------------*/
/* Main function */
/*----------------------------------------------------------------------------*/

int main (int argc, char* argv[])
{
    GtkBuilder *builder;
    GtkWidget *label, *page;
    void *phandle;

    setlocale (LC_ALL, "");
    bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
    textdomain (GETTEXT_PACKAGE);

    gtk_init (&argc, &argv);

    /* create the dialog */
    builder = gtk_builder_new_from_file (PACKAGE_DATA_DIR "/ui/rpcc.ui");

    dlg = (GtkWidget *) gtk_builder_get_object (builder, "dlg");
    nb = (GtkWidget *) gtk_builder_get_object (builder, "notebook");

    g_object_unref (builder);

    /* load a plugin... */
    phandle = dlopen ("/usr/lib/aarch64-linux-gnu/rpcc/libtestpl.so", RTLD_LAZY);
    plugin_name = dlsym (phandle, "plugin_name");
    get_plugin = dlsym (phandle, "get_plugin");

    label = gtk_label_new (plugin_name ());
    page = get_plugin ();
    gtk_notebook_insert_page (GTK_NOTEBOOK (nb), page, label, -1);

    /* run the dialog */
    if (gtk_dialog_run (GTK_DIALOG (dlg)) != GTK_RESPONSE_OK)
    {
    }

    dlclose (phandle);

    gtk_widget_destroy (dlg);

    return 0;
}

/* End of file */
/*============================================================================*/
