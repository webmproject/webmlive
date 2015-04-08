#!/usr/bin/python
# Copyright (c) 2015 The WebM project authors. All Rights Reserved.
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
    pass

class TestServer(SimpleHTTPServer.SimpleHTTPRequestHandler):
    """
    TestServer - SimpleHTTPRequestHandler that implements do_POST to provide a
                 target for webmlive's encoder.
    """
    def send_response_and_end_headers(self, code, message=''):
        """
        Utility method for sending HTTP responses.
        """
        self.send_response(code)
        self.end_headers()
        if message:
            self.wfile.write(message)

    def do_POST(self):  # pylint: disable=invalid-name
        """
        HTTP POST handler.
        """
        message = ''
        response_code = 400
        try:
            print '%s' % self.headers
            (content_type, param_dictionary) = cgi.parse_header(
                self.headers['content-type'])

            # Strip everything but the last component when the client sent a
            # POST w/an x-content-id header that could be a full path.
            file_name = os.path.basename(self.headers['x-content-id'])
            if not file_name:
                response_code = 400
                message = 'Bad request: sanitized x-content-id value empty.'
                self.send_response_and_end_headers(response_code, message)
                print message
                return

            if self.path.startswith('/dash'):
                post_file = open(file_name, 'wb')
                post_file.write(self.rfile.read(
                    int(self.headers['content-length'])))
                post_file.close()
                message = 'wrote {}'.format(file_name)
                response_code = 200
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
                    response_code = 200
                elif content_type == 'video/webm':
                    length = int(self.headers['content-length'])
                    if length > 0:
                        self.file.write(self.rfile.read(length))
                        response_code = 200
                    else:
                        response_code = 200
                        message = 'POST has 0 content-length/missing field'
                        print message
                else:
                    response_code = 400
                    message = 'unsupported content-type, cannot handle POST!'
                    self.file.close()

            self.send_response_and_end_headers(response_code, message)

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
