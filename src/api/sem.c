#include <uv.h>
#include "sem.h"

struct auto_sem_s
{
    uv_sem_t    sem;
};

static void api_sem_destroy(auto_sem_t* self)
{
    uv_sem_destroy(&self->sem);
    api.memory->free(self);
}

static void api_sem_wait(auto_sem_t* self)
{
    uv_sem_wait(&self->sem);
}

static void api_sem_post(auto_sem_t* self)
{
    uv_sem_post(&self->sem);
}

static auto_sem_t* api_sem_create(unsigned int value)
{
    auto_sem_t* impl = api.memory->malloc(sizeof(auto_sem_t));
    uv_sem_init(&impl->sem, value);
    return impl;
}

const auto_api_sem_t api_sem = {
    api_sem_create,                     /* .sem.create */
    api_sem_destroy,                    /* .sem.destroy */
    api_sem_wait,                       /* .sem.wait */
    api_sem_post,                       /* .sem.post */
};
