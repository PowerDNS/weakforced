from bottle import route, run, template, request, response
import hmac
import hashlib
import base64
import json
import os

logfile = open('/tmp/webhook-server.log', 'w', 0)
logfile.write("foo\n")
mypid = os.getpid()

@route('/webhook/<event>', method='POST')
def webhook(event):
    digest_match = False
    event = request.headers.get('X-Wforce-Event')
    secret = request.headers.get('X-Wforce-Signature')
    delivery_id = request.headers.get('X-Wforce-Delivery')
    hook_id = request.headers.get('X-Wforce-HookID')
    body_json = request.json
    shmac = hmac.new('secret', '', hashlib.sha256)
    for myline in request.body.readlines():
        shmac.update(myline)
    sdigest = base64.b64encode(shmac.digest())
    if sdigest == secret:
        digest_match = True
    log_msg = "[%s] Received webhook id=%s, digest_match=%s, event=%s, body=%s\n" % (mypid, hook_id, digest_match, event, json.dumps(body_json))
    logfile.write(log_msg)
    return "ok\n"

run(host='localhost', port=8080)
