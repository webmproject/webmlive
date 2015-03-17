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
POSTCOUNT = 0

class WebMStreamServer(BaseHTTPRequestHandler):
  def do_POST(self):
    global POSTCOUNT
    try:
      ctype, pdict = cgi.parse_header(self.headers.getheader('content-type'))
      self.file = file(FILENAME, 'ab')
      if self.path.startswith("/dash"):
        # Hooooorible... terrrrrrible hack: post count is magic!
        if POSTCOUNT == 0:
          # this is the manifest
          print "manifest"
          print "%s" % self.headers
          mpd_file = open('webmlive.mpd', 'w')
          mpd_file.write(self.rfile.read(int(self.headers['content-length'])))
          mpd_file.close()
        elif POSTCOUNT == 1:
          # this is the hdr chunk
          print "header chunk"
          hdr_file = open('webmlive_webmlive.hdr', 'wb')
          hdr_file.write(self.rfile.read(int(self.headers['content-length'])))
          hdr_file.close()
        else:
          # this and all following chunks are media data
          print "media chunk"
          fname = "webmlive_webmlive_" + str(POSTCOUNT-1) + ".chk"
          chk_file = open(fname, 'wb')
          chk_file.write(self.rfile.read(int(self.headers['content-length'])))
          chk_file.close()
        self.send_response(200)
        POSTCOUNT += 1
        print "POSTCOUNT = " + str(POSTCOUNT)
      else:
        print self.path
        if ctype == 'multipart/form-data':
          query = cgi.parse_multipart(self.rfile, pdict)
          upfilecontent = query.get('webm_file')
          self.file.write(upfilecontent[0])
          self.send_response(200)
          self.wfile.write('Post OK')
        elif ctype == 'video/webm':
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
          self.send_response(500)
          self.wfile.write('Unsupported content-type')

        self.file.close()
        self.end_headers()
    except:
      print "you suck!"
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
