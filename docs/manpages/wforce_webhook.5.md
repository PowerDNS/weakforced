% WFORCE_WEBHOOK(5)
% Dovecot Oy
% 2016

# NAME
**wforce_webhook** - Documentation for wforce webhook functionality

# DESCRIPTION
The **wforce** daemon supports generating webhooks which run for
specific events. The list of supported events and the supported
configuration keys for those events are documented here. A brief
summary of the custom HTTP(S) POST headers is also included. For full
documentation of the WebHook HTTP API, please see
*wforce_webhook_api(7)*. 

# WEBHOOK EVENTS

The following events are available for generating webhooks:

* report - Runs each time a valid report command is received. The webhook
  includes the report request payload but not the report response. The
  following configuration keys are supported:

* allow - Runs each time a valid allow command is received. The
  webhook includes the request payload and the response payload. This
  hook can be filtered on the return value using the "allow_filter" 
  configuration key.

* reset - Runs each time a valid reset command is received. The
  webhook includes the request payload.

* addbl - Runs each time a blacklist entry is added (from Lua or the
  REST API). The webhook includes the request payload.

* delbl - Runs each time a blacklist entry is deleted (from Lua or the
  REST API). The webhook includes the request payload.

* expirebl - Runs each time a blacklist entry is expired. 

# WEBHOOK CONFIGURATION KEYS

The following configuration keys can be used for all events:

* url - The URL that the webhook should POST to. Supports http and
  https, for example:

		config_key["url"] = "https://example.com/foo"

* secret - A string that is used to generate the Hmac digest for the
  X-Wforce-Signature header. Should be unique for each webhook, so
  that the remote web server can verify the digest as being
  authentic. For example:

		config_key["secret"] = "12345"

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

# WEBHOOK CUSTOM HTTP HEADERS

The following custom headers will be added to the POST request for
each event:

* X-Wforce-Event - The name of the event, e.g. "report"

* X-Wforce-HookID - The ID of the WebHook (use "showWebHooks()" in the
  console to see the list of IDs.

* X-Wforce-Signature - If the "secret" configuration key is specified,
  this header will contain a base64-encoded HMAC-SHA256 digest of the POST body.

* X-Wforce-Delivery - A unique ID for this webhook. 

# FILES
*/etc/wforce.conf*

# SEE ALSO
wforce(1) wforce_webhook(5) wforce_webhook_api(7) wforce_api(7)
