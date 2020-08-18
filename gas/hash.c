/* hash.c -- gas hash table code
   Copyright (C) 1987-2020 Free Software Foundation, Inc.

   This file is part of GAS, the GNU Assembler.

   GAS is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3, or (at your option)
   any later version.

   GAS is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GAS; see the file COPYING.  If not, write to the Free
   Software Foundation, 51 Franklin Street - Fifth Floor, Boston, MA
   02110-1301, USA.  */

/* This version of the hash table code is a wholescale replacement of
   the old hash table code, which was fairly bad.  This is based on
   the hash table code in BFD, but optimized slightly for the
   assembler.  The assembler does not need to derive structures that
   are stored in the hash table.  Instead, it always stores a pointer.
   The assembler uses the hash table mostly to store symbols, and we
   don't need to confuse the symbol structure with a hash table
   structure.  */

#include "as.h"
#include "safe-ctype.h"
#include "obstack.h"

/* Insert ELEMENT into HTAB.  If the element exists, it is overwritten.  */

void
htab_insert (htab_t htab, PTR element)
{
  void **slot = htab_find_slot (htab, element, INSERT);
  if (slot != NULL && htab->del_f)
    (*htab->del_f) (*slot);

  *slot = element;
}

/* Print statistics about a hash table.  */

void
htab_print_statistics (FILE *f, const char *name, htab_t table)
{
  fprintf (f, "%s hash statistics:\n", name);
  fprintf (f, "\t%u searches\n", table->searches);
  fprintf (f, "\t%lu elements\n", htab_elements (table));
  fprintf (f, "\t%lu table size\n", htab_size (table));
  fprintf (f, "\t%.2f collisions per search\n", htab_collisions (table));
}
