#ifndef DISPLAY_H__
#define DISPLAY_H__


#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>


void display_init (uint32_t *background);
bool display_ready (void);
void display_vprintf (const char *fmt, va_list args);
void display_printf (const char* fmt, ...);


#endif
