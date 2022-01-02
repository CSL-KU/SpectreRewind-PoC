#Step 1
#Dockerfile to build an image for different versions of chrome
#Pull ubuntu 16.04 base image
FROM ubuntu:16.04
 
#Step 2
#Essential tools and xvfb
RUN apt-get update && apt-get install -y \
    software-properties-common \
    unzip \
    curl \
    wget \
    xvfb
 
#Step 3
#Chrome browser to run the tests
# ARG CHROME_VERSION=65.0.3325.181
ARG CHROME_VERSION=62.0.3202.75
RUN curl https://dl-ssl.google.com/linux/linux_signing_key.pub | apt-key add \
      && wget https://www.slimjet.com/chrome/download-chrome.php?file=lnx%2Fchrome64_$CHROME_VERSION.deb \
      && dpkg -i download-chrome*.deb || true
RUN apt-get install -y -f \
      && rm -rf /var/lib/apt/lists/*
 
#Disable the SUID sandbox so that chrome can launch without being in a privileged container
RUN dpkg-divert --add --rename --divert /opt/google/chrome/google-chrome.real /opt/google/chrome/google-chrome \
        && echo "#! /bin/bash\nexec /opt/google/chrome/google-chrome.real --no-sandbox --disable-setuid-sandbox \"\$@\"" > /opt/google/chrome/google-chrome \
        && chmod 755 /opt/google/chrome/google-chrome

ENV DISPLAY :0
ADD spectreRewind.html /
ADD worker.js /
ADD start.sh /
RUN chmod +x /start.sh
CMD ["/start.sh"]
