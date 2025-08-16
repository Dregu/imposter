#include <algorithm>
#include <cstring>
#include <format>
#include <random>
#include <string>
#include <vector>

#include <gtk/gtk.h>
#include <gtk4-layer-shell.h>
#include <signal.h>

static const std::array colors{
    "#7dab60", "#fecf37", "#ffbdce", "#fe8898", "#1f99f6", "#a1e9e3", "#36d1d1",
    "#fed523", "#fddae3", "#ffab8f", "#f9969e", "#ff9a5a", "#4ad3d3", "#fe74a5",
    "#d3f251", "#fe9e57", "#00caee", "#9dd26c", "#fed93f", "#ef91b3", "#ff5251",
    "#fccc00", "#55c377", "#00c5e4", "#cf99d7", "#fe965c", "#00d7dc", "#d8f35b",
    "#fe99a0", "#ff99d0", "#d99fd0"};

int note_x = 0;
int note_y = 0;
int note_w = 220;
int note_h = 220;
int extra_margin = 10;
int note_margin = 0;
int note_index = 0;
int note_create = 0;
double note_angle = FLT_MIN;
const char *note_bg;
const char *note_color;
const char *note_font;
const char *note_exclusive;
const char *note_text;
const char *note_pen_color;
const char *note_output;
const char *note_gravity;
const char *note_organize;

int monitor_w, monitor_h;

class Imposter;
Imposter *imposter = NULL;

float rand_float(float low, float high) {
  thread_local static std::random_device rd;
  thread_local static std::mt19937 rng(rd());
  thread_local std::uniform_real_distribution<float> urd;
  return urd(rng, decltype(urd)::param_type{low, high});
}

int rand_int(int low, int high) {
  thread_local static std::random_device rd;
  thread_local static std::mt19937 rng(rd());
  thread_local std::uniform_int_distribution<int> urd;
  return urd(rng, decltype(urd)::param_type{low, high});
}

void replace(std::string &subject, const std::string &search,
             const std::string &replace) {
  size_t pos = 0;
  while ((pos = subject.find(search, pos)) != std::string::npos) {
    subject.replace(pos, search.length(), replace);
    pos += replace.length();
  }
}

class Note {
public:
  void clear_surface(void) {
    cairo_t *cr = cairo_create(surface);
    cairo_set_source_rgba(cr, 1, 1, 1, 0);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_paint(cr);
    cairo_destroy(cr);
  }

  static void resize_cb(GtkWidget *widget, int width, int height,
                        gpointer data) {
    auto note = reinterpret_cast<Note *>(data);
    if (gtk_native_get_surface(gtk_widget_get_native(widget))) {
      auto new_w = gtk_widget_get_width(widget);
      auto new_h = gtk_widget_get_height(widget);
      auto new_surface =
          cairo_image_surface_create(CAIRO_FORMAT_ARGB32, new_w, new_h);
      if (note->surface) {
        cairo_t *cr = cairo_create(new_surface);
        cairo_set_source_surface(cr, note->surface, 0, 0);
        cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
        cairo_rectangle(cr, 0, 0, note->nw, note->nh);
        cairo_paint(cr);
        cairo_surface_destroy(note->surface);
      } else {
        note->clear_surface();
      }
      note->surface = new_surface;
      note->nw = width;
      note->nh = height;
    }
    gtk_widget_grab_focus(note->text_area);
  }

  static void draw_cb(GtkDrawingArea *drawing_area, cairo_t *cr, int width,
                      int height, gpointer data) {
    auto note = reinterpret_cast<Note *>(data);
    if (note->surface) {
      cairo_set_source_surface(cr, note->surface, 0, 0);
      cairo_paint(cr);
    }
  }

  void draw_brush(GtkWidget *widget, double x, double y) {
    GdkRGBA color;
    gdk_rgba_parse(&color, note_pen_color ? note_pen_color : "#222");
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
                         gpointer data) {
    auto note = reinterpret_cast<Note *>(data);
    note->draw_x = x;
    note->draw_y = y;
    note->prev_x = x;
    note->prev_y = y;
    note->draw_brush(note->drawing, x, y);
  }

  static void draw_update(GtkGestureDrag *gesture, double x, double y,
                          gpointer data) {
    auto note = reinterpret_cast<Note *>(data);
    note->draw_brush(note->drawing, note->draw_x + x, note->draw_y + y);
  }

  static void draw_end(GtkGestureDrag *gesture, double x, double y,
                       gpointer data) {
    auto note = reinterpret_cast<Note *>(data);
    note->draw_brush(note->drawing, note->draw_x + x, note->draw_y + y);
  }

  void set_position(int x_, int y_) {
    nx = std::clamp(x_, 0, monitor_w - note_w);
    ny = std::clamp(y_, 0, monitor_h - note_h);
    ;
    gtk_fixed_move(GTK_FIXED(notes), frame, nx, ny);
  }

  void set_size(int w_, int h_) {
    nw = w_;
    nh = h_;
    gtk_widget_set_size_request(frame, nw, nh);
  }

  static void drag_begin(GtkGestureDrag *gesture, double x, double y,
                         gpointer data) {
    auto note = reinterpret_cast<Note *>(data);
    note->start_x = x;
    note->start_y = y;
  }

  static void drag_update(GtkGestureDrag *gesture, double x, double y,
                          gpointer data) {
    auto note = reinterpret_cast<Note *>(data);
    note->set_position(note->nx + note->start_x + x - note_w / 2 + extra_margin,
                       note->ny + note->start_y + y - note_h / 2 +
                           extra_margin);
  }

  static void drag_end(GtkGestureDrag *gesture, double x, double y,
                       gpointer data) {
    auto note = reinterpret_cast<Note *>(data);
    note_index = 0;
  }

  static void enter(GtkEventControllerMotion *self, double x, double y,
                    gpointer data) {
    auto note = reinterpret_cast<Note *>(data);
    gtk_layer_set_keyboard_mode(note->win,
                                GTK_LAYER_SHELL_KEYBOARD_MODE_EXCLUSIVE);
    gtk_widget_grab_focus(note->text_area);
  }

  static void leave(GtkEventControllerMotion *self, double x, double y,
                    gpointer data) {
    auto note = reinterpret_cast<Note *>(data);
    gtk_layer_set_keyboard_mode(note->win,
                                GTK_LAYER_SHELL_KEYBOARD_MODE_ON_DEMAND);
    gtk_root_set_focus(GTK_ROOT(note->win), NULL);
  }

  static void key_press(GtkEventControllerKey *self, guint keyval,
                        guint keycode, GdkModifierType state, gpointer data) {
    auto note = reinterpret_cast<Note *>(data);
    std::printf("key %d %d\n", (int)keyval, (int)state);
    if (keyval == GDK_KEY_Escape) {
      gtk_layer_set_keyboard_mode(note->win,
                                  GTK_LAYER_SHELL_KEYBOARD_MODE_ON_DEMAND);
      gtk_root_set_focus(GTK_ROOT(note->win), NULL);
    } else if (keyval == GDK_KEY_q && state == GDK_CONTROL_MASK) {
      gtk_layer_set_keyboard_mode(note->win,
                                  GTK_LAYER_SHELL_KEYBOARD_MODE_ON_DEMAND);
      gtk_root_set_focus(GTK_ROOT(note->win), NULL);
      note->close();
    }
  }

  static void middle_press(GtkGestureClick *gesture, int n_press, double x,
                           double y, gpointer data) {
    auto note = reinterpret_cast<Note *>(data);
    note->clear_surface();
    gtk_widget_queue_draw(note->drawing);
  }

  static void realize(GtkEventControllerMotion *self, gpointer data) {
    auto note = reinterpret_cast<Note *>(data);
    gtk_widget_grab_focus(note->text_area);
  }

  void close(void) {
    if (surface) {
      cairo_surface_destroy(surface);
      surface = NULL;
    }
    gtk_fixed_remove(GTK_FIXED(notes), frame);
    deleted = true;
    note_index = 0;
  }

  Note(GtkWindow *win_, GtkWidget *notes_) {
    win = win_;
    notes = notes_;
  }

  GtkWidget *create() {
    frame = gtk_frame_new(NULL);
    // GdkDisplay *display = gtk_widget_get_display(GTK_WIDGET(frame));
    GtkCssProvider *provider = gtk_css_provider_new();
    auto css = std::format(
        R""(
frame {{ margin: {}px; border: none; transform: rotate({}deg); }}
textview {{ color: {}; font: {}; padding: 8px; background: linear-gradient(to bottom, rgba(0,0,0,0), rgba(0,0,0,0.33)), {}; }}
)"",
        extra_margin,
        note_angle == FLT_MIN ? rand_float(-3.f, 3.f) : note_angle,
        note_color ? note_color : "#222",
        note_font ? note_font : "bold 1.5em 'Comic Neue'",
        note_bg ? note_bg : colors[rand_int(0, colors.size() - 1)]);
    gtk_css_provider_load_from_string(provider, css.c_str());
    text_area = gtk_text_view_new();
    auto ctx = gtk_widget_get_style_context(frame);
    auto ctx2 = gtk_widget_get_style_context(text_area);
    gtk_style_context_add_provider(ctx, GTK_STYLE_PROVIDER(provider),
                                   GTK_STYLE_PROVIDER_PRIORITY_USER);
    gtk_style_context_add_provider(ctx2, GTK_STYLE_PROVIDER(provider),
                                   GTK_STYLE_PROVIDER_PRIORITY_USER);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text_area), GTK_WRAP_WORD_CHAR);
    gtk_widget_set_can_target(text_area, FALSE);
    gtk_text_view_set_accepts_tab(GTK_TEXT_VIEW(text_area), FALSE);
    if (note_text) {
      auto *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_area));
      std::string text(note_text);
      replace(text, "\\n", "\n");
      gtk_text_buffer_set_text(buffer, text.c_str(), -1);
      note_text = NULL;
    }
    drawing = gtk_drawing_area_new();

    overlay = gtk_overlay_new();
    gtk_overlay_set_child(GTK_OVERLAY(overlay), text_area);
    gtk_overlay_add_overlay(GTK_OVERLAY(overlay), drawing);
    gtk_frame_set_child(GTK_FRAME(frame), overlay);

    gtk_widget_set_size_request(frame, note_w + extra_margin * 2,
                                note_h + extra_margin * 2);

    GtkGesture *draw = gtk_gesture_drag_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(draw), GDK_BUTTON_PRIMARY);
    gtk_widget_add_controller(drawing, GTK_EVENT_CONTROLLER(draw));
    g_signal_connect(draw, "drag-begin", G_CALLBACK(draw_begin), this);
    g_signal_connect(draw, "drag-update", G_CALLBACK(draw_update), this);
    g_signal_connect(draw, "drag-end", G_CALLBACK(draw_end), this);

    GtkGesture *press = gtk_gesture_click_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(press), GDK_BUTTON_MIDDLE);
    gtk_widget_add_controller(drawing, GTK_EVENT_CONTROLLER(press));
    g_signal_connect(press, "pressed", G_CALLBACK(middle_press), this);

    auto *motion = gtk_event_controller_motion_new();
    gtk_widget_add_controller(GTK_WIDGET(frame), GTK_EVENT_CONTROLLER(motion));
    g_signal_connect(motion, "enter", G_CALLBACK(enter), this);
    g_signal_connect(motion, "motion", G_CALLBACK(enter), this);
    g_signal_connect(motion, "leave", G_CALLBACK(leave), this);

    auto *keys = gtk_event_controller_key_new();
    gtk_widget_add_controller(GTK_WIDGET(frame), GTK_EVENT_CONTROLLER(keys));
    g_signal_connect(keys, "key-pressed", G_CALLBACK(key_press), this);

    GtkGesture *drag = gtk_gesture_drag_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(drag),
                                  GDK_BUTTON_SECONDARY);
    gtk_widget_add_controller(frame, GTK_EVENT_CONTROLLER(drag));
    g_signal_connect(drag, "drag-begin", G_CALLBACK(drag_begin), this);
    g_signal_connect(drag, "drag-update", G_CALLBACK(drag_update), this);
    g_signal_connect(drag, "drag-end", G_CALLBACK(drag_end), this);

    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(drawing), draw_cb, this,
                                   NULL);
    g_signal_connect_after(drawing, "resize", G_CALLBACK(resize_cb), this);
    g_signal_connect_after(text_area, "realize", G_CALLBACK(realize), this);

    gtk_layer_set_keyboard_mode(win, GTK_LAYER_SHELL_KEYBOARD_MODE_EXCLUSIVE);
    return frame;
  }

  cairo_surface_t *surface = NULL;
  GdkDisplay *display;
  GtkWidget *frame;
  GtkWidget *text_area;
  GtkWidget *drawing;
  GtkWidget *overlay;
  GtkWidget *notes;
  GtkWindow *win;

  double start_x;
  double start_y;
  double draw_x;
  double draw_y;
  double prev_x;
  double prev_y;

  int nx;
  int ny;
  int nw;
  int nh;

  bool deleted = false;
};

class Imposter {
public:
  Imposter(GtkApplication *app_, [[maybe_unused]] gpointer data) { app = app_; }

  void fix_input_region() {
    notes.erase(std::remove_if(std::begin(notes), std::end(notes),
                               [](Note *n) { return n->deleted; }),
                notes.end());
    auto surf =
        gtk_native_get_surface(gtk_widget_get_native(GTK_WIDGET(window)));
    if (notes.empty()) {
      gtk_widget_set_visible(GTK_WIDGET(window), FALSE);
      gtk_window_set_default_size(window, -1, -1);
      // const auto rect = cairo_rectangle_int_t{1, 1, 1, 1};
      // auto reg = cairo_region_create_rectangles(&rect, 1);
      // gdk_surface_set_input_region(surf, reg);
      return;
    }
    gtk_widget_set_visible(GTK_WIDGET(window), TRUE);
    std::vector<cairo_rectangle_int_t> rectv;
    for (auto n : notes)
      rectv.push_back({n->nx, n->ny, n->nw, n->nh});
    const cairo_rectangle_int_t *rects = &rectv[0];
    auto reg = cairo_region_create_rectangles(rects, rectv.size());
    gdk_surface_set_input_region(surf, reg);
  }

  static bool timer(gpointer data) {
    auto win = reinterpret_cast<Imposter *>(data);
    while (note_create > 0) {
      win->note();
      note_create--;
    }
    win->fix_input_region();
    return true;
  }

  void note() {
    gtk_widget_set_visible(GTK_WIDGET(window), TRUE);
    gtk_layer_set_layer(window, GTK_LAYER_SHELL_LAYER_OVERLAY);

    auto surf =
        gtk_native_get_surface(gtk_widget_get_native(GTK_WIDGET(window)));
    // TODO: fix multi monitor switching
    auto mon =
        monitor ? monitor : gdk_display_get_monitor_at_surface(display, surf);
    GdkRectangle geometry;
    gdk_monitor_get_geometry(mon, &geometry);
    int tw = geometry.width;
    int th = geometry.height;
    monitor_w = tw;
    monitor_h = th;
    if (note_exclusive) {
      if (strstr(note_exclusive, "t") || strstr(note_exclusive, "b")) {
        th = note_h;
        gtk_layer_set_exclusive_zone(window, note_h + note_margin);
      } else if (strstr(note_exclusive, "l") || strstr(note_exclusive, "r")) {
        tw = note_w;
        gtk_layer_set_exclusive_zone(window, note_w + note_margin);
      }
    }
    if (note_gravity) {
      if (strstr(note_gravity, "t") || strstr(note_gravity, "b")) {
        th = note_h;
      } else if (strstr(note_gravity, "l") || strstr(note_gravity, "r")) {
        tw = note_w;
      }
    }
    int tx = tw / 2 - note_w / 2;
    int ty = th / 2 - note_h / 2;
    if (note_exclusive) {
      if (strstr(note_exclusive, "b"))
        ty = geometry.height - note_h - note_margin;
      else if (strstr(note_exclusive, "r"))
        tx = geometry.width - note_w - note_margin;
    }
    if (note_gravity) {
      if (strstr(note_gravity, "l"))
        tx = note_margin;
      else if (strstr(note_gravity, "r"))
        tx = geometry.width - note_w - note_margin;
      if (strstr(note_gravity, "t"))
        ty = note_margin;
      else if (strstr(note_gravity, "b"))
        ty = geometry.height - note_h - note_margin;
    }
    if (note_organize && note_index > 0) {
      int offset_x = 0;
      int offset_y = 0;
      if (std::strcmp(note_organize, "l") == 0)
        offset_x = -note_index;
      else if (std::strcmp(note_organize, "r") == 0)
        offset_x = note_index;
      else if (std::strcmp(note_organize, "t") == 0)
        offset_y = -note_index;
      else if (std::strcmp(note_organize, "b") == 0)
        offset_y = note_index;
      else if (std::strcmp(note_organize, "lr") == 0)
        offset_x = (note_index % 2 == 0 ? 1 : -1) * ((note_index + 1) / 2);
      else if (std::strcmp(note_organize, "tb") == 0)
        offset_y = (note_index % 2 == 0 ? 1 : -1) * ((note_index + 1) / 2);
      else if (std::strcmp(note_organize, "rl") == 0)
        offset_x = (note_index % 2 == 0 ? -1 : 1) * ((note_index + 1) / 2);
      else if (std::strcmp(note_organize, "bt") == 0)
        offset_y = (note_index % 2 == 0 ? -1 : 1) * ((note_index + 1) / 2);
      offset_x *= (note_w + extra_margin / 2);
      offset_y *= (note_h + extra_margin / 2);
      tx += offset_x;
      ty += offset_y;
    }
    auto note = new Note(window, fixed);
    auto frame = note->create();
    // gtk_widget_set_size_request(fixed, geometry.width, geometry.height);
    gtk_window_set_default_size(window, geometry.width, geometry.height);
    gtk_fixed_put(GTK_FIXED(fixed), frame, 0, 0);
    note->set_position(tx, ty);
    note->set_size(note_w, note_h);
    notes.push_back(note);
    note_index++;
    fix_input_region();
  }

  void create() {
    srand(time(0) + rand());
    window = GTK_WINDOW(gtk_application_window_new(app));

    gtk_window_set_decorated(GTK_WINDOW(window), FALSE);

    GtkCssProvider *provider = gtk_css_provider_new();
    auto css = std::format(
        R""(
window {{ background: alpha(black, 0); }}
)"");
    gtk_css_provider_load_from_string(provider, css.c_str());
    display = gtk_widget_get_display(GTK_WIDGET(window));
    gtk_style_context_add_provider_for_display(
        display, GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_USER);
    gtk_layer_init_for_window(window);

    auto list = gdk_display_get_monitors(display);
    if (g_list_model_get_n_items(list) == 1) {
      monitor = reinterpret_cast<GdkMonitor *>(g_list_model_get_item(list, 0));
    } else if (note_output) {
      for (guint i = 0; i < g_list_model_get_n_items(list); i++) {
        auto mon =
            reinterpret_cast<GdkMonitor *>(g_list_model_get_item(list, i));
        auto connector = gdk_monitor_get_connector(mon);
        if (std::strcmp(connector, note_output) == 0) {
          gtk_layer_set_monitor(window, mon);
          monitor = mon;
        }
      }
    }

    gtk_layer_set_namespace(window, "imposter");
    gtk_layer_set_layer(window, GTK_LAYER_SHELL_LAYER_OVERLAY);
    gtk_window_set_title(GTK_WINDOW(window), "imposter");

    // g_signal_connect(window, "destroy", G_CALLBACK(close), this);
    fixed = gtk_fixed_new();
    gtk_window_set_child(GTK_WINDOW(window), fixed);
    gtk_layer_set_keyboard_mode(window,
                                GTK_LAYER_SHELL_KEYBOARD_MODE_EXCLUSIVE);

    if (!note_exclusive) {
      gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
      gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);
      gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_TOP, TRUE);
      gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
    } else {
      if (strstr(note_exclusive, "l")) {
        gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
      } else if (strstr(note_exclusive, "r")) {
        gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);
      } else if (strstr(note_exclusive, "t")) {
        gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_TOP, TRUE);
      } else if (strstr(note_exclusive, "b")) {
        gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
      }
    }
    g_timeout_add(50, G_SOURCE_FUNC(timer), this);
    gtk_window_present(window);
    if (!note_create)
      gtk_widget_set_visible(GTK_WIDGET(window), FALSE);
  }

  GtkApplication *app = NULL;
  cairo_surface_t *surface = NULL;
  GtkWindow *window = NULL;
  GdkDisplay *display = NULL;
  GdkMonitor *monitor = NULL;
  GtkWidget *fixed = NULL;

  double start_x;
  double start_y;
  double draw_x;
  double draw_y;
  double prev_x;
  double prev_y;

  std::vector<Note *> notes;
};

static void signal_handler(int sig) {
  if (!imposter)
    return;
  if (sig == SIGUSR1)
    gtk_layer_set_layer(imposter->window,
                        gtk_layer_get_layer(imposter->window) ==
                                GTK_LAYER_SHELL_LAYER_OVERLAY
                            ? GTK_LAYER_SHELL_LAYER_BOTTOM
                            : GTK_LAYER_SHELL_LAYER_OVERLAY);
  else if (sig == SIGUSR2)
    imposter->note();
  else if (sig == SIGTERM)
    gtk_window_destroy(imposter->window);
}

static int command_line(GApplication *app, GVariantDict *opts, void *) {
  return -1;
}

static void activate(GtkApplication *app, gpointer data) {
  imposter = new Imposter(app, data);
  imposter->create();
}

int main(int argc, char *argv[]) {
  signal(SIGUSR1, signal_handler);
  signal(SIGUSR2, signal_handler);
  signal(SIGTERM, signal_handler);
  GtkApplication *app = gtk_application_new(NULL, G_APPLICATION_DEFAULT_FLAGS);

  const GOptionEntry entries[] = {
      {"num", 'n', G_OPTION_FLAG_NONE, G_OPTION_ARG_INT, &note_create,
       "Open n notes on launch (0)", NULL},
      /*{"xpos", 'x', G_OPTION_FLAG_NONE, G_OPTION_ARG_INT, &note_x,
       "X-position of the initial notes (center)", NULL},
      {"ypos", 'y', G_OPTION_FLAG_NONE, G_OPTION_ARG_INT, &note_y,
       "Y-position of the initial notes (center)", NULL},*/
      {"width", 'w', G_OPTION_FLAG_NONE, G_OPTION_ARG_INT, &note_w,
       "Width of the notes (220)", NULL},
      {"height", 'h', G_OPTION_FLAG_NONE, G_OPTION_ARG_INT, &note_h,
       "Height of the notes (220)", NULL},
      {"margin", 'm', G_OPTION_FLAG_NONE, G_OPTION_ARG_INT, &note_margin,
       "Margin between notes and screen edge (0)", NULL},
      {"angle", 'a', G_OPTION_FLAG_NONE, G_OPTION_ARG_DOUBLE, &note_angle,
       "Angle of the notes (random)", NULL},
      // TODO: multiple csv colors
      {"bg", 'b', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &note_bg,
       "Color of the notes (random)", NULL},
      {"color", 'c', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &note_color,
       "Color of the text (#222)", NULL},
      {"pen", 'p', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &note_pen_color,
       "Color of the pen (#222)", NULL},
      {"font", 'f', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &note_font,
       "Font of the text (bold 1.5em 'Comic Neue')", NULL},
      {"text", 't', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &note_text,
       "Text on the first note", NULL},
      {"exclusive", 'e', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING,
       &note_exclusive, "Reserve exclusive zone on screen edge", "l|r|t|b"},
      {"gravity", 'g', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &note_gravity,
       "Stick notes on specific screen edge (center)", "l|r|t|b|tl..."},
      {"organize", 'z', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &note_organize,
       "Add new note next to previous one in direction", "l|r|t|b|rl|bt"},
      {"output", 'o', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &note_output,
       "Monitor output name", NULL},
      {NULL}};
  g_application_add_main_option_entries(G_APPLICATION(app), entries);
  g_application_set_option_context_summary(
      G_APPLICATION(app),
      "Little colorful gtk4-layer-shell notes you can write and draw on.");
  g_application_set_option_context_description(G_APPLICATION(app),
                                               R""(Signals:
  pkill -SIGUSR1 imposter          Toggle between overlay and bottom layer
  pkill -SIGUSR2 imposter          Create a new note

Controls:
  Mouse Left                       Draw on a note
  Mouse Right                      Move a note around
  Mouse Middle                     Clear drawing
  Escape                           Restore exclusive focus from new note
  Ctrl+Q                           Destroy focused note
)"");
  g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
  g_signal_connect(G_APPLICATION(app), "handle-local-options",
                   G_CALLBACK(command_line), NULL);
  int status = g_application_run(G_APPLICATION(app), argc, argv);
  g_object_unref(app);
  return status;
}
