/* Copyright (c) 2014, Google Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. */

/*
 * Changes from Qualcomm Technologies, Inc. are provided under the following license:
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "../inc/FastbootSsl.h"
#include <openssl/bytestring.h>

void CBS_init(CBS *cbs, const uint8_t *data, size_t len) {
  cbs->data = data;
  cbs->len = len;
}

static int cbs_get(CBS *cbs, const uint8_t **p, size_t n) {
  if (cbs->len < n) {
    return 0;
  }

  *p = cbs->data;
  cbs->data += n;
  cbs->len -= n;
  return 1;
}

int CBS_skip(CBS *cbs, size_t len) {
  const uint8_t *dummy;
  return cbs_get(cbs, &dummy, len);
}

const uint8_t *CBS_data(const CBS *cbs) {
  return cbs->data;
}

size_t CBS_len(const CBS *cbs) {
  return cbs->len;
}

static int cbs_get_u(CBS *cbs, uint64_t *out, size_t len) {
  uint64_t result = 0;
  const uint8_t *data;

  if (!cbs_get(cbs, &data, len)) {
    return 0;
  }
  for (size_t i = 0; i < len; i++) {
    result <<= 8;
    result |= data[i];
  }
  *out = result;
  return 1;
}

int CBS_get_u8(CBS *cbs, uint8_t *out) {
  const uint8_t *v;
  if (!cbs_get(cbs, &v, 1)) {
    return 0;
  }
  *out = *v;
  return 1;
}

int CBS_get_bytes(CBS *cbs, CBS *out, size_t len) {
  const uint8_t *v;
  if (!cbs_get(cbs, &v, len)) {
    return 0;
  }
  CBS_init(out, v, len);
  return 1;
}

static int parse_base128_integer(CBS *cbs, uint64_t *out) {
  uint64_t v = 0;
  uint8_t b;
  do {
    if (!CBS_get_u8(cbs, &b)) {
      return 0;
    }
    if ((v >> (64 - 7)) != 0) {
      // The value is too large.
      return 0;
    }
    if (v == 0 && b == 0x80) {
      // The value must be minimally encoded.
      return 0;
    }
    v = (v << 7) | (b & 0x7f);

    // Values end at an octet with the high bit cleared.
  } while (b & 0x80);

  *out = v;
  return 1;
}

static int parse_asn1_tag(CBS *cbs, CBS_ASN1_TAG *out) {
  uint8_t tag_byte;
  if (!CBS_get_u8(cbs, &tag_byte)) {
    return 0;
  }

  // ITU-T X.690 section 8.1.2.3 specifies the format for identifiers with a tag
  // number no greater than 30.
  //
  // If the number portion is 31 (0x1f, the largest value that fits in the
  // allotted bits), then the tag is more than one byte long and the
  // continuation bytes contain the tag number.
  CBS_ASN1_TAG tag = ((CBS_ASN1_TAG)tag_byte & 0xe0) << CBS_ASN1_TAG_SHIFT;
  CBS_ASN1_TAG tag_number = tag_byte & 0x1f;
  if (tag_number == 0x1f) {
    uint64_t v;
    if (!parse_base128_integer(cbs, &v) ||
        // Check the tag number is within our supported bounds.
        v > CBS_ASN1_TAG_NUMBER_MASK ||
        // Small tag numbers should have used low tag number form, even in BER.
        v < 0x1f) {
      return 0;
    }
    tag_number = (CBS_ASN1_TAG)v;
  }

  tag |= tag_number;

  // Tag [UNIVERSAL 0] is reserved for use by the encoding. Reject it here to
  // avoid some ambiguity around ANY values and BER indefinite-length EOCs. See
  // https://crbug.com/boringssl/455.
  if ((tag & ~CBS_ASN1_CONSTRUCTED) == 0) {
    return 0;
  }

  *out = tag;
  return 1;
}

int CBS_is_valid_asn1_integer(const CBS *cbs, int *out_is_negative) {
  CBS copy = *cbs;
  uint8_t first_byte, second_byte;
  if (!CBS_get_u8(&copy, &first_byte)) {
    return 0;  // INTEGERs may not be empty.
  }
  if (out_is_negative != NULL) {
    *out_is_negative = (first_byte & 0x80) != 0;
  }
  if (!CBS_get_u8(&copy, &second_byte)) {
    return 1;  // One byte INTEGERs are always minimal.
  }
  if ((first_byte == 0x00 && (second_byte & 0x80) == 0) ||
      (first_byte == 0xff && (second_byte & 0x80) != 0)) {
    return 0;  // The value is minimal iff the first 9 bits are not all equal.
  }
  return 1;
}

int CBS_is_unsigned_asn1_integer(const CBS *cbs) {
  int is_negative;
  return CBS_is_valid_asn1_integer(cbs, &is_negative) && !is_negative;
}

int CBS_get_asn1_uint64(CBS *cbs, uint64_t *out) {
  CBS bytes;
  if (!CBS_get_asn1(cbs, &bytes, CBS_ASN1_INTEGER) ||
      !CBS_is_unsigned_asn1_integer(&bytes)) {
    return 0;
  }

  *out = 0;
  const uint8_t *data = CBS_data(&bytes);
  size_t len = CBS_len(&bytes);
  for (size_t i = 0; i < len; i++) {
    if ((*out >> 56) != 0) {
      // Too large to represent as a uint64_t.
      return 0;
    }
    *out <<= 8;
    *out |= data[i];
  }

  return 1;
}

static int cbs_get_any_asn1_element(CBS *cbs, CBS *out, CBS_ASN1_TAG *out_tag,
                                    size_t *out_header_len, int *out_ber_found,
                                    int *out_indefinite, int ber_ok) {
  CBS header = *cbs;
  CBS throwaway;

  if (out == NULL) {
    out = &throwaway;
  }
  if (ber_ok) {
    *out_ber_found = 0;
    *out_indefinite = 0;
  } else {
    assert(out_ber_found == NULL);
    assert(out_indefinite == NULL);
  }

  CBS_ASN1_TAG tag;
  if (!parse_asn1_tag(&header, &tag)) {
    return 0;
  }
  if (out_tag != NULL) {
    *out_tag = tag;
  }

  uint8_t length_byte;
  if (!CBS_get_u8(&header, &length_byte)) {
    return 0;
  }

  size_t header_len = CBS_len(cbs) - CBS_len(&header);

  size_t len;
  // The format for the length encoding is specified in ITU-T X.690 section
  // 8.1.3.
  if ((length_byte & 0x80) == 0) {
    // Short form length.
    len = ((size_t) length_byte) + header_len;
    if (out_header_len != NULL) {
      *out_header_len = header_len;
    }
  } else {
    // The high bit indicate that this is the long form, while the next 7 bits
    // encode the number of subsequent octets used to encode the length (ITU-T
    // X.690 clause 8.1.3.5.b).
    const size_t num_bytes = length_byte & 0x7f;
    uint64_t len64;

    if (ber_ok && (tag & CBS_ASN1_CONSTRUCTED) != 0 && num_bytes == 0) {
      // indefinite length
      if (out_header_len != NULL) {
        *out_header_len = header_len;
      }
      *out_ber_found = 1;
      *out_indefinite = 1;
      return CBS_get_bytes(cbs, out, header_len);
    }

    // ITU-T X.690 clause 8.1.3.5.c specifies that the value 0xff shall not be
    // used as the first byte of the length. If this parser encounters that
    // value, num_bytes will be parsed as 127, which will fail this check.
    if (num_bytes == 0 || num_bytes > 4) {
      return 0;
    }
    if (!cbs_get_u(&header, &len64, num_bytes)) {
      return 0;
    }
    // ITU-T X.690 section 10.1 (DER length forms) requires encoding the
    // length with the minimum number of octets. BER could, technically, have
    // 125 superfluous zero bytes. We do not attempt to handle that and still
    // require that the length fit in a |uint32_t| for BER.
    if (len64 < 128) {
      // Length should have used short-form encoding.
      if (ber_ok) {
        *out_ber_found = 1;
      } else {
        return 0;
      }
    }
    if ((len64 >> ((num_bytes - 1) * 8)) == 0) {
      // Length should have been at least one byte shorter.
      if (ber_ok) {
        *out_ber_found = 1;
      } else {
        return 0;
      }
    }
    len = len64;
    if (len + header_len + num_bytes < len) {
      // Overflow.
      return 0;
    }
    len += header_len + num_bytes;
    if (out_header_len != NULL) {
      *out_header_len = header_len + num_bytes;
    }
  }

  return CBS_get_bytes(cbs, out, len);
}

int CBS_get_any_asn1_element(CBS *cbs, CBS *out, CBS_ASN1_TAG *out_tag,
                                    size_t *out_header_len) {
  return cbs_get_any_asn1_element(cbs, out, out_tag, out_header_len, NULL, NULL,
                                  /*ber_ok=*/0);
}

static int cbs_get_asn1(CBS *cbs, CBS *out, CBS_ASN1_TAG tag_value,
                        int skip_header) {
  size_t header_len;
  CBS_ASN1_TAG tag;
  CBS throwaway;

  if (out == NULL) {
    out = &throwaway;
  }

  if (!CBS_get_any_asn1_element(cbs, out, &tag, &header_len) ||
      tag != tag_value) {
    return 0;
  }

  if (skip_header && !CBS_skip(out, header_len)) {
    assert(0);
    return 0;
  }

  return 1;
}

int CBS_get_asn1(CBS *cbs, CBS *out, CBS_ASN1_TAG tag_value) {
  return cbs_get_asn1(cbs, out, tag_value, 1 /* skip header */);
}
