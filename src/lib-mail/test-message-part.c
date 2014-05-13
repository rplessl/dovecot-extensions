/* Copyright (c) 2014 Dovecot authors, see the included COPYING file */

#include "lib.h"
#include "istream.h"
#include "message-parser.h"
#include "test-common.h"

static const char test_msg[] =
"From user@domain  Fri Feb 22 17:06:23 2008\n"
"From: user@domain.org\n"
"Date: Sat, 24 Mar 2007 23:00:00 +0200\n"
"Mime-Version: 1.0\n"
"Content-Type: multipart/mixed; boundary=\"foo bar\"\n"
"\n"
"Root MIME prologue\n"
"\n"
"--foo bar\n"
"Content-Type: text/x-myown; charset=us-ascii\n"
"\n"
"hello\n"
"\n"
"--foo bar\n"
"Content-Type: message/rfc822\n"
"\n"
"From: sub@domain.org\n"
"Date: Sun, 12 Aug 2012 12:34:56 +0300\n"
"Subject: submsg\n"
"Content-Type: multipart/alternative; boundary=\"sub1\"\n"
"\n"
"Sub MIME prologue\n"
"--sub1\n"
"Content-Type: text/html\n"
"\n"
"<p>Hello world</p>\n"
"\n"
"--sub1\n"
"Content-Type: text/plain\n"
"\n"
"Hello another world\n"
"\n"
"--sub1--\n"
"Sub MIME epilogue\n"
"\n"
"--foo bar--\n"
"Root MIME epilogue\n"
"\n";
#define TEST_MSG_LEN (sizeof(test_msg)-1)

static void test_message_part_idx(void)
{
	struct message_parser_ctx *parser;
	struct istream *input;
	struct message_part *parts, *part, *prev_part;
	struct message_block block;
	unsigned int i;
	pool_t pool;
	int ret;

	test_begin("message part indexes");
	pool = pool_alloconly_create("message parser", 10240);
	input = i_stream_create_from_data(test_msg, TEST_MSG_LEN);

	parser = message_parser_init(pool, input, 0, 0);
	while ((ret = message_parser_parse_next_block(parser, &block)) > 0) ;
	test_assert(ret < 0);
	test_assert(message_parser_deinit(&parser, &parts) == 0);

	part = message_part_by_idx(parts, 0);
	test_assert(part == parts);
	test_assert(message_part_by_idx(parts, 1) == parts->children);

	for (i = 1; i < 6; i++) {
		prev_part = part;
		part = message_part_by_idx(parts, i);
		test_assert(part != NULL);
		test_assert(part != NULL && message_part_to_idx(part) == i);
		test_assert(part != NULL &&
			    prev_part->physical_pos < part->physical_pos);
	}
	test_assert(message_part_by_idx(parts, i) == NULL);

	i_stream_unref(&input);
	pool_unref(&pool);
	test_end();
}

int main(void)
{
	static void (*test_functions[])(void) = {
		test_message_part_idx,
		NULL
	};
	return test_run(test_functions);
}
