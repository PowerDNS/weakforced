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

Many more metrics are available to base decisions on.


