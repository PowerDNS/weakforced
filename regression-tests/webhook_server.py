from bottle import route, run, template, request, response
import hmac
import hashlib
import base64
import json
import syslog

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
    print sdigest, secret
    if sdigest == secret:
        digest_match = True
    syslog.syslog(syslog.LOG_NOTICE, "Received webhook id=%s, digest_match=%s, event=%s, body=%s" % (hook_id, digest_match, event, json.dumps(body_json)))
    return "ok\n"

run(host='localhost', port=8080)
