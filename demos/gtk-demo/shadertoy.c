/* Shadertoy
 *
 * Play with shaders. Everybody does it.
 */

#include <gtk/gtk.h>

enum {
  PROP_0,
  PROP_TIME,
  PROP_RUNNING,
  N_PROPS
};

G_DECLARE_FINAL_TYPE (GtkShadertoy, gtk_shadertoy, GTK, SHADERTOY, GtkWidget)

struct _GtkShadertoy {
  GtkWidget parent_instance;

  GBytes *vertex_shader;
  GBytes *fragment_shader;

  guint tick;
  guint64 starttime;

  float time;
  float base;
};

G_DEFINE_TYPE (GtkShadertoy, gtk_shadertoy, GTK_TYPE_WIDGET);

static void
gtk_shadertoy_snapshot (GtkWidget   *widget,
                        GtkSnapshot *snapshot)
{
  GtkShadertoy *self = GTK_SHADERTOY (widget);
  GdkRGBA white = { 1, 1, 1, 1 };
  GdkRGBA purple = { 1, 0, 1, 1 };
  GdkRGBA green = { 0, 1, 0, 1 };
  GtkAllocation alloc;
  graphene_rect_t bounds;
  int offset_x, offset_y;
  float time = (float)(g_get_monotonic_time () - self->starttime)/1000000.0;

  gtk_widget_get_allocation (widget, &alloc);
  bounds.origin.x = 0;
  bounds.origin.y = 0;
  bounds.size.width = alloc.width;
  bounds.size.height = alloc.height;

  if (self->fragment_shader)
    {
      GskRenderNode *node, *child1, *child2;

      gtk_snapshot_get_offset (snapshot, &offset_x, &offset_y);
      bounds.origin.x = offset_x;
      bounds.origin.y = offset_y;

      child1 = gsk_color_node_new (&purple, &GRAPHENE_RECT_INIT (offset_x + 20, offset_y + 20, 80, 40));
      child2 = gsk_color_node_new (&green, &GRAPHENE_RECT_INIT (offset_x + 40, offset_y + 80, 40, 80));
      node = gsk_pixel_shader_node_new (&bounds,
                                        child1, child2,
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
gtk_shadertoy_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  GtkShadertoy *self = GTK_SHADERTOY (object);

  switch (prop_id)
    {
    case PROP_TIME:
      g_value_set_float (value, self->time);
      break;

    case PROP_RUNNING:
      g_value_set_boolean (value, self->tick != 0);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void gtk_shadertoy_set_running (GtkShadertoy *self,
                                       gboolean      running);
static void gtk_shadertoy_reset_time (GtkShadertoy *self);

static void
gtk_shadertoy_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  GtkShadertoy *self = GTK_SHADERTOY (object);

  switch (prop_id)
    {
    case PROP_RUNNING:
      gtk_shadertoy_set_running (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_shadertoy_class_init (GtkShadertoyClass *klass)
{
  GParamSpec *pspec;

  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gtk_shadertoy_finalize;
  object_class->get_property = gtk_shadertoy_get_property;
  object_class->set_property = gtk_shadertoy_set_property;
  widget_class->snapshot = gtk_shadertoy_snapshot;

  pspec = g_param_spec_float ("time", NULL, NULL,
                              0.0, G_MAXFLOAT, 0.0,
                              G_PARAM_READABLE);
  g_object_class_install_property (object_class, PROP_TIME, pspec);

  pspec = g_param_spec_boolean ("running", NULL, NULL,
                                FALSE,
                                G_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_RUNNING, pspec);
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
  GtkShadertoy *self = GTK_SHADERTOY (widget);

  self->time = self->base + (g_get_monotonic_time () - self->starttime) / 1000000.0;
  g_object_notify (G_OBJECT (widget), "time");

  gtk_widget_queue_draw (widget);
  return G_SOURCE_CONTINUE;
}

static void
gtk_shadertoy_set_running (GtkShadertoy *self,
                           gboolean      running)
{
  if (running && self->tick == 0)
    {
      self->tick = gtk_widget_add_tick_callback (GTK_WIDGET (self), tick_cb, NULL, NULL);
      self->starttime = g_get_monotonic_time ();
      self->time = 0;
    }
  else if (!running && self->tick != 0)
    {
      gtk_widget_remove_tick_callback (GTK_WIDGET (self), self->tick);
      self->base += (g_get_monotonic_time () - self->starttime) / 1000000.0;
      self->tick = 0;
    }
  else
    return;

  gtk_widget_queue_draw (GTK_WIDGET (self));
  g_object_notify (G_OBJECT (self), "running");
}

static gboolean
gtk_shadertoy_is_running (GtkShadertoy *self)
{
  return self->tick != 0;
}

static void
gtk_shadertoy_reset_time (GtkShadertoy *self)
{
  self->base = 0;
  self->time = 0;
  self->starttime = g_get_monotonic_time ();

  gtk_widget_queue_draw (GTK_WIDGET (self));
  g_object_notify (G_OBJECT (self), "time");
}

static GtkWidget *window = NULL;
static GtkWidget *textview;
static GtkWidget *toy;

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
"layout(location = 1) in vec4 inTexRect1;\n"
"layout(location = 2) in vec4 inTexRect2;\n"
"layout(location = 3) in float inTime;\n"
"\n"
"layout(location = 0) out vec2 outPos;\n"
"layout(location = 1) out vec2 outTexCoord1;\n"
"layout(location = 2) out vec2 outTexCoord2;\n"
"layout(location = 3) out float outTime;\n"
"layout(location = 4) out vec2 outResolution;\n"
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
"  vec2 pos = rect.xy + rect.zw * offsets[gl_VertexIndex];\n"
"  gl_Position = push.mvp * vec4 (pos, 0.0, 1.0);\n"
"\n"
"  outPos = pos;\n"
"\n"
"  vec4 texrect = vec4((rect.xy - inRect.xy) / inRect.zw,\n"
"                      rect.zw / inRect.zw);\n"
"  vec4 texrect1 = vec4(inTexRect1.xy + inTexRect1.zw * texrect.xy,\n"
"                      inTexRect1.zw * texrect.zw);\n"
"  outTexCoord1 = texrect1.xy + texrect1.zw * offsets[gl_VertexIndex];\n"
"\n"
"  vec4 texrect2 = vec4(inTexRect2.xy + inTexRect2.zw * texrect.xy,\n"
"                      inTexRect2.zw * texrect.zw);\n"
"  outTexCoord2 = texrect2.xy + texrect2.zw * offsets[gl_VertexIndex];\n"
"\n"
"  outTime = inTime;\n"
"  outResolution = inRect.zw;\n"
"}";

static const char *frag_text =
"#version 420 core\n"
"\n"
"layout(location = 0) in vec2 iPos;\n"
"layout(location = 1) in vec2 iTexCoord1;\n"
"layout(location = 2) in vec2 iTexCoord2;\n"
"layout(location = 3) in float iTime;\n"
"layout(location = 4) in vec2 iResolution;\n"
"\n"
"layout(set = 0, binding = 0) uniform sampler2D iTexture1;\n"
"layout(set = 1, binding = 0) uniform sampler2D iTexture2;\n"
"\n"
"layout(location = 0) out vec4 color;\n"
"\n"
"void\n"
"mainImage (out vec4 fragColor, in vec2 fragCoord)\n"
"{\n"
"  vec2 uv = fragCoord.xy / iResolution.xy;\n"
"  fragColor = vec4(uv,0.5+0.5*sin(iTime), 1.0)*texture(iTexture1,iTexCoord1);\n"
"}\n"
"\n"
"void main()\n"
"{\n"
"  mainImage (color, iPos);\n"
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
play_cb (GtkButton *button)
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
      gtk_shadertoy_set_running (GTK_SHADERTOY (toy), FALSE);
      gtk_button_set_icon_name (button, "media-playback-start-symbolic");
    }
  else
    {
      gtk_shadertoy_set_running (GTK_SHADERTOY (toy), TRUE);
      gtk_button_set_icon_name (button, "media-playback-stop-symbolic");
    }
}

static void
rewind_cb (GtkButton *button)
{
  gtk_shadertoy_reset_time (GTK_SHADERTOY (toy));
}

static gboolean
format_time (GBinding     *binding,
             const GValue *from_value,
             GValue       *to_value,
             gpointer      data)
{
  char buffer[256];

  g_snprintf (buffer, sizeof (buffer), "%.2f", g_value_get_float (from_value));
  g_value_set_string (to_value, buffer);

  return TRUE;
}

GtkWidget *
do_shadertoy (GtkWidget *do_widget)
{

  if (!window)
    {
      GtkBuilder *builder;
      GtkWidget *content;
      GtkWidget *rewind;
      GtkWidget *play;
      GtkWidget *time;
      GtkTextBuffer *buffer;

      builder = gtk_builder_new_from_resource ("/shadertoy/shadertoy.ui");

      window = GTK_WIDGET (gtk_builder_get_object (builder, "window"));
      content = GTK_WIDGET (gtk_builder_get_object (builder, "content"));
      textview = GTK_WIDGET (gtk_builder_get_object (builder, "text"));
      rewind = GTK_WIDGET (gtk_builder_get_object (builder, "rewind"));
      play = GTK_WIDGET (gtk_builder_get_object (builder, "play"));
      time = GTK_WIDGET (gtk_builder_get_object (builder, "time"));

      gtk_window_set_screen (GTK_WINDOW (window),
                             gtk_widget_get_screen (do_widget));
      g_signal_connect (window, "destroy",
                        G_CALLBACK (gtk_widget_destroyed), &window);

      toy = gtk_shadertoy_new ();
      setup_vertex_shader ();
      buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (textview));
      gtk_text_buffer_set_text (buffer, frag_text, -1);

      gtk_widget_set_hexpand (toy, TRUE);
      gtk_widget_set_vexpand (toy, TRUE);
      gtk_container_add (GTK_CONTAINER (content), toy);

      g_signal_connect (play, "clicked", G_CALLBACK (play_cb), NULL);
      g_signal_connect (rewind, "clicked", G_CALLBACK (rewind_cb), NULL);
      g_object_bind_property_full (toy, "time",
                                   time, "label",
                                   G_BINDING_SYNC_CREATE,
                                   format_time,
                                   NULL,
                                   NULL,
                                   NULL);

      g_object_unref (builder);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);

  return window;
}
