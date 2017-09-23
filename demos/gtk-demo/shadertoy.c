/* Shadertoy
 *
 * Play with shaders. Everybody does it.
 */

#include <gtk/gtk.h>

G_DECLARE_FINAL_TYPE (GtkShadertoy, gtk_shadertoy, GTK, SHADERTOY, GtkWidget)

struct _GtkShadertoy {
  GtkWidget parent_instance;

  GBytes *vertex_shader;
  GBytes *fragment_shader;

  guint64 starttime;
  guint tick;
};

G_DEFINE_TYPE (GtkShadertoy, gtk_shadertoy, GTK_TYPE_WIDGET);

static void
gtk_shadertoy_snapshot (GtkWidget   *widget,
                        GtkSnapshot *snapshot)
{
  GtkShadertoy *self = GTK_SHADERTOY (widget);
  GdkRGBA white = { 1, 1, 1, 1 };
  GtkAllocation alloc;
  graphene_rect_t bounds;
  int offset_x, offset_y;
  float time = (float)(g_get_monotonic_time () - self->starttime)/1000000.0;

  gtk_widget_get_allocation (widget, &alloc);
  bounds.origin.x = 0;
  bounds.origin.y = 0;
  bounds.size.width = alloc.width;
  bounds.size.height = alloc.height;

  if (self->fragment_shader && self->tick)
    {
      gtk_snapshot_get_offset (snapshot, &offset_x, &offset_y);
      bounds.origin.x = offset_x;
      bounds.origin.y = offset_y;
      GskRenderNode *node = gsk_pixel_shader_node_new (&bounds,
                                                       self->vertex_shader,
                                                       self->fragment_shader,
                                                       time);
      gsk_render_node_set_name (node, "shader");
      gtk_snapshot_append_node (snapshot, node);
    }
  else
    gtk_snapshot_append_color (snapshot, &white, &bounds, "no shader");
}


static void
gtk_shadertoy_finalize (GObject *object)
{
  GtkShadertoy *self = GTK_SHADERTOY (object);

  if (self->vertex_shader)
    g_bytes_unref (self->vertex_shader);
  if (self->fragment_shader)
    g_bytes_unref (self->fragment_shader);

  G_OBJECT_CLASS (gtk_shadertoy_parent_class)->finalize (object);
}

static void
gtk_shadertoy_class_init (GtkShadertoyClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gtk_shadertoy_finalize;
  widget_class->snapshot = gtk_shadertoy_snapshot;
}

static void
gtk_shadertoy_init (GtkShadertoy *self)
{
  gtk_widget_set_has_window (GTK_WIDGET (self), FALSE);
}

static GtkWidget *
gtk_shadertoy_new (void)
{
  return g_object_new (gtk_shadertoy_get_type (), NULL);
}

static void
gtk_shadertoy_set_vertex_shader (GtkShadertoy *self,
                                 GBytes       *shader)
{
  if (self->vertex_shader)
    g_bytes_unref (self->vertex_shader);
  self->vertex_shader = shader;
  if (self->vertex_shader)
    g_bytes_ref (self->vertex_shader);

  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
gtk_shadertoy_set_fragment_shader (GtkShadertoy *self,
                                   GBytes       *shader)
{
  if (self->fragment_shader)
    g_bytes_unref (self->fragment_shader);
  self->fragment_shader = shader;
  if (self->fragment_shader)
    g_bytes_ref (self->fragment_shader);
}

static gboolean
tick_cb (GtkWidget *widget, GdkFrameClock *clock, gpointer data)
{
  gtk_widget_queue_draw (widget);
  return G_SOURCE_CONTINUE;
}

static void
gtk_shadertoy_start (GtkShadertoy *self)
{
  if (self->tick == 0)
    {
      self->starttime = g_get_monotonic_time ();
      self->tick = gtk_widget_add_tick_callback (GTK_WIDGET (self), tick_cb, NULL, NULL);
    }
}

static void
gtk_shadertoy_stop (GtkShadertoy *self)
{
  if (self->tick != 0)
    {
      gtk_widget_remove_tick_callback (GTK_WIDGET (self), self->tick);
      self->tick = 0;
    }
}

static gboolean
gtk_shadertoy_is_running (GtkShadertoy *self)
{
  return self->tick != 0;
}

static GtkWidget *window = NULL;
static GtkWidget *textview;
static GtkWidget *toy;
static GtkWidget *run;

static const char *vert_text =
"#version 420 core\n"
"\n"
"layout(push_constant) uniform PushConstants {\n"
"    mat4 mvp;\n"
"    vec4 clip_bounds;\n"
"    vec4 clip_widths;\n"
"    vec4 clip_heights;\n"
"} push;\n"
"\n"
"layout(location = 0) in vec4 inRect;\n"
"layout(location = 1) in float inTime;\n"
"\n"
"layout(location = 0) out vec2 outPos;\n"
"layout(location = 1) out float outTime;\n"
"\n"
"out gl_PerVertex {\n"
"  vec4 gl_Position;\n"
"};\n"
"\n"
"vec4 clip(vec4 rect) { return rect; }\n"
"\n"
"vec2 offsets[6] = { vec2(0.0, 0.0),\n"
"                    vec2(1.0, 0.0),\n"
"                    vec2(0.0, 1.0),\n"
"                    vec2(0.0, 1.0),\n"
"                    vec2(1.0, 0.0),\n"
"                    vec2(1.0, 1.0) };\n"
"\n"
"void main() {\n"
"  vec4 rect = clip (inRect);\n"
"\n"
"  vec2 pos = rect.xy + rect.zw * offsets[gl_VertexIndex];\n"
"  gl_Position = push.mvp * vec4 (pos, 0.0, 1.0);\n"
"  outPos = pos;\n"
"  outTime = inTime;\n"
"}";

static void
setup_vertex_shader (void)
{
  GError *error = NULL;
  int status;
  char *spv;
  gsize length;
  GBytes *shader;

  if (!g_file_set_contents ("gtk4-demo-shader.vert", vert_text, -1, &error))
    {
      g_print ("Failed to write shader file: %s\n", error->message);
      g_error_free (error);
      return;
    }

  if (!g_spawn_command_line_sync ("glslc -fshader-stage=vertex gtk4-demo-shader.vert -o gtk4-demo-shader.vert.spv", NULL, NULL, &status, &error))
    {
      g_print ("Running glslc failed (%d): %s\n", status, error->message);
      g_error_free (error);
      return;
    }

  if (!g_file_get_contents ("gtk4-demo-shader.vert.spv", &spv, &length, &error))
    {
      g_print ("Reading compiled shader failed: %s\n", error->message);
      g_error_free (error);
      return;
    }

  shader = g_bytes_new_take (spv, length);
  gtk_shadertoy_set_vertex_shader (GTK_SHADERTOY (toy), shader);
  g_bytes_unref (shader);
}

static void
run_cb (GtkButton *button)
{
  GtkTextBuffer *buffer;
  GtkTextIter start, end;
  char *text;
  GError *error = NULL;
  int status;
  char *spv;
  gsize length;
  GBytes *shader;

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (textview));
  gtk_text_buffer_get_bounds (buffer, &start, &end);
  text = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);

  if (!g_file_set_contents ("gtk4-demo-shader.frag", text, -1, &error))
    {
      g_print ("Failed to write shader file: %s\n", error->message);
      g_error_free (error);
      g_free (text);
      return;
    }

  g_free (text);

  if (!g_spawn_command_line_sync ("glslc -fshader-stage=fragment -DCLIP_NONE gtk4-demo-shader.frag -o gtk4-demo-shader.frag.spv", NULL, NULL, &status, &error))
    {
      g_print ("Running glslc failed (%d): %s\n", status, error->message);
      g_error_free (error);
      return;
    }

  if (!g_file_get_contents ("gtk4-demo-shader.frag.spv", &spv, &length, &error))
    {
      g_print ("Reading compiled shader failed: %s\n", error->message);
      g_error_free (error);
      return;
    }

  shader = g_bytes_new_take (spv, length);
  gtk_shadertoy_set_fragment_shader (GTK_SHADERTOY (toy), shader);
  g_bytes_unref (shader);

  if (gtk_shadertoy_is_running (GTK_SHADERTOY (toy)))
    {
      gtk_shadertoy_stop (GTK_SHADERTOY (toy));
      gtk_button_set_icon_name (button, "media-playback-start-symbolic");
    }
  else
    {
      gtk_shadertoy_start (GTK_SHADERTOY (toy));
      gtk_button_set_icon_name (button, "media-playback-stop-symbolic");
    }
}

GtkWidget *
do_shadertoy (GtkWidget *do_widget)
{

  if (!window)
    {
      GtkBuilder *builder;
      GtkWidget *content;

      builder = gtk_builder_new_from_resource ("/shadertoy/shadertoy.ui");

      window = GTK_WIDGET (gtk_builder_get_object (builder, "window"));
      content = GTK_WIDGET (gtk_builder_get_object (builder, "content"));
      textview = GTK_WIDGET (gtk_builder_get_object (builder, "text"));
      run = GTK_WIDGET (gtk_builder_get_object (builder, "run"));

      gtk_window_set_screen (GTK_WINDOW (window),
                             gtk_widget_get_screen (do_widget));
      g_signal_connect (window, "destroy",
                        G_CALLBACK (gtk_widget_destroyed), &window);

      toy = gtk_shadertoy_new ();
      setup_vertex_shader ();

      gtk_widget_set_hexpand (toy, TRUE);
      gtk_widget_set_vexpand (toy, TRUE);
      gtk_container_add (GTK_CONTAINER (content), toy);

      g_signal_connect (run, "clicked", G_CALLBACK (run_cb), NULL);

      g_object_unref (builder);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);

  return window;
}
