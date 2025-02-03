# 🗣️ Speak - A Simple TCP Chat Program  

## 📌 Overview  
**Speak** is a **half-duplex chat application** that allows a **client (`speak`)** and a **server (`speakd`)** to communicate over a TCP connection.

### 🔹 **How It Works**  
- The **client always speaks first**.  
- **Turns switch** when a participant sends `"x"`.  
- The **chat session ends** when either side sends `"xx"`.  

---

## 🆕 **Enhancements & Features**  

### 🎨 **Improved User Interface**  
- **Text Color Differentiation:**  
  - 🟢 **Green** → Client messages  
  - 🔵 **Blue** → System prompts (e.g., waiting for response)  
  - 🔹 **Cyan** → Server messages  

- **Formatted prompts for clarity:**  
  ```plaintext
  [INPUT] Your turn to speak (Type 'x' to end turn, 'xx' to quit):  
  [WAITING] Waiting for server response...  
  ```

### 🖥️ **Screen Management**  
- **Helper function (`clear_screen()`)** allows **clearing the terminal** for a fresh chat interface.  

### 🆘 **Help Command (Server-Side)**  
- **Server user can type `help`** to see available commands:  
  ```plaintext
  Commands:  
  x    - End your turn  
  xx   - Quit the chat  
  help - Show this message  
  ```

### 📜 **Chat Logging**  
- All messages are **saved to a file (`chat_log.txt`)** for **later review**.  

---

## ⚙️ **Installation & Compilation**  

### 🔨 **Compile & Clean**  
```bash
# Compile the program
make  

# Clean compiled binaries
make clean  
```

---

## 🚀 **How to Use**  

### 🖥️ **Start the Server (`speakd`)**  
Run the server on any machine:  
```bash
./speakd
```
Example:  
```bash
./speakd 8080
```

### 💻 **Start the Client (`speak`)**  
On a different machine, run:  
```bash
./speak <port> <server-ip>
```
Example (if server is at `192.168.1.10`):  
```bash
./speak 8080 192.168.1.10
```

---

## 💬 **Example Chat Session**  

```plaintext
[INFO] Client running on node localhost  
[INFO] Connecting to server at port 8080 on node 192.168.1.10  
[INFO] Server node details - Name: myserver, Address: 192.168.1.10  

[WAITING] Waiting for server response...  

[SERVER]: Hello Client  
[INPUT] Your turn to speak (Type 'x' to end turn, 'xx' to quit):  
> Hi Server!  

[WAITING] Waiting for server response...  
```

---

## 📖 **Commands List**  

| Command  | Description |
|----------|------------|
| `x`      | End current turn |
| `xx`     | Quit the chat |
| `help`   | Show available commands (server only) |
| `clear`  | Clear screen (server only) |

---

## 👨‍💻 **Developed By**  

👤 **Jiachuan Wu**  
📅 **Version 1.0**  
🏫 **Khoury College of Computer Sciences**  
