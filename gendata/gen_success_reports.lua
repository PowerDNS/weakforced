# This lua script generates random reports for wforce logins

counter = 0
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
   {protocol="imap", device='\\"name\\" \\"iPhone Mail\\" \\"version\\" \\"14D27\\" \\"os\\" \\"iOS\\" \\"os-version\\" \\"10.2.1 (14D27)\\"'},
   {protocol="imap", device='\\"name\\" \\"iPad Mail\\" \\"version\\" \\"14D27\\" \\"os\\" \\"iOS\\" \\"os-version\\" \\"10.2.1 (14D27)\\"'},
   {protocol="imap", device='\\"name\\" \\"Mac OS X Mail\\" \\"version\\" \\"10.2 (3259)\\" \\"os\\" \\"Mac OS X\\" \\"os-version\\" \\"10.12.3 (16D32)\\" \\"vendor\\" \\"Apple Inc.\\"'},
   {protocol="imap", device='\\"name\\" \\"Mac OS X Notes\\" \\"version\\" \\"4.3.1 (698.50)\\" \\"os\\" \\"Mac OS X\\" \\"os-version\\" \\"10.12.3 (16D32)\\" \\"vendor\\" \\"Apple Inc.\\"'},
   {protocol="imap", device='\\"name\\" \\"Mac OS X accountsd\\" \\"version\\" \\"113 (113)\\" \\"os\\" \\"Mac OS X\\" \\"os-version\\" \\"10.12.3 (16D32)\\" \\"vendor\\" \\"Apple Inc.\\"'},
   {protocol="imap", device='\\"name\\" \\"Microsoft Outlook\\" \\"version\\" \\"14.0.7172.5000\\"'},
   {protocol="imap", device='\\"name\\" \\"Thunderbird\\" \\"version\\" \\"45.7.1\\"'},
   {protocol="imap", device='\\"name\\" \\"Icedove\\" \\"version\\" \\"38.5.0\\"'},
   {protocol="imap", device='vendor\\" \\"Microsoft\\" \\"os\\" \\"Windows Mobile\\" \\"os-version\\" \\"10.0\\" \\"guid\\" \\"31414643323441464239373433384242374438463641383146423942364639383931324435323232\\"'},
   {protocol="imap", device='\\"name\\" \\"com.google.android.gm\\" \\"os\\" \\"android\\" \\"os-version\\" \\"7.1.1; N4F26O\\" \\"vendor\\" \\"LGE\\" \\"x-android-device-model\\" \\"Nexus 5X\\" \\"x-android-mobile-net-operator\\" \\"Sonera AGUID\\" \\"XwzK66ekvB2DkNmD8EyAuwr8M4s\\"'},
   {protocol="imap", device='\\"name\\" \\"com.android.email\\" \\"os\\" \\"android\\" \\"os-version\\" \\"6.0; 32.1.F.1.67\\" \\"vendor\\" \\"Sony\\" \\"x-android-device-model\\" \\"SO-01H\\" \\"x-android-mobile-net-operator\\" \\"NTT DOCOMO\\" \\"AGUID\\" \\"QHaFdAshRmaLYCPM0KVo7lwajhE\\"'},
   {protocol="imap", device='\\"name\\" \\"com.samsung.android.email.provider\\" \\"os\\" \\"android\\" \\"os-version\\" \\"6.0.1; MMB29K\\" \\"vendor\\" \\"samsung\\" \\"x-android-device-model\\" \\"SM-G925F\\" \\"x-android-mobile-net-operator\\" \\"Sonera\\" \\"AGUID\\" \\"X9I/elUUlMNosGZjexmq6mp1zuA\\"'},
   {protocol="imap", device='\\"name\\" \\"com.sonymobile.email\\" \\"os\\" \\"android\\" \\"os-version\\" \\"6.0.1; 32.2.A.5.11\\" \\"vendor\\" \\"Sony\\" \\"x-android-device-model\\" \\"SGP771\\" \\"AGUID\\" \\"cJwASIogV2kpgCa3kPG+77TgCZk\\"'},
   {protocol="imap", device='\\"name\\" \\"eM Client for OX App Suite\\" \\"version\\" \\"6.0.28376.0\\" \\"GUID\\" \\"1\\"'}
}

ip_access = {
   "23.96.52.53",
   "86.128.243.146",
   "213.20.82.16",
   "212.223.44.11",
   "8.8.8.8",
   "2a03:b0c0:2:d0::4ab:8001",
   "2a00:1450:4009:80a::200e",
   "2a00:1450:4009:806::2005",
   "180.22.47.32",
   "52.48.64.3"
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
   ip_mod = counter % table.getn(ip_access) + 1
   dp_mod = counter % table.getn(dp_access) + 1
   pwd_mod = counter % table.getn(pwd_access) + 1
   mybody = '{"login":"user' .. counter .. '", "remote":"' .. ip_access[ip_mod] .. '", "pwhash":"' .. pwd_access[pwd_mod] .. '", "success": true, "policy_reject":false, "protocol":"' .. dp_access[dp_mod].protocol .. '", "device_id":"' .. dp_access[dp_mod].device .. '" }'
   counter = counter + 1
   if (counter==1000)
        then
                counter = 0
        end
   return wrk.format(nil, nil, nil, mybody)
end
