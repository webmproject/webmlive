#!/usr/bin/python
# -*- coding: utf-8 -*-

# Copyright (c) 2011 The WebM project authors. All Rights Reserved.
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

"""
TestServer - A SimpleHTTPServer based web server that handles POSTs from the
             webmlive encoder.
"""

import BaseHTTPServer
import cgi
import os
import SimpleHTTPServer
import SocketServer
import sys


FILENAME = 'test.webm'


class ThreadedHTTPServer(BaseHTTPServer.HTTPServer,
                         SocketServer.ThreadingMixIn):
    """
    BaseHTTPServer with concurrency support.
    """

class TestServer(SimpleHTTPServer.SimpleHTTPRequestHandler):
    """
    TestServer - SimpleHTTPRequestHandler that implements do_POST to provide a
                 target for webmlive's encoder.
    """

    def do_POST(self):  # pylint: disable=invalid-name
        """
        HTTP POST handler.
        """
        try:
            print '%s' % self.headers
            (content_type, param_dictionary) = \
                cgi.parse_header(self.headers['content-type'])

            # Strip everything but the last component when the client sent a
            # POST w/an x-content-id header that could be a full path.
            file_name = os.path.basename(self.headers['x-content-id'])
            if not file_name:
                self.send_response(400)
                self.end_headers()
                print 'Bad request: sanitized x-content-id value empty.'
                return

            if self.path.startswith('/dash'):
                post_file = open(file_name, 'wb')
                post_file.write(self.rfile.read(
                    int(self.headers['content-length'])))
                post_file.close()
                print 'wrote %s' % file_name
                self.send_response(200)
            else:
                # TODO(tomfinegan): read x-session-id and use that for the file
                # name here, and then get rid of |FILENAME|, because yuck.
                print self.path
                self.file = file(FILENAME, 'ab')
                if content_type == 'multipart/form-data':
                    query = cgi.parse_multipart(
                        self.rfile, param_dictionary)
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
        except Exception as exception:
            print 'In except: something is broken!'
            print 'type(exception): %s' % str(type(exception))
            print 'exception args: %s' % str(exception.args)
            print 'exception: %s' % str(exception)

def main():
    """
    Parses command line for a single argument: The port on which the HTTP server
    will listen for incoming requests. Then kicks off the server and runs until
    it dies or there's a keyboard interrupt.
    """
    try:
        if len(sys.argv) > 1:
            port = int(sys.argv[1])
        else:
            port = 8000

        if os.path.exists(FILENAME):
            print 'removed ' + FILENAME
            os.remove(FILENAME)

        httpd = ThreadedHTTPServer(('', port), TestServer)
        print 'Started TestServer on port {}.'.format(port)
        httpd.serve_forever()
    except KeyboardInterrupt:
        print 'Stopping TestServer...'
        httpd.socket.close()


if __name__ == '__main__':
    main()
