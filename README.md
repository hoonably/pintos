
# PintOS Project

20201118 Jeonghoon Park  
20201032 Deokhyeon Kim  

<br>

## Dockerfile

```Dockerfile
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
```

---

<br>

## Building the Docker Image

### For amd64 (x86_64) – Windows

```bash
sudo docker build -t pintos .
```

### For ARM64 (Apple Silicon – macOS)

```bash
docker build --platform=linux/amd64 -t pintos .
```

<details><summary> ⚠️ **Rosetta must be enabled** for x86_64 emulation on Apple Silicon. </summary>

![Rosetta Setting](https://github.com/user-attachments/assets/b73e6e6e-b851-4611-82ce-3899333feb6e)
</details>

---

<br>

## Running the Container

### On amd64 (x86_64) – Windows

```bash
sudo docker run -it -p 80:80 \
  -v [your_pintos_root_dir]/pintos:/root/pintos \
  --name [container_name] [image_name]
```

### On ARM64 (Apple Silicon – macOS)

```bash
docker run --platform=linux/amd64 -it -p 80:80 \
  -v [your_pintos_root_dir]/pintos:/root/pintos \
  --name [container_name] [image_name]
```

---

<br>

## Subsequent Runs

To start the container again after the initial run:

```bash
docker start -ai [container_name]
```

Once inside the container, you can exit by typing `exit` or pressing <kbd>Ctrl</kbd>+<kbd>D</kbd>.  
This will automatically stop the container as long as no background processes are keeping it alive.

---
