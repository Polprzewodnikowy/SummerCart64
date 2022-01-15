#ifndef STORAGE_H__
#define STORAGE_H__


typedef enum {
    STORAGE_BACKEND_SD = 0,
    STORAGE_BACKEND_USB = 1,
} storage_backend_t;


void storage_run_menu (storage_backend_t storage_backend);


#endif
