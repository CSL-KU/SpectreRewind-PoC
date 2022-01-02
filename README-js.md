# SpectreRewind JavaScript PoC

This repository contains a PoC for demonstrating the SpectreRewind attack in JavaScript. The code can be found at [spectreRewind.html](./spectreRewind.html)

## Prerequisites

We use Docker to safely luanch [Google Chrome version 62.0.3202.75](https://www.slimjet.com/chrome/download-chrome.php?file=lnx%2Fchrome64_62.0.3202.75.deb) (earlier versions also should work).

You can build a docker image with the provided the Dockerfile as follows.

```
$ docker build -t sprjs:chrome62 .
```
We tested it on ubuntu 20.04 with the docker version 20.10.12.

## Run

You can launch the docker image as follows. This will automatically launch the script within the browser.

```
$ XAUTH=/tmp/.docker.xauth
$ XSOCK=/tmp/.X11-unix
$ docker run -ti -v $XSOCK:$XSOCK -v $XAUTH:$XAUTH -v$PWD:/host -e XAUTHORITY=$XAUTH sprjs:chrome62
```
You then copy the experiment results shown in the browser screen and store them as hist.dat on the host.

```
$ cat > hist.dat
<-- paste the copied data
$ gnuplot plot.gnu > hist.pdf
```