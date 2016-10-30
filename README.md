shhchat
=======
SHHH! It's a secret!

A Linux command-line multi-user chat program using SSL.

<h3>Project Status</h3>
Current: Beta

Next Milestone: 0.1 release

[![Build Status](https://travis-ci.org/triforce/shhchat.svg?branch=master)](https://travis-ci.org/triforce/shhchat)

<h3>Release Schedule</h3>

0.1

* A secure encrypted message stream between client, server and other clients.
* A simple key file will contain the encryption key used for messages.
* A simple user/password file managed by the server.
* SSL support.
* Example configuration files in conf.

<h3>Environment Setup</h3>

Both openssl devel and websocket libraries are required to compile shhchat.

<h5>Debian</h5>
<pre>sudo apt-get install libssl-dev libwebsockets-dev</pre>

<h3>Setting up SSL</h3>

shhchat supports SSL that requires a certificate and private key for both client and server.

There is now a script in the certs directory that will automatically generate the client and server certificates/keys for a basic setup.

<h3>Bare minimal setup without SSL</h3>

<pre>git clone https://github.com/triforce/shhchat</pre>
<pre>cd shhchat</pre>
<pre>make</pre>
<pre>cd build</pre>

The build directory contains a bare bones deployment with some example config. You can change the simple key in build/conf/key and the user's db in /build/conf/users.

Start the server:

<pre>./shhchatd</pre>

Connect with a client:

<pre>./shhclient 127.0.0.1</pre>

Login with one of the users specified in the example 'users' db file.

Once you are connected typing '??' will display a list of available chat commands.

<h3>Full setup with SSL</h3>
As above however you will need a key/certificate pair for the client and server.

The locations of these certificates can be set via a shhchat.conf file in /etc/shhchat/shhchat.conf. If running shhchat from source you will obviously need to create the shhchat directory.

The server will <b>always</b> read the certificate locations from the /etc/shhchat directory by default:
<pre>/etc/shhchat/key - Basic key</pre>
<pre>/etc/shhchat/shh_key.pem - SSL key</pre>
<pre>/etc/shhchat/shh_certificate.pem - SSL cert</pre>

The client will look for them in the current user's $HOME directory by default.
<pre>$HOME/.key - Basic key</pre>
<pre>$HOME/.shh_key - SSL key</pre>
<pre>$HOME/.shh_certificate - SSL cert</pre>

<h3>Contributing</h3>
I am on the lookout for people to test shhchat so give it a go and if necessary raise issues!

Pull requests are also appreciated.
