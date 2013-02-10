#ifndef REFERENCE_H
#define REFERENCE_H

typedef struct snet_refcount snet_refcount_t;

void SNetReferenceInit(void);
void SNetReferenceDestroy(void);

int SNetRefInterface(snet_ref_t *ref);
int SNetRefNode(snet_ref_t *ref);
void SNetRefSerialise(snet_ref_t *ref, void *buf);
snet_ref_t *SNetRefDeserialise(void *buf);

void SNetRefIncoming(snet_ref_t *ref);
void SNetRefOutgoing(snet_ref_t *ref);
void SNetRefUpdate(snet_ref_t *ref, int value);
void SNetRefSet(snet_ref_t *ref, void *data);

/* To be implemented by distrib/implementation */
void SNetDistribFetchRef(snet_ref_t *ref);
void SNetDistribUpdateRef(snet_ref_t *ref, int count);
#endif
