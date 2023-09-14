#include "private_api.h"

typedef struct shadow_vs_uniforms_t {
    mat4 mat_light_vp;
    vec2 joint_uv;
    float joint_pixel_width;
} shadow_vs_uniforms_t;

sg_image joint_texture();
float joint_pixel_width();
float joint_texture_u();
float joint_texture_v();

static const char *shd_v = 
    SOKOL_SHADER_HEADER
    "uniform sampler2D u_joint_tex;\n"

    "uniform mat4 u_mat_light_vp;\n"
    "uniform vec2 u_joint_uv;\n"
    "uniform float u_joint_pixel_width;\n"

    "in vec4 v_position;\n"
    "in vec3 v_normal;\n"
    "in vec4 jindices;\n"
    "in vec4 jweights;\n"
    
    "out vec2 proj_zw;\n"

    "void skinned_pos(in vec4 pos, in vec4 skin_weights, in vec4 skin_indices, in vec2 u_joint_uv, out vec4 skin_pos) {\n"
            "skin_pos = vec4(0.0, 0.0, 0.0, 1.0);\n"
            "vec4 weights = skin_weights / dot(skin_weights, vec4(1.0));\n"
            "vec2 step = vec2(u_joint_pixel_width, 0.0);\n"
            "vec2 uv;\n"
            "vec4 xxxx, yyyy, zzzz;\n"

            "if (skin_weights.x <= 0.0 && skin_weights.y <= 0.0 && skin_weights.z <= 0.0 && skin_weights.w <= 0.0) {\n"
            "    skin_pos = pos;\n"
            "    return;\n"
            "}\n"

            "if (weights.x > 0.0) {\n"
                "uv = vec2(u_joint_uv.x + (3.0 * skin_indices.x)*u_joint_pixel_width, u_joint_uv.y);\n"
                "xxxx = textureLod(u_joint_tex, uv, 0.0);\n"
                "yyyy = textureLod(u_joint_tex, uv + step, 0.0);\n"
                "zzzz = textureLod(u_joint_tex, uv + 2.0 * step, 0.0);\n"
                "skin_pos.xyz += vec3(dot(pos,xxxx), dot(pos,yyyy), dot(pos,zzzz)) * weights.x;\n"
            "}\n"
            "if (weights.y > 0.0) {\n"
                "uv = vec2(u_joint_uv.x + (3.0 * skin_indices.y)*u_joint_pixel_width, u_joint_uv.y);\n"
                "xxxx = textureLod(u_joint_tex, uv, 0.0);\n"
                "yyyy = textureLod(u_joint_tex, uv + step, 0.0);\n"
                "zzzz = textureLod(u_joint_tex, uv + 2.0 * step, 0.0);\n"
                "skin_pos.xyz += vec3(dot(pos,xxxx), dot(pos,yyyy), dot(pos,zzzz)) * weights.y;\n"
            "}\n"
            "if (weights.z > 0.0) {\n"
                "uv = vec2(u_joint_uv.x + (3.0 * skin_indices.z)*u_joint_pixel_width, u_joint_uv.y);\n"
                "xxxx = textureLod(u_joint_tex, uv, 0.0);\n"
                "yyyy = textureLod(u_joint_tex, uv + step, 0.0);\n"
                "zzzz = textureLod(u_joint_tex, uv + 2.0 * step, 0.0);\n"
                "skin_pos.xyz += vec3(dot(pos,xxxx), dot(pos,yyyy), dot(pos,zzzz)) * weights.z;\n"
            "}\n"
            "if (weights.w > 0.0) {\n"
                "uv = vec2(u_joint_uv.x + (3.0 * skin_indices.w)*u_joint_pixel_width, u_joint_uv.y);\n"
                "xxxx = textureLod(u_joint_tex, uv, 0.0);\n"
                "yyyy = textureLod(u_joint_tex, uv + step, 0.0);\n"
                "zzzz = textureLod(u_joint_tex, uv + 2.0 * step, 0.0);\n"
                "skin_pos.xyz += vec3(dot(pos,xxxx), dot(pos,yyyy), dot(pos,zzzz)) * weights.w;\n"
            "}\n"
        "}\n"
        
        "void main() {\n"
        "  vec4 pos;\n"
        "  skinned_pos(v_position, jweights, jindices * 255.0, u_joint_uv, pos);\n"
        "  if (jweights.x <= 0.0 && jweights.y <= 0.0 && jweights.z <= 0.0 && jweights.w <= 0.0) {\n"
        "   gl_Position = u_mat_light_vp * mat4(1.0) * v_position;\n"
        "  } else {\n"
        "   pos = vec4(dot(pos,vec4(25.0, 0.0, 0.0, 0.0)), dot(pos,vec4(0.0, 25.0, 0.0, 0.0)), dot(pos,vec4(0.0, 0.0, -25.0, -25.0)), 1.0);\n"
        "   gl_Position = u_mat_light_vp * pos;\n"
        "  }\n"
        "  proj_zw = gl_Position.zw;\n"
        "}\n";

static const char *shd_f =
    SOKOL_SHADER_HEADER
    "in vec2 proj_zw;\n"
    "out vec4 frag_color;\n"

    "vec4 encodeDepth(float v) {\n"
    "    vec4 enc = vec4(1.0, 255.0, 65025.0, 160581375.0) * v;\n"
    "    enc = fract(enc);\n"
    "    enc -= enc.yzww * vec4(1.0/255.0,1.0/255.0,1.0/255.0,0.0);\n"
    "    return enc;\n"
    "}\n"

    "void main() {\n"
    "  float depth = proj_zw.x / proj_zw.y;\n"
    "  frag_color = encodeDepth(depth);\n"
    "}\n";

sokol_offscreen_pass_t sokol_init_shadow_pass(
    int size)
{
    ecs_trace("sokol: initialize shadow pipeline");

    sokol_offscreen_pass_t result = {0};

    result.pass_action  = (sg_pass_action) {
        .colors[0] = { 
            .load_action = SG_LOADACTION_CLEAR, 
            .clear_value = { 1.0f, 1.0f, 1.0f, 1.0f}
        }
    };

    result.depth_target = sokol_target_depth(size, size, 1);
    result.color_target = sokol_target_rgba8("Shadow map", size, size, 1);

    result.pass = sg_make_pass(&(sg_pass_desc){
        .color_attachments[0].image = result.color_target,
        .depth_stencil_attachment.image = result.depth_target,
        .label = "shadow-map-pass"
    });

    sg_shader shd = sg_make_shader(&(sg_shader_desc){
        .vs = {
            .images = {
                [0] = {
                    .name = "u_joint_tex",
                    .image_type = SG_IMAGETYPE_2D
                }
            },
            .uniform_blocks = {
                [0] = {
                    .size = sizeof(shadow_vs_uniforms_t),
                    .uniforms = {
                        [0] = { .name="u_mat_light_vp", .type=SG_UNIFORMTYPE_MAT4 },
                        [1] = { .name="u_joint_uv", .type=SG_UNIFORMTYPE_FLOAT2 },
                        [2] = { .name="u_joint_pixel_width", .type=SG_UNIFORMTYPE_FLOAT},
                        [3] = { .name="padding", .type=SG_UNIFORMTYPE_FLOAT}
                    },
                }
            }
        },
        .vs.source = shd_v,
        .fs.source = shd_f
    });

    /* Create pipeline that mimics the normal pipeline, but without the material
     * normals and color, and with front culling instead of back culling */
    result.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = shd,
        .index_type = SG_INDEXTYPE_UINT16,
        .layout = {
            .buffers = {
                [0] =    { .stride = sizeof(sokol_geometry_vertex_t) }
            },

            .attrs = {
                /* Static geometry */
                [0] = { .buffer_index=0, .format=SG_VERTEXFORMAT_FLOAT3, .offset=offsetof(sokol_geometry_vertex_t, position) },
                [1] = { .buffer_index=0, .format=SG_VERTEXFORMAT_BYTE4N, .offset=offsetof(sokol_geometry_vertex_t, normal) },
                [2] = { .buffer_index=0, .format=SG_VERTEXFORMAT_UBYTE4N, .offset=offsetof(sokol_geometry_vertex_t, joint_indices) },
                [3] = { .buffer_index=0, .format=SG_VERTEXFORMAT_UBYTE4N, .offset=offsetof(sokol_geometry_vertex_t, joint_weights) },
            }
        },
        .depth = {
            .pixel_format = SG_PIXELFORMAT_DEPTH,
            .compare = SG_COMPAREFUNC_LESS_EQUAL,
            .write_enabled = true
        },
        .colors = {{
            .pixel_format = SG_PIXELFORMAT_RGBA8
        }},
        .face_winding = SG_FACEWINDING_CCW,
        .cull_mode = SG_CULLMODE_FRONT
    });

    return result;
}

static
void shadow_draw_instances(
    SokolGeometry *geometry,
    sokol_geometry_buffers_t *buffers)
{
    if (!buffers->instance_count) {
        return;
    }

    sg_bindings bind = {
        .vertex_buffers = {
            [0] = geometry->vertices
        },
        .index_buffer = geometry->indices,
        .vs_images[0] = joint_texture()
    };

    sg_apply_bindings(&bind);
    sg_draw(0, geometry->index_count, buffers->instance_count);
}

void sokol_run_shadow_pass(
    sokol_offscreen_pass_t *pass,
    sokol_render_state_t *state) 
{
    /* Render to offscreen texture so screen-space effects can be applied */
    sg_begin_pass(pass->pass, &pass->pass_action);
    sg_apply_pipeline(pass->pip);

    shadow_vs_uniforms_t vs_u;
    glm_mat4_copy(state->uniforms.light_mat_vp, vs_u.mat_light_vp);
    vs_u.joint_uv[0] = joint_texture_u();
    vs_u.joint_uv[1] = joint_texture_v();
    vs_u.joint_pixel_width = joint_pixel_width();
    sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, &(sg_range){ 
        &vs_u, sizeof(shadow_vs_uniforms_t) 
    });

    /* Loop buffers, render scene */
    ecs_iter_t qit = ecs_query_iter(state->world, state->q_scene);
    while (ecs_query_next(&qit)) {
        SokolGeometry *geometry = ecs_field(&qit, SokolGeometry, 1);
        
        int b;
        for (b = 0; b < qit.count; b ++) {
            /* Only draw solids, ignore emissive and transparent (for now) */
            shadow_draw_instances(&geometry[b], geometry[b].solid);
        }
    }

    sg_end_pass();
}
