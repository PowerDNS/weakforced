% WFORCE_WEBHOOK(5)
% Dovecot Oy
% 2017

# NAME
**wforce_webhook** - Documentation for wforce webhook functionality

# DESCRIPTION
The **wforce** daemon supports generating webhooks which run for
specific events. The list of supported events, example payload for
those events, and the supported configuration keys for those events
are documented here. A summary of the custom HTTP(S) POST headers is
also included.

# WEBHOOK EVENTS

See **wforce.conf(5)** for details of the "addWebHook()" configuration
setting. The following events are available for generating webhooks:

* report - Runs each time a valid report command is received. The webhook
  includes the report request payload but not the report response. An
  example payload for the HTTP post is shown below:

		{"remote": "1.4.3.1", "success": false, "policy_reject":
		false, "attrs": {"cos": "basic"},
		"login": "webhooktest@foobar.com", "pwhash": "1234",
		"t": 1472718468.412865}

* allow - Runs each time a valid allow command is received. The
  webhook includes the request payload and the response payload. This
  hook can be filtered on the return value using the "allow_filter" 
  configuration key. An example payload for the HTTP post is shown below:

		{"request": {"remote": "1.4.3.2", "success": false,
		"policy_reject": true, "attrs": {},
		"login": "webhooktest@foobar.com", "pwhash": "1234",
		"t": 1472718469.827362}, "response": {"msg": "", "status": 0}}

* reset - Runs each time a valid reset command is received. The
  webhook includes the request payload. An example payload for the
  HTTP post is shown below:

		{"ip": "1.4.3.3", "login": "webhooktest@foobar.com"}

* addbl - Runs each time a blacklist entry is added (from Lua or the
  REST API). The webhook includes the request payload. An example
  payload for the HTTP post is shown below:

		{"key": "webhooktest@foobar.com",
		"reason": "Too many different bad password attempts",
		"expire_secs": 10, "bl_type": "login_bl"}

* delbl - Runs each time a blacklist entry is deleted (from Lua or the 
  REST API). The webhook includes the request payload. An example
  payload for the HTTP post is shown below:

		{"key": "1.4.3.3:webhooktest@foobar.com",
		"bl_type": "ip_login_bl"}

* expirebl - Runs each time a blacklist entry is expired. An example
  payload for the HTTP post is shown below:

		{"key": "webhooktest@foobar.com", "bl_type": "login_bl"}

# CUSTOM WEBHOOKS

See **wforce.conf(5)** for details of the "addCustomWebHook()" configuration
setting. This enables custom webhooks to be generated from Lua. 

# WEBHOOK CONFIGURATION KEYS

See **wforce.conf(5)** for details of the "addWebHook()" configuration
setting. The following configuration keys can be used for all events:

* url - The URL that the webhook should POST to. Supports http and
  https, for example:

		config_key["url"] = "https://example.com/foo"

* secret - A string that is used to generate the Hmac digest for the
  X-Wforce-Signature header. Should be unique for each webhook, so
  that the remote web server can verify the digest as being
  authentic. For example:

		config_key["secret"] = "12345"

* num_conns - This configuration key is now deprecated - setting it
  will have no effect. Use "setNumWebHookConnsPerThread()"
  configuration setting instead (see **wforce.conf(5)**).

* basic-auth - Adds an Authorization header to Webhooks, for servers
  which expect Basic Authentication. The username and password are
  provided as "user:pass". For example

		config_key["basic-auth"] = "wforce:super"

* api-key - Adds an X-API-Key header to Webhooks, for servers that
  expect such a header for Authorization. For example:

        config_key["api-key"] = "myapikeysecret"

The following configuration keys are custom to specific events:

* allow_filter - Filters allow webhooks based on the allow response
  status. If not specified, the webhook runs for all allow
  events. Possible values are: "reject", "allow" and
  "tarpit". The values are OR'd together, so specifying all values
  will have the same effect as not specifying allow_filter. For
  example: 

		config_key["allow_filter"] = "allow reject"
		config_key["allow_filter"] = "tarpit allow"
		config_key["allow_filter"] = "reject"

* content-type - Only custom webhooks can set a custom
  content-type. Doing so will set the HTTP Content-Type header to the
  value specified in this key, so ensure it is a valid MIME
  Content-Type. For example:

		config_key["content-type"] = "text/plain"

# WEBHOOK CUSTOM HTTP HEADERS

The following custom headers will be added to the POST request for
each event:

* X-Wforce-Event - The name of the event, e.g. "report"

* X-Wforce-HookID - The ID of the WebHook (use "showWebHooks()" in the
  console to see the list of IDs.

* X-Wforce-Signature - If the "secret" configuration key is specified,
  this header will contain a base64-encoded HMAC-SHA256 digest of the POST body.

* X-Wforce-Delivery - A unique ID for this webhook.

# WEBHOOK HTTP Content-Type HEADER

The HTTP Content-Type header defaults to application/json, and cannot
be changed for normal webhooks. Custom webhooks however can change
it using the "content-type" configuration key.

# FILES
*/etc/wforce.conf*

# SEE ALSO
wforce(1) wforce_webhook(5) wforce_api(7)
