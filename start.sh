#!/bin/bash
python3 -m http.server 80 &
/usr/bin/google-chrome http://localhost/spectreRewind.html
