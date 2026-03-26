#include <trx/game/console/registry.h>

#include <trx/core/memory.h>
#include <trx/core/strings.h>

#include <stdio.h>
#include <string.h>

typedef struct M_NODE {
    CONSOLE_COMMAND cmd;
    struct M_NODE *next;
} M_NODE;

static M_NODE *m_List = nullptr;

__attribute__((destructor)) static void M_Shutdown(void)
{
    M_NODE *current = m_List;

    while (current != nullptr) {
        M_NODE *const next = current->next;
        Memory_Free(current);
        current = next;
    }

    m_List = nullptr;
}

const CONSOLE_COMMAND *Console_Registry_Get(const char *const cmdline)
{
    const M_NODE *current = m_List;
    while (current != nullptr) {
        const M_NODE *const next = current->next;
        char regex[strlen(current->cmd.prefix) + 13];
        sprintf(regex, "^(%s)(\\s+.*)?$", current->cmd.prefix);
        if (String_Match(cmdline, regex)) {
            return &current->cmd;
        }
        current = next;
    }
    return nullptr;
}

void Console_Registry_Add(CONSOLE_COMMAND cmd)
{
    M_NODE *node = Memory_Alloc(sizeof(M_NODE));
    node->cmd = cmd;
    node->next = m_List;
    m_List = node;
}

VECTOR *Console_Registry_GetAll(void)
{
    VECTOR *vec = Vector_Create(sizeof(const CONSOLE_COMMAND *));
    M_NODE *node = m_List;
    while (node != nullptr) {
        const CONSOLE_COMMAND *cmd_ptr = &node->cmd;
        Vector_Add(vec, &cmd_ptr);
        node = node->next;
    }
    return vec;
}
