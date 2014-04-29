dovecot-extensions
==================

Extensions for dovecot

Added Ceritificate and Certificate Checks to Dovecot 2.2.x

- ssl_verify_depth:      will check the maximal certificate chain depth
- ssl_cert_md_algorithm: will check the corresponding certificate fingerprint algorithm (md5/sha1/...)
- cert_loginname:        will handle the loginname included in special client certificates (x509 fields)
- cert_fingerprint:      allows to access the fingerprint of a certificate inbound of the dovecot (used for select and  compare with LDAP backend where the fingerprint of a user is stored)

This patches are ported to dovecot-2.2.x from a patch serie done on Dovecot 2.0.16 done in 2011/2012
