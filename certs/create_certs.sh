#!/bin/bash
serial="00"

# Create a CA
openssl req -out ca.pem -new -x509

# Create server cert/key
mkdir server
cd server
openssl genrsa -out server.key 1024
openssl req -key server.key -new -out server.req
echo "$serial" > serial.srl
openssl x509 -req -in server.req -CA ../ca.pem -CAkey ../privkey.pem -CAserial serial.srl -out server.pem
cd ..
mkdir client
cd client
openssl genrsa -out client.key 1024 
openssl req -key client.key -new -out client.req
echo "$serial" > serial.srl
openssl x509 -req -in client.req -CA ../ca.pem -CAkey ../privkey.pem -CAserial serial.srl -out client.pem
