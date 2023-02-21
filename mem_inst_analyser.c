#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include "dr_api.h"
#include "drmgr.h"
#include "drreg.h"
#include "drutil.h"
#include "drx.h"

#include "include/memtrace.h"

#define MAX_THREADS 100
const u64 linear_set_size_increment = 100;

// todo => shrink to 8 bytes. Should be possible if memory address access/accessing only store 
// bytes of the virtual address that define the memory locations relative to the process pages)
// reference: https://developer.arm.com/documentation/den0024/a/The-Memory-Management-Unit/Translating-a-Virtual-Address-to-a-Physical-Address
// abstract: only the last 28 bits of an virtual address encode the actual loation(or index) of the address, the rest is context
typedef struct MemoryAccess {
   usize address_accessed;
   u64 size;
   u32 thread_id;
} MemoryAccess;

typedef struct LockAccess {
   usize lock_address;
   u32 thread_id;
} LockAccess;

typedef struct ThreadState {
    u64 idx;
    MemoryAccess *mem_read_set;
    u64 mem_read_set_capacity;
    u64 mem_read_set_len;

    MemoryAccess *mem_write_set;
    u64 mem_write_set_capacity;
    u64 mem_write_set_len;

    LockAccess *lock_unlock_set;
    u64 lock_unlock_set_capacity;
    u64 lock_unlock_set_len;

    LockAccess *lock_lock_set;
    u64 lock_lock_set_capacity;
    u64 lock_lock_set_len;
} ThreadState;
ThreadState threads[MAX_THREADS] = {};
u64 n_threads = 0;

i64 findThreadByTlsIdx(u64 idx) {
    u64 i;
    for (i = 0; i <= n_threads; i++) {
        printf("1 \n");
        printf("threads[i].idx: %lu idx: %lu \n", threads[i].idx, idx);
        if (threads[i].idx == idx) {
            return i;
        }
    }
    return -1;
}
u32 mem_analyse_init() { 
        return 1;
}

u32 mem_analyse_new_thread_init() {
    if (n_threads >= MAX_THREADS) return 0;
    threads[n_threads].idx = tls_idx;

    threads[n_threads].mem_read_set = (MemoryAccess*)malloc(sizeof(MemoryAccess) * linear_set_size_increment);
    threads[n_threads].mem_read_set_capacity = linear_set_size_increment;
    if (threads[n_threads].mem_read_set == NULL) {
        printf("set allocation error \n");
        return 0;    
    }
    threads[n_threads].mem_write_set = (MemoryAccess*)malloc(sizeof(MemoryAccess) * linear_set_size_increment);
    threads[n_threads].mem_write_set_capacity = linear_set_size_increment;
    if (threads[n_threads].mem_write_set == NULL) {
        printf("set allocation error \n");
        return 0;
    }

    threads[n_threads].lock_lock_set = (LockAccess*)malloc(sizeof(LockAccess) * linear_set_size_increment);
    threads[n_threads].lock_lock_set_capacity = linear_set_size_increment;
    if (threads[n_threads].lock_lock_set == NULL) {
        printf("set allocation error \n");
        return 0;
    }
    threads[n_threads].lock_unlock_set = (LockAccess*)malloc(sizeof(LockAccess) * linear_set_size_increment);
    threads[n_threads].lock_unlock_set_capacity = linear_set_size_increment;    
    if (threads[n_threads].lock_unlock_set == NULL) {
        printf("set allocation error \n");
        return 0;
    }

    n_threads += 1;
    return 1;
}

void mem_analyse_thread_exit() {
    i64 t_index = findThreadByTlsIdx(tls_idx);
    printf("LEAVING");
    u64 i;
    for (i = 0; i < threads[t_index].mem_write_set_len; i++) {
        printf("write access to address: %ld, size: %ld, tid: %d", threads[t_index].mem_write_set[i].address_accessed, threads[t_index].mem_write_set[i].size, threads[t_index].mem_write_set[i].thread_id);
    }

    free(threads[t_index].mem_read_set);
    free(threads[t_index].mem_write_set);
    free(threads[t_index].lock_lock_set);
    free(threads[t_index].lock_unlock_set);
}

void *increase_set_capacity(void *set, u64 *set_capacity) {
    *set_capacity += linear_set_size_increment;
    printf("new set_capacity: %ld \n", *set_capacity);
    return realloc(set, *set_capacity);
}

// this is an event like fn that is envoked on every memory access (called by DynamRIO)
void memtrace(void *drcontext) {
    per_thread_t *data;
    mem_ref_t *mem_ref, *buf_ptr;

    data = drmgr_get_tls_field(drcontext, tls_idx);
    buf_ptr = BUF_PTR(data->seg_base);
    
    i64 t_index = findThreadByTlsIdx(tls_idx);
    if (t_index < 0) {
        printf("error finding tls_idx. %ld \n", t_index);
        return;    
    } 
    for (mem_ref = (mem_ref_t *)data->buf_base; mem_ref < buf_ptr; mem_ref++) {
        printf("lol1 \n");
        if (mem_ref->type < REF_TYPE_WRITE) {
            if (mem_ref->type == REF_TYPE_WRITE) {
                // mem write
                if (threads[t_index].mem_write_set_len >= threads[t_index].mem_write_set_capacity) threads[t_index].mem_write_set = increase_set_capacity(threads[t_index].mem_write_set, &threads[t_index].mem_write_set_capacity);
                if (threads[t_index].mem_write_set == NULL) exit(1);
                threads[t_index].mem_write_set[threads[t_index].mem_write_set_len].address_accessed = (usize)mem_ref->addr;
                threads[t_index].mem_write_set[threads[t_index].mem_write_set_len].size = mem_ref->size;
                threads[t_index].mem_write_set_len += 1;
            } else {
                // mem read
                if (threads[t_index].mem_read_set_len >= threads[t_index].mem_read_set_capacity) threads[t_index].mem_read_set = increase_set_capacity(threads[t_index].mem_read_set, &threads[t_index].mem_read_set_capacity);
                if (threads[t_index].mem_read_set == NULL) exit(1);
                printf("AAAAAAAAAAAAAAAAAAAAAA1\n");
                printf("t_index: %ld len %ld \n", t_index, threads[t_index].mem_read_set_len);
                threads[t_index].mem_read_set[threads[t_index].mem_read_set_len].address_accessed = (usize)mem_ref->addr;
                printf("AAAAAAAAAAAAAAAAAAAAAA2\n");
                threads[t_index].mem_read_set_len += 1;
            }
        }
        printf("lol\n");
        /* We use PIFX to avoid leading zeroes and shrink the resulting file. */
        fprintf(data->logf, "" PIFX ": %2d, %s\n", (ptr_uint_t)mem_ref->addr,
                mem_ref->size,
                (mem_ref->type > REF_TYPE_WRITE)
                    ? decode_opcode_name(mem_ref->type) /* opcode for instr */
                    : (mem_ref->type == REF_TYPE_WRITE ? "w" : "r"));
        data->num_refs++;
    }
    BUF_PTR(data->seg_base) = data->buf_base;
}
