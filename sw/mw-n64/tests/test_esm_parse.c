/*
 * Host-compiled test for Morrowind ESM parser.
 *
 * Build: gcc -o test_esm_parse test_esm_parse.c ../src/esm/mw_esm.c
 *        -I../src -Wall
 * Run:   python3 gen_test_esm.py . && ./test_esm_parse Test.esm
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* Stub n64_mem_alloc for host */
static uint8_t heap[8 * 1024 * 1024];
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

#include "../src/esm/mw_esm.h"
#include "../src/world/mw_cell_manager.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define CHECK(cond, msg) do { \
    if (cond) { tests_passed++; } \
    else { tests_failed++; printf("  FAIL: %s\n", msg); } \
} while(0)

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: %s <test.esm>\n", argv[0]);
        return 1;
    }

    FILE *f = fopen(argv[1], "rb");
    if (!f) { printf("Cannot open %s\n", argv[1]); return 1; }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *data = malloc(size);
    fread(data, 1, size, f);
    fclose(f);
    printf("Loaded %s (%ld bytes)\n\n", argv[1], size);

    printf("=== Morrowind ESM Parser Tests ===\n\n");

    /* Build index */
    esm_reader_t reader;
    esm_index_t index;
    memset(&index, 0, sizeof(index));

    esm_reader_init(&reader, data, (uint32_t)size);
    esm_build_index(&reader, &index);

    /* Check header */
    printf("  Header: v%.1f, type=%d, records=%d\n",
           index.header.version, index.header.type, index.header.record_count);
    CHECK(index.header.version > 1.0f && index.header.version < 2.0f,
          "Version is ~1.3");
    CHECK(index.header.type == 1, "Type is ESM (1)");

    /* Check statics */
    printf("  Statics: %d\n", index.static_count);
    CHECK(index.static_count == 2, "2 static objects");
    if (index.static_count >= 1) {
        CHECK(strcmp(index.statics[0].name, "test_rock") == 0,
              "Static 0 is test_rock");
        CHECK(strstr(index.statics[0].model, "test_rock.nif") != NULL,
              "Static 0 model is test_rock.nif");
        printf("  Static 0: '%s' -> '%s'\n",
               index.statics[0].name, index.statics[0].model);
    }
    if (index.static_count >= 2) {
        CHECK(strcmp(index.statics[1].name, "test_tree") == 0,
              "Static 1 is test_tree");
        printf("  Static 1: '%s' -> '%s'\n",
               index.statics[1].name, index.statics[1].model);
    }

    /* Check NPCs */
    printf("  NPCs: %d\n", index.npc_count);
    CHECK(index.npc_count == 1, "1 NPC");
    if (index.npc_count >= 1) {
        CHECK(strcmp(index.npcs[0].name, "test_npc") == 0, "NPC is test_npc");
        CHECK(strcmp(index.npcs[0].display_name, "Test NPC") == 0,
              "NPC display name is 'Test NPC'");
        CHECK(index.npcs[0].level == 5, "NPC level is 5");
        CHECK(index.npcs[0].health == 100, "NPC health is 100");
        printf("  NPC: '%s' (%s), Lv%d, HP=%d, Gold=%d\n",
               index.npcs[0].display_name, index.npcs[0].race,
               index.npcs[0].level, index.npcs[0].health, index.npcs[0].gold);
    }

    /* Check cells */
    printf("  Cells: %d\n", index.cell_count);
    CHECK(index.cell_count == 2, "2 cells");

    /* Find interior cell */
    int interior_idx = -1, exterior_idx = -1;
    for (int i = 0; i < index.cell_count; i++) {
        if (index.cells[i].is_interior) interior_idx = i;
        else exterior_idx = i;
    }
    CHECK(interior_idx >= 0, "Found interior cell");
    CHECK(exterior_idx >= 0, "Found exterior cell");

    if (interior_idx >= 0) {
        printf("  Interior: cell index %d\n", interior_idx);
    }
    if (exterior_idx >= 0) {
        CHECK(index.cells[exterior_idx].grid_x == 0, "Exterior at grid (0,0) x");
        CHECK(index.cells[exterior_idx].grid_y == 0, "Exterior at grid (0,0) y");
        printf("  Exterior: grid (%d, %d)\n",
               index.cells[exterior_idx].grid_x,
               index.cells[exterior_idx].grid_y);
    }

    /* Check land */
    printf("  Land: %d\n", index.land_count);
    CHECK(index.land_count == 1, "1 LAND record");
    if (index.land_count >= 1) {
        CHECK(index.lands[0].grid_x == 0, "LAND at grid (0,0) x");
        CHECK(index.lands[0].grid_y == 0, "LAND at grid (0,0) y");
    }

    /* Load the exterior cell and check refs */
    if (exterior_idx >= 0) {
        esm_cell_t cell;
        esm_cell_ref_t refs[64];
        int ref_count = 0;

        bool ok = esm_load_cell(&reader, &index, 0, 0, &cell, refs, &ref_count, 64);
        CHECK(ok, "Exterior cell loads OK");
        printf("  Exterior cell refs: %d\n", ref_count);
        CHECK(ref_count >= 1, "At least 1 ref in exterior");
        if (ref_count >= 1) {
            CHECK(strcmp(refs[0].object_id, "test_tree") == 0,
                  "First ref is test_tree");
            printf("  Ref 0: '%s' at (%.0f, %.0f, %.0f)\n",
                   refs[0].object_id, refs[0].pos.x, refs[0].pos.y, refs[0].pos.z);
        }
    }

    /* Load LAND and check heightmap */
    if (index.land_count >= 1) {
        esm_land_t land;
        bool ok = esm_load_land(&reader, &index, 0, 0, &land);
        CHECK(ok, "LAND loads OK");
        printf("  LAND grid: (%d, %d), flags=0x%x\n",
               land.grid_x, land.grid_y, land.flags);
        /* Check that heights were reconstructed */
        CHECK(land.heights[0] != 0 || land.flags & 0x01, "Heights present");
        printf("  Height[0,0]=%d, Height[32,32]=%d\n",
               land.heights[0], land.heights[32*65+32]);
    }

    /* Memory usage */
    printf("\n  Heap used: %u KB / %lu KB\n",
           (unsigned)(heap_pos / 1024), sizeof(heap) / 1024);

    printf("\n=== Results: %d passed, %d failed ===\n",
           tests_passed, tests_failed);

    free(data);
    return tests_failed > 0 ? 1 : 0;
}
