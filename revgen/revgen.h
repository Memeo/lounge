//
//  revgen.h
//  LoungeAct
//
//  Created by Casey Marshall on 3/7/12.
//  Copyright (c) 2012 Memeo, Inc. All rights reserved.
//

#ifndef LoungeAct_revgen_h
#define LoungeAct_revgen_h

#include <stdint.h>
#include <Codec/Codec.h>

typedef int (*la_revgen_generator)(const la_codec_value_t *value,
                                   uint64_t start, uint64_t rev,
                                   int is_delete, char *buffer, size_t len);

/**
 * Generate a revision number for the given document (as a decoded value),
 * sequence start (?), revision number, and deleted flag. The revision
 * number is stored in the given buffer.
 *
 * @param value The document to generate the revision number for.
 * @param start ???
 * @param rev The sequence number of this revision.
 * @param is_delete Nonzero if this is a "deleted" document ("tombstone").
 * @param buffer The buffer to store the revision number. May be NULL (and
 *  the len parameter 0) to determine the length of the computed revision.
 * @param len The size of the buffer. Only this many bytes will be stored
 *  in the buffer.
 * @return The length of the revision number. This may be greater than the
 *  len parameter; it is always the length the revision number *should* be.
 *  A negative value is returned on error. The value written to the buffer
 *  is NOT null terminated.
 */
int la_revgen(const la_codec_value_t *value, uint64_t start,
              const char *rev, size_t revlen, int is_delete,
              char *buffer, size_t len);

#endif
