#include <barium/alpc.h>
#include <barium/heap.h>
#include <barium/sched.h>
#include <barium/vmm.h>
#include <barium/pmm.h>
#include <barium/lib.h>
#include <barium/console.h>

static alpc_port_t *port_list = NULL;
static spinlock_t port_list_lock;

static alpc_section_t sections[64];
static uint64_t next_section_id = 1;
static uint64_t next_section_virt = 0x8000000000;
static spinlock_t section_lock;

void alpc_init() {
    port_list_lock.lock = 0;
    section_lock.lock = 0;
    b_memset(sections, 0, sizeof(sections));
}

uint64_t alpc_create_port(const char *name, uint32_t queue_size) {
    alpc_port_t *port = (alpc_port_t*)kmalloc(sizeof(alpc_port_t));
    b_memset(port, 0, sizeof(alpc_port_t));
    
    b_strcpy(port->name, name);
    port->owner_tid = sched_get_tid();
    port->server_tid = port->owner_tid;
    port->queue_size = queue_size;
    port->msg_queue = (alpc_message_t*)kmalloc(sizeof(alpc_message_t) * queue_size);
    port->lock.lock = 0;
    
    uint64_t flags = b_irq_save();
    spin_lock(&port_list_lock);
    port->next = port_list;
    port_list = port;
    spin_unlock(&port_list_lock);
    b_irq_restore(flags);
    
    return (uint64_t)port;
}

uint64_t alpc_connect_port(const char *name) {
    uint64_t flags = b_irq_save();
    spin_lock(&port_list_lock);
    alpc_port_t *curr = port_list;
    while (curr) {
        if (b_strcmp(curr->name, name) == 0) {
            spin_unlock(&port_list_lock);
            b_irq_restore(flags);
            return (uint64_t)curr;
        }
        curr = curr->next;
    }
    spin_unlock(&port_list_lock);
    b_irq_restore(flags);
    return 0;
}

int alpc_send(uint64_t port_id, alpc_message_t *msg) {
    alpc_port_t *port = (alpc_port_t*)port_id;
    if (!port) return -1;
    
    msg->sender_tid = sched_get_tid();
    msg->receiver_tid = port->server_tid;
    
    uint64_t flags = b_irq_save();
    spin_lock(&port->lock);
    
    if (((port->queue_tail + 1) % port->queue_size) == port->queue_head) {
        spin_unlock(&port->lock);
        b_irq_restore(flags);
        return -1; 
    }
    
    b_memcpy(&port->msg_queue[port->queue_tail], msg, sizeof(alpc_message_t));
    port->queue_tail = (port->queue_tail + 1) % port->queue_size;
    
    uint64_t target = port->server_tid;
    spin_unlock(&port->lock);
    b_irq_restore(flags);
    
    sched_handoff(target);
    
    return 0;
}

int alpc_recv(uint64_t port_id, alpc_message_t *msg) {
    alpc_port_t *port = (alpc_port_t*)port_id;
    if (!port) return -1;
    
    uint64_t flags = b_irq_save();
    spin_lock(&port->lock);
    
    if (port->queue_head == port->queue_tail) {
        spin_unlock(&port->lock);
        b_irq_restore(flags);
        return -1;
    }
    
    b_memcpy(msg, &port->msg_queue[port->queue_head], sizeof(alpc_message_t));
    port->queue_head = (port->queue_head + 1) % port->queue_size;
    
    spin_unlock(&port->lock);
    b_irq_restore(flags);
    return 0;
}

uint64_t alpc_create_section(uint64_t size) {
    uint64_t pages = (size + 4095) / 4096;
    void *phys = pmm_alloc(pages);
    
    spin_lock(&section_lock);
    uint64_t id = next_section_id++;
    for (int i = 0; i < 64; i++) {
        if (sections[i].id == 0) {
            sections[i].id = id;
            sections[i].phys_addr = (uint64_t)phys;
            sections[i].size = pages * 4096;
            sections[i].owner_tid = sched_get_tid();
            spin_unlock(&section_lock);
            return id;
        }
    }
    spin_unlock(&section_lock);
    return 0;
}

void *alpc_map_section(uint64_t section_id) {
    spin_lock(&section_lock);
    for (int i = 0; i < 64; i++) {
        if (sections[i].id == section_id) {
            uint64_t phys = sections[i].phys_addr;
            uint64_t size = sections[i].size;
            uint64_t virt = next_section_virt;
            next_section_virt += size;
            spin_unlock(&section_lock);
            
            pml4_t *pml4 = vmm_get_kernel_pml4();
            for (uint64_t j = 0; j < size; j += 4096) {
                vmm_map(pml4, virt + j, phys + j, PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
            }
            return (void*)virt;
        }
    }
    spin_unlock(&section_lock);
    return NULL;
}
