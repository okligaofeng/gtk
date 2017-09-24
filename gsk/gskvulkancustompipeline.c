#include "config.h"

#include "gskvulkancustompipelineprivate.h"

struct _GskVulkanCustomPipeline
{
  GObject parent_instance;
};

typedef struct _GskVulkanCustomInstance GskVulkanCustomInstance;

struct _GskVulkanCustomInstance
{
  float rect[4];
  float tex_rect1[4];
  float tex_rect2[4];
  float time;
};

G_DEFINE_TYPE (GskVulkanCustomPipeline, gsk_vulkan_custom_pipeline, GSK_TYPE_VULKAN_PIPELINE)

static const VkPipelineVertexInputStateCreateInfo *
gsk_vulkan_custom_pipeline_get_input_state_create_info (GskVulkanPipeline *self)
{
  static const VkVertexInputBindingDescription vertexBindingDescriptions[] = {
      {
          .binding = 0,
          .stride = sizeof (GskVulkanCustomInstance),
          .inputRate = VK_VERTEX_INPUT_RATE_INSTANCE
      }
  };
  static const VkVertexInputAttributeDescription vertexInputAttributeDescription[] = {
      {
          .location = 0,
          .binding = 0,
          .format = VK_FORMAT_R32G32B32A32_SFLOAT,
          .offset = 0,
      },
      {
          .location = 1,
          .binding = 0,
          .format = VK_FORMAT_R32G32B32A32_SFLOAT,
          .offset = G_STRUCT_OFFSET (GskVulkanCustomInstance, tex_rect1),
      },
      {
          .location = 2,
          .binding = 0,
          .format = VK_FORMAT_R32G32B32A32_SFLOAT,
          .offset = G_STRUCT_OFFSET (GskVulkanCustomInstance, tex_rect2),
      },
      {
          .location = 3,
          .binding = 0,
          .format = VK_FORMAT_R32_SFLOAT,
          .offset = G_STRUCT_OFFSET (GskVulkanCustomInstance, time),
      },
  };
  static const VkPipelineVertexInputStateCreateInfo info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      .vertexBindingDescriptionCount = G_N_ELEMENTS (vertexBindingDescriptions),
      .pVertexBindingDescriptions = vertexBindingDescriptions,
      .vertexAttributeDescriptionCount = G_N_ELEMENTS (vertexInputAttributeDescription),
      .pVertexAttributeDescriptions = vertexInputAttributeDescription
  };

  return &info;
}

static void
gsk_vulkan_custom_pipeline_finalize (GObject *gobject)
{
  //GskVulkanCustomPipeline *self = GSK_VULKAN_BLUR_PIPELINE (gobject);

  G_OBJECT_CLASS (gsk_vulkan_custom_pipeline_parent_class)->finalize (gobject);
}

static void
gsk_vulkan_custom_pipeline_class_init (GskVulkanCustomPipelineClass *klass)
{
  GskVulkanPipelineClass *pipeline_class = GSK_VULKAN_PIPELINE_CLASS (klass);

  G_OBJECT_CLASS (klass)->finalize = gsk_vulkan_custom_pipeline_finalize;

  pipeline_class->get_input_state_create_info = gsk_vulkan_custom_pipeline_get_input_state_create_info;
}

static void
gsk_vulkan_custom_pipeline_init (GskVulkanCustomPipeline *self)
{
}

GskVulkanPipeline *
gsk_vulkan_custom_pipeline_new (GdkVulkanContext      *context,
                                VkPipelineLayout       layout,
                                GBytes                *vertex_bytes,
                                GBytes                *fragment_bytes,
                                VkRenderPass           render_pass)
{
  GskVulkanShader *vertex_shader;
  GskVulkanShader *fragment_shader;
  GError *error = NULL;

  if (vertex_bytes != NULL)
    vertex_shader = gsk_vulkan_shader_new_from_bytes (context,
                                                      GSK_VULKAN_SHADER_VERTEX,
                                                      vertex_bytes,
                                                      &error);
  else
    vertex_shader = gsk_vulkan_shader_new_from_resource (context,
                                                         GSK_VULKAN_SHADER_VERTEX,
                                                         "color", // FIXME
                                                         &error);
  if (vertex_shader == NULL)
    {
      g_error ("%s", error->message);
      g_error_free (error);
      return NULL;
    }

  fragment_shader = gsk_vulkan_shader_new_from_bytes (context,
                                                      GSK_VULKAN_SHADER_FRAGMENT,
                                                      fragment_bytes,
                                                      &error);
  if (fragment_shader == NULL)
    {
      g_error ("%s", error->message);
      g_error_free (error);
      return NULL;
    }

  return gsk_vulkan_pipeline_new_with_shaders (GSK_TYPE_VULKAN_CUSTOM_PIPELINE,
                                               context, layout,
                                               vertex_shader,
                                               fragment_shader,
                                               render_pass,
                                               VK_BLEND_FACTOR_ONE,
                                               VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA);
}

gsize
gsk_vulkan_custom_pipeline_count_vertex_data (GskVulkanCustomPipeline *pipeline)
{
  return sizeof (GskVulkanCustomInstance);
}

void
gsk_vulkan_custom_pipeline_collect_vertex_data (GskVulkanCustomPipeline *pipeline,
                                                guchar                  *data,
                                                const graphene_rect_t   *bounds,
                                                const graphene_rect_t   *child1_bounds,
                                                const graphene_rect_t   *child2_bounds,
                                                float                    time)
{
  GskVulkanCustomInstance *instance = (GskVulkanCustomInstance *) data;

  instance->rect[0] = bounds->origin.x;
  instance->rect[1] = bounds->origin.y;
  instance->rect[2] = bounds->size.width;
  instance->rect[3] = bounds->size.height;

  instance->tex_rect1[0] = (bounds->origin.x - child1_bounds->origin.x)/child1_bounds->size.width;
  instance->tex_rect1[1] = (bounds->origin.y - child1_bounds->origin.y)/child1_bounds->size.height;
  instance->tex_rect1[2] = (bounds->size.width + bounds->origin.x - child1_bounds->origin.x)/child1_bounds->size.width;
  instance->tex_rect1[3] = (bounds->size.height + bounds->origin.y - child1_bounds->origin.y)/child1_bounds->size.height;

  instance->tex_rect2[0] = (bounds->origin.x - child2_bounds->origin.x)/child2_bounds->size.width;
  instance->tex_rect2[1] = (bounds->origin.y - child2_bounds->origin.y)/child2_bounds->size.height;
  instance->tex_rect2[2] = (bounds->size.width + bounds->origin.x - child2_bounds->origin.x)/child2_bounds->size.width;
  instance->tex_rect2[3] = (bounds->size.height + bounds->origin.y - child2_bounds->origin.y)/child2_bounds->size.height;

  instance->time = time;
}

gsize
gsk_vulkan_custom_pipeline_draw (GskVulkanCustomPipeline *pipeline,
                                 VkCommandBuffer          command_buffer,
                                 gsize                    offset,
                                 gsize                    n_commands)
{
  vkCmdDraw (command_buffer,
             6, n_commands,
             0, offset);

  return n_commands;
}
