/**
 * Riccardo M. Cefala                     Thu Jan 10 16:52:26 CET 2013
 *
 * Serialization/Deserialization stuff for zmq distrib layer.
 *
 */

#ifndef DISTRIBZMQPACK_H
#define DISTRIBZMQPACK_H

/**
 * packs an integer array into a zframe_t. It relies on ZMQ de/allocation.
 * If buf is NULL, a new frame is created, otherwise data is appended.
 * FIXME: should we enforce a maximum buffer_size?
 */
inline static void PackInt(void *buf, int count, int *src);

/**
 * unpacks an integer array from a zframe_t (buf) into dst
 * FIXME: again, should I care about dst size?
 */
inline static void UnpackInt(void *buf, int count, int *dst);

/**
 * packs a byte array into a zframe_t. It relies on ZMQ de/allocation.
 * If buf is NULL, a new frame is created, otherwise data is appended.
 * FIXME: should we enforce a maximum buffer_size?
 */
inline static void PackByte(void *buf, int count, char *src);

/**
 * unpacks a byte array from a zframe_t (buf) into dst
 * FIXME: again, should I care about dst size?
 */
inline static void UnpackByte(void *buf, int count, char *dst);


/**
 * SNet Packing Functions
 *
inline static void PackRef(void *buf, int count, snet_ref_t **src)

inline static void UnpackRef(void *buf, int count, snet_ref_t **dst)

inline static void PackDest(void *buf, snet_dest_t *dest)

inline static void UnpackDest(void *buf, snet_dest_t *dest)
*/
#endif

