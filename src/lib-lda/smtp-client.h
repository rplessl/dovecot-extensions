#ifndef SMTP_CLIENT_H
#define SMTP_CLIENT_H

struct smtp_client * ATTR_NULL(3)
smtp_client_init(const struct lda_settings *set, const char *return_path);
/* Add a new recipient */
void smtp_client_add_rcpt(struct smtp_client *client, const char *address);
/* Get an output stream where the message can be written to. The recipients
   must already be added before calling this. */
struct ostream *smtp_client_send(struct smtp_client *client);
/* Returns 1 on success, 0 on permanent failure (e.g. invalid destination),
   -1 on temporary failure. */
int smtp_client_deinit(struct smtp_client *client, const char **error_r);

/* FIXME: obsolete API, remove in v2.3: */
struct smtp_client * ATTR_NULL(3)
smtp_client_open(const struct lda_settings *set, const char *destination,
		 const char *return_path, struct ostream **output_r);
/* Returns sysexits-compatible return value */
int smtp_client_close(struct smtp_client *client);

#endif
