/* Copyright (c) 2014 Dovecot authors, see the included COPYING file */

#include "lib.h"
#include "ioloop.h"
#include "istream.h"
#include "module-context.h"
#include "http-url.h"
#include "http-client.h"
#include "message-parser.h"
#include "mail-user.h"
#include "fts-parser.h"

#define TIKA_USER_CONTEXT(obj) \
	MODULE_CONTEXT(obj, fts_parser_tika_user_module)

struct fts_parser_tika_user {
	union mail_user_module_context module_ctx;
	struct http_url *http_url;
};

struct tika_fts_parser {
	struct fts_parser parser;
	struct mail_user *user;
	struct http_client_request *http_req;

	struct ioloop *ioloop;
	struct io *io;
	struct istream *payload;

	bool failed;
};

static struct http_client *tika_http_client = NULL;
static MODULE_CONTEXT_DEFINE_INIT(fts_parser_tika_user_module,
				  &mail_user_module_register);

static int
tika_get_http_client_url(struct mail_user *user, struct http_url **http_url_r)
{
	struct fts_parser_tika_user *tuser = TIKA_USER_CONTEXT(user);
	struct http_client_settings http_set;
	const char *url, *error;

	url = mail_user_plugin_getenv(user, "fts_tika");
	if (url == NULL) {
		/* fts_tika disabled */
		return -1;
	}

	if (tuser != NULL) {
		*http_url_r = tuser->http_url;
		return *http_url_r == NULL ? -1 : 0;
	}

	tuser = p_new(user->pool, struct fts_parser_tika_user, 1);
	MODULE_CONTEXT_SET(user, fts_parser_tika_user_module, tuser);

	if (http_url_parse(url, NULL, 0, user->pool,
			   &tuser->http_url, &error) < 0) {
		i_error("fts_tika: Failed to parse HTTP url %s: %s", url, error);
		return -1;
	}

	if (tika_http_client == NULL) {
		memset(&http_set, 0, sizeof(http_set));
		http_set.max_idle_time_msecs = 100;
		http_set.max_parallel_connections = 1;
		http_set.max_pipelined_requests = 1;
		http_set.max_redirects = 1;
		http_set.max_attempts = 3;
		http_set.debug = user->mail_debug;
		tika_http_client = http_client_init(&http_set);
	}
	*http_url_r = tuser->http_url;
	return 0;
}

static void
fts_tika_parser_response(const struct http_response *response,
			 struct tika_fts_parser *parser)
{
	i_assert(parser->payload == NULL);

	switch (response->status) {
	case 200:
		/* read response */
		i_stream_ref(response->payload);
		parser->payload = response->payload;
		break;
	case 204: /* empty response */
	case 422: /* Unprocessable Entity */
		if (parser->user->mail_debug) {
			i_debug("fts_tika: PUT %s failed: %s",
				mail_user_plugin_getenv(parser->user, "fts_tika"),
				response->reason);
		}
		parser->payload = i_stream_create_from_data("", 0);
		break;
	default:
		i_error("fts_tika: PUT %s failed: %s",
			mail_user_plugin_getenv(parser->user, "fts_tika"),
			response->reason);
		parser->failed = TRUE;
		break;
	}
	parser->http_req = NULL;
	io_loop_stop(current_ioloop);
}

static struct fts_parser *
fts_parser_tika_try_init(struct mail_user *user, const char *content_type,
			 const char *content_disposition)
{
	struct tika_fts_parser *parser;
	struct http_url *http_url;
	struct http_client_request *http_req;

	if (tika_get_http_client_url(user, &http_url) < 0)
		return NULL;

	parser = i_new(struct tika_fts_parser, 1);
	parser->parser.v = fts_parser_tika;
	parser->user = user;

	http_req = http_client_request(tika_http_client, "PUT",
			http_url->host_name,
			t_strconcat(http_url->path, http_url->enc_query, NULL),
			fts_tika_parser_response, parser);
	http_client_request_set_port(http_req, http_url->port);
	http_client_request_set_ssl(http_req, http_url->have_ssl);
	http_client_request_add_header(http_req, "Content-Type", content_type);
	http_client_request_add_header(http_req, "Content-Disposition",
				       content_disposition);
	http_client_request_add_header(http_req, "Accept", "text/plain");

	parser->http_req = http_req;
	return &parser->parser;
}

static void fts_parser_tika_more(struct fts_parser *_parser,
				 struct message_block *block)
{
	struct tika_fts_parser *parser = (struct tika_fts_parser *)_parser;
	const unsigned char *data;
	size_t size;
	ssize_t ret;

	if (block->size > 0) {
		/* first we'll send everything to Tika */
		if (!parser->failed &&
		    http_client_request_send_payload(&parser->http_req,
						     block->data,
						     block->size) < 0)
			parser->failed = TRUE;
		block->size = 0;
		return;
	}

	if (parser->payload == NULL) {
		/* read the result from Tika */
		if (!parser->failed &&
		    http_client_request_finish_payload(&parser->http_req) < 0)
			parser->failed = TRUE;
		if (!parser->failed && parser->payload == NULL)
			http_client_wait(tika_http_client);
		if (parser->failed)
			return;
		i_assert(parser->payload != NULL);
	}
	/* continue returning data from Tika */
	while ((ret = i_stream_read_data(parser->payload, &data, &size, 0)) == 0) {
		if (parser->failed)
			return;
		/* wait for more input from Tika */
		if (parser->ioloop == NULL) {
			parser->ioloop = io_loop_create();
			parser->io = io_add_istream(parser->payload, io_loop_stop,
						    current_ioloop);
		} else {
			io_loop_set_current(parser->ioloop);
		}
		io_loop_run(current_ioloop);
	}
	if (size > 0) {
		i_assert(ret > 0);
		block->data = data;
		block->size = size;
		i_stream_skip(parser->payload, size);
	} else {
		/* finished */
		i_assert(ret == -1);
	}
}

static void fts_parser_tika_deinit(struct fts_parser *_parser)
{
	struct tika_fts_parser *parser = (struct tika_fts_parser *)_parser;

	if (parser->ioloop != NULL) {
		io_remove(&parser->io);
		io_loop_destroy(&parser->ioloop);
	}
	if (parser->payload != NULL)
		i_stream_unref(&parser->payload);
	/* FIXME: kludgy, http_req should be NULL here if we don't want to
	   free it. requires lib-http changes. */
	if (parser->http_req != NULL)
		http_client_request_abort(&parser->http_req);
	i_free(parser);
}

static void fts_parser_tika_unload(void)
{
	if (tika_http_client != NULL)
		http_client_deinit(&tika_http_client);
}

struct fts_parser_vfuncs fts_parser_tika = {
	fts_parser_tika_try_init,
	fts_parser_tika_more,
	fts_parser_tika_deinit,
	fts_parser_tika_unload
};
