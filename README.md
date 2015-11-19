Weakforced
----------
The goal of 'wforced' is to detect brute forcing of passwords across many
servers, services and instances.  In order to support the real world, brute
force detection policy can be tailored to deal with "bulk, but legitimate"
users of your service, as well as botnet-wide slowscans of passwords.

The aim is to support the largest of installations, providing services to
hundreds of millions of users.  The current version of weakforced is not
quit there yet.

Weakforced is a project by PowerDNS and Dovecot. For now, if you have any questions, email
bert.hubert@powerdns.com.

Here is how it works:
 * Report successful logins via JSON http-api
 * Report unsuccessful logins via JSON http-api
 * Query if a login should be allowed to proceed, should be delayed, or ignored via http-api

Runtime console for querying the status of logins, IP addresses, subnets.

wforced is aimed to receive message from services like:

 * IMAP
 * POP3
 * Webmail logins
 * FTP logins
 * Authenticated SMTP
 * Self-service logins
 * Password recovery services

By gathering failed and successful login attempts from as many services as
possible, brute forcing attacks can be detected and prevented more
effectively. 

Inspiration:
http://www.techspot.com/news/58199-developer-reported-icloud-brute-force-password-hack-to-apple-nearly-six-month-ago.html

Installing
----------
From GitHub:

```
$ git clone git@github.com:ahupowerdns/weakforced.git
$ cd weakforced
$ autoreconf -i
$ ./configure
$ make
```

This requires recent versions of libtool, automake and autoconf to be
installed.  Secondly, we require a recent g++ (4.8), Boost 1.40+, and Lua
5.1 development libraries.

To build on OS X, `brew install readline gcc` and use
`./configure LDFLAGS=-L/usr/local/opt/readline/lib CPPFLAGS=-I/usr/local/opt/readline/include CC=gcc-5 CXX=g++-5 CPP=cpp-5`

Policies
--------

There is a sensible default policy, and extensive support for crafting your own policies using
the insanely great Lua scripting language. 

Sample:

```
function allow(wfdb, lt)
	if(wfdb:countDiffFailuresAddress(lt.remote, 10) > 50)
	then
		return -1 -- BLOCK!
	end

	if(wfdb:countDiffFailuresAddressLogin(lt.remote, lt.login, 10) > 3)
	then
		return 3  -- must wait for 3 seconds 
	end

	return 0          -- OK!
end
```

Many more metrics are available to base decisions on. Some example code is in [wforce.conf](wforce.conf).

To report (if you configured with 'webserver("127.0.0.1:8084", "secret")'):

```
$ for a in {1..101}
  do 
    curl -X POST --data '{"login":"ahu", "remote": "127.0.0.1", "pwhash":"1234'$a'", "success":"false"}' \
    http://127.0.0.1:8084/?command=report -u wforce:secret
  done 
```

This reports 101 failed logins for one user, but with different password hashes.

Now to look up if we're still allowed in:

```
$ curl -X POST --data '{"login":"ahu", "remote": "127.0.0.1", "pwhash":"1234"}' \
  http://127.0.0.1:8084/?command=allow -u ahu:super
{"status": -1}
```

It appears we are not!

Console
-------
Available over TCP/IP, like this:
```
setKey("Ay9KXgU3g4ygK+qWT0Ut4gH8PPz02gbtPeXWPdjD0HE=")
controlSocket("0.0.0.0:4004")
```

Launch wforce as a daemon (`wforce --daemon`), to connect, run `wforce -c`.
Comes with autocomplete and command history. If you put an actual IP address
in place of 0.0.0.0, you can use the same config to listen and connect
remotely.

To get some stats, try:
```
> stats()
40 reports, 8 allow-queries, 40 entries in database
```

Spec
----
We report 4 fields in the LoginTuple

 * login (string): the user name or number or whatever
 * remote (ip address): the address the user arrived on
 * pwhash (string): a highly truncated hash of the password used
 * succes (boolean): was the login a success or not?

All are rather clear, but pwhash deserves some clarification. In order to
distinguish actual brute forcing of a password, and repeated incorrect but
identical login attempts, we need some marker that tells us if passwords are
different.

Naively, we could hash the password, but this would spread knowledge of
secret credentials beyond where it should reasonably be. Even if we salt and
iterate the hash, or use a specific 'slow' hash, we're still spreading
knowledge.

However, if we take any kind of hash and truncate it severely, for example
to 12 bits, the hash tells us very little about the password itself - since
one in 4096 random strings will match it anyhow. But for detecting multiple
identical logins, it is good enough.

For additional security, hash the login name together with the password - this
prevents detecting different logins that might have the same password.

NOTE: wforced does not require any specific kind of hashing scheme, but it
is important that all services reporting successful/failed logins use the
same scheme!

When in doubt, try:

```
TRUNCATE(SHA256(SECRET + LOGIN + '\x00' + PASSWORD), 12)
```

Which denotes to take the first 12 bits of the hash of the concatenation of
a secret, the login, a 0 bytes and the password.  Prepend 4 0 bits to get
something that can be expressed as two bytes.

API Calls
---------
We can call 'report', 'allow' and (near future) 'clear', which removes
entries from a listed 'login' and/or 'remote'.

To report, POST to /?command=report a JSON object with fields from the
LoginTuple as described above.

To request if a login should be allowed, POST to /?command=allow, again with
the LoginTuple. The result is a JSON object with a "status" field. If this is -1, do
not perform login validation (ie, provide no clue to the client of the password
was correct or not, or even if the account exists).

If 0, allow login validation to proceed. If a positive number, sleep this
many seconds until allowing login validation to proceed.

Load balancing: siblings
------------------------
For high-availability or performance reasons it may be desireable to run
multiple instances of wforced. To present a unified view of status however,
these instances then need to share the login tuples. To do so, wforce
implements a simple knowledge-sharing system.

Tuples received are broadcast (best effort, UDP) to all siblings. The
sibling list is parsed such that we don't broadcast messages to ourselves
accidentally, and can thus be identical across all servers.

To define siblings, use:

```
setKey("Ay9KXgU3g4ygK+qWT0Ut4gH8PPz02gbtPeXWPdjD0HE=")
addSibling("192.168.1.79")
addSibling("192.168.1.30")
addSibling("192.168.1.54")
siblingListener("0.0.0.0")
```

The first line sets the authentication and encryption key for our sibling
communications. To make your own key (recommended), run `makeKey()` on the
console and paste the output in all your configuration files.

This last line configures that we also listen to our other siblings (which
is nice).  The default port is 4001, the protocol is UDP.

To view sibling stats:

```
> siblings()
Address                             Sucesses  Failures     Note
192.168.1.79:4001                   18        7            
192.168.1.30:4001                   25        0            
192.168.1.54:4001                   0         0            Self
```


With this setup, several wforces are all kept in sync, and can be load
balanced behind for example haproxy, which incidentally can also offer SSL.
