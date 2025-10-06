#include <gtk/gtk.h>

/*
 * Called when the plugin is first loaded to perform
 * any initialisation it requires, including setting
 * translation domain.
 */
void init_plugin(void);

/*
 * Should return the number of tab pages the plugin
 * provides. Must be 1 or greater.
 */
int plugin_tabs(void);

/*
 * For the numbered tab, return the title which should
 * be displayed for it. International translations for
 * these should be included in the plugin; use the
 * C_ macro with the context "tab" to define these names.
 */
const char *tab_name(int tab);

/*
 * For the numbered tab, return a unique ID string which
 * can be used to identify the tab. This is optional - the
 * ID is only used if direct access to the tab is needed by
 * executing "rpcc <tab_id>", which will then launch the
 * application with that tab open.
 * Return NULL for any tabs for which this is not required.
 */
const char *tab_id(int tab);

/*
 * For the numbered tab, return the widget (usually a box)
 * containing the controls. The widget must have no parent
 * when it is returned, so may need to be removed (using
 * gtk_container_remove) from any parent in which it is
 * defined, such as a GtkWindow toplevel in a UI file.
 * The widget will be displayed in the relevant tab of rpcc.
 * All controls should take effect in real time, with a busy
 * mouse cursor displayed by the plugin if required.
 */
 GtkWidget *get_tab(int tab);

/*
 * For the numbered tab, return the name of the icon in
 * a loaded icon theme which should be displayed for it.
 */
const char *icon_name(int tab);

/*
 * Called when the application is closed to tidy up and determine
 * if any control change made while the tab was running requires
 * a reboot. Should return TRUE if a reboot is required;
 * FALSE otherwise.
 */
gboolean reboot_needed(void);

/*
 * Called when the application is closed to
 * unload the plugin resources from memory.
 */
void free_plugin(void);
