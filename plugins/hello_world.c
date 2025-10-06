#include "rpcc_api.h"

static GtkWidget *root = NULL;

void
init_plugin(void)
{
}

int
plugin_tabs(void)
{
	return 1;
}

const char *
tab_name(int tab)
{
	return "hello world!";
}

const char *
tab_id(int tab)
{
	return "hello-world-example";
}

GtkWidget *
get_tab(int tab)
{
	if (!root) {
		root = gtk_label_new("Hello world!");
		gtk_widget_show(root);
	}
	return root;
}

const char *
icon_name(int tab)
{
	return "applications-system";
}

gboolean
reboot_needed(void)
{
	return FALSE;
}

void
free_plugin(void)
{
	/* widget is destroyed by rpcc */
	root = NULL;
}
