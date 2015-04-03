Weakforced
----------

Here is how it works:
 * Report successful logins via JSON http-api
 * Report unsuccessful logins via JSON http-api
 * Query if a login should be allowed to proceed, should be delayed, or ignored via http-api

Runtime console for querying the status of logins, IP addresses, subnets.

Policy in Lua.

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

Many more metrics are available to base decisions on. Some example code is in [wforce.conf].

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

Spec
----
We report 4 fields in the LoginTuple

 * login (string): the user name or number or whatever
 * remote (ip address, no power): the address the user arrived on
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

API Calls
---------
We can call 'report', 'allow' and (near future) 'clear', which removes
entries from a listed 'login' and/or 'remote'. 

