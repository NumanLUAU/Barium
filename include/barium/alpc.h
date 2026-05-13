#ifndef BARIUM_ALPC_H
#define BARIUM_ALPC_H

#include <stdint.h>
#include <stddef.h>
#include <barium/lib.h>

#define ALPC_MAX_MESSAGE_SIZE 4096
#define ALPC_MAX_PORTS 256

typedef enum {
    ALPC_MSG_SMALL,
    ALPC_MSG_SECTION
} alpc_message_type_t;

typedef struct {
    uint64_t sender_tid;
    uint64_t receiver_tid;
    alpc_message_type_t type;
    uint64_t length;
    uint64_t section_id;
    uint64_t section_offset;
    uint8_t data[128]; 
} alpc_message_t;

typedef struct alpc_port {
    char name[32];
    uint64_t owner_tid;
    uint64_t server_tid;
    alpc_message_t *msg_queue;
    uint32_t queue_head;
    uint32_t queue_tail;
    uint32_t queue_size;
    spinlock_t lock;
    struct alpc_port *next;
} alpc_port_t;

typedef struct {
    uint64_t id;
    uint64_t phys_addr;
    uint64_t size;
    uint64_t owner_tid;
} alpc_section_t;

void alpc_init();
uint64_t alpc_create_port(const char *name, uint32_t queue_size);
uint64_t alpc_connect_port(const char *name);
int alpc_send(uint64_t port_id, alpc_message_t *msg);
int alpc_recv(uint64_t port_id, alpc_message_t *msg);

uint64_t alpc_create_section(uint64_t size);
void *alpc_map_section(uint64_t section_id);

#endif
