#!/usr/bin/env bash

set -eux

CERTS_DIR=certs
mkdir -p $CERTS_DIR

# Create root key
openssl genrsa -out $CERTS_DIR/root.key 4096

# Create root CA
openssl req -x509 -new -nodes -key $CERTS_DIR/root.key -sha256 -days 100 \
  -subj '/CN=localhost/O=OX Abuse Shield Root/C=US' -out $CERTS_DIR/root.pem

# Create client key
openssl genrsa -out $CERTS_DIR/client.key 4096

# Create Certificate Signing Request for client.key
openssl req -new -key $CERTS_DIR/client.key -out $CERTS_DIR/client.csr \
  -subj '/CN=localhost/O=OX Abuse Shield Client/C=US'

# Create server key
openssl genrsa -out $CERTS_DIR/server.key 4096

# Create Certificate Signing Request for server.key
openssl req -new -key $CERTS_DIR/server.key -out $CERTS_DIR/server.csr \
  -subj '/CN=localhost/O=OX Abuse Shield Server/C=US'

# Sign client.key and server.key
cat >$CERTS_DIR/csr.ext <<EOF
authorityKeyIdentifier = keyid,issuer
basicConstraints = CA:FALSE
keyUsage = digitalSignature, nonRepudiation, keyEncipherment, dataEncipherment
EOF

openssl x509 -req -in $CERTS_DIR/client.csr -CA $CERTS_DIR/root.pem \
  -CAkey $CERTS_DIR/root.key -CAcreateserial -out $CERTS_DIR/client.crt \
  -days 50 -sha256 -extfile $CERTS_DIR/csr.ext

openssl x509 -req -in $CERTS_DIR/server.csr -CA $CERTS_DIR/root.pem \
  -CAkey $CERTS_DIR/root.key -CAcreateserial -out $CERTS_DIR/server.crt \
  -days 50 -sha256 -extfile $CERTS_DIR/csr.ext
