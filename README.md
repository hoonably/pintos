
# PintOS Project

UNIST CSE 311 Operating System

20201118 Jeonghoon Park  
20201032 Deokhyeon Kim  

<br>

## Setup

### Building the Docker Image

📁 **Make sure you are inside the `setting/` directory before building the image:**

```bash
cd setting
```

#### For amd64 (x86\_64) – Windows

```bash
sudo docker build -t pintos-image .
```

#### For ARM64 (Apple Silicon – macOS)

```bash
docker build --platform=linux/amd64 -t pintos-image .
```

<details><summary> ⚠️ Rosetta must be enabled for x86_64 emulation on Apple Silicon. </summary>

![Rosetta Setting](https://github.com/user-attachments/assets/b73e6e6e-b851-4611-82ce-3899333feb6e)

</details>

---

<br>

### Running the Container

📁 **Before running the container, move to the project root directory**
(so that the `pintos/` folder is mounted correctly):

```bash
cd ..
```

#### On amd64 (x86\_64) – Windows

```bash
sudo docker run -it -p 80:80 \
  -v $(pwd)/pintos:/root/pintos \
  --name pintos pintos-image
```

#### On ARM64 (Apple Silicon – macOS)

```bash
docker run --platform=linux/amd64 -it -p 80:80 \
  -v $(pwd)/pintos:/root/pintos \
  --name pintos pintos-image
```

---
<br>

### ⚠️ First-Time Setup (Install Bochs in the Container)

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

#### 🔁 Subsequent Runs

To start the container again after it has been created:

```bash
docker start -ai pintos
```

<br><br>

---

<br><br>

## 🧪 Project Test Results

Each project is implemented and tested on a separate branch.
To view and re-run the tests for each project, follow the instructions below.

---

### 📂 Project 1 – Threads (Partial Implementation)

Manual: [Pintos Project 1](https://web.stanford.edu/class/cs140/projects/pintos/pintos_2.html#SEC20)

This project involved modifying the thread scheduler and timer mechanisms in PintOS.
We implemented the **alarm clock** and **priority-based scheduling**, but **priority donation** was not included as it was optional.

#### 📌 Related Code

* `threads/thread.c`, `threads/thread.h` – Alarm list logic, priority-aware scheduling (`thread_set_priority()`, `thread_get_priority()`)
* `devices/timer.c` – Reimplemented `timer_sleep()` using sleep queue
* `lib/kernel/list.c` – Priority comparison for thread queue ordering

#### 🔧 Check

```bash
git checkout project1
docker start -ai pintos
```

```bash
cd /root/pintos/src/threads
make clean
make check
```

<details>
<summary>📋 Result</summary>

```bash
```

</details>


---

### 📂 Project 2 – User Programs (Full Implementation)

Manual: [Pintos Project 2](https://web.stanford.edu/class/cs140/projects/pintos/pintos_3.html#SEC32)

This project consisted of two parts:

* **2-1: Argument passing, system call framework, and basic file I/O**
* **2-2: Full implementation of user-level system calls**

The final submission includes both phases, with complete support for all required functionality and test cases.

#### 📌 Related Code

* `userprog/process.c` – Executable loading, argument stack setup, and child process tracking
* `userprog/syscall.c` – System call interface: `read`, `write`, `exec`, `wait`, `remove`, `filesize`, `seek`, `tell`, etc.
* `lib/user/syscall.c` – User-space syscall interface
* `lib/string.c`, `threads/thread.c` – String helpers, per-thread file descriptor table
* `filesys/file.c`, `filesys/inode.c` – Backend logic used by syscall layer for file operations

#### 🔧 Check

```bash
git checkout project2-2
docker start -ai pintos
```

```bash
cd /root/pintos/src/userprog
make clean
make check
```

<details>
<summary>📋 Result</summary>

```bash
```

</details>


---

### 📂 Project 3 – Virtual Memory (Partial Implementation)

Manual: [Pintos Project 3](https://web.stanford.edu/class/cs140/projects/pintos/pintos_4.html#SEC53)

This project required implementing virtual memory features such as supplemental page tables, demand paging, stack growth, and swapping.
Due to time constraints, only **a subset of core VM functionality** was implemented, focusing on demand paging and stack growth.

#### 📌 Related Code

* `vm/frame.c`, `vm/frame.h` – Frame table and eviction policy
* `vm/page.c`, `vm/page.h` – Supplemental page table, lazy loading, and memory tracking
* `vm/swap.c`, `vm/swap.h` – Swap disk interface and slot management
* `userprog/exception.c` – Page fault handler
* `userprog/process.c` – Stack growth and lazy segment loading

> 📌 All code in the `vm/` directory was written from scratch, as the directory is empty by default.

#### 🔧 Check

```bash
git checkout project3
docker start -ai pintos
```

```bash
cd /root/pintos/src/vm
make clean
make check
```

<details>
<summary>📋 Result</summary>

```bash
```

</details>


---

### 📂 Project 4 – File Systems (Partial Implementation)

Manual: [Pintos Project 4](https://www.scs.stanford.edu/10wi-cs140/pintos/pintos_5.html#SEC75)

This extra credit project focused solely on file growth features, omitting full file system extensions due to time constraints.

#### 📌 Related Code  
* `filesys/inode.c` – Indexed inode (direct + single indirect), dynamic growth, and partial free logic

#### 🔧 Check

```bash
git checkout project4
docker start -ai pintos
````

```bash
cd /root/pintos/src/filesys
make clean
make check
```

<details>
<summary>📋 Result</summary>

**7/7 tests pass**

```bash
pass tests/filesys/extended/grow-create
pass tests/filesys/extended/grow-file-size
pass tests/filesys/extended/grow-seq-lg
pass tests/filesys/extended/grow-seq-sm
pass tests/filesys/extended/grow-sparse
pass tests/filesys/extended/grow-tell
pass tests/filesys/extended/grow-two-files
```

total test
86/121 tests pass

</details>

---
