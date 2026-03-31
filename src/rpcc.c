/*============================================================================
Copyright (c) 2024 Raspberry Pi
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
#include <sys/stat.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <gdk/gdkwayland.h>
#include "xdg-activation-v1-client-protocol.h"
#include "rpcc.h"

/*----------------------------------------------------------------------------*/
/* Typedefs and macros */
/*----------------------------------------------------------------------------*/

typedef enum {
    WM_OPENBOX,
    WM_WAYFIRE,
    WM_LABWC } wm_type;

/* DBus */

#define DBUS_BUS_NAME       "com.raspberrypi.rpcc"
#define DBUS_OBJECT_PATH    "/com/raspberrypi/rpcc"
#define DBUS_INTERFACE_NAME "com.raspberrypi.rpcc"

/*----------------------------------------------------------------------------*/
/* Global data */
/*----------------------------------------------------------------------------*/

static wm_type wm;
static GList *plugin_handles = NULL;
static GtkWidget *dlg, *msg_dlg, *nb;
static gulong draw_id;
static gboolean reboot = FALSE;
static char *st_tab[3];
static int tabs_x;
static GdkCursor *watch;
static guint busid;
static GDBusNodeInfo *introspection_data = NULL;

static const gchar introspection_xml[] =
  "<node>"
  "  <interface name='" DBUS_INTERFACE_NAME "'>"
  "    <method name='newtab'>"
  "      <arg type='s' name='tab' direction='in'/>"
  "      <arg type='s' name='fn' direction='in'/>"
  "      <arg type='s' name='arg' direction='in'/>"
  "    </method>"
  "  </interface>"
  "</node>";

struct xdg_activation_v1 *activation;
struct wl_seat *wseat;

/*----------------------------------------------------------------------------*/
/* Function prototypes */
/*----------------------------------------------------------------------------*/

static int (*plugin_tabs) (void);
static void (*init_plugin) (GtkWidget *parent);
static const char *(*tab_name) (int tab);
static const char *(*tab_id) (int tab);
static GtkWidget *(*get_tab) (int tab);
static gboolean (*reboot_needed) (void);
static void (*free_plugin) (void);
static const char *(*icon_name) (int tab);

static void load_plugin (GtkWidget *nb, const char *filename);
static void free_plugins (void *phandle, gpointer);
static void call_func (void *phandle, gpointer data);
static void update_icons (GtkWidget *, gpointer);
static void reboot_check (void *phandle, gpointer);
static void close_with_prompt (void);
static gboolean close_app (GtkButton *button, gpointer);
static gboolean close_app_reboot (GtkButton *button, gpointer);
static void message (char *msg);
static gboolean ok_main (GtkButton *button, gpointer data);
static gboolean close_prog (GtkWidget *widget, GdkEvent *event, gpointer data);
static gboolean scroll (GtkWidget *, GdkEventScroll *ev, gpointer);
static void load_config (int *w, int *h, int *tab);
static void save_config (void);
static gboolean init_window (gpointer);
static gboolean exec_plugin_func (gpointer);
static gboolean draw (GtkWidget *wid, cairo_t *cr, gpointer data);
static void init_dbus (void);
static void close_dbus (void);
static void name_acquired (GDBusConnection *connection, const gchar *name, gpointer);
static void name_lost (GDBusConnection *connection, const gchar *name, gpointer);
static void handle_method_call (GDBusConnection *, const gchar*, const gchar*, const gchar*,
    const gchar *method_name, GVariant *parameters, GDBusMethodInvocation *invocation, gpointer);
static void activate_app (void);

/*----------------------------------------------------------------------------*/
/* Plugin management */
/*----------------------------------------------------------------------------*/

static gboolean verify_interface (void *phandle)
{
    static const char * const symbols[] = {
        "init_plugin",
        "plugin_tabs",
        "tab_name",
        "tab_id",
        "get_tab",
        "icon_name",
        "reboot_needed",
        "free_plugin",
        NULL,
    };
    for (size_t i = 0; symbols[i]; i++) {
        if (!dlsym (phandle, symbols[i])) {
            fprintf (stderr, "Missing symbol '%s'\n", symbols[i]);
            return FALSE;
        }
    }
    return TRUE;
}

static void load_plugin (GtkWidget *, const char *filename)
{
    GtkWidget *label, *page, *icon, *box;
    void *phandle;
    char *path;
    int count, tab, font_height;
    const char *name, *tablabel;
    GdkPixbuf *pixbuf;
    PangoFontDescription *font_desc;
    GtkStyleContext *sc;
    int scale;

    if (!strstr (filename, ".so")) return;
    path = g_build_filename (PLUGIN_PATH, filename, NULL);
    phandle = dlopen (path, RTLD_LAZY);
    g_free (path);
    if (!phandle) {
        return;
    }
    if (!verify_interface (phandle)) {
        dlclose (phandle);
        fprintf (stderr, "%s does not conform to the API interface\n", filename);
        return;
    }

    init_plugin = dlsym (phandle, "init_plugin");
    plugin_tabs = dlsym (phandle, "plugin_tabs");
    tab_name = dlsym (phandle, "tab_name");
    tab_id = dlsym (phandle, "tab_id");
    get_tab = dlsym (phandle, "get_tab");
    icon_name = dlsym (phandle, "icon_name");

    init_plugin (dlg);

    sc = gtk_widget_get_style_context (nb);
    gtk_style_context_get (sc, gtk_style_context_get_state (sc), GTK_STYLE_PROPERTY_FONT, &font_desc, NULL);
    font_height = pango_font_description_get_size (font_desc) / PANGO_SCALE;
    pango_font_description_free (font_desc);
    scale = gtk_widget_get_scale_factor (dlg);

    for (tab = 0; tab < plugin_tabs (); tab++)
    {
        name = tab_name (tab);
        label = gtk_label_new (name);

        icon = gtk_image_new ();
        gtk_widget_set_name (icon, icon_name (tab));
        pixbuf = gtk_icon_theme_load_icon_for_scale (gtk_icon_theme_get_default (), gtk_widget_get_name (icon), font_height < 12 ? 24 : 32, scale, GTK_ICON_LOOKUP_FORCE_SIZE, NULL);
        if (pixbuf)
        {
            if (scale == 1) gtk_image_set_from_pixbuf (GTK_IMAGE (icon), pixbuf);
            else
            {
                cairo_surface_t *cr = gdk_cairo_surface_create_from_pixbuf (pixbuf, scale, NULL);
                gtk_image_set_from_surface (GTK_IMAGE (icon), cr);
                cairo_surface_destroy (cr);
            }
            g_object_unref (pixbuf);
        }

        box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 5);
        gtk_box_pack_start (GTK_BOX (box), icon, FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 0);
        gtk_widget_show_all (box);

        page = get_tab (tab);
        gtk_widget_set_name (page, tab_id (tab));
        for (count = 0; count < gtk_notebook_get_n_pages (GTK_NOTEBOOK (nb)); count++)
        {
            GList *list = gtk_container_get_children (GTK_CONTAINER (gtk_notebook_get_tab_label (GTK_NOTEBOOK (nb), gtk_notebook_get_nth_page (GTK_NOTEBOOK (nb), count))));
            tablabel = gtk_label_get_label (GTK_LABEL (g_list_last (list)->data));
            g_list_free (list);
            if (g_strcmp0 (tablabel, name) > 0) break;
        }
        gtk_notebook_insert_page (GTK_NOTEBOOK (nb), page, box, count);
        gtk_notebook_set_menu_label_text (GTK_NOTEBOOK (nb), page, name);
        if (st_tab[0])
        {
            if (!g_strcmp0 (st_tab[0], tab_id (tab)))
            {
                gtk_notebook_set_current_page (GTK_NOTEBOOK (nb), count);
            }
        }
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
    void (*func) (char *) = dlsym (phandle, (char *) data);
    if (func) func (st_tab[2]);
}

void call_plugin_func (char *name)
{
    g_list_foreach (plugin_handles, call_func, name);
}

const char *dgetfixt (const char *domain, const char *msgctxid)
{
    const char *text = dgettext (domain, msgctxid);
    if (g_strcmp0 (msgctxid, text)) return text;
    return strchr (msgctxid, 0x04) + 1;
}

static void update_icons (GtkWidget *, gpointer)
{
    GtkWidget *icon;
    GdkPixbuf *pixbuf;
    int count, font_height;
    PangoFontDescription *font_desc;
    GtkStyleContext *sc;
    GList *list;
    int scale;

    sc = gtk_widget_get_style_context (nb);
    gtk_style_context_get (sc, gtk_style_context_get_state (sc), GTK_STYLE_PROPERTY_FONT, &font_desc, NULL);
    font_height = pango_font_description_get_size (font_desc) / PANGO_SCALE;
    pango_font_description_free (font_desc);
    scale = gtk_widget_get_scale_factor (dlg);

    for (count = 0; count < gtk_notebook_get_n_pages (GTK_NOTEBOOK (nb)); count++)
    {
        list = gtk_container_get_children (GTK_CONTAINER (gtk_notebook_get_tab_label (GTK_NOTEBOOK (nb), gtk_notebook_get_nth_page (GTK_NOTEBOOK (nb), count))));
        icon = GTK_WIDGET (g_list_first (list)->data);
        g_list_free (list);

        pixbuf = gtk_icon_theme_load_icon_for_scale (gtk_icon_theme_get_default (), gtk_widget_get_name (icon), font_height < 12 ? 24 : 32, scale, GTK_ICON_LOOKUP_FORCE_SIZE, NULL);
        if (pixbuf)
        {
            if (scale == 1) gtk_image_set_from_pixbuf (GTK_IMAGE (icon), pixbuf);
            else
            {
                cairo_surface_t *cr = gdk_cairo_surface_create_from_pixbuf (pixbuf, scale, NULL);
                gtk_image_set_from_surface (GTK_IMAGE (icon), cr);
                cairo_surface_destroy (cr);
            }
            g_object_unref (pixbuf);
        }
    }
}

/*----------------------------------------------------------------------------*/
/* Busy cursor */
/*----------------------------------------------------------------------------*/

void set_watch_cursor (void)
{
    gdk_window_set_cursor (gtk_widget_get_window (dlg), watch);
}

void clear_watch_cursor (void)
{
    gdk_window_set_cursor (gtk_widget_get_window (dlg), NULL);
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
    save_config ();
    g_list_foreach (plugin_handles, reboot_check, NULL);

    gtk_widget_destroy (dlg);
    if (reboot)
    {
        GtkWidget *wid;

        // the plugins need to use their own textdomain to load translations for builders, so set it back here
        textdomain (GETTEXT_PACKAGE);

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
    system ("/usr/sbin/reboot");
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

static gboolean scroll (GtkWidget *, GdkEventScroll *ev, gpointer)
{
    int page;

    if (ev->x < tabs_x)
    {
        page = gtk_notebook_get_current_page (GTK_NOTEBOOK (nb));
        if (ev->direction == 0 && page > 0) page--;
        if (ev->direction == 1 && page < gtk_notebook_get_n_pages (GTK_NOTEBOOK (nb)) - 1) page++;
        gtk_notebook_set_current_page (GTK_NOTEBOOK (nb), page);
    }
    return FALSE;
}

/*----------------------------------------------------------------------------*/
/* Load / save config */
/*----------------------------------------------------------------------------*/

static void load_config (int *w, int *h, int *tab)
{
    char *conffile;
    GKeyFile *kf;
    GError *err;
    int val;

    *w = 500;
    *h = 400;
    *tab = 0;

    conffile = g_build_filename (g_get_user_config_dir (), "rpcc", "config.ini", NULL);
    kf = g_key_file_new ();
    if (g_key_file_load_from_file (kf, conffile, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, NULL))
    {
        err = NULL;
        val = g_key_file_get_integer (kf, "*", "width", &err);
        if (err == NULL) *w = val;
        else *w = 500;

        err = NULL;
        val = g_key_file_get_integer (kf, "*", "height", &err);
        if (err == NULL) *h = val;
        else *h = 400;

        err = NULL;
        val = g_key_file_get_integer (kf, "*", "tab", &err);
        if (err == NULL) *tab = val;
        else *tab = 0;
    }

    g_key_file_free (kf);
    g_free (conffile);
}

static void save_config (void)
{
    char *conffile, *str;
    GKeyFile *kf;
    gsize len;
    int w, h;

    conffile = g_build_filename (g_get_user_config_dir (), "rpcc", "config.ini", NULL);

    str = g_path_get_dirname (conffile);
    g_mkdir_with_parents (str, S_IRUSR | S_IWUSR | S_IXUSR);
    g_free (str);

    kf = g_key_file_new ();
    g_key_file_load_from_file (kf, conffile, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, NULL);

    gtk_window_get_size (GTK_WINDOW (dlg), &w, &h);
    g_key_file_set_integer (kf, "*", "width", w);
    g_key_file_set_integer (kf, "*", "height", h);
    g_key_file_set_integer (kf, "*", "tab", gtk_notebook_get_current_page (GTK_NOTEBOOK (nb)));

    str = g_key_file_to_data (kf, &len, NULL);
    g_file_set_contents (conffile, str, len, NULL);
    g_free (str);

    g_key_file_free (kf);
    g_free (conffile);
}

/*----------------------------------------------------------------------------*/
/* Startup */
/*----------------------------------------------------------------------------*/

uint32_t last_serial = 0;
struct wl_surface *surface;

static void pointer_enter (void *, struct wl_pointer *, uint32_t serial, struct wl_surface *surf, wl_fixed_t sx, wl_fixed_t sy)
{
    last_serial = serial;
    surface = surf;
}

static void pointer_button (void *, struct wl_pointer *, uint32_t serial, uint32_t, uint32_t, uint32_t) 
{
    last_serial = serial;
}

static void pointer_leave (void *, struct wl_pointer *, uint32_t serial, struct wl_surface *surf)
{
    last_serial = serial;
    surface = surf;
}
static void pointer_motion (void *, struct wl_pointer *, uint32_t serial, wl_fixed_t, wl_fixed_t) {}
static void pointer_axis (void *, struct wl_pointer *, uint32_t serial, uint32_t, wl_fixed_t) {}
static void pointer_frame (void* data, struct wl_pointer*) {}

static const struct wl_pointer_listener pointer_listener =
{
    .enter = pointer_enter,
    .leave = pointer_leave,
    .motion = pointer_motion,
    .button = pointer_button,
    .axis = pointer_axis,
    .frame = pointer_frame,
};

static gboolean init_window (gpointer)
{
    GtkBuilder *builder;
    GdkWindow *win;
    GtkWidget *wid;
    DIR *d;
    struct dirent *dir;
    int w, h, tab;

    /* create the dialog */
    builder = gtk_builder_new_from_file (PACKAGE_DATA_DIR "/ui/rpcc.ui");

    dlg = (GtkWidget *) gtk_builder_get_object (builder, "dlg");
    nb = (GtkWidget *) gtk_builder_get_object (builder, "notebook");

    load_config (&w, &h, &tab);
    gtk_window_set_default_size (GTK_WINDOW (dlg), w, h);

    g_signal_connect (dlg, "delete_event", G_CALLBACK (close_prog), NULL);
    g_signal_connect (dlg, "scroll-event", G_CALLBACK (scroll), NULL);

    wid = (GtkWidget *) gtk_builder_get_object (builder, "btn_close");
    g_signal_connect (wid, "clicked", G_CALLBACK (ok_main), NULL);
    gtk_widget_grab_focus (wid);

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

    if (!st_tab[0]) gtk_notebook_set_current_page (GTK_NOTEBOOK (nb), tab);

    gtk_widget_show (dlg);
    gtk_widget_destroy (msg_dlg);
    win = gtk_widget_get_window (dlg);
    gdk_window_set_events (win, gdk_window_get_events (win) | GDK_SCROLL_MASK);

    /* find the x position of the child widgets - anything to the left is tabs... */
    GList *l = gtk_container_get_children (GTK_CONTAINER (nb));
    if (l)
    {
        GtkAllocation alloc;
        gtk_widget_get_allocation (GTK_WIDGET (g_list_nth_data (l, 0)), &alloc);
        tabs_x = alloc.x;
        g_list_free (l);
    }
    else tabs_x = 0;

    g_idle_add (exec_plugin_func, NULL);
    g_signal_connect (nb, "style-updated", G_CALLBACK (update_icons), NULL);

    struct wl_pointer *wptr = wl_seat_get_pointer (wseat);
    wl_pointer_add_listener (wptr, &pointer_listener, NULL);

    return FALSE;
}

static gboolean exec_plugin_func (gpointer)
{
    if (st_tab[1]) call_plugin_func (st_tab[1]);
    return FALSE;
}

static gboolean draw (GtkWidget *wid, cairo_t *cr, gpointer data)
{
    g_signal_handler_disconnect (wid, draw_id);
    g_idle_add (init_window, NULL);
    return FALSE;
}

/*----------------------------------------------------------------------------*/
/* DBus interface                                                             */
/*----------------------------------------------------------------------------*/

static const GDBusInterfaceVTable interface_vtable =
{
    handle_method_call, NULL, NULL, { 0 }
};

static void init_dbus (void)
{
    busid = g_bus_own_name (G_BUS_TYPE_SESSION, DBUS_BUS_NAME, G_BUS_NAME_OWNER_FLAGS_NONE,
        NULL, name_acquired, name_lost, NULL, NULL);
}

static void close_dbus (void)
{
    g_bus_unown_name (busid);
}

static void name_acquired (GDBusConnection *connection, const gchar *name, gpointer)
{
    /* name not on DBus, so this is the first instance - set up handler for newtab function */
    introspection_data = g_dbus_node_info_new_for_xml (introspection_xml, NULL);
    g_dbus_connection_register_object (connection, DBUS_OBJECT_PATH, introspection_data->interfaces[0],
        &interface_vtable, NULL, NULL, NULL);
}

static void name_lost (GDBusConnection *connection, const gchar *name, gpointer)
{
    GDBusProxy *proxy;
    GVariant *var;

    /* name already on DBus, so application already running - call the newtab function on the existing instance and then exit */
    proxy = g_dbus_proxy_new_sync (connection, G_DBUS_PROXY_FLAGS_NONE, NULL, DBUS_BUS_NAME, DBUS_OBJECT_PATH, DBUS_INTERFACE_NAME, NULL, NULL);
    var = g_variant_new ("(sss)", st_tab[0], st_tab[1] ? st_tab[1] : "", st_tab[2] ? st_tab[2] : "");
    g_dbus_proxy_call_sync (proxy, "newtab", var, G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL);
    g_dbus_connection_close_sync (connection, NULL, NULL);

    g_free (var);
    g_object_unref (proxy);
    exit (0);
}

static void handle_method_call (GDBusConnection *, const gchar*, const gchar*, const gchar*,
    const gchar *method_name, GVariant *parameters, GDBusMethodInvocation *invocation, gpointer)
{
    char *tab, *fn, *arg;
    GtkWidget *page;
    int pnum = 0;

    if (g_strcmp0 (method_name, "newtab") == 0)
    {
        g_dbus_method_invocation_return_value (invocation, NULL);

        g_variant_get (parameters, "(&s&s&s)", &tab, &fn, &arg);

        // change to requested tab
        if (strlen (tab))
        {
            while (1)
            {
                page = gtk_notebook_get_nth_page (GTK_NOTEBOOK (nb), pnum);
                if (!page) break;
                if (!g_strcmp0 (gtk_widget_get_name (page), tab))
                {
                    gtk_notebook_set_current_page (GTK_NOTEBOOK (nb), pnum);
                    break;
                }
                pnum++;
            }
        }

        // call plugin function
        if (strlen (fn))
        {
            if (st_tab[1]) g_free (st_tab[1]);
            if (st_tab[2]) g_free (st_tab[2]);
            st_tab[1] = g_strdup (fn);
            st_tab[2] = g_strdup (arg);
            exec_plugin_func (NULL);
        }

        if (wm != WM_OPENBOX) activate_app ();
    }
    else g_dbus_method_invocation_return_dbus_error (invocation, DBUS_INTERFACE_NAME ".Failed", "Unsupported method call");
}

/*----------------------------------------------------------------------------*/
/* Wayland activation protocol */
/*----------------------------------------------------------------------------*/

static void token_handle_done (void *data, struct xdg_activation_token_v1 *token, const char *token_string)
{
    xdg_activation_v1_activate (activation, token_string, surface);
}

static const struct xdg_activation_token_v1_listener token_listener =
{
    .done = token_handle_done,
};

static void activate_app (void)
{
    struct xdg_activation_token_v1 *token = xdg_activation_v1_get_activation_token (activation);
    xdg_activation_token_v1_add_listener (token, &token_listener, NULL);
    xdg_activation_token_v1_set_app_id (token, "rpcc");
    xdg_activation_token_v1_set_serial (token, last_serial, wseat);
    xdg_activation_token_v1_commit (token);
}

static void registry_add_object (void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version)
{
    if (!g_strcmp0 (interface, xdg_activation_v1_interface.name))
    {
        activation = wl_registry_bind (registry, name, &xdg_activation_v1_interface, 1);
    }
}

static void registry_remove_object (void *, struct wl_registry *, uint32_t)
{
    xdg_activation_v1_destroy (activation);
}

static struct wl_registry_listener registry_listener =
{
    &registry_add_object,
    &registry_remove_object
};

/*----------------------------------------------------------------------------*/
/* Main function */
/*----------------------------------------------------------------------------*/

int main (int argc, char* argv[])
{
    int i;

    for (i = 0; i < 3; i++)
    {
        if (argc > i + 1) st_tab[i] = g_strdup (argv[i + 1]);
        else st_tab[i] = NULL;
    }
    init_dbus ();

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

    GdkDisplay *gdk_display = gdk_display_get_default ();
    GdkSeat *seat = gdk_display_get_default_seat (gdk_display);
    struct wl_display *display = gdk_wayland_display_get_wl_display (gdk_display);
    struct wl_registry *registry = wl_display_get_registry (display);
    wseat  = gdk_wayland_seat_get_wl_seat (seat);

    wl_registry_add_listener (registry, &registry_listener, NULL);
    wl_display_roundtrip (display);
    wl_registry_destroy (registry);

    watch = gdk_cursor_new_for_display (gdk_display_get_default (), GDK_WATCH);

    /* show wait message */
    message (_("Loading configuration - please wait..."));
    draw_id = g_signal_connect (msg_dlg, "draw", G_CALLBACK (draw), NULL);

    gtk_main ();

    /* close the plugins cleanly */
    g_list_foreach (plugin_handles, free_plugins, NULL);

    close_dbus ();

    return 0;
}

/* End of file */
/*============================================================================*/
