#include "test.h"

#include <libnautilus-private/nautilus-file-operations.h>

int 
main (int argc, char* argv[])
{
	GtkWidget *window;
	GList *sources;
	GFile *dest;
	GFile *source;
	int i;
	
	g_thread_init (NULL);
	
	test_init (&argc, &argv);

	if (argc < 3) {
		g_print ("Usage test-copy <sources...> <dest dir>\n");
		return 1;
	}

	sources = NULL;
	for (i = 1; i < argc - 1; i++) {
		source = g_file_new_for_commandline_arg (argv[i]);
		sources = g_list_prepend (sources, source);
	}
	sources = g_list_reverse (sources);
	
	dest = g_file_new_for_commandline_arg (argv[i]);
	
	window = test_window_new ("copy test", 5);
	
	gtk_widget_show (window);

	nautilus_file_operations_copy (sources,
				       NULL /* GArray *relative_item_points */,
				       dest,
				       GTK_WINDOW (window),
				       NULL, NULL);
	
	gtk_main ();
	
	return 0;
}


