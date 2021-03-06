/* $OpenBSD: tls_buffer.c,v 1.1 2021/10/23 13:12:14 jsing Exp $ */
/*
 * Copyright (c) 2018, 2019 Joel Sing <jsing@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdlib.h>
#include <string.h>

#include "bytestring.h"
#include "tls_internal.h"

struct tls_buffer {
	size_t capacity;
	uint8_t *data;
	size_t len;
	size_t offset;
};

static int tls_buffer_resize(struct tls_buffer *buf, size_t capacity);

struct tls_buffer *
tls_buffer_new(size_t init_size)
{
	struct tls_buffer *buf = NULL;

	if ((buf = calloc(1, sizeof(struct tls_buffer))) == NULL)
		goto err;

	if (!tls_buffer_resize(buf, init_size))
		goto err;

	return buf;

 err:
	tls_buffer_free(buf);

	return NULL;
}

void
tls_buffer_free(struct tls_buffer *buf)
{
	if (buf == NULL)
		return;

	freezero(buf->data, buf->capacity);
	freezero(buf, sizeof(struct tls_buffer));
}

static int
tls_buffer_resize(struct tls_buffer *buf, size_t capacity)
{
	uint8_t *data;

	if (buf->capacity == capacity)
		return 1;

	if ((data = recallocarray(buf->data, buf->capacity, capacity, 1)) == NULL)
		return 0;

	buf->data = data;
	buf->capacity = capacity;

	return 1;
}

int
tls_buffer_set_data(struct tls_buffer *buf, CBS *data)
{
	if (!tls_buffer_resize(buf, CBS_len(data)))
		return 0;
	memcpy(buf->data, CBS_data(data), CBS_len(data));
	return 1;
}

ssize_t
tls_buffer_extend(struct tls_buffer *buf, size_t len,
    tls_read_cb read_cb, void *cb_arg)
{
	ssize_t ret;

	if (len == buf->len)
		return buf->len;

	if (len < buf->len)
		return TLS_IO_FAILURE;

	if (!tls_buffer_resize(buf, len))
		return TLS_IO_FAILURE;

	for (;;) {
		if ((ret = read_cb(&buf->data[buf->len],
		    buf->capacity - buf->len, cb_arg)) <= 0)
			return ret;

		if (ret > buf->capacity - buf->len)
			return TLS_IO_FAILURE;

		buf->len += ret;

		if (buf->len == buf->capacity)
			return buf->len;
	}
}

void
tls_buffer_cbs(struct tls_buffer *buf, CBS *cbs)
{
	CBS_init(cbs, buf->data, buf->len);
}

int
tls_buffer_finish(struct tls_buffer *buf, uint8_t **out, size_t *out_len)
{
	if (out == NULL || out_len == NULL)
		return 0;

	*out = buf->data;
	*out_len = buf->len;

	buf->capacity = 0;
	buf->data = NULL;
	buf->len = 0;

	return 1;
}
