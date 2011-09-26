#ifndef REFERENCE_H
#define REFERENCE_H

typedef struct snet_refcount snet_refcount_t;

void SNetReferenceInit(void);
void SNetReferenceDestroy(void);

snet_ref_t *SNetRefIncoming(snet_ref_t *ref);
void SNetRefOutgoing(snet_ref_t *ref);
void SNetRefUpdate(snet_ref_t *ref, int value);
void SNetRefSet(snet_ref_t *ref, void *data);
#endif
