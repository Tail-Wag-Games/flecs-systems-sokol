#include "private_api.h"

typedef struct depth_vs_uniforms_t {
    mat4 mat_vp;
    vec2 joint_uv;
    float joint_pixel_width;
} depth_vs_uniforms_t;

typedef struct depth_fs_uniforms_t {
    vec3 eye_pos;
    float near_;
    float far_;
    float depth_c;
    float inv_log_far;
} depth_fs_uniforms_t;

sg_image joint_texture();
float joint_pixel_width();
float joint_texture_u();
float joint_texture_v();

const char* sokol_vs_depth(void) 
{
    return SOKOL_SHADER_HEADER
        "uniform sampler2D u_joint_tex;\n"

        "uniform mat4 u_mat_vp;\n"
        "uniform vec2 u_joint_uv;\n"
        "uniform float u_joint_pixel_width;\n"
        
        "in vec4 v_position;\n"
        "in vec4 jindices;\n"
        "in vec4 jweights;\n"

        "in mat4 i_mat_m;\n"
        "out vec3 position;\n"

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
        "   gl_Position = u_mat_vp * i_mat_m * v_position;\n"
        "  } else {\n"
        "   pos = vec4(dot(pos,vec4(25.0, 0.0, 0.0, 0.0)), dot(pos,vec4(0.0, 25.0, 0.0, 0.0)), dot(pos,vec4(0.0, 0.0, -25.0, -25.0)), 1.0);\n"
        "   gl_Position = u_mat_vp * pos;\n"
        "  }\n"
        "  position = gl_Position.xyz;\n"
        "}\n";
}

const char* sokol_fs_depth(void) 
{
    return SOKOL_SHADER_HEADER
        "uniform vec3 u_eye_pos;\n"
        "uniform float u_near;\n"
        "uniform float u_far;\n"
        "uniform float u_depth_c;\n"
        "uniform float u_inv_log_far;\n"
        "in vec3 position;\n"
        "out vec4 frag_color;\n"

        SOKOL_SHADER_FUNC_FLOAT_TO_RGBA

#ifdef SOKOL_LOG_DEPTH
        "vec4 depth_to_rgba(vec3 pos) {\n"
        "  vec3 pos_norm = pos / u_far;\n"
        "  float d = length(pos_norm) * u_far;\n"
        "  d = clamp(d, 0.0, u_far * 0.98);\n"
        "  d = log(u_depth_c * d + 1.0) / log(u_depth_c * u_far + 1.0);\n"
        "  return float_to_rgba(d);\n"
        "}\n"
#else
        "vec4 depth_to_rgba(vec3 pos) {\n"
        "  vec3 pos_norm = pos / u_far;\n"
        "  float d = length(pos_norm);\n"
        "  d = clamp(d, 0.0, 1.0);\n"
        "  return float_to_rgba(d);\n"
        "}\n"
#endif

        "void main() {\n"
        "  frag_color = depth_to_rgba(position);\n"
        "}\n";
}

sg_pipeline init_depth_pipeline(int32_t sample_count) {
    ecs_trace("sokol: initialize depth pipeline");

    /* create an instancing shader */
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
                    .size = sizeof(depth_vs_uniforms_t),
                    .uniforms = {
                        [0] = { .name="u_mat_vp", .type=SG_UNIFORMTYPE_MAT4 },
                        [1] = { .name="u_joint_uv", .type=SG_UNIFORMTYPE_FLOAT2 },
                        [2] = { .name="u_joint_pixel_width", .type=SG_UNIFORMTYPE_FLOAT},
                        [3] = { .name="padding", .type=SG_UNIFORMTYPE_FLOAT}
                    },
                }
            }
        },
        .fs.uniform_blocks = { 
            [0] = {
                .size = sizeof(depth_fs_uniforms_t),
                .uniforms = {
                    [0] = { .name="u_eye_pos", .type=SG_UNIFORMTYPE_FLOAT3 },
                    [1] = { .name="u_near", .type=SG_UNIFORMTYPE_FLOAT },
                    [2] = { .name="u_far", .type=SG_UNIFORMTYPE_FLOAT },
                    [3] = { .name="u_depth_c", .type = SG_UNIFORMTYPE_FLOAT },
                    [4] = { .name="u_inv_log_far", .type = SG_UNIFORMTYPE_FLOAT }
                },
            }            
        },
        .vs.source = sokol_vs_depth(),
        .fs.source = sokol_fs_depth()
    });

    return sg_make_pipeline(&(sg_pipeline_desc){
        .shader = shd,
        .index_type = SG_INDEXTYPE_UINT16,
        .layout = {
            .buffers = {
                [0] =    { .stride = sizeof(sokol_geometry_vertex_t) },
                [1] = { .stride = 64, .step_func=SG_VERTEXSTEP_PER_INSTANCE }
            },

            .attrs = {
                /* Static geometry */
                [0] = { .buffer_index=0, .format=SG_VERTEXFORMAT_FLOAT3, .offset=offsetof(sokol_geometry_vertex_t, position) },
                [1] = { .buffer_index=0, .format=SG_VERTEXFORMAT_UBYTE4N, .offset=offsetof(sokol_geometry_vertex_t, joint_indices) },
                [2] = { .buffer_index=0, .format=SG_VERTEXFORMAT_UBYTE4N, .offset=offsetof(sokol_geometry_vertex_t, joint_weights) },
         
                /* Matrix (per instance) */
                [3] = { .buffer_index=1, .offset=0,  .format=SG_VERTEXFORMAT_FLOAT4 },
                [4] = { .buffer_index=1, .offset=16, .format=SG_VERTEXFORMAT_FLOAT4 },
                [5] = { .buffer_index=1, .offset=32, .format=SG_VERTEXFORMAT_FLOAT4 },
                [6] = { .buffer_index=1, .offset=48, .format=SG_VERTEXFORMAT_FLOAT4 }
            }
        },
        .depth = {
            .pixel_format = SG_PIXELFORMAT_DEPTH,
            .compare = SG_COMPAREFUNC_LESS_EQUAL,
            .write_enabled = true
        },
        .face_winding = SG_FACEWINDING_CCW,
        .colors = {{
            .pixel_format = SG_PIXELFORMAT_RGBA8
        }},
        .cull_mode = SG_CULLMODE_BACK,
        .sample_count = sample_count
    });
}

sokol_offscreen_pass_t sokol_init_depth_pass(
    int32_t w, 
    int32_t h,
    sg_image depth_target,
    int32_t sample_count) 
{
    ecs_trace("sokol: initialize depth pass");

    sg_image color_target = sokol_target_rgba8(
        "Depth color target", w, h, sample_count);
    ecs_rgb_t background_color = {1, 1, 1};

    return (sokol_offscreen_pass_t){
        .pass_action = sokol_clear_action(background_color, true, true),
        .pass = sg_make_pass(&(sg_pass_desc){
            .color_attachments[0].image = color_target,
            .depth_stencil_attachment.image = depth_target
        }),
        .pip = init_depth_pipeline(sample_count),
        .color_target = color_target,
        .depth_target = depth_target
    };
}

void sokol_update_depth_pass(
    sokol_offscreen_pass_t *pass,
    int32_t w,
    int32_t h,
    sg_image depth_target,
    int32_t sample_count)
{
    sg_destroy_image(pass->color_target);
    sg_destroy_pass(pass->pass);

    pass->color_target = sokol_target_rgba8(
        "Depth color target", w, h, sample_count);

    pass->pass = sg_make_pass(&(sg_pass_desc){
        .color_attachments[0].image = pass->color_target,
        .depth_stencil_attachment.image = depth_target
    });
}

static
void depth_draw_instances(
    SokolGeometry *geometry,
    sokol_geometry_buffers_t *buffers)
{
    if (!buffers->instance_count) {
        return;
    }

    sg_bindings bind = {
        .vertex_buffers = {
            [0] = geometry->vertices,
            [1] = buffers->transforms
        },
        .index_buffer = geometry->indices,
        .vs_images[0] = joint_texture()
    };

    sg_apply_bindings(&bind);
    sg_draw(0, geometry->index_count, buffers->instance_count);
}

void sokol_run_depth_pass(
    sokol_offscreen_pass_t *pass,
    sokol_render_state_t *state)
{
    depth_vs_uniforms_t vs_u;
    glm_mat4_copy(state->uniforms.mat_vp, vs_u.mat_vp);
    vs_u.joint_uv[0] = joint_texture_u();
    vs_u.joint_uv[1] = joint_texture_v();
    vs_u.joint_pixel_width = joint_pixel_width();
    
    depth_fs_uniforms_t fs_u;
    glm_vec3_copy(state->uniforms.eye_pos, fs_u.eye_pos);
    fs_u.near_ = state->uniforms.near_;
    fs_u.far_ = state->uniforms.far_;
    fs_u.depth_c = SOKOL_DEPTH_C;
    fs_u.inv_log_far = 1.0 / log(SOKOL_DEPTH_C * fs_u.far_ + 1.0);

    /* Render to offscreen texture so screen-space effects can be applied */
    sg_begin_pass(pass->pass, &pass->pass_action);
    sg_apply_pipeline(pass->pip);

    sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, &(sg_range){&vs_u, sizeof(depth_vs_uniforms_t)});
    sg_apply_uniforms(SG_SHADERSTAGE_FS, 0, &(sg_range){&fs_u, sizeof(depth_fs_uniforms_t)});

    /* Loop geometry, render scene */
    ecs_iter_t qit = ecs_query_iter(state->world, state->q_scene);
    while (ecs_query_next(&qit)) {
        SokolGeometry *geometry = ecs_field(&qit, SokolGeometry, 1);

        int b;
        for (b = 0; b < qit.count; b ++) {
            depth_draw_instances(&geometry[b], geometry[b].solid);
            depth_draw_instances(&geometry[b], geometry[b].emissive);
        }
    }

    sg_end_pass();
}
