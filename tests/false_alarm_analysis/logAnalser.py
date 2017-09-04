#!/usr/bin/python

# Requirements
# requests (pip installl requests)

import requests
import json

osd_url = "http://192.168.0.3:8080"

logFileListUrl = "%s/logs" % osd_url

print "logFileListUrl = %s" % logFileListUrl

res = requests.get(logFileListUrl)
res.raise_for_status()

print res
print res.text

jsonObj = json.loads(res.text)

for rec in jsonObj['logFileList']:
    print rec



