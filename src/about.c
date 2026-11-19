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

#define TO_REMOVE "\" \n\t:"

/*----------------------------------------------------------------------------*/
/* Global data */
/*----------------------------------------------------------------------------*/

static GtkBuilder *builder;
static GtkWidget *main_dlg;

/*----------------------------------------------------------------------------*/
/* Function prototypes */
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/* Helper functions */
/*----------------------------------------------------------------------------*/

static void read_label_from_file (const char *label, const char *file, const char *start)
{
    FILE *fp;
    char *line;
    size_t len, index;
    GtkWidget *wid;

    fp = fopen (file, "rb");
    if (!fp) return;

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
                }
            }
            break;
        }
    }

    free (line);
    fclose (fp);
}

/*----------------------------------------------------------------------------*/
/* Initial configuration                                                      */
/*----------------------------------------------------------------------------*/

static void init_config (void)
{
    /*
     * /etc/os-release contains PRETTY_NAME="Debian GNU/Linux 13 (trixie)"
     * /proc/version contains Linux version 6.18.62+rpt-rpi-2712 (serge@raspberrypi.com) (aarch64-linux-gnu-gcc-14 (Debian 14.2.0-19) 14.2.0, GNU ld (GNU Binutils for Debian) 2.44) #1 SMP PREEMPT Debian 1:6.18.62-1+rpt1~beta1 (2026-11-12)
     * /proc/sys/kernel/osrelease contains 6.18.62+rpt-rpi-2712
     * /etc/rpi-issue contains Raspberry Pi reference 2026-10-06
Generated using pi-gen, https://github.com/RPi-Distro/pi-gen, b2cd98a9ee08a31ef7
d0110ed09f2492bbd972ec, stage4
        * /proc/meminfo contains MemTotal:        8251520 kB

 Host- cat /sys/class/dmi/id/product_name 2>/dev/null || tr -d '\0' < /proc/device-tree/model
 /sys/firmware/devicetree/base/model */

    GtkWidget *wid;

    read_label_from_file ("lbl_platform", "/sys/firmware/devicetree/base/model", NULL);
    read_label_from_file ("lbl_memory", "/proc/meminfo", "MemTotal:");
    read_label_from_file ("lbl_debian", "/etc/os-release", "PRETTY_NAME=");
    read_label_from_file ("lbl_kernel", "/proc/sys/kernel/osrelease", NULL);
    read_label_from_file ("lbl_image", "/etc/rpi-issue", NULL);
    read_label_from_file ("lbl_serial", "/proc/cpuinfo", "Serial");

    wid = (GtkWidget *) gtk_builder_get_object (builder, "lbl_arch");
    gtk_label_set_text (GTK_LABEL (wid), ARCH_NAME);
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
