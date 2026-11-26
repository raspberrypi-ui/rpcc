/*============================================================================
Copyright (c) 2026 Raspberry Pi
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
#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include <libudev.h>

/*----------------------------------------------------------------------------*/
/* Typedefs and macros */
/*----------------------------------------------------------------------------*/

#if defined(__aarch64__)
    #define ARCH_NAME "arm64"     /* 64-bit ARM */
#elif defined(__arm__)
    #define ARCH_NAME "armhf"     /* 32-bit ARM */
#elif defined(__x86_64__)
    #define ARCH_NAME "amd64"     /* 64-bit x86 */
#elif defined(__i386__)
    #define ARCH_NAME "i386"      /* 32-bit x86 */
#else
    #define ARCH_NAME "unknown"
#endif

#define TO_REMOVE "\"\n\t: "

/*----------------------------------------------------------------------------*/
/* Global data */
/*----------------------------------------------------------------------------*/

static GtkBuilder *builder;
static GtkWidget *main_dlg;

/*----------------------------------------------------------------------------*/
/* Function prototypes */
/*----------------------------------------------------------------------------*/

static int read_label_from_file (const char *label, const char *file, const char *start);
static unsigned long get_mem (void);
static GDataInputStream *open_log (const char *path);
static gboolean get_last_update_apt (char **ctype, char **ctime);

/*----------------------------------------------------------------------------*/
/* Helper functions */
/*----------------------------------------------------------------------------*/

static int read_label_from_file (const char *label, const char *file, const char *start)
{
    FILE *fp;
    char *line;
    size_t len, index;
    GtkWidget *wid;

    fp = fopen (file, "rb");
    if (!fp) return 0;

    line = NULL;
    len = 0;
    while (getline (&line, &len, fp) != -1)
    {
        if (!start || !strncmp (start, line, strlen (start)))
        {
            index = strlen (line);
            if (index)
            {
                index--;
                while (index && strchr (TO_REMOVE, line[index])) line[index--] = 0;
                index = start ? strlen (start) : 0;
                while (line[index] && strchr (TO_REMOVE, line[index])) index++;
                if (line[index])
                {
                    wid = (GtkWidget *) gtk_builder_get_object (builder, label);
                    gtk_label_set_text (GTK_LABEL (wid), &line[index]);
                    free (line);
                    fclose (fp);
                    return 1;
                }
            }
            break;
        }
    }

    free (line);
    fclose (fp);
    return 0;
}

static unsigned long get_mem (void)
{
    const char *prop;
    char *id;
    guint64 mem = 0;
    int i, n;
    unsigned char b[4];

    // check Pi first
    FILE *fp = fopen ("/proc/device-tree/chosen/rpi-sdram-size-gbit", "rb");
    if (fp)
    {
        if (fread (b, 1, 4, fp) == 4)
        {
            mem = (b[0] << 24) | (b[1] << 16) | (b[2] << 8) | b[3];
        }
        fclose (fp);
        if (mem) return mem * 128L;
    }

    // fail - try udev for PC
    struct udev *udev = udev_new ();
    struct udev_device *dev = udev_device_new_from_syspath (udev, "/sys/class/dmi/id");
    if (dev)
    {
        prop = udev_device_get_property_value (dev, "MEMORY_ARRAY_NUM_DEVICES");
        if (prop)
        {
            n = atoi (prop);
            for (i = 0; i < n; i++)
            {
                id = g_strdup_printf ("MEMORY_DEVICE_%d_SIZE", i);
                prop = udev_device_get_property_value (dev, id);
                if (prop) mem += g_ascii_strtoull (prop, NULL, 10);
                g_free (id);
            }
        }
        udev_device_unref (dev);
    }
    udev_unref (udev);
    mem /= 1048576L;

    return mem;
}

static GDataInputStream *open_log (const char *path)
{
    GFile *file;
    GFileInputStream *fis;
    GInputStream *is;
    GConverter *conv;
    GDataInputStream *dis;

    file = g_file_new_for_path (path);
    fis = g_file_read (file, NULL, NULL);
    g_object_unref (file);
    if (!fis) return NULL;

    if (g_str_has_suffix (path, ".gz"))
    {
        conv = G_CONVERTER (g_zlib_decompressor_new (G_ZLIB_COMPRESSOR_FORMAT_GZIP));
        is = g_converter_input_stream_new (G_INPUT_STREAM (fis), conv);
        g_object_unref (conv);
        g_object_unref (fis);
    }
    else is = G_INPUT_STREAM (fis);

    dis = g_data_input_stream_new (is);
    g_object_unref (is);
    return dis;
}

static gboolean get_last_update_apt (char **ctype, char **ctime)
{
    GDataInputStream *dis;
    char *line, *path, *cmd, *last_cmd = NULL, *last_end = NULL;
    gboolean valid;
    int i;

    // match apt upgrades, plus PackageKit system updates
    GRegex *re = g_regex_new ("^Commandline: (apt(-get)?( \\S+)* (full-|dist-)?upgrade( |$)|packagekit role='update-packages')", 0, 0, NULL);

    // search the current log, then rotated logs from newest to oldest, until an update is found
    for (i = 0; last_end == NULL; i++)
    {
        valid = FALSE;
        cmd = NULL;

        // open either the current (uncompressed) log or an older compressed log
        if (i == 0) path = g_strdup ("/var/log/apt/history.log");
        else path = g_strdup_printf ("/var/log/apt/history.log.%d.gz", i);
        dis = open_log (path);

        // just in case, try to open an uncompressed older log - never happens at present on RPiOS
        if (!dis && i > 0)
        {
            g_free (path);
            path = g_strdup_printf ("/var/log/apt/history.log.%d", i);
            dis = open_log (path);
        }
        g_free (path);

        // if this log doesn't exist, give up - first check if the current log has just rotated
        if (!dis)
        {
            if (i == 0) continue;
            break;
        }

        // at this point, we have a valid input stream from a log - parse it for the last valid entry matching the regex
        while ((line = g_data_input_stream_read_line (dis, NULL, NULL, NULL)))
        {
            if (!strncmp (line, "Start-Date:", 11))
            {
                g_free (cmd);
                cmd = NULL;
                valid = TRUE;
            }
            else if (!strncmp (line, "Error:", 6)) valid = FALSE;
            else if (g_regex_match (re, line, 0, NULL))
            {
                g_free (cmd);
                cmd = g_strdup (line + 13);
            }
            else if (!strncmp (line, "End-Date:", 9))
            {
                if (cmd && valid)
                {
                    g_free (last_end);
                    last_end = g_strdup (line + 9);
                    g_free (last_cmd);
                    last_cmd = cmd;
                    cmd = NULL;
                }
            }
            g_free (line);
        }
        g_object_unref (dis);
        g_free (cmd);

        // loop exits when last_end has a value in it, or if there is no valid stream any more
    }

    g_regex_unref (re);

    if (last_end)
    {
        *ctype = last_cmd;
        *ctime = last_end;
        return TRUE;
    }
    else return FALSE;
}

/*----------------------------------------------------------------------------*/
/* Initial configuration                                                      */
/*----------------------------------------------------------------------------*/

static void init_config (void)
{
    GtkWidget *wid;
    unsigned long memsize;
    char *buf;
    char *upcmd = NULL, *uptime = NULL;

    if (!read_label_from_file ("lbl_platform", "/sys/firmware/devicetree/base/model", NULL))
        read_label_from_file ("lbl_platform", "/sys/class/dmi/id/product_name", NULL);
    read_label_from_file ("lbl_debian", "/etc/os-release", "PRETTY_NAME=");
    read_label_from_file ("lbl_kernel", "/proc/sys/kernel/osrelease", NULL);
    read_label_from_file ("lbl_image", "/etc/rpi-issue", NULL);
    read_label_from_file ("lbl_serial", "/proc/cpuinfo", "Serial");

    wid = (GtkWidget *) gtk_builder_get_object (builder, "lbl_arch");
    gtk_label_set_text (GTK_LABEL (wid), ARCH_NAME);

    wid = (GtkWidget *) gtk_builder_get_object (builder, "lbl_memory");
    memsize = get_mem ();
    if (memsize)
    {
        if (memsize % 1024 == 0)
            buf = g_strdup_printf (_("%lu GB"), memsize / 1024);
        else
            buf = g_strdup_printf (_("%lu MB"), memsize);
        gtk_label_set_text (GTK_LABEL (wid), buf);
        g_free (buf);
    }

    if (get_last_update_apt (&upcmd, &uptime))
    {
        wid = (GtkWidget *) gtk_builder_get_object (builder, "lbl_update");
        gtk_label_set_text (GTK_LABEL (wid), uptime);
        g_free (uptime);

        wid = (GtkWidget *) gtk_builder_get_object (builder, "lbl_updateby");
        gtk_label_set_text (GTK_LABEL (wid), upcmd);
        g_free (upcmd);
    }
}

/*----------------------------------------------------------------------------*/
/* Plugin interface */
/*----------------------------------------------------------------------------*/

void init_plugin (GtkWidget *parent)
{
    setlocale (LC_ALL, "");
    bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
    textdomain (GETTEXT_PACKAGE);

    main_dlg = parent;
    builder = gtk_builder_new_from_file (PACKAGE_DATA_DIR "/ui/about.ui");

    init_config ();
}

int plugin_tabs (void)
{
    return 1;
}

const char *tab_name (int tab)
{
    return C_("tab", "About");
}

const char *icon_name (int tab)
{
    return "dialog-question";
}

const char *tab_id (int tab)
{
    return NULL;
}

GtkWidget *get_tab (int tab)
{
    GtkWidget *window, *plugin;

    window = (GtkWidget *) gtk_builder_get_object (builder, "main_window");
    plugin = (GtkWidget *) gtk_builder_get_object (builder, "plugin");

    gtk_container_remove (GTK_CONTAINER (window), plugin);

    return plugin;
}

gboolean reboot_needed (void)
{
    return FALSE;
}

void free_plugin (void)
{
    g_object_unref (builder);
}

/* End of file */
/*============================================================================*/
