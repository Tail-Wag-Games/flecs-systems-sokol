#include "private_api.h"

typedef struct scene_vs_uniforms_t {
    mat4 mat_v;
    mat4 mat_vp;
    mat4 light_mat_vp;
    vec2 joint_uv;
    float joint_pixel_width;
} scene_vs_uniforms_t;

typedef struct scene_fs_uniforms_t {
    vec3 light_ambient;
    vec3 light_direction;
    vec3 light_color;
    vec3 eye_pos;
    float shadow_map_size;
    float shadow_far;
} scene_fs_uniforms_t;

typedef struct scene_fs_sun_atmos_uniforms_t {
    vec3 sun_screen_pos;
    vec3 sun_color;
    vec2 target_size;
    float aspect;
    float sun_intensity;
} scene_fs_sun_atmos_uniforms_t;

sg_image joint_texture();
sg_sampler joint_sampler();
float joint_pixel_width();
float joint_texture_u();
float joint_texture_v();

#define VERTEX_I 0
#define COLOR_I 1
#define MATERIAL_I 2
#define TRANSFORM_I 3
#define SKIN_I 4

#define VERTEX_ATTR_I 0
#define COLOR_ATTR_I 4
#define MATERIAL_ATTR_I 5
#define TRANSFORM_ATTR_I 6
#define SKIN_ATTR_I 10

#define LAYOUT_I_STR(i) #i
#define LAYOUT(loc) "layout(location=" LAYOUT_I_STR(loc) ") "

sg_pipeline init_scene_pipeline(int32_t sample_count) {
    char *vs = sokol_shader_from_str(
        SOKOL_SHADER_HEADER
        "uniform sampler2D u_joint_tex;\n"

        "uniform mat4 u_mat_vp;\n"
        "uniform mat4 u_mat_v;\n"
        "uniform mat4 u_light_vp;\n"
        "uniform vec2 u_joint_uv;\n"
        "uniform float u_joint_pixel_width;\n"

        "in vec4 v_position;\n"
        "in vec3 v_normal;\n"
        "in vec4 jindices;\n"
        "in vec4 jweights;\n"
        "in vec3 i_color;\n"
        "in vec3 i_material;\n"
        "in mat4 i_mat_m;\n"
        "#include \"etc/sokol/shaders/scene_vert.glsl\"\n"
    );

    char *fs = sokol_shader_from_str(
        SOKOL_SHADER_HEADER
        "#include \"etc/sokol/shaders/scene_frag.glsl\"\n"
    );

    /* create an instancing shader */
    sg_shader shd = sg_make_shader(&(sg_shader_desc){
        .vs = {
            .images = {
                [0] = {
                    .image_type = SG_IMAGETYPE_2D,
                    .used = true
                }
            },
            .samplers = {
                [0] = {
                    .used = true
                }
            },
            .image_sampler_pairs = {
                [0] = {
                    .used = true,
                    .image_slot = 0,
                    .sampler_slot = 0,
                    .glsl_name = "u_joint_tex"
                }
            },
            .uniform_blocks = {
                [0] = {
                    .size = sizeof(scene_vs_uniforms_t),
                    .uniforms = {
                        [0] = { .name="u_mat_v", .type=SG_UNIFORMTYPE_MAT4 },
                        [1] = { .name="u_mat_vp", .type=SG_UNIFORMTYPE_MAT4 },
                        [2] = { .name="u_light_vp", .type=SG_UNIFORMTYPE_MAT4 },
                        [3] = { .name="u_joint_uv", .type=SG_UNIFORMTYPE_FLOAT2 },
                        [4] = { .name="u_joint_pixel_width", .type=SG_UNIFORMTYPE_FLOAT},
                        [5] = { .name="padding", .type=SG_UNIFORMTYPE_FLOAT}
                    },
                }
            }
        },
        .fs = {
            .images = {
                [0] = {
                    .image_type = SG_IMAGETYPE_2D,
                    .used = true
                }
            },
            .samplers = {
                [0] = {
                    .used = true
                }
            },
            .image_sampler_pairs = {
                [0] = {
                    .used = true,
                    .image_slot = 0,
                    .sampler_slot = 0,
                    .glsl_name = "shadow_map"
                }
            },
            .uniform_blocks = {
                [0] = {
                    .size = sizeof(scene_fs_uniforms_t),
                    .uniforms = {
                        [0] = { .name="u_light_ambient", .type=SG_UNIFORMTYPE_FLOAT3 },
                        [1] = { .name="u_light_direction", .type=SG_UNIFORMTYPE_FLOAT3 },
                        [2] = { .name="u_light_color", .type=SG_UNIFORMTYPE_FLOAT3 },
                        [3] = { .name="u_eye_pos", .type=SG_UNIFORMTYPE_FLOAT3 },
                        [4] = { .name="u_shadow_map_size", .type=SG_UNIFORMTYPE_FLOAT },
                        [5] = { .name="u_shadow_far", .type=SG_UNIFORMTYPE_FLOAT }
                    }
                }
            }
        },

        .vs.source = vs,
        .fs.source = fs
    });

    ecs_os_free(vs);
    ecs_os_free(fs);

    return sg_make_pipeline(&(sg_pipeline_desc){
        .shader = shd,
        .index_type = SG_INDEXTYPE_UINT16,
        .layout = {
            .buffers = {
                [VERTEX_I] =    { .stride = sizeof(sokol_geometry_vertex_t) },
                [COLOR_I] =     { .stride = 12, .step_func=SG_VERTEXSTEP_PER_INSTANCE },
                [MATERIAL_I] =  { .stride = 12, .step_func=SG_VERTEXSTEP_PER_INSTANCE },
                [TRANSFORM_I] = { .stride = 64, .step_func=SG_VERTEXSTEP_PER_INSTANCE },
                [SKIN_I] = { .stride = sizeof(sokol_skin_t), .step_func=SG_VERTEXSTEP_PER_INSTANCE }
            },

            .attrs = {
                /* Static geometry */
                [VERTEX_ATTR_I] =      { .buffer_index=VERTEX_I, .format=SG_VERTEXFORMAT_FLOAT3, .offset=offsetof(sokol_geometry_vertex_t, position) },
                [VERTEX_ATTR_I + 1] =        { .buffer_index=VERTEX_I, .format=SG_VERTEXFORMAT_BYTE4N, .offset=offsetof(sokol_geometry_vertex_t, normal)},
                [VERTEX_ATTR_I + 2] =        { .buffer_index=VERTEX_I, .format=SG_VERTEXFORMAT_UBYTE4N, .offset=offsetof(sokol_geometry_vertex_t, joint_indices)},
                [VERTEX_ATTR_I + 3] =        { .buffer_index=VERTEX_I, .format=SG_VERTEXFORMAT_UBYTE4N, .offset=offsetof(sokol_geometry_vertex_t, joint_weights)},

                /* Color buffer (per instance) */
                [COLOR_ATTR_I] =         { .buffer_index=COLOR_I, .offset=0, .format=SG_VERTEXFORMAT_FLOAT3 },

                /* Material buffer (per instance) */
                [MATERIAL_ATTR_I] =      { .buffer_index=MATERIAL_I, .offset=0, .format=SG_VERTEXFORMAT_FLOAT3 },

                /* Matrix (per instance) */
                [TRANSFORM_ATTR_I] =     { .buffer_index=TRANSFORM_I, .offset=0,  .format=SG_VERTEXFORMAT_FLOAT4 },
                [TRANSFORM_ATTR_I + 1] = { .buffer_index=TRANSFORM_I, .offset=16, .format=SG_VERTEXFORMAT_FLOAT4 },
                [TRANSFORM_ATTR_I + 2] = { .buffer_index=TRANSFORM_I, .offset=32, .format=SG_VERTEXFORMAT_FLOAT4 },
                [TRANSFORM_ATTR_I + 3] = { .buffer_index=TRANSFORM_I, .offset=48, .format=SG_VERTEXFORMAT_FLOAT4 },

                [SKIN_ATTR_I] = { .buffer_index=SKIN_I, .offset=48, .format=SG_VERTEXFORMAT_FLOAT2 }
            }
        },

        .depth = {
            .pixel_format = SG_PIXELFORMAT_DEPTH,
            .compare = SG_COMPAREFUNC_LESS_EQUAL,
            .write_enabled = false
        },

        .colors = {{
            .pixel_format = SG_PIXELFORMAT_RGBA16F
        }},

        .face_winding = SG_FACEWINDING_CCW,
        .cull_mode = SG_CULLMODE_BACK,
        .sample_count = sample_count
    });
}

sg_pipeline init_scene_atmos_sun_pipeline(int32_t sample_count) {
    char *fs = sokol_shader_from_str(
        SOKOL_SHADER_HEADER
        "#include \"etc/sokol/shaders/scene_atmos_sun.frag\"\n"
    );

    sg_shader shd = sg_make_shader(&(sg_shader_desc){
        .vs.source = 
             SOKOL_SHADER_HEADER
            "in vec4 v_position;\n"
            "in vec2 v_uv;\n"
            "out vec2 uv;\n"
            "void main() {\n"
            "  gl_Position = v_position;\n"
            "  gl_Position.z = 1.0;\n"
            "  uv = v_uv;\n"
            "}\n",

        .fs = {
            .source = fs,
            .uniform_blocks = {
                [0] = {
                    .size = sizeof(scene_fs_sun_atmos_uniforms_t),
                    .uniforms = {
                        [0] = { .name="u_sun_screen_pos", .type=SG_UNIFORMTYPE_FLOAT3 },
                        [1] = { .name="u_sun_color", .type=SG_UNIFORMTYPE_FLOAT3 },
                        [2] = { .name="u_target_size", .type=SG_UNIFORMTYPE_FLOAT2 },
                        [3] = { .name="u_aspect", .type=SG_UNIFORMTYPE_FLOAT },
                        [4] = { .name="u_sun_intensity", .type=SG_UNIFORMTYPE_FLOAT }
                    }
                }
            },
            .images[0] = {
                .used = true,
                .image_type = SG_IMAGETYPE_2D
            },
            .samplers[0] = {
                .used = true,
            },
            .image_sampler_pairs[0] = {
                .used = true,
                .image_slot = 0,
                .sampler_slot = 0,
                .glsl_name = "atmos"
            },
        }
    });

    ecs_os_free(fs);

    /* create a pipeline object (default render state is fine) */
    return sg_make_pipeline(&(sg_pipeline_desc){
        .shader = shd,
        .layout = {         
            .attrs = {
                /* Static geometry (position, uv) */
                [0] = { .buffer_index=0, .format=SG_VERTEXFORMAT_FLOAT3 },
                [1] = { .buffer_index=0, .format=SG_VERTEXFORMAT_FLOAT2 }
            }
        },

        .depth = {
            .pixel_format = SG_PIXELFORMAT_DEPTH,
            .compare = SG_COMPAREFUNC_LESS_EQUAL,
            .write_enabled = false
        },

        .colors = {{
            .pixel_format = SG_PIXELFORMAT_RGBA16F
        }},
    });
}

static
void update_scene_pass(
    sokol_offscreen_pass_t *pass,
    int32_t w, 
    int32_t h,
    int32_t sample_count)
{
    pass->color_target = sokol_target_rgba16f(
        "Scene color target", w, h, sample_count, 1);
    pass->color_sampler = sokol_sampler(1);
    pass->depth_target = sokol_target_depth(w, h, sample_count);

    pass->pass = sg_make_pass(&(sg_pass_desc){
        .color_attachments[0].image = pass->color_target,
        .depth_stencil_attachment.image = pass->depth_target
    });
}

sokol_offscreen_pass_t sokol_init_scene_pass(
    ecs_rgb_t background_color,
    int32_t w, 
    int32_t h,
    int32_t sample_count,
    sokol_offscreen_pass_t *depth_pass_out) 
{
    ecs_trace("sokol: initialize scene pass");
    sokol_offscreen_pass_t pass;
    ecs_os_zeromem(&pass);
    update_scene_pass(&pass, w, h, sample_count);

    *depth_pass_out = sokol_init_depth_pass(w, h, 
        pass.depth_target, sample_count);

    pass.pass_action = sokol_clear_action(background_color, false, false);
    pass.pip = init_scene_pipeline(sample_count);
    pass.pip_2 = init_scene_atmos_sun_pipeline(sample_count);
    pass.sample_count = sample_count;

    ecs_trace("sokol: initialize scene pipeline");
    return pass;
}

void sokol_update_scene_pass(
    sokol_offscreen_pass_t *pass,
    int32_t w,
    int32_t h,
    sokol_offscreen_pass_t *depth_pass)
{
    ecs_dbg_3("sokol: update scene pass");
    sg_destroy_pass(pass->pass);
    sg_destroy_image(pass->color_target);
    sg_destroy_image(pass->depth_target);

    update_scene_pass(pass, w, h, pass->sample_count);
    sokol_update_depth_pass(depth_pass, w, h, 
        pass->depth_target, pass->sample_count);
}

static
void scene_draw_atmos(
    sokol_render_state_t *state)
{
    sg_bindings bind = {
        .vertex_buffers = { 
            [0] = state->resources->quad 
        },
        .fs.images[0] = state->atmos,
        .fs.samplers[0] = state->atmos_sampler
    };

    sg_apply_bindings(&bind);
    sg_draw(0, 6, 1);
}

static
void scene_draw_instances(
    SokolGeometry *geometry,
    sokol_geometry_buffers_t *buffers,
    sg_image shadow_map,
    sg_sampler shadow_map_sampler)
{
    if (!buffers->instance_count) {
        return;
    }

    sg_bindings bind = {
        .vertex_buffers = {
            [VERTEX_I] =  geometry->vertices,
            [COLOR_I] =     buffers->colors,
            [MATERIAL_I] =  buffers->materials,
            [TRANSFORM_I] = buffers->transforms,
            [SKIN_I] = buffers->skins,
        },
        .index_buffer = geometry->indices,
        .vs.images[0] = joint_texture(),
        .vs.samplers[0] = joint_sampler(),
        .fs.images[0] = shadow_map,
        .fs.samplers[0] = shadow_map_sampler
    };

    sg_apply_bindings(&bind);
    sg_draw(0, geometry->index_count, buffers->instance_count);
}

void sokol_run_scene_pass(
    sokol_offscreen_pass_t *pass,
    sokol_render_state_t *state)
{
    scene_vs_uniforms_t vs_u;
    glm_mat4_copy(state->uniforms.mat_v, vs_u.mat_v);
    glm_mat4_copy(state->uniforms.mat_vp, vs_u.mat_vp);
    glm_mat4_copy(state->uniforms.light_mat_vp, vs_u.light_mat_vp);
    vs_u.joint_uv[0] = joint_texture_u();
    vs_u.joint_uv[1] = joint_texture_v();
    vs_u.joint_pixel_width = joint_pixel_width();

    scene_fs_uniforms_t fs_u;
    glm_vec3_copy(state->uniforms.light_ambient, fs_u.light_ambient);
    glm_vec3_copy(state->uniforms.sun_direction, fs_u.light_direction);
    glm_vec3_copy(state->uniforms.sun_color, fs_u.light_color);
    glm_vec3_scale(fs_u.light_color, state->uniforms.sun_intensity, fs_u.light_color);
    glm_vec3_copy(state->uniforms.eye_pos, fs_u.eye_pos);
    fs_u.shadow_map_size = state->uniforms.shadow_map_size;
    fs_u.shadow_far = state->uniforms.shadow_far;
    fs_u.eye_pos[0] *= -1;

    scene_fs_sun_atmos_uniforms_t fs_sun_atmos_u;
    glm_vec3_copy(state->uniforms.sun_screen_pos, fs_sun_atmos_u.sun_screen_pos);
    glm_vec3_copy(state->uniforms.sun_color, fs_sun_atmos_u.sun_color);
    fs_sun_atmos_u.target_size[0] = state->width;
    fs_sun_atmos_u.target_size[1] = state->height;
    fs_sun_atmos_u.aspect = state->uniforms.aspect;
    fs_sun_atmos_u.sun_intensity = 1.0 + state->uniforms.sun_intensity * 6;

    /* Render to offscreen texture so screen-space effects can be applied */
    sg_begin_pass(pass->pass, &pass->pass_action);

    /* Step 1: render atmosphere background */
    sg_apply_pipeline(pass->pip_2);
    sg_apply_uniforms(SG_SHADERSTAGE_FS, 0, &(sg_range){&fs_sun_atmos_u, sizeof(scene_fs_sun_atmos_uniforms_t)});
    scene_draw_atmos(state);

    /* Step 2: render scene */
    sg_apply_pipeline(pass->pip);
    sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, &(sg_range){&vs_u, sizeof(scene_vs_uniforms_t)});
    sg_apply_uniforms(SG_SHADERSTAGE_FS, 0, &(sg_range){&fs_u, sizeof(scene_fs_uniforms_t)});

    /* Loop geometry, render scene */
    ecs_iter_t qit = ecs_query_iter(state->world, state->q_scene);
    while (ecs_query_next(&qit)) {
        SokolGeometry *geometry = ecs_field(&qit, SokolGeometry, 1);

        int b;
        for (b = 0; b < qit.count; b ++) {
            scene_draw_instances(&geometry[b], geometry[b].solid, state->shadow_map, state->shadow_map_sampler);
            scene_draw_instances(&geometry[b], geometry[b].emissive, state->shadow_map, state->shadow_map_sampler);
        }
    }
    sg_end_pass();
}

#undef VERTEX_I
#undef COLOR_I
#undef MATERIAL_I
#undef TRANSFORM_I

#undef VERTEX_ATTR_I
#undef COLOR_ATTR_I
#undef MATERIAL_ATTR_I
#undef TRANSFORM_ATTR_I

#undef LAYOUT
