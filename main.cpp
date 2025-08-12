#include <gtk/gtk.h>
#include <gtk4-layer-shell.h>
#include <signal.h>

static double start_x;
static double start_y;
static const char *colors[6] = {"green",  "blue", "orange",
                                "purple", "red",  "yellow"};
static int color_index = 0;

static int margin_x = 0;
static int margin_y = 0;
GtkWindow *window;

static void drag_begin(GtkGestureDrag *gesture, double x, double y,
                       GtkWidget *area) {
  start_x = x;
  start_y = y;
  // gtk_widget_set_cursor_from_name(GTK_WIDGET(window), "grabbing");
}

static void drag_update(GtkGestureDrag *gesture, double x, double y,
                        GtkWidget *area) {
  int wx = gtk_layer_get_margin(window, GTK_LAYER_SHELL_EDGE_LEFT);
  int wy = gtk_layer_get_margin(window, GTK_LAYER_SHELL_EDGE_TOP);
  gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
  gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_TOP, TRUE);
  gtk_layer_set_margin(window, GTK_LAYER_SHELL_EDGE_LEFT,
                       wx + start_x + x - 100);
  gtk_layer_set_margin(window, GTK_LAYER_SHELL_EDGE_TOP,
                       wy + start_y + y - 100);
  // gtk_widget_set_cursor_from_name(GTK_WIDGET(window), "grabbing");
}

static void drag_end(GtkGestureDrag *gesture, double x, double y,
                     GtkWidget *area) {
  // gtk_widget_set_cursor_from_name(GTK_WIDGET(window), "grab");
}

static void close_window(void) {}

static void signal_handler(int sig) {
  if (sig == SIGUSR1) {
    if (gtk_layer_get_layer(window) == GTK_LAYER_SHELL_LAYER_BOTTOM) {
      gtk_layer_set_layer(window, GTK_LAYER_SHELL_LAYER_OVERLAY);
    } else {
      gtk_layer_set_layer(window, GTK_LAYER_SHELL_LAYER_BOTTOM);
    }
  } else if (sig == SIGTERM) {
    gtk_window_destroy(window);
  }
}

static void key_press(GtkEventControllerKey *self, guint keyval, guint keycode,
                      GdkModifierType state, gpointer user_data) {
  if (keyval == GDK_KEY_Escape)
    gtk_window_destroy(window);
}

static void activate(GtkApplication *app, [[maybe_unused]] gpointer user_data) {
  window = GTK_WINDOW(gtk_application_window_new(app));
  gtk_window_set_decorated(GTK_WINDOW(window), FALSE);
  GtkCssProvider *provider = gtk_css_provider_new();
  gtk_css_provider_load_from_string(provider, R""""(
window { background: rgba(0, 0, 0, 0); }
frame { margin: 10px; border: none; }
textview { color: #222; font-size: 1.5em; font-family: "Comic Neue"; font-weight: bold; padding: 8px; min-width: 200px; min-height: 200px; }
frame.green { transform: rotate(-1.6deg); }
frame.green textview { background: linear-gradient(to bottom, rgba(0,0,0,0), rgba(0,0,0,0.3)), #5bd078; }
frame.blue { transform: rotate(0.8deg); }
frame.blue textview { background: linear-gradient(to bottom, rgba(0,0,0,0), rgba(0,0,0,0.3)), #59c1de; }
frame.orange { transform: rotate(-1.2deg); }
frame.orange textview { background: linear-gradient(to bottom, rgba(0,0,0,0), rgba(0,0,0,0.3)), #ff803b; }
frame.purple { transform: rotate(1.7deg); }
frame.purple textview { background: linear-gradient(to bottom, rgba(0,0,0,0), rgba(0,0,0,0.3)), #b661dc; }
frame.red { transform: rotate(-0.8deg); }
frame.red textview { background: linear-gradient(to bottom, rgba(0,0,0,0), rgba(0,0,0,0.3)), #f06d99; }
frame.yellow { transform: rotate(2.2deg); }
frame.yellow textview { background: linear-gradient(to bottom, rgba(0,0,0,0), rgba(0,0,0,0.3)), #fdf08c; }
)"""");
  GdkDisplay *display = gtk_widget_get_display(GTK_WIDGET(window));
  gtk_style_context_add_provider_for_display(
      display, GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_USER);
  gtk_layer_init_for_window(window);
  gtk_layer_set_namespace(window, "posted");
  gtk_layer_set_layer(window, GTK_LAYER_SHELL_LAYER_OVERLAY);
  gtk_window_set_title(GTK_WINDOW(window), "posted");
  g_signal_connect(window, "destroy", G_CALLBACK(close_window), NULL);
  GtkWidget *frame = gtk_frame_new(NULL);
  auto *style = gtk_widget_get_style_context(frame);
  srand(time(0));
  gtk_style_context_add_class(style, colors[std::rand() % 6]);
  gtk_window_set_child(GTK_WINDOW(window), frame);
  GtkWidget *drawing_area = gtk_text_view_new();
  gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(drawing_area), GTK_WRAP_WORD_CHAR);
  gtk_text_view_set_extra_menu(GTK_TEXT_VIEW(drawing_area), NULL);
  gtk_widget_set_can_target(drawing_area, FALSE);
  gtk_frame_set_child(GTK_FRAME(frame), drawing_area);

  GtkGesture *drag = gtk_gesture_drag_new();
  gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(drag), GDK_BUTTON_PRIMARY);
  gtk_widget_add_controller(frame, GTK_EVENT_CONTROLLER(drag));
  g_signal_connect(drag, "drag-begin", G_CALLBACK(drag_begin), frame);
  g_signal_connect(drag, "drag-update", G_CALLBACK(drag_update), frame);
  g_signal_connect(drag, "drag-end", G_CALLBACK(drag_end), frame);

  auto *keys = gtk_event_controller_key_new();
  gtk_widget_add_controller(frame, GTK_EVENT_CONTROLLER(keys));
  g_signal_connect(keys, "key-pressed", G_CALLBACK(key_press), NULL);

  gtk_layer_set_keyboard_mode(window, GTK_LAYER_SHELL_KEYBOARD_MODE_ON_DEMAND);
  gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
  gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_TOP, TRUE);
  if (margin_x > 115 && margin_y > 115) {
    gtk_layer_set_margin(window, GTK_LAYER_SHELL_EDGE_LEFT, margin_x - 115);
    gtk_layer_set_margin(window, GTK_LAYER_SHELL_EDGE_TOP, margin_y - 115);
  }

  // gtk_widget_set_cursor_from_name(GTK_WIDGET(window), "grab");

  gtk_window_present(window);
}

static int command_line(GApplication *app, GVariantDict *opts, void *) {
  return -1;
}

int main(int argc, char *argv[]) {
  signal(SIGUSR1, signal_handler);
  signal(SIGTERM, signal_handler);
  GtkApplication *app = gtk_application_new(NULL, G_APPLICATION_DEFAULT_FLAGS);

  const GOptionEntry entries[] = {
      {"x", 'x', G_OPTION_FLAG_NONE, G_OPTION_ARG_INT, &margin_x,
       "x-position of note on current monitor", NULL},
      {"y", 'y', G_OPTION_FLAG_NONE, G_OPTION_ARG_INT, &margin_y,
       "y-position of note on current monitor", NULL},
      {NULL}};
  g_application_add_main_option_entries(G_APPLICATION(app), entries);
  g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
  g_signal_connect(G_APPLICATION(app), "handle-local-options",
                   G_CALLBACK(command_line), NULL);
  int status = g_application_run(G_APPLICATION(app), argc, argv);
  g_object_unref(app);
  return status;
}
