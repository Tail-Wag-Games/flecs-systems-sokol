#include "geometry.h"
#include "../materials/materials.h"

ECS_COMPONENT_DECLARE(SokolGeometry);
ECS_COMPONENT_DECLARE(SokolGeometryQuery);

ECS_DECLARE(SokolRectangleGeometry);
ECS_DECLARE(SokolBoxGeometry);

static
sokol_geometry_buffers_t* sokol_create_buffers(void) {
    sokol_geometry_buffers_t *result = ecs_os_calloc_t(sokol_geometry_buffers_t);
    result->first = NULL;
    ecs_map_init(&result->index, sokol_geometry_buffer_t*, NULL, 0);
    return result;
}

static
sokol_geometry_buffer_t* sokol_create_buffer(
    ecs_world_t *world,
    sokol_geometry_buffers_t *buffers,
    ecs_entity_t buffer_id)
{
    sokol_geometry_buffer_t **ptr = ecs_map_ensure(&buffers->index, 
        sokol_geometry_buffer_t*, buffer_id);
    sokol_geometry_buffer_t *result = *ptr;
    if (!result) {
        result = *ptr = ecs_os_calloc_t(sokol_geometry_buffer_t);
        sokol_geometry_buffer_t *next = buffers->first;
        buffers->first = result;
        result->next = next;
        result->id = buffer_id;
        if (buffer_id) {
            buffer_id = ecs_get_alive(world, buffer_id);
            result->cell_coord = ecs_ref_init(world, buffer_id, 
                EcsWorldCellCoord);
        }
        if (next) {
            next->prev = result;
        }
    }
    return result;
}

static
void sokol_delete_buffer(
    sokol_geometry_buffers_t *buffers,
    sokol_geometry_buffer_t *buffer)
{
    ecs_assert(buffer != NULL, ECS_INTERNAL_ERROR, NULL);
    ecs_assert(buffers != NULL, ECS_INTERNAL_ERROR, NULL);
    ecs_assert(buffer == ecs_map_get_ptr(&buffers->index, 
        sokol_geometry_buffer_t*, buffer->id), 
            ECS_INTERNAL_ERROR, NULL);
    ecs_assert(buffer->groups == NULL, ECS_INTERNAL_ERROR, NULL);

    sokol_geometry_buffer_t *prev = buffer->prev, *next = buffer->next;
    if (prev) {
        prev->next = next;
    }
    if (next) {
        next->prev = prev;
    }

    if (buffers->first == buffer) {
        buffers->first = next;
    }

    sg_destroy_buffer(buffer->colors);
    sg_destroy_buffer(buffer->transforms);
    sg_destroy_buffer(buffer->materials);

    ecs_map_remove(&buffers->index, buffer->id);
    ecs_os_free(buffer);
}

ECS_CTOR(SokolGeometry, ptr, {
    *ptr = (SokolGeometry) {0};
    ptr->solid = sokol_create_buffers();
    ptr->emissive = sokol_create_buffers();
})

ECS_DTOR(SokolGeometry, ptr, {
    ecs_os_free(ptr->solid);
    ecs_os_free(ptr->emissive);
})

static
int32_t next_pow_of_2(
    int32_t n)
{
    n --;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    n ++;

    return n;
}

static
void sokol_populate_rectangle(
    mat4 *transforms, 
    EcsRectangle *data, 
    int32_t count,
    bool self) 
{
    int i;
    if (self) {
        for (i = 0; i < count; i ++) {
            vec3 scale = {data[i].width, data[i].height, 1.0};
            glm_scale(transforms[i], scale);
        }
    } else {
        vec3 scale = {data->width, data->height, 1.0};
        for (i = 0; i < count; i ++) {
            glm_scale(transforms[i], scale);
        }
    }
}

static
void sokol_populate_box(
    mat4 *transforms, 
    EcsBox *data, 
    int32_t count,
    bool self)
     
{    
    int i;
    if (self) {
        for (i = 0; i < count; i ++) {
            vec3 scale = {data[i].width, data[i].height, data[i].depth};
            glm_scale(transforms[i], scale);
        }
    } else {
        vec3 scale = {data->width, data->height, data->depth};
        for (i = 0; i < count; i ++) {
            glm_scale(transforms[i], scale);
        }
    }
}

static
void sokol_init_rectangle(
    ecs_world_t *world,
    sokol_resources_t *resources) 
{
    SokolGeometry *g = ecs_get_mut(
        world, ecs_id(SokolRectangleGeometry), SokolGeometry);
    ecs_assert(g != NULL, ECS_INTERNAL_ERROR, NULL);

    g->vertices = resources->rect;
    g->normals = resources->rect_normals;
    g->indices = resources->rect_indices;
    g->index_count = sokol_rectangle_index_count();
    g->populate = (sokol_geometry_action_t)sokol_populate_rectangle;
}

static
void sokol_init_box(
    ecs_world_t *world,
    sokol_resources_t *resources) 
{
    if (SokolBoxGeometry) {
        SokolGeometry *g = ecs_get_mut(
            world, ecs_id(SokolBoxGeometry), SokolGeometry);
        ecs_assert(g != NULL, ECS_INTERNAL_ERROR, NULL);

        g->vertices = resources->box;
        g->normals = resources->box_normals;
        g->indices = resources->box_indices;
        g->index_count = sokol_box_index_count();
        g->populate = (sokol_geometry_action_t)sokol_populate_box;
    }
}

void sokol_init_geometry(
    ecs_world_t *world,
    sokol_resources_t *resources) 
{
    sokol_init_rectangle(world, resources);
    sokol_init_box(world, resources);
}

// ecs_vec_t functions are not public
static
void sokol_groups_clear(
    ecs_vec_t *vec)
{
    vec->count = 0;
}

static
uint64_t* sokol_groups_add(
    ecs_vec_t *vec)
{
    if (vec->count == vec->size) {
        vec->size = ECS_MAX(vec->size * 2, 8);
        vec->array = ecs_os_realloc_n(vec->array, uint64_t, vec->size);
    }

    uint64_t *result = &((uint64_t*)vec->array)[vec->count];
    vec->count ++;
    return result;
}

static
sokol_geometry_page_t* sokol_group_add_page(
    sokol_geometry_group_t *group)
{
    sokol_geometry_page_t *page = ecs_os_calloc_t(sokol_geometry_page_t);
    page->colors = ecs_os_calloc_n(ecs_rgb_t, SOKOL_GEOMETRY_PAGE_SIZE);
    page->transforms = ecs_os_calloc_n(mat4, SOKOL_GEOMETRY_PAGE_SIZE);
    page->materials = ecs_os_calloc_n(SokolMaterial, SOKOL_GEOMETRY_PAGE_SIZE);

    if (!group->first_page) {
        ecs_assert(!group->last_page, ECS_INTERNAL_ERROR, NULL);
        group->first_page = page;
        group->last_page = page;
    } else {
        group->last_page->next = page;
        group->last_page = page;
    }

    return page;
}

int32_t sokol_page_space_left(
    sokol_geometry_page_t *page)
{
    return SOKOL_GEOMETRY_PAGE_SIZE - page->count;
}

static
int64_t sokol_sqr(int64_t v) {
    return v * v;
}

static
int8_t sokol_cond_sign(int8_t cond) {
    return 1 - 2 * cond;
}

static
int64_t sokol_abs(int64_t v) {
    static const int SOKOL_CHAR_BIT = 8;
    int64_t mask = v >> (sizeof(int64_t) * SOKOL_CHAR_BIT - 1);
    return (v + mask) ^ mask;
}

static
void sokol_group_add_changed(
    const ecs_world_t *world,
    ecs_query_t *query,
    ecs_vec_t *group_ids,
    uint64_t group_id)
{
    ecs_iter_t qit = ecs_query_iter(world, query);
    ecs_query_set_group(&qit, group_id);

    while (ecs_query_next_table(&qit)) {
        if (ecs_query_changed(NULL, &qit)) {
            // At least one table has changed in the group, add it
            sokol_groups_add(group_ids)[0] = group_id;
            ecs_iter_fini(&qit);
            return;
        }
    }
}

static
void sokol_groups_add_changed(
    const ecs_world_t *world,
    ecs_query_t *query,
    ecs_vec_t *group_ids,
    sokol_geometry_buffer_t *buffer)
{
    // Check all groups for geometry for changes
    sokol_geometry_group_t *group = buffer->groups;
    while (group) {
        sokol_group_add_changed(world, query, group_ids, group->id);
        group = group->next;
    }
}

static
void sokol_update_visibility(
    const ecs_world_t *world,
    sokol_geometry_buffer_t *buffer,
    const EcsPosition3 *view_pos,
    ecs_query_t *query,
    ecs_vec_t *group_ids)
{
    const EcsWorldCellCoord *cell = NULL;
    if (buffer->cell_coord.entity) {
        cell = ecs_ref_get(world, &buffer->cell_coord, EcsWorldCellCoord);
    }
    if (!cell) {
        // Entities without a world cell are always visible, so check if group
        // data changed.
        sokol_groups_add_changed(world, query, group_ids, buffer);
        return;
    }

    int64_t view_x = view_pos->x;
    int64_t view_z = view_pos->z;
    int64_t cell_x = cell->x;
    int64_t cell_y = cell->y;
    int64_t cell_size = cell->size / 2;

    int64_t dx = sokol_abs(view_x - cell_x);
    dx -= cell_size;
    dx *= (dx > 0);
    int64_t dz = sokol_abs(view_z - cell_y);
    dz -= cell_size;
    dz *= (dz > 0);
    int64_t dy = sokol_abs(view_pos->y);
    dy -= cell_size;
    dy *= (dy > 0);
    int64_t d_sqr = sokol_sqr(dx) + sokol_sqr(dy) + sokol_sqr(dz);

    sokol_geometry_group_t *group = buffer->groups;
    while (group) {
        if (!group->draw_distance.id) {
            // Group has no view distance / world cell
            ecs_assert(group->visible == true, ECS_INTERNAL_ERROR, NULL);
            sokol_group_add_changed(world, query, group_ids, group->id);
            group = group->next;
            continue;
        }

        const EcsDrawDistance *vd = ecs_ref_get(world, &group->draw_distance, 
            EcsDrawDistance);
        if (!vd) {
            // Shouldn't happen, but don't crash
            ecs_assert(group->visible == true, ECS_INTERNAL_ERROR, NULL);
            sokol_group_add_changed(world, query, group_ids, group->id);
            group = group->next;
            continue;
        }

        // Test if visibility of group has changed
        float draw_distance_sqr = sokol_sqr(vd->far_);
        bool visible = d_sqr < draw_distance_sqr;
        bool changed = visible != group->visible;
        int32_t count = group->count;
        buffer->changed |= changed;
        buffer->count += changed * (visible * count - !visible * count);
        group->visible = visible;
        if (visible) {
            sokol_group_add_changed(world, query, group_ids, group->id);
        }

        group = group->next;
    }
}

static
void sokol_update_group(
    const ecs_world_t *world,
    const  SokolMaterial *material_data,
    SokolGeometry *geometry,
    ecs_query_t *query,
    uint64_t group_id)
{
    sokol_geometry_group_t *group = ecs_query_get_group_ctx(query, group_id);
    if (!group->visible) {
        return;
    }

    sokol_geometry_buffer_t *buffer = group->buffer;
    ecs_assert(buffer != NULL, ECS_INTERNAL_ERROR, NULL);
    sokol_geometry_page_t *page = group->first_page;
    if (!page) {
        page = sokol_group_add_page(group);
    } else {
        page->count = 0;
    }

    // Subtract old group count from buffer (update later with new count)
    buffer->count -= group->count;
    group->count = 0;

    // Copy component data to group cache
    ecs_iter_t qit = ecs_query_iter(world, query);
    ecs_query_set_group(&qit, group_id);
    while (ecs_query_next(&qit)) {
        EcsTransform3 *transforms = ecs_field(&qit, EcsTransform3, 1);
        EcsRgb *colors = ecs_field(&qit, EcsRgb, 2);
        SokolMaterialId *materials = ecs_field(&qit, SokolMaterialId, 3);
        void *geometry_data = ecs_field_w_size(&qit, 0, 4);
        bool geometry_self = ecs_field_is_self(&qit, 4);
        ecs_size_t geometry_size = ecs_field_size(&qit, 4);

        int32_t remaining = qit.count;
        group->count += qit.count;

        do {
            int32_t space_left = sokol_page_space_left(page);
            if (!space_left) {
                page = page->next;
                if (!page) {
                    page = sokol_group_add_page(group);
                } else {
                    page->count = 0;
                }
                space_left = SOKOL_GEOMETRY_PAGE_SIZE;
            }

            int32_t to_copy = ECS_MIN(space_left, remaining);
            int32_t i, pstart = page->count;

            // Copy transform data
            ecs_os_memcpy_n(&page->transforms[pstart], transforms, mat4, to_copy);
            transforms += to_copy;

            // Copy color data
            if (ecs_field_is_self(&qit, 2)) {
                ecs_os_memcpy_n(&page->colors[pstart], colors, ecs_rgb_t, to_copy);
                colors += to_copy;
            } else {
                for (i = 0; i < to_copy; i ++) {
                    page->colors[pstart + i] = *colors;
                }
            }

            // Uncomment to use group id as vertex color
            // for (i = 0; i < to_copy; i ++) {
            //     page->colors[pstart + i].r = ((float)(group_id % 7)) / 7.0;
            //     page->colors[pstart + i].g = ((float)(group_id % 3)) / 3.0;
            //     page->colors[pstart + i].b = ((float)(group_id % 11)) / 11.0;
            // }

            // Copy material data
            if (materials) {
                uint64_t material_id = materials->value;
                for (i = 0; i < to_copy; i ++) {
                    page->materials[pstart + i] = material_data[material_id];
                }
            } else {
                ecs_os_memset_n(&page->materials[pstart], 0, SokolMaterial, to_copy);
            }

            // Apply scaling to transform matrix
            geometry->populate(&page->transforms[pstart], geometry_data, 
                to_copy, geometry_self);
            geometry_data = ECS_OFFSET(geometry_data, geometry_size * to_copy);

            remaining -= to_copy;
            page->count += to_copy;
        } while(remaining);
    }

    // TODO: instanced draw calls can't work with single instances
    if (group->count == 1) { }

    // Store first page that doesn't contain data for current frame
    group->first_no_data = page->next;

    // Add group count to buffer
    buffer->count += group->count;

    // Flag buffer as changed
    buffer->changed = true;
}

static
void sokol_update_buffer(
    const ecs_world_t *world,
    sokol_geometry_buffer_t *buffer)
{
    if (!buffer->changed) {
        return;
    }

    buffer->changed = false;

    // Make sure buffers with GPU data are large enough
    ecs_size_t size = next_pow_of_2(buffer->count);
    if (size != buffer->size) {
        if (buffer->size) {
            sg_destroy_buffer(buffer->colors);
            sg_destroy_buffer(buffer->transforms);
            sg_destroy_buffer(buffer->materials);
        }

        buffer->colors = sg_make_buffer(&(sg_buffer_desc){
            .size = size * sizeof(ecs_rgb_t), .usage = SG_USAGE_STREAM });
        buffer->transforms = sg_make_buffer(&(sg_buffer_desc){
            .size = size * sizeof(EcsTransform3), .usage = SG_USAGE_STREAM });
        buffer->materials = sg_make_buffer(&(sg_buffer_desc){
            .size = size * sizeof(SokolMaterial), .usage = SG_USAGE_STREAM });
        buffer->size = size;
    }

    // Copy data from groups to GPU buffers
    sokol_geometry_group_t *group = buffer->groups;
    int32_t buffer_count = 0;
    while (group) {
        if (!group->visible) {
            // Skip groups that aren't visible
            group = group->next;
            continue;
        }

        sokol_geometry_page_t *end = group->first_no_data;
        sokol_geometry_page_t *page = group->first_page;
        int32_t group_count = 0;

        while (page != end) {
            int32_t count = page->count;

            sg_append_buffer(buffer->colors, &(sg_range) {
                page->colors, count * sizeof(ecs_rgb_t) });
            sg_append_buffer(buffer->transforms, &(sg_range) {
                page->transforms, count * sizeof(EcsTransform3) });
            sg_append_buffer(buffer->materials, &(sg_range) {
                page->materials, count * sizeof(SokolMaterial) });

            group_count += count;
            page = page->next;
        }

        if (group_count != group->count) {
            char *str = ecs_id_str(world, group->id);
            ecs_err("group %s reports incorrect number of instances (%d vs %d)",
                str, group_count, group->count);
            ecs_os_free(str);
        }

        buffer_count += group_count;
        group = group->next;
    }

    if (buffer_count != buffer->count) {
        char *str = ecs_get_fullpath(world, buffer->id);
        ecs_err("buffer %s reports incorrect number of instances (%d vs %d)", 
            str, buffer_count, buffer->count);
        ecs_os_free(str);
    }
}

static
void sokol_populate_buffers(
    SokolGeometry *geometry,
    sokol_geometry_buffers_t *buffers,
    ecs_query_t *query,
    const EcsPosition3 *view_pos)
{
    const ecs_world_t *world = ecs_get_world(query);

    // Vector to keep track of changed group ids
    ecs_vec_t *group_ids = &geometry->group_ids;
    sokol_groups_clear(group_ids);

    // Compute group visibility for all buffers
    {
        sokol_geometry_buffer_t *buffer = buffers->first;
        while (buffer) {
            sokol_update_visibility(world, buffer, view_pos, query, group_ids);
            buffer = buffer->next;
        }
    }

    // Update data for groups that changed and are visible
    int32_t i, count = group_ids->count;
    if (count) {
        const SokolMaterials *materials = ecs_get(
            world, SokolRendererInst, SokolMaterials);
        const SokolMaterial *material_data = materials->array;
        uint64_t *array = group_ids->array;
        for (i = 0; i < count; i ++) {
            sokol_update_group(world, material_data, geometry, query, array[i]);
        }
    }

    // Update GPU buffers with changed group data
    {
        sokol_geometry_buffer_t *buffer = buffers->first;
        while (buffer) {
            sokol_update_buffer(world, buffer);
            buffer = buffer->next;
        }
    }
}

static
void SokolPopulateGeometry(
    ecs_iter_t *it) 
{
    SokolGeometry *g = ecs_field(it, SokolGeometry, 1);
    SokolGeometryQuery *q = ecs_field(it, SokolGeometryQuery, 2);

    // Get current camera position so we can calculate view distance
    const EcsPosition3 *view_pos = NULL;
    ecs_world_t *world = it->real_world;
    const SokolRenderer *r = ecs_get(world, SokolRendererInst, SokolRenderer);
    ecs_entity_t camera = r->camera;
    if (camera) {
        view_pos = ecs_get(world, camera, EcsPosition3);
    } else {
        // Wait until renderer has populated camera field
        return;
    }

    int i;
    for (i = 0; i < it->count; i ++) {
        sokol_populate_buffers(&g[i], g[i].solid, q[i].solid, view_pos);
        sokol_populate_buffers(&g[i], g[i].emissive, q[i].emissive, view_pos);
    }
}

static
void* sokol_on_group_create(
    ecs_world_t *world,
    uint64_t group_id,
    void *group_by_ctx)
{
    sokol_geometry_buffers_t *buffers = group_by_ctx;
    ecs_entity_t buffer_id = ECS_PAIR_FIRST(group_id); // world cell
    ecs_entity_t prefab = ECS_PAIR_SECOND(group_id);
    sokol_geometry_buffer_t *buffer = sokol_create_buffer(
        world, buffers, buffer_id);

    sokol_geometry_group_t *result = ecs_os_calloc_t(sokol_geometry_group_t);
    result->buffer = buffer;
    result->id = group_id;

    sokol_geometry_group_t *next = buffer->groups;
    buffer->groups = result;
    result->next = next;
    if (next) {
        next->prev = result;
    }

    if (prefab) {
        prefab = ecs_get_alive(world, prefab);
        result->draw_distance = ecs_ref_init(world, prefab, EcsDrawDistance);
    }

    result->visible = true;

    return result;
}

static
void sokol_on_group_delete(
    ecs_world_t *world,
    uint64_t group_id,
    void *group_ctx,
    void *group_by_ctx)
{
    sokol_geometry_group_t *group = group_ctx;
    sokol_geometry_buffers_t *buffers = group_by_ctx;
    sokol_geometry_buffer_t *buffer = group->buffer;
    
    sokol_geometry_group_t *prev = group->prev, *next = group->next;
    if (prev) {
        prev->next = next;
    }
    if (next) {
        next->prev = prev;
    }

    buffer->count -= group->count;

    if (buffer->groups == group) {
        buffer->groups = next;
        if (!next) {
            // Last group for buffer was deleted, delete buffer
            sokol_delete_buffer(buffers, buffer);
        }
    }

    sokol_geometry_page_t *next_page, *page = group->first_page;
    while (page != NULL) {
        next_page = page->next;
        ecs_os_free(page->colors);
        ecs_os_free(page->transforms);
        ecs_os_free(page->materials);
        ecs_os_free(page);
        page = next_page;
    }

    ecs_os_free(group);
}

static
uint64_t sokol_group_by(
    ecs_world_t *world, 
    ecs_table_t *table, 
    ecs_id_t id, 
    void *ctx)
{
    ecs_entity_t src = 0, prefab = 0, cell = 0;
    ecs_id_t match;

    /* Group by prefab & world cell. Prefer prefabs with DrawDistance, in case
     * an entity inherits from multiple prefabs. */

    /* First, try to find a parent that has (or inherits) DrawDistance. */
    if (ecs_search_relation(world, table, 0, ecs_id(EcsDrawDistance), EcsChildOf,
        EcsUp, &src, 0, 0) != -1)
    {
        prefab = src;
    } else {
        /* If no parent with DrawDistance is found, check if table inherits
         * DrawDistance directly */
        if (ecs_search_relation(world, table, 0, ecs_id(EcsDrawDistance), EcsIsA,
            EcsUp, &src, 0, 0) != -1)
        {
            prefab = src;
        } else {
            /* If no prefab with DrawDistance was found, select any prefab. Use
             * ChildOf to look for prefab relationships, as instance children
             * are not created with an IsA relationship. */
            if (ecs_search_relation(world, table, 0, ecs_pair(id, EcsWildcard), 
                EcsChildOf, EcsSelf|EcsUp, 0, &match, 0) != -1)
            {
                prefab = ECS_PAIR_SECOND(match);
            }
        }
    }

    /* Find world cell. World cells are only added to top-level entities that
     * have a Position component, so for instance children we'll have to find 
     * the world cell by traversing the ChildOf relationship. */
    if (ecs_search_relation(world, table, 0, 
        ecs_pair(EcsWorldCell, EcsWildcard), EcsChildOf,
        EcsSelf | EcsUp, 0, &match, 0) != -1)
    {
        cell = ECS_PAIR_SECOND(match);
    }

    return ecs_pair(cell, prefab);
}

static
void CreateGeometryQueries(ecs_iter_t *it) {
    ecs_world_t *world = it->world;
    SokolGeometry *g = ecs_field(it, SokolGeometry, 1);
    SokolGeometryQuery *gq = ecs_field(it, SokolGeometryQuery, 2);

    int i;
    for (i = 0; i < it->count; i ++) {
        ecs_query_desc_t desc = {
            .filter = {
                .terms = {{
                    .id        = ecs_id(EcsTransform3), 
                    .inout     = EcsIn,
                    .src.flags = EcsSelf
                }, {
                    .id        = ecs_id(EcsRgb),
                    .inout     = EcsIn
                }, {
                    .id        = ecs_id(SokolMaterialId), 
                    .oper      = EcsOptional,
                    .inout     = EcsIn,
                    .src.flags = EcsUp
                }, {
                    .id        = gq[i].component, 
                    .inout     = EcsIn 
                }, {
                    .id        = ecs_id(EcsEmissive),
                    .inout     = EcsInOutNone,
                    .oper      = EcsNot
                }},
                .instanced = true
            },
            .group_by_id = EcsIsA,
            .group_by = sokol_group_by,
            .on_group_create = sokol_on_group_create,
            .on_group_delete = sokol_on_group_delete
        };

        /* Query for solid, non-emissive objects */
        desc.group_by_ctx = g[i].solid;
        gq[i].solid = ecs_query_init(world, &desc);

        /* Query for solid, emissive objects */
        desc.group_by_ctx = g[i].emissive;
        desc.filter.terms[4].oper = 0; /* Remove Not operator */
        gq[i].emissive = ecs_query_init(world, &desc);
    }
}

void FlecsSystemsSokolGeometryImport(
    ecs_world_t *world)
{
    ECS_MODULE(world, FlecsSystemsSokolGeometry);
    ECS_IMPORT(world, FlecsComponentsTransform);
    ECS_IMPORT(world, FlecsComponentsGeometry);
    ECS_IMPORT(world, FlecsSystemsTransform);

    /* Store components in parent sokol scope */
    ecs_entity_t parent = ecs_lookup_fullpath(world, "flecs.systems.sokol");
    ecs_entity_t module = ecs_set_scope(world, parent);
    ecs_set_name_prefix(world, "Sokol");

    ECS_COMPONENT_DEFINE(world, SokolGeometry);
    ECS_COMPONENT_DEFINE(world, SokolGeometryQuery);

    ecs_set_hooks(world, SokolGeometry, {
        .ctor = ecs_ctor(SokolGeometry),
        .dtor = ecs_dtor(SokolGeometry)
    });

    ecs_set_scope(world, module);

    /* Create queries for solid and emissive */
    ECS_OBSERVER(world, CreateGeometryQueries, EcsOnSet, 
        Geometry, GeometryQuery);

    /* Support for rectangle primitive */
    ECS_ENTITY_DEFINE(world, SokolRectangleGeometry, Geometry);
        ecs_set(world, SokolRectangleGeometry, SokolGeometryQuery, {
            .component = ecs_id(EcsRectangle)
        });

    /* Support for box primitive */
    ECS_ENTITY_DEFINE(world, SokolBoxGeometry, Geometry);
        ecs_set(world, SokolBoxGeometry, SokolGeometryQuery, {
            .component = ecs_id(EcsBox)
        });

    /* Create system that manages buffers */
    ECS_SYSTEM(world, SokolPopulateGeometry, EcsPreStore, 
        Geometry, [in] GeometryQuery);  
}
