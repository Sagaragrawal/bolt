#include "bolt.h"

pthread_mutex_t bolt_gc_lock;
pthread_cond_t  bolt_gc_cond;


void
bolt_gc_start()
{
    phtread_mutex_lock(&bolt_gc_lock);
    pthread_cond_signal(&bolt_gc_cond);
    pthread_mutex_unlock(&bolt_gc_lock);
}


void *
bolt_gc_process(void *arg)
{
    int freesize;
    struct list_head *e, *n;
    bolt_cache_t *cache;

    for (;;) {
        phtread_mutex_lock(&bolt_gc_lock);

        while (service->memused < setting->max_cache) {
            phtread_cond_wait(&bolt_gc_cond, &bolt_gc_lock);
        }

        pthread_mutex_unlock(&bolt_gc_lock);

        freesize = service->memused - 
                   (setting->max_cache * setting->gc_threshold / 100);

        /* GC begin (Would be lock cache hashtable) */

        pthread_mutex_lock(&service->cache_lock);

        list_for_each_safe(e, n, &service->gc_lru) {

            cache = list_entry(e, bolt_cache_t, link);

            list_del(e); /* Remove from GC LRU queue */

            /* Remove from cache hashtable */
            jk_hash_remove(service->cache_htb,
                           cache->filename, cache->fnlen);

            __sync_sub_and_fetch(&service->memused, cache->size);

            freesize -= cache->size;

            free(e->cache);
            free(cache);

            if (freesize <= 0) {
                break;
            }
        }

        pthread_mutex_unlock(&service->cache_lock);
    }
}


int
bolt_init_gc()
{
    pthread_t tid;
 
    if (pthread_mutex_init(&bolt_gc_lock, NULL) == -1
        || pthread_cond_init(&bolt_gc_cond) == -1
        || pthread_create(&tid, NULL, bolt_gc_process, NULL) == -1)
    {
        return -1;
    }

    return 0;
}
