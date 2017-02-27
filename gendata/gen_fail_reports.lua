# This lua script generates random reports for wforce logins

counter = 0
one_counter = 0
wrk.headers["Content-Type"] = "application/json"
wrk.headers["Authorization"] = "Basic d2ZvcmNlOnN1cGVy"
wrk.method = 'POST'
wrk.path = "/?command=report"

dp_access = {
   {protocol="http", device="Mozilla/5.0 (Macintosh; Intel Mac OS X 10_9_3) AppleWebKit/537.75.14 (KHTML, like Gecko) Version/7.0.3 Safari/7046A194A"},
   {protocol="https", device="Mozilla/5.0 (iPad; CPU OS 6_0 like Mac OS X) AppleWebKit/536.26 (KHTML, like Gecko) Version/6.0 Mobile/10A5355d Safari/8536.25"},
   {protocol="https", device="Mozilla/5.0 (Windows NT 6.1) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/41.0.2228.0 Safari/537.36"},
   {protocol="http", device="Mozilla/5.0 (Macintosh; Intel Mac OS X 10_10_1) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/37.0.2062.124 Safari/537.36"},
   {protocol="https", device="Mozilla/5.0 (Windows NT 6.1; WOW64; Trident/7.0; AS; rv:11.0) like Gecko"},
   {protocol="http", device="Mozilla/5.0 (compatible; MSIE 10.0; Windows NT 7.0; InfoPath.3; .NET CLR 3.1.40767; Trident/6.0; en-IN)"},
   {protocol="https", device="Mozilla/5.0 (Windows NT 6.1; WOW64; rv:40.0) Gecko/20100101 Firefox/40.1"},
   {protocol="imap", device='\\"name\\" \\"Bad Guy Mailer\\" \\"version\\" \\"10.0 (3226)\\" \\"os\\" \\"Mac OS X\\" \\"os-version\\" \\"10.12 (16A323)\\" \\"vendor\\" \\"Bad Guy Inc.\\"'}
}

ip_access = {
   "2.2.2.2",
   "3.3.3.3",
   "4.4.4.4",
   "5.5.5.5",
   "6.6.6.6",
   "7.7.7.7"
}

pwd_access = {
   "ijfds",
   "sokee",
   "sdefw",
   "93ked",
   "0kwepw",
   "ow3002",
   "2303wp",
   "2020ew",
   "309oka"
}

request = function()
   ip_mod = one_counter % table.getn(ip_access) + 1
   dp_mod = one_counter % table.getn(dp_access) + 1
   pwd_mod = counter % table.getn(pwd_access) + 1
   mybody = '{"login":"user' .. counter .. '", "remote":"' .. ip_access[ip_mod] .. '", "pwhash":"' .. pwd_access[pwd_mod] .. '", "success": false, "policy_reject":false, "protocol":"' .. dp_access[dp_mod].protocol .. '", "device_id":"' .. dp_access[dp_mod].device .. '" }'
   counter = counter + 10
   one_counter = one_counter + 1
   if (counter>1000)
        then
                counter = 0
        end
   return wrk.format(nil, nil, nil, mybody)
end
