#include <string.h>
#include "io.h"
#include "joybus.h"


#define PIF_ESCAPE_SKIP_CHANNEL     (0x00)
#define PIF_ESCAPE_END_OF_FRAME     (0xFE)

#define PIF_COMMAND_JOYBUS_START    (1 << 0)

#define JOYBUS_CMD_INFO             (0x00)
#define JOYBUS_CMD_STATE            (0x01)
#define JOYBUS_CMD_RESET            (0xFF)


typedef struct __attribute__((packed)) {
    struct __attribute__((packed)) {
        uint8_t skip : 1;
        uint8_t reset : 1;
        uint8_t length : 6;
    } tx;
    struct __attribute__((packed)) {
        uint8_t no_device : 1;
        uint8_t timeout : 1;
        uint8_t length : 6;
    } rx;
    uint8_t cmd;
} joybus_trx_info_t;

typedef struct __attribute__((packed)) {
    joybus_trx_info_t info;
    struct __attribute__((packed)) {
        uint16_t id;
        uint8_t flags;
    } rx;
} joybus_cmd_info_t;

typedef struct __attribute__((packed)) {
    joybus_trx_info_t info;
    struct __attribute__((packed)) {
        uint8_t a : 1;
        uint8_t b : 1;
        uint8_t z : 1;
        uint8_t start : 1;
        uint8_t up : 1;
        uint8_t down : 1;
        uint8_t left : 1;
        uint8_t right : 1;
        uint8_t reset : 1;
        uint8_t __unused : 1;
        uint8_t l : 1;
        uint8_t r : 1;
        uint8_t c_up : 1;
        uint8_t c_down : 1;
        uint8_t c_left : 1;
        uint8_t c_right : 1;
        int8_t x;
        int8_t y;
    } rx;
} joybus_cmd_state_t;


static void joybus_clear_buffer (uint8_t *buffer) {
    for (int i = 0; i < PIF_RAM_LENGTH; i++) {
        buffer[i] = 0;
    }
}

static void joybus_set_starting_channel (uint8_t **ptr, int channel) {
    for (int i = 0; i < channel; i++) {
        **ptr = PIF_ESCAPE_SKIP_CHANNEL;
        *ptr += 1;
    }
}

static uint8_t *joybus_append_command (uint8_t **ptr, void *cmd, size_t length) {
    ((joybus_trx_info_t *) (cmd))->tx.length += 1;
    uint8_t *cmd_ptr = *ptr;
    memcpy(cmd_ptr, cmd, length);
    *ptr += length;
    return cmd_ptr;
}

static void joybus_load_result (uint8_t *cmd_ptr, void *cmd, size_t length) {
    memcpy(cmd, cmd_ptr, length);
}

static void joybus_finish_frame (uint8_t **ptr) {
    **ptr = PIF_ESCAPE_END_OF_FRAME;
    *ptr += 1;
}

static void joybus_perform_transaction (uint8_t *buffer) {
    buffer[PIF_RAM_LENGTH - 1] = PIF_COMMAND_JOYBUS_START;

    si_dma_write(buffer);
    si_dma_read(buffer);
}

static bool joybus_device_present (joybus_trx_info_t *trx_info) {
    return !(trx_info->rx.no_device || trx_info->rx.timeout);
}

static bool joybus_execute_command (int port, void *cmd, size_t length) {
    uint8_t buffer[PIF_RAM_LENGTH] __attribute__((aligned(8)));
    uint8_t *ptr = buffer;

    joybus_clear_buffer(buffer);
    joybus_set_starting_channel(&ptr, port);
    uint8_t *cmd_ptr = joybus_append_command(&ptr, cmd, length);
    joybus_finish_frame(&ptr);
    joybus_perform_transaction(buffer);
    joybus_load_result(cmd_ptr, cmd, length);
    return joybus_device_present((joybus_trx_info_t *) (cmd_ptr));
}

static void joybus_copy_controller_info (joybus_cmd_info_t *cmd, joybus_controller_info_t *info) {
    info->id = cmd->rx.id;
    info->flags = cmd->rx.flags;
}

static void joybus_copy_controller_state (joybus_cmd_state_t *cmd, joybus_controller_state_t *state) {
    state->a = cmd->rx.a;
    state->b = cmd->rx.b;
    state->z = cmd->rx.z;
    state->start = cmd->rx.start;
    state->up = cmd->rx.up;
    state->down = cmd->rx.down;
    state->left = cmd->rx.left;
    state->right = cmd->rx.right;
    state->reset = cmd->rx.reset;
    state->l = cmd->rx.l;
    state->r = cmd->rx.r;
    state->c_up = cmd->rx.c_up;
    state->c_down = cmd->rx.c_down;
    state->c_left = cmd->rx.c_left;
    state->c_right = cmd->rx.c_right;
    state->x = cmd->rx.x;
    state->y = cmd->rx.y;
}


bool joybus_get_controller_info (int port, joybus_controller_info_t *info, bool reset) {
    joybus_cmd_info_t cmd = { .info = {
        .cmd = reset ? JOYBUS_CMD_RESET : JOYBUS_CMD_INFO,
        .rx = { .length = sizeof(cmd.rx) }
    } };

    if (joybus_execute_command(port, &cmd, sizeof(cmd))) {
        joybus_copy_controller_info(&cmd, info);
        return true;
    }

    return false;
}

bool joybus_get_controller_state (int port, joybus_controller_state_t *state) {
    joybus_cmd_state_t cmd = { .info = {
        .cmd = JOYBUS_CMD_STATE,
        .rx = { .length = sizeof(cmd.rx) }
    } };

    if (joybus_execute_command(port, &cmd, sizeof(cmd))) {
        joybus_copy_controller_state(&cmd, state);
        return true;
    }

    return false;
}
