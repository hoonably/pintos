FROM ubuntu:12.04

RUN sed -i -e 's/archive.ubuntu.com/old-releases.ubuntu.com/g' /etc/apt/sources.list
RUN sed -i -e 's/security.ubuntu.com/old-releases.ubuntu.com/g' /etc/apt/sources.list

RUN apt-get update && apt-get install -y \
	bash \
	vim \
	build-essential \
	gcc-4.4 \
	gcc-multilib \
	g++-4.4 \
	perl \
	wget \
	patch \
	libncurses5-dev \
	libx11-dev libxrandr-dev xorg-dev \
	make

RUN mv /usr/bin/gcc-4.4 /usr/bin/gcc
RUN mv /usr/bin/g++-4.4 /usr/bin/g++
COPY ./bochs-2.2.6.tar.gz /root/
RUN mkdir /root/pintos 
RUN echo 'export PATH="$PATH:/root/pintos/src/utils"' >> ~/.bashrc

CMD ["/bin/bash"]