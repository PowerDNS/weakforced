from wforce import app
from flask import request, abort, jsonify, config, make_response
from flask_elastic import Elastic
from flask_httpauth import HTTPBasicAuth
import copy
import elasticsearch
import pytz
from datetime import datetime
import logging
import logging.handlers
from logging.handlers import SysLogHandler

global_query = {
    "size": 0,
    "query": {
        "constant_score": {
            "filter": {
                "bool": {
                    "must": [
                    ]
                }
            }
        }
    }
}

app.config.from_pyfile('report.cfg')

auth = HTTPBasicAuth()
elastic = Elastic(app, timeout=app.config['ELASTICSEARCH_TIMEOUT'])
# Configure logging
handler = SysLogHandler(facility=SysLogHandler.LOG_DAEMON)
handler.setLevel(app.config['LOG_LEVEL'])
formatter = logging.Formatter(app.config['LOG_FORMAT'])
handler.setFormatter(formatter)
app.logger.addHandler(handler)
device_unique_attrs = app.config['DEVICE_UNIQUE_ATTRS']

def constructSearchTerms(j):
    query = []
    if 'login' in j:
        query.append({'term': { "login": j['login'] }})
    if 'ip' in j:
        query.append({'term': { "remote": j['ip'] }})
    if 'device' in j:
        for dattr in j['device']:
            query.append({'term': { dattr: j['device'][dattr] }})
    return query

def constructQuery(j, query):
    my_query = copy.deepcopy(query)
    my_query['query']['constant_score']['filter']['bool']['must'].extend(constructSearchTerms(j))

    max_age = "1w"
    if 'max_age' in j:
        max_age = j['max_age']
    my_query['query']['constant_score']['filter']['bool']['must'].append({
"range": { "@timestamp": { "gt": "now-"+max_age }}})
    max_num = 100
    if 'max_num' in j:
        max_num = j['max_num']
    my_query['size'] = max_num

    return my_query

def queryElastic(login_query, client_ip):
    try:
        response = elastic.search(index=app.config['ELASTICSEARCH_INDEX'],
                                  body=login_query,
                                  _source=True)
    except elasticsearch.TransportError as err:
        app.logger.error("Elasticsearch transport error: %s remote_ip=%s", err.error, client_ip)
        return None
    except elasticsearch.ElasticsearchException:
        app.logger.error("Elasticsearch Exception error remote_ip=%s", client_ip)
        return None
    return response

def filterDeviceAttrs(device_attrs):
    ret_attrs = {}
    for attr in device_attrs:
        if attr in device_unique_attrs:
            if device_attrs[attr] != "" and device_attrs[attr] != "Other":
                ret_attrs[attr] = device_attrs[attr]
    return ret_attrs

def getLoginObject(doc):
    login_obj = {}
    login_obj['id'] = doc['_id'] + "@" + doc['_index']
    source = doc['_source']
    if 'device_attrs' in source:
        login_obj['device_attrs'] = filterDeviceAttrs(source['device_attrs'])
    if 'device_id' in source:
        login_obj['device_id'] = source['device_id']
    if 'login' in source:
        login_obj['login'] = source['login']
    if 'remote' in source:
        login_obj['ip'] = source['remote']
    if 'protocol' in source:
        login_obj['protocol'] = source['protocol']
    if 'geoip' in source:
        if 'country_code2' in source['geoip']:
            login_obj['country_code'] = source['geoip']['country_code2']
        if 'country_name' in source['geoip']:
            login_obj['country_name'] = source['geoip']['country_name']
    if 't' in source:
        login_obj['login_datetime'] = datetime.fromtimestamp(source['t'], pytz.utc).isoformat()
    return login_obj

def makeLoginsResponse(es_response):
    if es_response is None:
        return None
    response = []
    if 'hits' in es_response and 'hits' in es_response['hits']:
        for doc in es_response['hits']['hits']:
            login_obj = getLoginObject(doc)
            response.append(login_obj)
    return response

def makeDevicesResponse(es_response):
    if es_response is None:
        return None
    response = []
    if 'hits' in es_response and 'hits' in es_response['hits']:
        for doc in es_response['hits']['hits']:
            source = doc['_source']
            login_obj = getLoginObject(doc)
            if 'device_attrs' in source:
                dup = False
                for (i, device) in enumerate(response):
                    dup = True
                    for attr in device_unique_attrs:
                        if not ((attr not in device['device_attrs'] and attr not in source['device_attrs']) or (attr in device['device_attrs'] and attr in source['device_attrs'] and device['device_attrs'][attr] == source['device_attrs'][attr])):
                            dup = False
                            break
                    if dup == True:
                        if datetime.fromtimestamp(source['t'], pytz.utc).isoformat() > device['login_datetime']:
                            response[i] = login_obj
                        break
                if dup == False:
                    response.append(login_obj)
            else:
                response.append(login_obj)
    return response

def getClientIP(environ):
    if 'HTTP_X_FORWARDED_FOR' in environ:
        return environ['HTTP_X_FORWARDED_FOR'].split(',')[-1].strip()
    else:
        return environ['REMOTE_ADDR']

@app.route('/logins', methods=['POST'])
@auth.login_required
def logins():
    client_ip = getClientIP(request.environ)
    if not request.json:
        return make_response(jsonify({'error_msg': 'Invalid query parameters'}), 400)
    if not 'login' in request.json:
        return make_response(jsonify({'error_msg': 'Invalid query parameters - must supply login'}), 400)  
    my_query = constructQuery(request.json, global_query)        
    es_response = queryElastic(my_query, client_ip)
    response = makeLoginsResponse(es_response)
    
    if response is None:
        return make_response(jsonify({'error_msg': 'Elasticsearch query failed'}), 500)
    else:
        app.logger.debug("/logins endpoint returning logins for login=%s remote_ip=%s", request.json['login'], client_ip)
        return make_response(jsonify({ "response": response, "query": request.json}), 200)

@app.route('/devices', methods=['POST'])
@auth.login_required
def devices():
    client_ip = getClientIP(request.environ)
    if not request.json:
        return make_response(jsonify({'error_msg': 'Invalid query parameters'}), 400)
    if not 'login' in request.json and not 'ip' in request.json:
        return make_response(jsonify({'error_msg': 'Invalid query parameters - must supply login'}), 400)
    my_query = constructQuery(request.json, global_query)
    es_response = queryElastic(my_query, client_ip)
    response = makeDevicesResponse(es_response)
    
    if response is None:
        return make_response(jsonify({'error_msg': 'Elasticsearch query failed'}), 500)
    else:
        login, ip = "",""
        if 'login' in request.json:
            login = request.json['login']
        if 'ip' in request.json:
            ip = request.json['ip']
        app.logger.debug("/devices endpoint returning devices for login=%s ip=%s remote_ip=%s", login, ip, client_ip)
        return make_response(jsonify({ "response": response, "query": request.json}), 200)

@app.route('/logins/confirm', methods=['POST'])
@auth.login_required
def confirm():
    client_ip = getClientIP(request.environ)
    if not request.json:
        return make_response(jsonify({'error_msg': 'Invalid query parameters'}), 400)
    if not 'id' in request.json or not 'confirm' in request.json:
        return make_response(jsonify({'error_msg': 'Invalid query parameters - must supply id and confirm'}), 400)

    user_confirmation = "none"
    if request.json['confirm'] == True:
        user_confirmation = "good"
    else:
        user_confirmation = "bad"

    id_index = request.json['id'].split("@")

    if len(id_index) != 2:
        return make_response(jsonify({'error_msg': 'Supplied id not in the correct format - should be xx@yyy'}))
    
    id = id_index[0]
    index = id_index[1]

    try:
        response = elastic.update(index=index,doc_type="wforce_report",id=id, body={ 'doc': { 'user_confirmation': user_confirmation }},refresh=True)
    except elasticsearch.TransportError as err:
        app.logger.error("Elasticsearch update exception (%s) trying to update doc id=%s in index=%s remote_ip=%s", err.error, id, index, client_ip)
        return make_response(jsonify({'error_msg': 'Elasticsearch update failed: %s'% err.error}), 500)
    except elasticsearch.ElasticsearchException:
        app.logger.error("Elasticsearch exception trying to update doc id=%s in index=%s remote_ip=%s", id, index, client_ip)
        return make_response(jsonify({'error_msg': 'Elasticsearch update failed'}), 500)

    if response['result'] == "updated" or response['result'] == "noop":
        app.logger.debug("Successfully updated doc id=%s in index=%s with user confirmation=%s remote_ip=%s", id, index, user_confirmation, client_ip)
        return make_response(jsonify(), 200)
    else:
        app.logger.error("Elasticsearch failed trying to update doc id=%s in index=%s remote_ip=%s", id, index, client_ip)
        return make_response(jsonify({'error_msg': 'Elasticsearch update failed'}), 500)
        
@auth.get_password
def get_password(username):
    if 'AUTH_PASSWORD' in app.config:
        return app.config['AUTH_PASSWORD']
    else:
        return None

@auth.error_handler
def unauthorized():
    client_ip = getClientIP(request.environ)
    app.logger.warning('Unauthorized access from remote_ip=%s', client_ip)
    return make_response(jsonify({'error_msg': 'Unauthorized access'}), 401)
