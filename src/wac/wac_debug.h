#ifndef __WAC_DEBUG_H
#define __WAC_DEBUG_H

#include "wac_page.h"

void wac_page_disass(wac_page_t *page, const char *name);
size_t wac_inst_disass(wac_page_t *page, size_t address);

#endif //__WAC_DEBUG_H
