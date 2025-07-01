
# PintOS Project

UNIST CSE 311 Operating System

20201118 Jeonghoon Park  
20201032 Deokhyeon Kim  

<br>

## Building the Docker Image

📁 **Make sure you are inside the `setting/` directory before building the image:**

```bash
cd setting
```

### For amd64 (x86\_64) – Windows

```bash
sudo docker build -t pintos-image .
```

### For ARM64 (Apple Silicon – macOS)

```bash
docker build --platform=linux/amd64 -t pintos-image .
```

<details><summary> ⚠️ Rosetta must be enabled for x86_64 emulation on Apple Silicon. </summary>

![Rosetta Setting](https://github.com/user-attachments/assets/b73e6e6e-b851-4611-82ce-3899333feb6e)

</details>

---

<br>

## Running the Container

📁 **Before running the container, move to the project root directory**
(so that the `pintos/` folder is mounted correctly):

```bash
cd ..
```

### On amd64 (x86\_64) – Windows

```bash
sudo docker run -it -p 80:80 \
  -v $(pwd)/pintos:/root/pintos \
  --name pintos pintos-image
```

### On ARM64 (Apple Silicon – macOS)

```bash
docker run --platform=linux/amd64 -it -p 80:80 \
  -v $(pwd)/pintos:/root/pintos \
  --name pintos pintos-image
```

---
<br>

## ⚠️ First-Time Setup (Install Bochs in the Container)

After the first run, enter the container and run:

```bash
cd /root/pintos/src/misc
env SRCDIR=/root/ PINTOSDIR=/root/pintos/ DSTDIR=/usr/local ./bochs-2.2.6-build.sh
cd ..
```

You only need to do this once, unless the container is deleted.

Once inside the container, you can exit by typing `exit` or pressing <kbd>Ctrl</kbd>+<kbd>D</kbd>.
This will automatically stop the container as long as no background processes are keeping it alive.

---

### 🔁 Subsequent Runs

To start the container again after it has been created:

```bash
docker start -ai pintos
```

---