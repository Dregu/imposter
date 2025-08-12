#include <format>
#include <string>

#include <gtk/gtk.h>
#include <gtk4-layer-shell.h>
#include <signal.h>

static double start_x;
static double start_y;
static const std::array colors{
    "#7dab60", "#fecf37", "#ffbdce", "#fe8898", "#1f99f6", "#a1e9e3", "#36d1d1",
    "#fed523", "#fddae3", "#ffab8f", "#f9969e", "#ff9a5a", "#4ad3d3", "#fe74a5",
    "#d3f251", "#fe9e57", "#00caee", "#9dd26c", "#fed93f", "#ef91b3", "#ff5251",
    "#fccc00", "#55c377", "#00c5e4", "#cf99d7", "#fe965c", "#00d7dc", "#d8f35b",
    "#fe99a0", "#ff99d0", "#d99fd0"};

static int note_margin = 10;
static int note_x = 0;
static int note_y = 0;
static int note_w = 220;
static int note_h = 220;
static double note_angle = FLT_MIN;
static const char *note_bg;
static const char *note_color;
static const char *note_font;
static const char *note_exclusive;
static const char *note_text;
GtkWindow *window;

static void drag_begin(GtkGestureDrag *gesture, double x, double y,
                       GtkWidget *area) {
  start_x = x;
  start_y = y;
}

static void drag_update(GtkGestureDrag *gesture, double x, double y,
                        GtkWidget *area) {
  int wx = gtk_layer_get_margin(window, GTK_LAYER_SHELL_EDGE_LEFT);
  int wy = gtk_layer_get_margin(window, GTK_LAYER_SHELL_EDGE_TOP);
  gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
  gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_TOP, TRUE);
  gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_RIGHT, FALSE);
  gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_BOTTOM, FALSE);
  gtk_layer_set_margin(window, GTK_LAYER_SHELL_EDGE_LEFT,
                       wx + start_x + x - note_w / 2 + note_margin);
  gtk_layer_set_margin(window, GTK_LAYER_SHELL_EDGE_TOP,
                       wy + start_y + y - note_h / 2 + note_margin);
  gtk_layer_set_exclusive_zone(window, 0);
}

static void drag_end(GtkGestureDrag *gesture, double x, double y,
                     GtkWidget *area) {}

static void close_window(void) {}

static void signal_handler(int sig) {
  if (sig == SIGUSR1)
    gtk_layer_set_layer(window, gtk_layer_get_layer(window) ==
                                        GTK_LAYER_SHELL_LAYER_OVERLAY
                                    ? GTK_LAYER_SHELL_LAYER_BOTTOM
                                    : GTK_LAYER_SHELL_LAYER_OVERLAY);
  else if (sig == SIGUSR2)
    gtk_layer_set_layer(window, GTK_LAYER_SHELL_LAYER_BOTTOM);
  else if (sig == SIGTERM)
    gtk_window_destroy(window);
}

static void key_press(GtkEventControllerKey *self, guint keyval, guint keycode,
                      GdkModifierType state, gpointer user_data) {
  if (keyval == GDK_KEY_Escape)
    gtk_window_destroy(window);
}

static void activate(GtkApplication *app, [[maybe_unused]] gpointer user_data) {
  srand(time(0) + getpid());
  if (note_angle == FLT_MIN) {
    note_angle = -2.f + static_cast<double>(rand()) /
                            (static_cast<double>(RAND_MAX / (2.f - -2.f)));
  }
  window = GTK_WINDOW(gtk_application_window_new(app));
  gtk_window_set_decorated(GTK_WINDOW(window), FALSE);
  GtkCssProvider *provider = gtk_css_provider_new();
  auto css = std::format(
      R""(
window {{ background: rgba(0, 0, 0, 0); }}
frame {{ margin: {}px; border: none; transform: rotate({}deg); }}
textview {{ color: {}; font-size: 1.5em; font-family: "{}"; font-weight: bold; padding: 8px; background: linear-gradient(to bottom, rgba(0,0,0,0), rgba(0,0,0,0.33)), {}; }}
)"",
      note_margin, note_angle, note_color ? note_color : "#222",
      note_font ? note_font : "Comic Neue",
      note_bg ? note_bg : colors[rand() % colors.size()]);
  gtk_css_provider_load_from_string(provider, css.c_str());
  GdkDisplay *display = gtk_widget_get_display(GTK_WIDGET(window));
  gtk_style_context_add_provider_for_display(
      display, GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_USER);
  gtk_layer_init_for_window(window);
  gtk_layer_set_namespace(window, "posted");
  gtk_layer_set_layer(window, GTK_LAYER_SHELL_LAYER_OVERLAY);
  gtk_window_set_title(GTK_WINDOW(window), "posted");
  g_signal_connect(window, "destroy", G_CALLBACK(close_window), NULL);
  GtkWidget *frame = gtk_frame_new(NULL);
  gtk_window_set_child(GTK_WINDOW(window), frame);
  GtkWidget *text_area = gtk_text_view_new();
  gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text_area), GTK_WRAP_WORD_CHAR);
  gtk_text_view_set_extra_menu(GTK_TEXT_VIEW(text_area), NULL);
  gtk_widget_set_can_target(text_area, FALSE);
  gtk_frame_set_child(GTK_FRAME(frame), text_area);
  if (note_text) {
    auto *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_area));
    gtk_text_buffer_set_text(buffer, note_text, -1);
  }
  gtk_window_set_default_size(window, note_w + note_margin * 2,
                              note_h + note_margin * 2);

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
  if (!note_exclusive) {
    if (note_x > note_w / 2) {
      gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
      gtk_layer_set_margin(window, GTK_LAYER_SHELL_EDGE_LEFT,
                           note_x - note_w / 2);
    }
    if (note_y > note_h / 2) {
      gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_TOP, TRUE);
      gtk_layer_set_margin(window, GTK_LAYER_SHELL_EDGE_TOP,
                           note_y - note_h / 2);
    }
  } else {
    if (strstr(note_exclusive, "l")) {
      gtk_layer_set_exclusive_zone(window, note_w + note_margin);
      gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
    } else if (strstr(note_exclusive, "r")) {
      gtk_layer_set_exclusive_zone(window, note_w + note_margin);
      gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);
    } else if (strstr(note_exclusive, "t")) {
      gtk_layer_set_exclusive_zone(window, note_h + note_margin);
      gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_TOP, TRUE);
    } else if (strstr(note_exclusive, "b")) {
      gtk_layer_set_exclusive_zone(window, note_h + note_margin);
      gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
    }
  }

  gtk_window_present(window);
}

static int command_line(GApplication *app, GVariantDict *opts, void *) {
  return -1;
}

int main(int argc, char *argv[]) {
  signal(SIGUSR1, signal_handler);
  signal(SIGUSR2, signal_handler);
  signal(SIGTERM, signal_handler);
  GtkApplication *app = gtk_application_new(NULL, G_APPLICATION_DEFAULT_FLAGS);

  const GOptionEntry entries[] = {
      {"xpos", 'x', G_OPTION_FLAG_NONE, G_OPTION_ARG_INT, &note_x,
       "X-position of the note", NULL},
      {"ypos", 'y', G_OPTION_FLAG_NONE, G_OPTION_ARG_INT, &note_y,
       "Y-position of the note", NULL},
      {"width", 'w', G_OPTION_FLAG_NONE, G_OPTION_ARG_INT, &note_w,
       "Width of the note", NULL},
      {"height", 'h', G_OPTION_FLAG_NONE, G_OPTION_ARG_INT, &note_h,
       "Height of the note", NULL},
      {"margin", 'm', G_OPTION_FLAG_NONE, G_OPTION_ARG_INT, &note_margin,
       "Margin around the note", NULL},
      {"angle", 'a', G_OPTION_FLAG_NONE, G_OPTION_ARG_DOUBLE, &note_angle,
       "Angle of the note", NULL},
      {"bg", 'b', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &note_bg,
       "Color of the note", NULL},
      {"color", 'c', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &note_color,
       "Color of the text", NULL},
      {"font", 'f', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &note_font,
       "Font of the note", NULL},
      {"text", 't', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &note_text,
       "Text on the note", NULL},
      {"exclusive", 'e', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING,
       &note_exclusive, "Put note in exclusive area on screen edge", "l|r|t|b"},
      {NULL}};
  g_application_add_main_option_entries(G_APPLICATION(app), entries);
  g_application_set_option_context_summary(
      G_APPLICATION(app), "Open a little colorful note on screen.");
  g_application_set_option_context_description(G_APPLICATION(app),
                                               R""(Signals:
  pkill -SIGUSR1 posted       Toggle between overlay and bottom layer
  pkill -SIGUSR2 posted       Send note to bottom layer

Keys:
  Escape                      Destroy the note
)"");
  g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
  g_signal_connect(G_APPLICATION(app), "handle-local-options",
                   G_CALLBACK(command_line), NULL);
  int status = g_application_run(G_APPLICATION(app), argc, argv);
  g_object_unref(app);
  return status;
}
