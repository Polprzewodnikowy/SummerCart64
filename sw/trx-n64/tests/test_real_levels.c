/*
 * Test TR1 .PHD parser against real Morrowind game data.
 * Parses every .PHD file in a directory, reports stats and errors.
 *
 * Build: gcc -o test_real_levels test_real_levels.c ../src/level/tr_level_parse.c
 *        -I../src -DASSET_HOST_BUILD -Wall -lm
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <dirent.h>

static uint8_t heap[16 * 1024 * 1024];  /* 16 MB heap for large levels */
static uint32_t heap_pos = 0;

void n64_mem_init(void) { heap_pos = 0; }
void *n64_mem_alloc(uint32_t size) {
    size = (size + 15) & ~15;
    if (heap_pos + size > sizeof(heap)) return NULL;
    void *p = &heap[heap_pos];
    memset(p, 0, size);
    heap_pos += size;
    return p;
}
uint32_t n64_mem_available(void) { return sizeof(heap) - heap_pos; }

#include "../src/level/tr_level_parse.h"
#include "../src/level/tr_types.h"

static int test_one_level(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) { printf("  Cannot open %s\n", path); return 0; }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *data = malloc(size);
    if (!data) { printf("  malloc failed for %ld bytes\n", size); fclose(f); return 0; }
    fread(data, 1, size, f);
    fclose(f);

    n64_mem_init();
    tr_level_t level;
    tr_parse_result_t res = tr_level_parse(data, (uint32_t)size, &level);

    /* Extract filename */
    const char *fname = strrchr(path, '/');
    fname = fname ? fname + 1 : path;

    if (res != TR_PARSE_OK) {
        printf("  FAIL %-16s  %s\n", fname, tr_parse_error_string(res));
        free(data);
        return 0;
    }

    /* Find Lara */
    int lara_room = -1;
    for (int i = 0; i < level.item_count; i++) {
        if (level.items[i].object_id == 0) {
            lara_room = level.items[i].room;
            break;
        }
    }

    /* Count total faces across all rooms */
    int total_faces = 0;
    int total_verts = 0;
    for (int r = 0; r < level.room_count; r++) {
        total_faces += level.rooms[r].mesh.face_count;
        total_verts += level.rooms[r].mesh.vertex_count;
    }

    printf("  OK   %-16s  %3d rooms  %5d faces  %5d verts  %3d items  %3d tex  %2d pages  %4d anims  Lara@room %d  heap %uKB\n",
           fname,
           level.room_count,
           total_faces,
           total_verts,
           (int)level.item_count,
           (int)level.object_texture_count,
           (int)level.tex_page_count,
           (int)level.anim_count,
           lara_room,
           (unsigned)(heap_pos / 1024));

    free(data);
    return 1;
}

int main(int argc, char **argv) {
    const char *dir = argc > 1 ? argv[1] : "../data/levels";

    printf("=== TR1 Real Level Parser Test ===\n");
    printf("Directory: %s\n\n", dir);

    DIR *d = opendir(dir);
    if (!d) { printf("Cannot open directory %s\n", dir); return 1; }

    int passed = 0, failed = 0;
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        /* Only .PHD files */
        const char *ext = strrchr(ent->d_name, '.');
        if (!ext || strcasecmp(ext, ".PHD") != 0) continue;

        char path[512];
        snprintf(path, sizeof(path), "%s/%s", dir, ent->d_name);

        if (test_one_level(path))
            passed++;
        else
            failed++;
    }
    closedir(d);

    printf("\n=== Results: %d passed, %d failed (of %d total) ===\n",
           passed, failed, passed + failed);

    return failed > 0 ? 1 : 0;
}
