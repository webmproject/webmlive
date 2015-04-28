Building

Tools needed:
- cmake v2.8 or higher.
- Microsoft Visual Studio 2013
  - Newer versions should be fine provided cmake includes a generator.
  - Older versions may not work because webmlive/encoder uses C++ 11 features.

Run cmake from the directory of your choice. Pass it the path to the encoder
directory of the webmlive repository. This will produce ENCODER.sln. Open it
to use the IDE, or pass it directly to msbuild to build the encoder.


Basic live streaming using dash.js WebM support

Requires:
- dash.js (specifically the webm stuff in the contrib sub dir of its repo)
- python and the RangeHTTPServer module
  - hint for people unfamiliar with python: install pip, then:
    $ pip install rangehttpserver
  - or, should the above not work, you might have to run:
    $ python -m pip install rangehttpserver

Step 1: Setup webmlive/testing/test_server.py:
  $ cd some/directory/for/encoder/output
  $ python path/to/webmlive/testing/test_server.py

Step 2: Run RangeHTTPServer:
  $ cd encoder/output/directory/from/step/1
  $ python -m RangeHTTPServer

Step 3: Copy webmlive.html to the serving directory:
  $ cp webmlive/testing/webmlive.html directory/from/step/1

Step 4: Copy dash.webm.js to the serving directory:
  $ cp path/to/dash.js.git/contrib/webmjs/dash.webm.debug.js dir/from/step/1

Step 5: Run encoder:
  $ cd some/directory/for/local/encoder/output
  $ webmlive/encoder.exe --url localhost:8001/dash --dash_name webmlive

Step 6: Watch your stream:
  Run a web browser that supports DASH playback and then open the page:
    localhost:8000/webmlive.html
