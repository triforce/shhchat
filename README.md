shhchat
=======
SHHH! It's a secret!

A Linux command-line multi-user chat program using SSL.

<h3>Project Status</h3>
Current: Beta

Next Milestone: 0.1 release

<h3>Release Schedule</h3>

0.1

* A secure encrypted message stream between client, server and other clients.
* A simple key file will contain the encryption key used for messages.
* A simple user/password file managed by the server.
* SSL support.
* Example configuration files in conf.

<h3>Setting up SSL</h3>

shhchat supports SSL that requires a certificate and private key for both client and server.

There is now a script in the certs directory that will automatically generate the client and server certificates/keys for a basic setup.

<h3>How to use</h3>

<h4>Bare minimal setup without SSL</h4>
<h5>Server</h5>
Create a directory in /etc named 'shhchat':

<pre>sudo mkdir /etc/shhchat</pre>

Create a file called 'key' in this newly created directory:

<pre>echo 123456 | sudo tee /etc/shhchat/key</pre>

Add a user:

<pre>echo user1: | sudo tee /etc/shhchat/users</pre>

The server will also check for the user db file in the conf directory where it is running locally.

Execute the server as root:

<pre>sudo shhchatd</pre>

<h5>Client</h5>
Create a file called 'key' in your user home directory with the same key as the server

<pre>echo 123456 > ~/.key</pre>

Connect to your server!

<pre>shhclient server_ip_address</pre>

<h4>Full setup with SSL</h4>
As above however you will need a key/certificate pair for the client and server.

The locations of these certificates can be set via the shhchat.conf file in /etc/shhchat/shhchat.conf.
