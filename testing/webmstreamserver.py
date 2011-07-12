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
      ctype, pdict = cgi.parse_header(self.headers.getheader('content-type'))
      print ctype
      print pdict
      query = cgi.parse_multipart(self.rfile, pdict)
      upfilecontent = query.get('webm_file')
      self.file = file(FILENAME, 'ab')
      self.file.write(upfilecontent[0])
      self.file.close()
      self.send_response(301)
      self.wfile.write('Post Ok!')
      self.end_headers()
    except:
      pass


def main():
  # TODO(hwasoo) : to consider if there is a new connection.  
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
