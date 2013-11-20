/*
 ============================================================================
 Name        : hev-slist.h
 Author      : Heiher <r@hev.cc>
 Copyright   : Copyright (c) 2013 everyone.
 Description : Singly-linked list data structure
 ============================================================================
 */

#ifndef __HEV_SLIST_H__
#define __HEV_SLIST_H__

typedef struct _HevSList HevSList;

HevSList * hev_slist_append (HevSList *self, void *data);
HevSList * hev_slist_prepend (HevSList *self, void *data);
HevSList * hev_slist_insert (HevSList *self, void *data, unsigned int position);
HevSList * hev_slist_remove (HevSList *self, const void *data);
HevSList * hev_slist_remove_all (HevSList *self, const void *data);

HevSList * hev_slist_last (HevSList *self);
HevSList * hev_slist_next (HevSList *self);

static inline void *
hev_slist_data (HevSList *self)
{
	return self ? *((void **) self) : NULL;
}

unsigned int hev_slist_length (HevSList *self);

void hev_slist_free (HevSList *self);

#endif /* __HEV_SLIST_H__ */

