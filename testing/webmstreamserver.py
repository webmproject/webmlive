#!/usr/bin/python2.4
# Copyright (c) 2011 The WebM project authors. All Rights Reserved.
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.
import cgi
import datetime
import os.path
import time

from BaseHTTPServer import BaseHTTPRequestHandler, HTTPServer

FILENAME = 'test.webm'

class WebMStreamServer(BaseHTTPRequestHandler):
  def do_POST(self):
    try:
      print "%s" % self.headers
      content_type, param_dictionary = cgi.parse_header(
          self.headers['content-type'])

      # Strip everything but the last component when the client sent a POST w/an
      # x-content-id header that could be a full path.
      file_name = os.path.basename(self.headers['x-content-id'])
      if not file_name:
        self.send_response(400)
        self.end_headers()
        print "Bad request: sanitized x-content-id value empty."
        return

      if self.path.startswith("/dash"):
        post_file = open(file_name, 'wb')
        post_file.write(self.rfile.read(int(self.headers['content-length'])))
        post_file.close()
        print "wrote %s" % file_name
        self.send_response(200)
      else:
        print self.path
        self.file = file(FILENAME, 'ab')
        if content_type == 'multipart/form-data':
          query = cgi.parse_multipart(self.rfile, param_dictionary)
          upfilecontent = query.get('webm_file')
          self.file.write(upfilecontent[0])
          self.send_response(200)
          self.wfile.write('Post OK')
        elif content_type == 'video/webm':
          length = int(self.headers['content-length'])
          if length > 0:
            self.file.write(self.rfile.read(length))
            self.send_response(200)
            self.wfile.write('Post OK')
          else:
            print 'post has 0 content-length (or is missing field)!'
            self.send_response(400)
            self.wfile.write('bad/missing content-length')
        else:
          print 'unsupported content-type, cannot handle POST!'
          self.send_response(400)
          self.wfile.write('Unsupported content-type')
          self.file.close()

        self.end_headers()
    except:
      print "In except: something is broken!"
      pass


def main():
  try:
    if os.path.exists(FILENAME):
     print "removed " + FILENAME
     os.remove(FILENAME)
    server = HTTPServer(('', 8000), WebMStreamServer)
    print 'started streaming server...'
    server.serve_forever()
  except KeyboardInterrupt:
    print '  shutting down server...'
    server.socket.close()


if __name__ == '__main__':
  main()
