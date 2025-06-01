## 🔧 Proxy Server with LRU Cache (C Project)

This is an **intermediate-level C project** that builds a multithreaded HTTP proxy server with support for **caching using the LRU (Least Recently Used)** policy. Designed to deepen your understanding of system-level I/O, networking, memory management, and concurrency, this project is a great stepping stone toward mastering systems programming in C.

---

### 🚀

Through this project, we achieve:

* 🔌 **Socket programming**: Understand how to create, bind, listen, and accept connections using low-level **TCP sockets**.
* 📥 **Low-level I/O system calls**: Master reading and writing directly from sockets using system-level APIs like `recv()`, `send()`, `read()`, and `write()`.
* 🧵 **Multithreading with pthreads**: Learn how to create and manage threads for handling concurrent client connections efficiently.
* 🔒 **Synchronization**: Explore the use of **mutex locks** and **semaphores** to handle shared resources and control access.
* 💾 **LRU Cache Design**: Implement a time-based **Least Recently Used cache** that automatically evicts old data when memory is full.
* 🧠 **Dynamic memory management**: Allocate and deallocate memory on the heap safely using `malloc()`, `free()`, and proper pointer handling.

---

### 📂 Project Structure

```
├── proxy_server_with_cache.c   # Core server logic
├── proxy_parse.c               # HTTP request parser
├── proxy_parse.h               # Parser header file
├── Makefile                    # Build instructions
├── README.md                   # Project documentation
```

---

### 🧪 How It Works

* The proxy listens for incoming HTTP requests on a configurable port.
* It parses requests and checks if a cached version exists.
* If not cached, it forwards the request to the target server.
* Response data is cached using an **LRU time-based policy**.
* Supports concurrent client handling via threads with semaphore control.

---

### 🔧 How to Build

```bash
make
```

Or compile manually:

```bash
gcc proxy_server_with_cache.c proxy_parse.c -o proxy_server_with_cache -lpthread
```

---

### ▶️ How to Run

```bash
./proxy_server_with_cache 8081
```

Then use `curl` or a browser (with proxy settings) to test:

```bash
curl -x http://localhost:8081 yourlink
```

---

### 📌 Notes

* This proxy only supports **HTTP GET** requests (not HTTPS).
* Designed for educational purposes; not production-ready.

---

### 🧠 Future Enhancements

* Add HTTPS support via the `CONNECT` method
* Cache size configuration via command-line args
* Logging system for analytics
* IP-based access control or rate limiting

---

### 🛠 Requirements

* GCC or Clang compiler
* POSIX-compatible system (Linux, macOS) or Windows with MinGW/WSL
* Basic understanding of threads, sockets, and memory

---

### 📜 
G.vamshi
