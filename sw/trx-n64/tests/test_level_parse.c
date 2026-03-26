/*
 * Host-compiled test for TR1 .PHD level parser.
 * Runs on x86 — no N64 hardware needed.
 *
 * Build: gcc -o test_level_parse test_level_parse.c ../src/level/tr_level_parse.c
 *        -I../src -DASSET_HOST_BUILD
 * Run:   python3 gen_test_level.py . && ./test_level_parse TEST.PHD
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* Stub out n64_mem_alloc for host builds */
static uint8_t heap[4 * 1024 * 1024];
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

static int tests_passed = 0;
static int tests_failed = 0;

#define CHECK(cond, msg) do { \
    if (cond) { tests_passed++; } \
    else { tests_failed++; printf("  FAIL: %s\n", msg); } \
} while(0)

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: %s <test.phd>\n", argv[0]);
        printf("Generate test data: python3 gen_test_level.py .\n");
        return 1;
    }

    /* Load file */
    FILE *f = fopen(argv[1], "rb");
    if (!f) { printf("Cannot open %s\n", argv[1]); return 1; }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *data = malloc(size);
    fread(data, 1, size, f);
    fclose(f);
    printf("Loaded %s (%ld bytes)\n\n", argv[1], size);

    /* Parse */
    n64_mem_init();
    tr_level_t level;
    tr_parse_result_t res = tr_level_parse(data, (uint32_t)size, &level);

    printf("=== TR1 Level Parser Tests ===\n\n");

    CHECK(res == TR_PARSE_OK, "Parse result OK");
    printf("  Parse result: %s\n", tr_parse_error_string(res));

    if (res != TR_PARSE_OK) {
        printf("\nParse failed, cannot continue.\n");
        free(data);
        return 1;
    }

    /* Check rooms */
    CHECK(level.room_count == 2, "Room count == 2");
    printf("  Rooms: %d\n", level.room_count);

    if (level.room_count >= 1) {
        tr_room_t *r = &level.rooms[0];
        CHECK(r->mesh.vertex_count == 8, "Room 0 has 8 vertices");
        CHECK(r->mesh.face_count == 6, "Room 0 has 6 faces");
        CHECK(r->portal_count == 1, "Room 0 has 1 portal");
        CHECK(r->sectors_x == 2, "Room 0 sectors_x == 2");
        CHECK(r->sectors_z == 2, "Room 0 sectors_z == 2");
        CHECK(r->light_count == 1, "Room 0 has 1 light");
        printf("  Room 0: %d verts, %d faces, %d portals, %dx%d sectors\n",
               r->mesh.vertex_count, r->mesh.face_count,
               r->portal_count, r->sectors_x, r->sectors_z);

        if (r->portal_count >= 1) {
            CHECK(r->portals[0].room == 1, "Portal 0 leads to room 1");
        }
    }

    if (level.room_count >= 2) {
        tr_room_t *r = &level.rooms[1];
        CHECK(r->mesh.vertex_count == 8, "Room 1 has 8 vertices");
        CHECK(r->portal_count == 1, "Room 1 has 1 portal");
        printf("  Room 1: %d verts, %d faces, %d portals\n",
               r->mesh.vertex_count, r->mesh.face_count, r->portal_count);
    }

    /* Check textures */
    CHECK(level.tex_page_count == 1, "1 texture page");
    CHECK(level.object_texture_count == 2, "2 object textures");
    printf("  Textures: %d pages, %d object textures\n",
           (int)level.tex_page_count, (int)level.object_texture_count);

    /* Check items */
    CHECK(level.item_count == 1, "1 item (Lara)");
    if (level.item_count >= 1) {
        CHECK(level.items[0].object_id == 0, "Item 0 is Lara (object_id 0)");
        CHECK(level.items[0].room == 0, "Lara in room 0");
        CHECK(level.items[0].pos.x == 1024, "Lara pos.x == 1024");
        printf("  Item 0: object_id=%d, room=%d, pos=(%d,%d,%d)\n",
               level.items[0].object_id, level.items[0].room,
               level.items[0].pos.x, level.items[0].pos.y, level.items[0].pos.z);
    }

    /* Check animations */
    CHECK(level.anim_count == 1, "1 animation");
    CHECK(level.object_count == 1, "1 object definition");
    printf("  Anims: %d, Objects: %d\n",
           (int)level.anim_count, (int)level.object_count);

    /* Check palette */
    CHECK(level.palette[0] == 0, "Palette[0].R == 0");
    CHECK(level.palette[3] == 4, "Palette[1].R == 4");
    printf("  Palette: first entry = (%d,%d,%d)\n",
           level.palette[0], level.palette[1], level.palette[2]);

    /* Memory usage */
    printf("\n  Heap used: %u KB / %lu KB\n",
           (unsigned)(heap_pos / 1024), sizeof(heap) / 1024);

    printf("\n=== Results: %d passed, %d failed ===\n",
           tests_passed, tests_failed);

    free(data);
    return tests_failed > 0 ? 1 : 0;
}
