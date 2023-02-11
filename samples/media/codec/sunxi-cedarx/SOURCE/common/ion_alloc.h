/*
 * Cedarx framework ION memory allocator.
 *
 * Copyright (c) 2020-2023 Leng Xujun <lengxujun2007@126.com>.
 *
 * This file is part of Cedarx.
 *
 * Cedarx is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 */

#ifndef ION_ALLOC_H
#define ION_ALLOC_H

extern int  ion_alloc_open(void);
extern int  ion_alloc_close(void);
extern long ion_alloc_alloc(int size);
extern int  ion_alloc_free(void *virt);
extern long ion_alloc_vir2phy(void *virt);
extern long ion_alloc_phy2vir(void *phys);
extern void ion_flush_cache(void *start, int size);

#endif /* ION_ALLOC_H */

