#include <format>
#include <string>

#include <gtk/gtk.h>
#include <gtk4-layer-shell.h>
#include <signal.h>

static cairo_surface_t *surface = NULL;

static double start_x;
static double start_y;
static double draw_x;
static double draw_y;
static double prev_x;
static double prev_y;
static const char *pen_color;

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

static void clear_surface(void) {
  cairo_t *cr = cairo_create(surface);
  cairo_set_source_rgba(cr, 1, 1, 1, 0);
  cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
  cairo_paint(cr);
  cairo_destroy(cr);
}

static void resize_cb(GtkWidget *widget, int width, int height, gpointer data) {
  if (surface) {
    cairo_surface_destroy(surface);
    surface = NULL;
  }
  if (gtk_native_get_surface(gtk_widget_get_native(widget))) {
    surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
                                         gtk_widget_get_width(widget),
                                         gtk_widget_get_height(widget));
    clear_surface();
  }
}

static void draw_cb(GtkDrawingArea *drawing_area, cairo_t *cr, int width,
                    int height, gpointer data) {
  cairo_set_source_surface(cr, surface, 0, 0);
  cairo_paint(cr);
}

static void draw_brush(GtkWidget *widget, double x, double y) {
  GdkRGBA color;
  gdk_rgba_parse(&color, pen_color ? pen_color : "#222");
  cairo_t *cr = cairo_create(surface);
  cairo_move_to(cr, prev_x + 2, prev_y + 2);
  cairo_line_to(cr, x + 2, y + 2);
  cairo_set_line_width(cr, 3.0);
  cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
  cairo_set_source_rgba(cr, color.red, color.green, color.blue, color.alpha);
  cairo_stroke(cr);
  cairo_destroy(cr);
  gtk_widget_queue_draw(widget);
  prev_x = x;
  prev_y = y;
}

static void draw_begin(GtkGestureDrag *gesture, double x, double y,
                       GtkWidget *area) {
  draw_x = x;
  draw_y = y;
  prev_x = x;
  prev_y = y;
  draw_brush(area, x, y);
}

static void draw_update(GtkGestureDrag *gesture, double x, double y,
                        GtkWidget *area) {
  draw_brush(area, draw_x + x, draw_y + y);
}

static void draw_end(GtkGestureDrag *gesture, double x, double y,
                     GtkWidget *area) {
  draw_brush(area, draw_x + x, draw_y + y);
}

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

static void close_window(void) {
  if (surface)
    cairo_surface_destroy(surface);
}

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

static void middle_press(GtkGestureClick *gesture, int n_press, double x,
                         double y, GtkWidget *area) {
  clear_surface();
  gtk_widget_queue_draw(area);
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
window {{ background: alpha(black, 0); }}
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
  GtkWidget *text_area = gtk_text_view_new();
  gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text_area), GTK_WRAP_WORD_CHAR);
  gtk_text_view_set_extra_menu(GTK_TEXT_VIEW(text_area), NULL);
  gtk_widget_set_can_target(text_area, FALSE);
  if (note_text) {
    auto *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_area));
    gtk_text_buffer_set_text(buffer, note_text, -1);
  }
  GtkWidget *drawing = gtk_drawing_area_new();
  gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(drawing), draw_cb, NULL,
                                 NULL);
  g_signal_connect_after(drawing, "resize", G_CALLBACK(resize_cb), NULL);
  GtkWidget *overlay = gtk_overlay_new();
  gtk_overlay_set_child(GTK_OVERLAY(overlay), text_area);
  gtk_overlay_add_overlay(GTK_OVERLAY(overlay), drawing);

  gtk_frame_set_child(GTK_FRAME(frame), overlay);
  gtk_window_set_child(GTK_WINDOW(window), frame);

  gtk_window_set_default_size(window, note_w + note_margin * 2,
                              note_h + note_margin * 2);

  GtkGesture *drag = gtk_gesture_drag_new();
  gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(drag), GDK_BUTTON_SECONDARY);
  gtk_widget_add_controller(frame, GTK_EVENT_CONTROLLER(drag));
  g_signal_connect(drag, "drag-begin", G_CALLBACK(drag_begin), frame);
  g_signal_connect(drag, "drag-update", G_CALLBACK(drag_update), frame);
  g_signal_connect(drag, "drag-end", G_CALLBACK(drag_end), frame);

  GtkGesture *draw = gtk_gesture_drag_new();
  gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(draw), GDK_BUTTON_PRIMARY);
  gtk_widget_add_controller(drawing, GTK_EVENT_CONTROLLER(draw));
  g_signal_connect(draw, "drag-begin", G_CALLBACK(draw_begin), drawing);
  g_signal_connect(draw, "drag-update", G_CALLBACK(draw_update), drawing);
  g_signal_connect(draw, "drag-end", G_CALLBACK(draw_end), drawing);

  GtkGesture *press = gtk_gesture_click_new();
  gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(press), GDK_BUTTON_MIDDLE);
  gtk_widget_add_controller(drawing, GTK_EVENT_CONTROLLER(press));
  g_signal_connect(press, "pressed", G_CALLBACK(middle_press), drawing);

  auto *keys = gtk_event_controller_key_new();
  gtk_widget_add_controller(frame, GTK_EVENT_CONTROLLER(keys));
  g_signal_connect(keys, "key-pressed", G_CALLBACK(key_press), NULL);

  gtk_layer_set_keyboard_mode(window, GTK_LAYER_SHELL_KEYBOARD_MODE_ON_DEMAND);
  if (!note_exclusive) {
    if (note_x != 0 || note_y != 0) {
      gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
      gtk_layer_set_margin(window, GTK_LAYER_SHELL_EDGE_LEFT,
                           note_x - note_w / 2);
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
      {"pen", 'p', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &pen_color,
       "Color of the pen", NULL},
      {"font", 'f', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &note_font,
       "Font of the text", NULL},
      {"text", 't', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &note_text,
       "Text on the note", NULL},
      {"exclusive", 'e', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING,
       &note_exclusive, "Put note in exclusive area on screen edge", "l|r|t|b"},
      {NULL}};
  g_application_add_main_option_entries(G_APPLICATION(app), entries);
  g_application_set_option_context_summary(
      G_APPLICATION(app),
      "Little colorful gtk4-layer-shell notes you can write and draw on.");
  g_application_set_option_context_description(G_APPLICATION(app),
                                               R""(Signals:
  pkill -SIGUSR1 posted       Toggle between overlay and bottom layer
  pkill -SIGUSR2 posted       Send note to bottom layer

Controls:
  Mouse Left                  Draw on the note
  Mouse Right                 Move the note around
  Mouse Middle                Clear drawing
  Escape                      Destroy the note
)"");
  g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
  g_signal_connect(G_APPLICATION(app), "handle-local-options",
                   G_CALLBACK(command_line), NULL);
  int status = g_application_run(G_APPLICATION(app), argc, argv);
  g_object_unref(app);
  return status;
}
