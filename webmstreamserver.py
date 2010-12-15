#!/usr/bin/python2.4
#
# Copyright 2010 Google Inc. All Rights Reserved.

"""This shows webm stream using http post request.

A detailed description of webmstreamserver.
"""

__author__ = 'hwasoolee@google.com (Hwasoo Lee)'

import cgi
import os.path

FILENAME = 'test.webm'

from BaseHTTPServer import BaseHTTPRequestHandler, HTTPServer

class WebMStreamServer(BaseHTTPRequestHandler):
  def do_POST(self):
    try:
      ctype, pdict = cgi.parse_header(self.headers.getheader('content-type'))
      print ctype
      print pdict
      query = cgi.parse_multipart(self.rfile, pdict)
      upfilecontent = query.get('send_webm_stream')
      self.send_response(301)
      self.end_headers()
      self.wfile.write('Post Ok!')
      self.file = file(FILENAME, 'ab')
      self.file.write(upfilecontent[0])
      self.file.close()
    except:
      pass


def main():
  # TODO(hwasoo) : to consider if there is a new connection.  
  try:
    server = HTTPServer(('', 8080), WebMStreamServer)
    print 'started streaming server...'
    if (os.path.exists(FILENAME)):
        os.remove(FILENAME)
    server.serve_forever()
  except KeyboardInterrupt:
    print '  shutting down server...'
    server.socket.close()


if __name__ == '__main__':
  main()
