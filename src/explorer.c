/*
 * explorer.c - An interactive GUI for manipulating a DeJong object and viewing its output
 *
 * de Jong Explorer - interactive exploration of the Peter de Jong attractor
 * Copyright (C) 2004 David Trowbridge and Micah Dowty
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 */

#include "explorer.h"
#include "color-button.h"
#include "curve-editor.h"
#include <stdlib.h>
#include <math.h>


static void explorer_class_init(ExplorerClass *klass);
static void explorer_init(Explorer *self);
static void explorer_dispose(GObject *gobject);

static int explorer_idle_handler(gpointer user_data);
static void explorer_get_params(Explorer *self);
static void explorer_resize(Explorer *self);
static int explorer_auto_limit_update_rate(Explorer *self);
static int explorer_limit_update_rate(Explorer *self, float max_rate);
static void explorer_get_current_keyframe(Explorer *self, GtkTreeIter *iter);
static void explorer_init_animation(Explorer *self);
static void explorer_update_animation(Explorer *self);
static void explorer_update_animation_length(Explorer *self);

static gdouble generate_random_param();

static gboolean on_expose(GtkWidget *widget, GdkEventExpose *event, gpointer user_data);
static void on_param_changed(GtkWidget *widget, gpointer user_data);
static void on_randomize(GtkWidget *widget, gpointer user_data);
static void on_load_defaults(GtkWidget *widget, gpointer user_data);
static void on_save(GtkWidget *widget, gpointer user_data);
static void on_quit(GtkWidget *widget, gpointer user_data);
static void on_pause_rendering_toggle(GtkWidget *widget, gpointer user_data);
static void on_load_from_image(GtkWidget *widget, gpointer user_data);
static void on_resize(GtkWidget *widget, gpointer user_data);
static void on_resize_cancel(GtkWidget *widget, gpointer user_data);
static void on_resize_ok(GtkWidget *widget, gpointer user_data);
static gboolean on_viewport_expose(GtkWidget *widget, GdkEventExpose *event, gpointer user_data);
static void on_widget_toggle(GtkWidget *widget, gpointer user_data);
static void on_color_changed(GtkWidget *widget, gpointer user_data);
static void on_anim_play_toggled(GtkWidget *widget, gpointer user_data);
static void on_keyframe_add(GtkWidget *widget, gpointer user_data);
static void on_keyframe_replace(GtkWidget *widget, gpointer user_data);
static void on_keyframe_delete(GtkWidget *widget, gpointer user_data);
static void on_keyframe_view_cursor_changed(GtkWidget *widget, gpointer user_data);
static gboolean on_anim_window_delete(GtkWidget *widget, GdkEvent *event, gpointer user_data);
static void on_anim_new(GtkWidget *widget, gpointer user_data);
static void on_anim_open(GtkWidget *widget, gpointer user_data);
static void on_anim_save(GtkWidget *widget, gpointer user_data);
static void on_anim_save_as(GtkWidget *widget, gpointer user_data);
static void on_anim_scale_changed(GtkWidget *widget, gpointer user_data);
static void on_anim_set_linear(GtkWidget *widget, gpointer user_data);
static void on_anim_set_smooth(GtkWidget *widget, gpointer user_data);
static void on_anim_curve_changed(GtkWidget *widget, gpointer user_data);
static void on_keyframe_duration_change(GtkWidget *widget, gpointer user_data);


/************************************************************************************/
/**************************************************** Initialization / Finalization */
/************************************************************************************/

GType explorer_get_type(void) {
  static GType exp_type = 0;

  if (!exp_type) {
    static const GTypeInfo dj_info = {
      sizeof(ExplorerClass),
      NULL, /* base_init */
      NULL, /* base_finalize */
      (GClassInitFunc) explorer_class_init,
      NULL, /* class_finalize */
      NULL, /* class_data */
      sizeof(Explorer),
      0,
      (GInstanceInitFunc) explorer_init,
    };

    exp_type = g_type_register_static(G_TYPE_OBJECT, "Explorer", &dj_info, 0);
  }

  return exp_type;
}

static void explorer_class_init(ExplorerClass *klass) {
  GObjectClass *object_class = (GObjectClass*) klass;

  object_class->dispose = explorer_dispose;

  glade_init();
}

static void explorer_init(Explorer *self) {
  self->xml = glade_xml_new("data/de-jong-explorer.glade", NULL, NULL);
  self->window = glade_xml_get_widget(self->xml, "explorer_window");

  /* Connect signal handlers
   */
  glade_xml_signal_connect_data(self->xml, "on_expose",                       G_CALLBACK(on_expose),                       self);
  glade_xml_signal_connect_data(self->xml, "on_param_changed",                G_CALLBACK(on_param_changed),                self);
  glade_xml_signal_connect_data(self->xml, "on_randomize",                    G_CALLBACK(on_randomize),                    self);
  glade_xml_signal_connect_data(self->xml, "on_load_defaults",                G_CALLBACK(on_load_defaults),                self);
  glade_xml_signal_connect_data(self->xml, "on_save",                         G_CALLBACK(on_save),                         self);
  glade_xml_signal_connect_data(self->xml, "on_quit",                         G_CALLBACK(on_quit),                         self);
  glade_xml_signal_connect_data(self->xml, "on_pause_rendering_toggle",       G_CALLBACK(on_pause_rendering_toggle),       self);
  glade_xml_signal_connect_data(self->xml, "on_load_from_image",              G_CALLBACK(on_load_from_image),              self);
  glade_xml_signal_connect_data(self->xml, "on_resize",                       G_CALLBACK(on_resize),                       self);
  glade_xml_signal_connect_data(self->xml, "on_resize_cancel",                G_CALLBACK(on_resize_cancel),                self);
  glade_xml_signal_connect_data(self->xml, "on_resize_ok",                    G_CALLBACK(on_resize_ok),                    self);
  glade_xml_signal_connect_data(self->xml, "on_viewport_expose",              G_CALLBACK(on_viewport_expose),              self);
  glade_xml_signal_connect_data(self->xml, "on_widget_toggle",                G_CALLBACK(on_widget_toggle),                self);
  glade_xml_signal_connect_data(self->xml, "on_anim_play_toggled",            G_CALLBACK(on_anim_play_toggled),            self);
  glade_xml_signal_connect_data(self->xml, "on_keyframe_add",                 G_CALLBACK(on_keyframe_add),                 self);
  glade_xml_signal_connect_data(self->xml, "on_keyframe_replace",             G_CALLBACK(on_keyframe_replace),             self);
  glade_xml_signal_connect_data(self->xml, "on_keyframe_delete",              G_CALLBACK(on_keyframe_delete),              self);
  glade_xml_signal_connect_data(self->xml, "on_keyframe_view_cursor_changed", G_CALLBACK(on_keyframe_view_cursor_changed), self);
  glade_xml_signal_connect_data(self->xml, "on_anim_window_delete",           G_CALLBACK(on_anim_window_delete),           self);
  glade_xml_signal_connect_data(self->xml, "on_anim_new",                     G_CALLBACK(on_anim_new),                     self);
  glade_xml_signal_connect_data(self->xml, "on_anim_open",                    G_CALLBACK(on_anim_open),                    self);
  glade_xml_signal_connect_data(self->xml, "on_anim_save",                    G_CALLBACK(on_anim_save),                    self);
  glade_xml_signal_connect_data(self->xml, "on_anim_save_as",                 G_CALLBACK(on_anim_save_as),                 self);
  glade_xml_signal_connect_data(self->xml, "on_anim_scale_changed",           G_CALLBACK(on_anim_scale_changed),           self);
  glade_xml_signal_connect_data(self->xml, "on_anim_set_linear",              G_CALLBACK(on_anim_set_linear),              self);
  glade_xml_signal_connect_data(self->xml, "on_anim_set_smooth",              G_CALLBACK(on_anim_set_smooth),              self);
  glade_xml_signal_connect_data(self->xml, "on_keyframe_duration_change",     G_CALLBACK(on_keyframe_duration_change),     self);

  /* Set up the drawing area
   */
  self->drawing_area = glade_xml_get_widget(self->xml, "main_drawingarea");
  gtk_widget_add_events(self->drawing_area,
			GDK_BUTTON_PRESS_MASK |
			GDK_BUTTON_RELEASE_MASK |
			GDK_BUTTON_MOTION_MASK |
			GDK_POINTER_MOTION_HINT_MASK);
  self->gc = gdk_gc_new(self->drawing_area->window);

  /* Add our custom foreground button
   */
  self->fgcolor_button = color_button_new("Foreground Color");
  gtk_box_pack_start(GTK_BOX(glade_xml_get_widget(self->xml, "fgcolor_box")),
		     self->fgcolor_button, TRUE, TRUE, 0);
  g_signal_connect(self->fgcolor_button, "changed", G_CALLBACK(on_color_changed), self);
  gtk_widget_show_all(self->fgcolor_button);

  /* Add our custom background button
   */
  self->bgcolor_button = color_button_new("Background Color");
  gtk_box_pack_start(GTK_BOX(glade_xml_get_widget(self->xml, "bgcolor_box")),
		     self->bgcolor_button, TRUE, TRUE, 0);
  g_signal_connect(self->bgcolor_button, "changed", G_CALLBACK(on_color_changed), self);
  gtk_widget_show_all(self->bgcolor_button);

  /* Add our CurveEditor, a modified GtkCurve widget
   */
  self->anim_curve = curve_editor_new();
  gtk_container_add(GTK_CONTAINER(glade_xml_get_widget(self->xml, "anim_curve_box")), self->anim_curve);
  g_signal_connect(self->anim_curve, "changed", G_CALLBACK(on_anim_curve_changed), self);
  gtk_widget_show_all(self->anim_curve);

  /* Set up the statusbar
   */
  self->statusbar = GTK_STATUSBAR(glade_xml_get_widget(self->xml, "statusbar"));
  self->render_status_context = gtk_statusbar_get_context_id(self->statusbar, "Rendering status");

  explorer_init_tools(self);
}

static void explorer_dispose(GObject *gobject) {
  Explorer *self = EXPLORER(gobject);

  if (self->dejong) {
    g_object_unref(self->dejong);
    self->dejong = NULL;
  }
  if (self->idler) {
    g_source_remove(self->idler);
    self->idler = 0;
  }
  if (self->animation) {
    g_object_unref(self->animation);
    self->animation = NULL;
  }

  explorer_dispose_tools(self);
}

Explorer* explorer_new(DeJong *dejong, Animation *animation) {
  Explorer *self = EXPLORER(g_object_new(explorer_get_type(), NULL));

  self->animation = ANIMATION(g_object_ref(animation));
  self->dejong = DE_JONG(g_object_ref(dejong));

  explorer_init_animation(self);
  explorer_set_params(self);
  explorer_resize(self);

  self->idler = g_idle_add(explorer_idle_handler, self);
}


/************************************************************************************/
/*********************************************************************** Parameters */
/************************************************************************************/

void explorer_set_params(Explorer *self) {
  self->setting_params = TRUE;
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(glade_xml_get_widget(self->xml, "param_a")), self->dejong->a);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(glade_xml_get_widget(self->xml, "param_b")), self->dejong->b);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(glade_xml_get_widget(self->xml, "param_c")), self->dejong->c);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(glade_xml_get_widget(self->xml, "param_d")), self->dejong->d);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(glade_xml_get_widget(self->xml, "param_zoom")), self->dejong->zoom);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(glade_xml_get_widget(self->xml, "param_xoffset")), self->dejong->xoffset);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(glade_xml_get_widget(self->xml, "param_yoffset")), self->dejong->yoffset);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(glade_xml_get_widget(self->xml, "param_rotation")), self->dejong->rotation);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(glade_xml_get_widget(self->xml, "param_blur_radius")), self->dejong->blur_radius);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(glade_xml_get_widget(self->xml, "param_blur_ratio")), self->dejong->blur_ratio);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(glade_xml_get_widget(self->xml, "param_exposure")), self->dejong->exposure);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(glade_xml_get_widget(self->xml, "param_gamma")), self->dejong->gamma);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(glade_xml_get_widget(self->xml, "param_clamped")), self->dejong->clamped);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(glade_xml_get_widget(self->xml, "param_tileable")), self->dejong->tileable);
  color_button_set_alpha(COLOR_BUTTON(self->fgcolor_button), self->dejong->fgalpha);
  color_button_set_alpha(COLOR_BUTTON(self->bgcolor_button), self->dejong->bgalpha);
  color_button_set_color(COLOR_BUTTON(self->fgcolor_button), &self->dejong->fgcolor);
  color_button_set_color(COLOR_BUTTON(self->bgcolor_button), &self->dejong->bgcolor);
  self->setting_params = FALSE;
}

static void explorer_get_params(Explorer *self) {
  GdkColor fg, bg;

  if (self->setting_params)
    return;

  color_button_get_color(COLOR_BUTTON(self->fgcolor_button), &fg);
  color_button_get_color(COLOR_BUTTON(self->bgcolor_button), &bg);

  g_object_set(self->dejong,
	       "a",           (gdouble) gtk_spin_button_get_value(GTK_SPIN_BUTTON(glade_xml_get_widget(self->xml, "param_a"))),
	       "b",           (gdouble) gtk_spin_button_get_value(GTK_SPIN_BUTTON(glade_xml_get_widget(self->xml, "param_b"))),
	       "c",           (gdouble) gtk_spin_button_get_value(GTK_SPIN_BUTTON(glade_xml_get_widget(self->xml, "param_c"))),
	       "d",           (gdouble) gtk_spin_button_get_value(GTK_SPIN_BUTTON(glade_xml_get_widget(self->xml, "param_d"))),
	       "zoom",        (gdouble) gtk_spin_button_get_value(GTK_SPIN_BUTTON(glade_xml_get_widget(self->xml, "param_zoom"))),
	       "xoffset",     (gdouble) gtk_spin_button_get_value(GTK_SPIN_BUTTON(glade_xml_get_widget(self->xml, "param_xoffset"))),
	       "yoffset",     (gdouble) gtk_spin_button_get_value(GTK_SPIN_BUTTON(glade_xml_get_widget(self->xml, "param_yoffset"))),
	       "rotation",    (gdouble) gtk_spin_button_get_value(GTK_SPIN_BUTTON(glade_xml_get_widget(self->xml, "param_rotation"))),
	       "blur_radius", (gdouble) gtk_spin_button_get_value(GTK_SPIN_BUTTON(glade_xml_get_widget(self->xml, "param_blur_radius"))),
	       "blur_ratio",  (gdouble) gtk_spin_button_get_value(GTK_SPIN_BUTTON(glade_xml_get_widget(self->xml, "param_blur_ratio"))),
	       "exposure",    (gdouble) gtk_spin_button_get_value(GTK_SPIN_BUTTON(glade_xml_get_widget(self->xml, "param_exposure"))),
	       "gamma",       (gdouble) gtk_spin_button_get_value(GTK_SPIN_BUTTON(glade_xml_get_widget(self->xml, "param_gamma"))),
	       "clamped",     gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(glade_xml_get_widget(self->xml, "param_clamped"))),
	       "tileable",    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(glade_xml_get_widget(self->xml, "param_tileable"))),
	       "fgalpha",     (guint) color_button_get_alpha(COLOR_BUTTON(self->fgcolor_button)),
	       "bgalpha",     (guint) color_button_get_alpha(COLOR_BUTTON(self->bgcolor_button)),
	       "fgcolor_gdk", &fg,
	       "bgcolor_gdk", &bg,
	       NULL);
}

static void on_param_changed(GtkWidget *widget, gpointer user_data) {
  Explorer *self = EXPLORER(user_data);
  explorer_get_params(self);

  /* If we're paused, poke the idle handler manually so we at least get one frame of updates */
  if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(glade_xml_get_widget(self->xml, "pause_menu"))))
    explorer_idle_handler(user_data);
}

static void on_color_changed(GtkWidget *widget, gpointer user_data) {
  /* This is a dirty trick that makes color updates run much more smoothly
   */
  Explorer *self = EXPLORER(user_data);
  if (self->setting_params)
    return;

  explorer_get_params(self);
  gtk_main_iteration();
  explorer_update_gui(self);
}

static gdouble generate_random_param() {
  return ((double)random())/RAND_MAX * 12 - 6;
}

static void on_randomize(GtkWidget *widget, gpointer user_data) {
  Explorer *self = EXPLORER(user_data);

  g_object_set(self->dejong,
	       "a", generate_random_param(),
	       "b", generate_random_param(),
	       "c", generate_random_param(),
	       "d", generate_random_param(),
	       NULL);
  explorer_set_params(self);
}

static void on_load_defaults(GtkWidget *widget, gpointer user_data) {
  Explorer *self = EXPLORER(user_data);
  de_jong_reset_to_defaults(self->dejong);
  explorer_set_params(self);
}

static void on_resize(GtkWidget *widget, gpointer user_data) {
  Explorer *self = EXPLORER(user_data);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(glade_xml_get_widget(self->xml, "resize_width")), self->dejong->width);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(glade_xml_get_widget(self->xml, "resize_height")), self->dejong->height);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(glade_xml_get_widget(self->xml, "resize_oversample")), self->dejong->oversample);

  gtk_widget_grab_focus(glade_xml_get_widget(self->xml, "resize_width"));
  gtk_widget_show(glade_xml_get_widget(self->xml, "resize_window"));
}

static void on_resize_cancel(GtkWidget *widget, gpointer user_data) {
  Explorer *self = EXPLORER(user_data);
  gtk_widget_hide(glade_xml_get_widget(self->xml, "resize_window"));
}

static void on_resize_ok(GtkWidget *widget, gpointer user_data) {
  Explorer *self = EXPLORER(user_data);
  guint new_width, new_height, new_oversample;
  GtkSpinButton *width_widget, *height_widget, *oversample_widget;

  width_widget = GTK_SPIN_BUTTON(glade_xml_get_widget(self->xml, "resize_width"));
  height_widget = GTK_SPIN_BUTTON(glade_xml_get_widget(self->xml, "resize_height"));
  oversample_widget = GTK_SPIN_BUTTON(glade_xml_get_widget(self->xml, "resize_oversample"));

  gtk_spin_button_update(width_widget);
  gtk_spin_button_update(height_widget);
  gtk_spin_button_update(oversample_widget);

  new_width = gtk_spin_button_get_value(width_widget);
  new_height = gtk_spin_button_get_value(height_widget);
  new_oversample = gtk_spin_button_get_value(oversample_widget);
  gtk_widget_hide(glade_xml_get_widget(self->xml, "resize_window"));

  g_object_set(self->dejong,
	       "width", new_width,
	       "height", new_height,
	       "oversample", new_oversample,
	       NULL);
  explorer_resize(self);
}


/************************************************************************************/
/******************************************************************** Misc GUI goop */
/************************************************************************************/

static void on_quit(GtkWidget *widget, gpointer user_data) {
  gtk_main_quit();
}

static void on_widget_toggle(GtkWidget *widget, gpointer user_data) {
  /* Toggle visibility of another widget. This widget should be named
   * toggle_foo to control the visibility of a widget named foo.
   */
  Explorer *self = EXPLORER(user_data);
  const gchar *name;
  GtkWidget *toggled;

  name = gtk_widget_get_name(widget);
  g_assert(!strncmp(name, "toggle_", 7));
  toggled = glade_xml_get_widget(self->xml, name+7);

  if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget)))
    gtk_widget_show(toggled);
  else
    gtk_widget_hide(toggled);
}

static void on_load_from_image(GtkWidget *widget, gpointer user_data) {
  Explorer *self = EXPLORER(user_data);
  GtkWidget *dialog;

  dialog = gtk_file_selection_new("Open Image Parameters");

  if(gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK) {
    const gchar *filename;
    filename = gtk_file_selection_get_filename(GTK_FILE_SELECTION(dialog));
    de_jong_load_image_file(self->dejong, filename);
    explorer_set_params(self);
  }
  gtk_widget_destroy(dialog);
}

static void on_save(GtkWidget *widget, gpointer user_data) {
  Explorer *self = EXPLORER(user_data);
  GtkWidget *dialog;

  dialog = gtk_file_selection_new("Save PNG Image");
  gtk_file_selection_set_filename(GTK_FILE_SELECTION(dialog), "rendering.png");

  if(gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK) {
    const gchar *filename;
    filename = gtk_file_selection_get_filename(GTK_FILE_SELECTION(dialog));
    de_jong_save_image_file(self->dejong, filename);
  }
  gtk_widget_destroy(dialog);
}


/************************************************************************************/
/************************************************************************ Rendering */
/************************************************************************************/

static void on_pause_rendering_toggle(GtkWidget *widget, gpointer user_data) {
  Explorer *self = EXPLORER(user_data);
  if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget)))
    g_source_remove(self->idler);
  else
    self->idler = g_idle_add(explorer_idle_handler, self);
}

static int explorer_idle_handler(gpointer user_data) {
  Explorer *self = EXPLORER(user_data);

  explorer_run_iterations(self);
  explorer_update_gui(self);
  explorer_update_animation(self);
  return 1;
}

void explorer_run_iterations(Explorer *self) {
  /* Run as many blocks of iterations as we can in 13 milliseconds
   */
  GTimer *timer;
  gulong elapsed;

  timer = g_timer_new();
  g_timer_start(timer);
  do {
    de_jong_calculate(self->dejong, 5000);
    g_timer_elapsed(timer, &elapsed);
  } while (elapsed < 13000);
  g_timer_destroy(timer);
}

static void explorer_resize(Explorer *self) {
  guint width = self->dejong->width;
  guint height = self->dejong->height;

  gtk_widget_set_size_request(self->drawing_area, width, height);

  /* A bit of a hack to make the default window size more sane */
  gtk_widget_set_size_request(glade_xml_get_widget(self->xml, "drawing_area_viewport"),
			      MIN(1000, width+5), MIN(1000, height+5));
  self->just_resized = TRUE;
}

static int explorer_limit_update_rate(Explorer *self, float max_rate) {
  /* Limit the frame rate to the given value. This should be called once per
   * frame, and will return 0 if it's alright to render another frame, or 1
   * otherwise.
   */
  GTimeVal now;
  gulong diff;

  /* Figure out how much time has passed, in milliseconds */
  g_get_current_time(&now);
  diff = ((now.tv_usec - self->last_update.tv_usec) / 1000 +
	  (now.tv_sec  - self->last_update.tv_sec ) * 1000);

  if (diff < (1000 / max_rate)) {
    return 1;
  }
  else {
    self->last_update = now;
    return 0;
  }
}

static int explorer_auto_limit_update_rate(Explorer *self) {
  /* Automatically determine a good maximum frame rate based on the current
   * number of iterations, and use limit_update_rate() to limit us to that.
   * Returns 1 if a frame should not be rendered.
   *
   * When we just start rendering an image, we want a quite high frame rate
   * (but not high enough we bog down the GUI) so the user can interactively
   * set parameters. After the rendering has been running for a while though,
   * the image changes much less and a very slow frame rate will leave more
   * CPU for calculations.
   */
  return explorer_limit_update_rate(self, 200 / (1 + (log(self->dejong->iterations) - 9.21) * 5));
}

void explorer_update_gui(Explorer *self) {
  /* If the GUI needs updating, update it. This includes limiting the maximum
   * update rate, updating the iteration count display, and actually rendering
   * frames to the drawing area.
   */
  GtkWidget *statusbar;
  gchar *iters;

  /* Skip frame rate limiting and updating the iteration counter if we're in
   * a hurry to show the user the result of a modified rendering parameter.
   */
  if (!self->dejong->render_dirty_flag) {

    if (explorer_auto_limit_update_rate(self))
      return;

    /* Update the iteration counter, removing the previous one if it existed */
    iters = g_strdup_printf("Iterations:    %.3e    \tPeak density:    %d    \tCurrent tool: %s",
			    self->dejong->iterations, self->dejong->current_density, self->current_tool);
    if (self->render_status_message_id)
      gtk_statusbar_remove(self->statusbar, self->render_status_context, self->render_status_message_id);
    self->render_status_message_id = gtk_statusbar_push(self->statusbar, self->render_status_context, iters);
    g_free(iters);
  }

  de_jong_update_image(self->dejong);

  /* Update our entire drawing area.
   * We use GdkRGB directly here to force ignoring the alpha channel.
   */
  gdk_draw_rgb_32_image(self->drawing_area->window, self->gc,
			0, 0, self->dejong->width, self->dejong->height, GDK_RGB_DITHER_NORMAL,
			gdk_pixbuf_get_pixels(self->dejong->image), self->dejong->width * 4);

  /* This keeps our memory from growing without bound if the user just hit a
   * stable oscillation, or left this running unattended for a long time...
   * Memory grows linearly with the size of the color lookup table. This should
   * force the user to pause at 1.5 million density, which would give
   * a table size of between 6 and 12 megabytes.
   */
  if (self->dejong->current_density > 1500000)
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(glade_xml_get_widget(self->xml, "pause_menu")), TRUE);
}

static gboolean on_viewport_expose(GtkWidget *widget, GdkEventExpose *event, gpointer user_data) {
  /* After the drawing area is shown, go back to the natural size request */
  Explorer *self = EXPLORER(user_data);
  if (self->just_resized) {
    gtk_widget_set_size_request(widget, -1, -1);
    self->just_resized = FALSE;
  }
  return FALSE;
}

static gboolean on_expose(GtkWidget *widget, GdkEventExpose *event, gpointer user_data) {
  Explorer *self = EXPLORER(user_data);
  GdkRectangle *rects;
  int n_rects, i;

  if (self->dejong->image && !self->dejong->size_dirty_flag) {

    gdk_region_get_rectangles(event->region, &rects, &n_rects);

    for (i=0; i<n_rects; i++) {
      /* Clip this rectangle to the image size, since reading outside our buffer is a bad thing */
      if (rects[i].x + rects[i].width > self->dejong->width)
	rects[i].width = self->dejong->width - rects[i].x;
      if (rects[i].y + rects[i].height > self->dejong->height)
	rects[i].height = self->dejong->height - rects[i].y;
      if (rects[i].width <= 0 || rects[i].height <= 0)
	continue;

      /* Render a rectangle taken from our pixbuf.
       * We use GdkRGB directly here to force ignoring the alpha channel.
       */
      gdk_draw_rgb_32_image(self->drawing_area->window, self->gc,
			    rects[i].x, rects[i].y,
			    rects[i].width, rects[i].height,
			    GDK_RGB_DITHER_NORMAL,
			    gdk_pixbuf_get_pixels(self->dejong->image) + rects[i].x * 4 + rects[i].y * self->dejong->width * 4,
			    self->dejong->width * 4);
    }

    g_free(rects);
  }
  return FALSE;
}


/************************************************************************************/
/************************************************************************ Animation */
/************************************************************************************/

static void explorer_init_animation(Explorer *self) {
  GtkTreeView *tv = GTK_TREE_VIEW(glade_xml_get_widget(self->xml, "keyframe_view"));
  GtkTreeViewColumn *col;
  GtkCellRenderer *renderer;

  gtk_tree_view_set_model(tv, GTK_TREE_MODEL(self->animation->model));

  /* The first column only displays the keyframe, in the form of a thumbnail
   */
  col = gtk_tree_view_column_new();
  gtk_tree_view_column_set_title(col, "Keyframe");

  renderer = gtk_cell_renderer_pixbuf_new();
  gtk_tree_view_column_pack_start(col, renderer, FALSE);
  gtk_tree_view_column_set_attributes(col, renderer,
				      "pixbuf", ANIMATION_MODEL_THUMBNAIL,
				      NULL);

  gtk_tree_view_append_column(tv, col);

  explorer_update_animation_length(self);
}

static void explorer_update_animation(Explorer *self) {
  /* Move on to the next frame if we're playing an animation
   */
  GTimeVal now;
  double diff, new_value;
  GtkRange *range;
  GtkAdjustment *adj;
  GtkWidget *loop_widget;

  if (!self->playing_animation)
    return;

  g_get_current_time(&now);
  diff = ((now.tv_usec - self->last_anim_frame_time.tv_usec) / 1000000.0 +
	  (now.tv_sec  - self->last_anim_frame_time.tv_sec ));
  self->last_anim_frame_time = now;

  range = GTK_RANGE(glade_xml_get_widget(self->xml, "anim_scale"));
  adj = gtk_range_get_adjustment(range);

  new_value = adj->value + diff;

  if (new_value < adj->upper) {
    gtk_range_set_value(range, new_value);
  }
  else {
    /* We've reached the end... */

    loop_widget = glade_xml_get_widget(self->xml, "loop_animation");
    if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(loop_widget))) {
      /* Loop the animation */
      gtk_range_set_value(range, adj->lower);
    }
    else {
      /* Stop at the end */
      gtk_range_set_value(range, adj->upper);
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(glade_xml_get_widget(self->xml, "anim_play_button")), FALSE);
    }
  }
}

static void explorer_get_current_keyframe(Explorer *self, GtkTreeIter *iter) {
  GtkTreePath *path;
  GtkTreeView *tv = GTK_TREE_VIEW(glade_xml_get_widget(self->xml, "keyframe_view"));
  gtk_tree_view_get_cursor(tv, &path, NULL);
  gtk_tree_model_get_iter(GTK_TREE_MODEL(self->animation->model), iter, path);
  gtk_tree_path_free(path);
}

static void on_anim_play_toggled(GtkWidget *widget, gpointer user_data) {
  Explorer *self = EXPLORER(user_data);
  GtkRange *range = GTK_RANGE(glade_xml_get_widget(self->xml, "anim_scale"));

  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {

    /* If our animation is already at its end, start it over */
    if (gtk_range_get_value(range) >= gtk_range_get_adjustment(range)->upper - 0.1)
      gtk_range_set_value(range, 0);

    g_get_current_time(&self->last_anim_frame_time);
    self->playing_animation = TRUE;
  }
  else {
    self->playing_animation = FALSE;
  }
}

static void on_keyframe_add(GtkWidget *widget, gpointer user_data) {
  Explorer *self = EXPLORER(user_data);
  animation_keyframe_append(self->animation, self->dejong);
  explorer_update_animation_length(self);
}

static void on_keyframe_replace(GtkWidget *widget, gpointer user_data) {
  Explorer *self = EXPLORER(user_data);
  GtkTreeIter iter;
  explorer_get_current_keyframe(self, &iter);
  animation_keyframe_store_dejong(self->animation, &iter, self->dejong);
}

static void on_keyframe_delete(GtkWidget *widget, gpointer user_data) {
  /* Determine which row the cursor is on, delete it, and make the delete
   * button insensitive again until another row is selected.
   */
  Explorer *self = EXPLORER(user_data);
  GtkTreeIter iter;
  explorer_get_current_keyframe(self, &iter);

  gtk_widget_set_sensitive(glade_xml_get_widget(self->xml, "keyframe_delete_button"), FALSE);
  gtk_widget_set_sensitive(glade_xml_get_widget(self->xml, "keyframe_replace_button"), FALSE);
  gtk_widget_set_sensitive(glade_xml_get_widget(self->xml, "anim_transition_box"), FALSE);

  gtk_list_store_remove(self->animation->model, &iter);
  explorer_update_animation_length(self);
}

static void on_keyframe_view_cursor_changed(GtkWidget *widget, gpointer user_data) {
  /* This is called when a new row in the keyframe view is selected.
   * enable the delete button, and load this row's parameters.
   */
  Explorer *self = EXPLORER(user_data);
  GtkTreeIter iter;
  Spline *spline;
  gdouble keyframe_duration;

  explorer_get_current_keyframe(self, &iter);

  gtk_widget_set_sensitive(glade_xml_get_widget(self->xml, "keyframe_delete_button"), TRUE);
  gtk_widget_set_sensitive(glade_xml_get_widget(self->xml, "keyframe_replace_button"), TRUE);
  gtk_widget_set_sensitive(glade_xml_get_widget(self->xml, "anim_transition_box"), TRUE);

  if (!self->seeking_animation) {
    /* Assuming the user clicked us rather than this being called as the result of
     * a seek, seek the animation to this keyframe's location.
     */
    self->selecting_keyframe = TRUE;
    gtk_range_set_value(GTK_RANGE(glade_xml_get_widget(self->xml, "anim_scale")),
			animation_keyframe_get_time(self->animation, &iter));

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(glade_xml_get_widget(self->xml, "anim_play_button")), FALSE);
    self->selecting_keyframe = FALSE;
  }

  /* Load this keyframe's transition parameters into our GUI */
  self->allow_transition_changes = FALSE;
  gtk_tree_model_get(GTK_TREE_MODEL(self->animation->model), &iter,
		     ANIMATION_MODEL_DURATION, &keyframe_duration,
		     ANIMATION_MODEL_SPLINE,   &spline,
		     -1);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(glade_xml_get_widget(self->xml, "keyframe_duration")), keyframe_duration);
  curve_editor_set_spline(CURVE_EDITOR(self->anim_curve), spline);
  spline_free(spline);
  self->allow_transition_changes = TRUE;
}

static gboolean on_anim_window_delete(GtkWidget *widget, GdkEvent *event, gpointer user_data) {
  /* Just hide the window when the user tries to close it
   */
  Explorer *self = EXPLORER(user_data);
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(glade_xml_get_widget(self->xml, "toggle_animation_window")), FALSE);
  return TRUE;
}

static void on_anim_new(GtkWidget *widget, gpointer user_data) {
  Explorer *self = EXPLORER(user_data);
  animation_clear(self->animation);
  explorer_update_animation_length(self);
}

static void on_anim_open(GtkWidget *widget, gpointer user_data) {
  Explorer *self = EXPLORER(user_data);
  GtkWidget *dialog;

  dialog = gtk_file_selection_new("Open Animation Keyframes");

  if(gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK) {
    const gchar *filename;
    filename = gtk_file_selection_get_filename(GTK_FILE_SELECTION(dialog));
    animation_load_file(self->animation, filename);
    explorer_update_animation_length(self);
  }
  gtk_widget_destroy(dialog);
}

static void on_anim_save(GtkWidget *widget, gpointer user_data) {
  on_anim_save_as(widget, user_data);
}

static void on_anim_save_as(GtkWidget *widget, gpointer user_data) {
  Explorer *self = EXPLORER(user_data);
  GtkWidget *dialog;

  dialog = gtk_file_selection_new("Save Animation Keyframes");
  gtk_file_selection_set_filename(GTK_FILE_SELECTION(dialog), "animation.dja");

  if(gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK) {
    const gchar *filename;
    filename = gtk_file_selection_get_filename(GTK_FILE_SELECTION(dialog));
    animation_save_file(self->animation, filename);
  }
  gtk_widget_destroy(dialog);
}

static void explorer_update_animation_length(Explorer *self) {
  /* Recalculate the length of the animation and update the anim_scale accordingly
   */
  GtkWidget *scale = glade_xml_get_widget(self->xml, "anim_scale");
  gdouble length = animation_get_length(self->animation);
  gboolean enable = length > 0.0001;

  /* To keep the GtkRange from complaining */
  if (!enable)
    length = 1;

  gtk_range_set_adjustment(GTK_RANGE(scale),
			   GTK_ADJUSTMENT(gtk_adjustment_new(gtk_range_get_value(GTK_RANGE(scale)), 0, length, 0.01, 1, 0)));
  gtk_widget_set_sensitive(scale, enable);
  gtk_widget_set_sensitive(glade_xml_get_widget(self->xml, "anim_play_button"), enable);
}

static void on_anim_scale_changed(GtkWidget *widget, gpointer user_data) {
  double v = gtk_range_get_adjustment(GTK_RANGE(widget))->value;
  Explorer *self = EXPLORER(user_data);
  AnimationIter iter;
  GtkTreePath *path;
  GtkTreeView *tv = GTK_TREE_VIEW(glade_xml_get_widget(self->xml, "keyframe_view"));

  /* Seek to the right place in the animation and load an interpolated frame */
  animation_iter_seek(self->animation, &iter, v);
  if (!iter.valid) {
    /* Don't do anything if we're past the end of the animation.
     * This will happen because the animation's range is only [0, length)
     * while the scale widget we represent it has a range of [0, length]
     */
    return;
  }
  animation_iter_load_dejong(self->animation, &iter, self->dejong);

  if (!self->selecting_keyframe) {
    /* Put the tree view's cursor on the current keyframe */
    path = gtk_tree_model_get_path(GTK_TREE_MODEL(self->animation->model), &iter.keyframe);
    self->seeking_animation = TRUE;
    gtk_tree_view_set_cursor(tv, path, NULL, FALSE);
    self->seeking_animation = FALSE;
    gtk_tree_path_free(path);
  }

  if (!self->playing_animation) {
    /* Just like the color picker, the hscale will probably try to suck up
     * all of the idle time we might have been spending rendering things.
     * Force at least a little rendering to happen right now.
     */
    explorer_run_iterations(self);
    explorer_update_gui(self);
  }
}

static void on_anim_set_linear(GtkWidget *widget, gpointer user_data) {
  Explorer *self = EXPLORER(user_data);
  curve_editor_set_spline(CURVE_EDITOR(self->anim_curve), &spline_template_linear);
}

static void on_anim_set_smooth(GtkWidget *widget, gpointer user_data) {
  Explorer *self = EXPLORER(user_data);
  curve_editor_set_spline(CURVE_EDITOR(self->anim_curve), &spline_template_smooth);
}

static void on_keyframe_duration_change(GtkWidget *widget, gpointer user_data) {
  /* The user just changed the current keyframe's duration.
   * Update the tree model, and recalculate the size of our animation.
   */
  Explorer *self = EXPLORER(user_data);
  GtkTreeIter iter;

  if (!self->allow_transition_changes)
    return;

  explorer_get_current_keyframe(self, &iter);

  gtk_list_store_set(self->animation->model, &iter,
		     ANIMATION_MODEL_DURATION, (gdouble) gtk_spin_button_get_value(GTK_SPIN_BUTTON(widget)),
		     -1);

  explorer_update_animation_length(self);
}

static void on_anim_curve_changed(GtkWidget *widget, gpointer user_data) {
  /* The user just changed the current keyframe's spline, update the model
   */
  Explorer *self = EXPLORER(user_data);
  GtkTreeIter iter;
  Spline *spline;

  if (!self->allow_transition_changes)
    return;

  explorer_get_current_keyframe(self, &iter);

  spline = curve_editor_get_spline(CURVE_EDITOR(self->anim_curve));
  gtk_list_store_set(self->animation->model, &iter,
		     ANIMATION_MODEL_SPLINE, spline,
		     -1);
  spline_free(spline);
}

/* The End */
