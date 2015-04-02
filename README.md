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
		return -1
	end

	if(wfdb:countDiffFailuresAddressLogin(lt.remote, lt.login, 10) > 3)
	then
		return -1
	end

	return 0
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
