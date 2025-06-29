# Interactive Shell (ishell)

`ishell` is a custom interactive shell written in C++ that emulates core functionalities of a Unix-like shell. It supports a variety of built-in commands, external command execution, input/output redirection, piping, background processes, signal handling, command history, and tab-based autocompletion. This README provides comprehensive instructions on building, running, and using the shell, along with details on all supported commands and features.

## Features
- **Built-in Commands**: `cd`, `pwd`, `echo`, `ls`, `pinfo`, `search`, `history`, `exit`
- **External Commands**: Execute any program available in the `PATH` environment variable
- **I/O Redirection**: Supports `<` (input), `>` (output overwrite), and `>>` (output append)
- **Piping**: Chain multiple commands using the `|` operator
- **Background Processes**: Run commands in the background with `&`
- **Signal Handling**: Handles `Ctrl-C` (SIGINT) and `Ctrl-Z` (SIGTSTP) for foreground processes
- **Command History**: Stores up to 20 commands, navigable with up/down arrow keys
- **Tab Autocompletion**: Autocompletes filenames in the current directory
- **Arithmetic Evaluation**: Supports arithmetic expressions within `echo` using `$((expression))`

## Prerequisites
- **Operating System**: Linux/Unix-based system (tested on Ubuntu)
- **Compiler**: g++ with C++11 support
- **Dependencies**: Standard C++ libraries and POSIX system calls (no external libraries required)

## Building the Shell
To build the `ishell` executable, follow these steps:

1. **Ensure Files are Present**:
   - `ishell.cpp`: The main source file containing the shell implementation
   - `Makefile`: For compiling the source code

2. **Compile the Code**:
   Run the following command in the directory containing `ishell.cpp` and `Makefile`:
   ```bash
   make
   ```
   This compiles `ishell.cpp` using g++ with C++11 standards and generates the `ishell` executable.

3. **Clean Up** (Optional):
   To remove the compiled executable, run:
   ```bash
   make clean
   ```

## Running the Shell
To start the shell, execute:
```bash
./ishell
```

This launches the interactive shell with a prompt in the format:
```
user@hostname:~/current/directory> 
```

The prompt is color-coded for readability (user in blue, `@` in red, hostname in green, directory in purple, `>` in yellow). Enter commands at the prompt, and press `Enter` to execute. To exit, type `exit` or press `Ctrl-D` when the input is empty.

## Supported Commands
Below is a comprehensive list of all commands and features supported by `ishell`.

### Built-in Commands
1. **cd [directory | ~ | -]**
   - **Description**: Changes the current working directory.
   - **Arguments**:
     - No argument or `~`: Changes to the user's home directory (from `HOME` environment variable).
     - `-`: Changes to the previous directory (tracked by the shell).
     - `[directory]`: Changes to the specified directory (absolute or relative path).
   - **Example**:
     ```bash
     cd /tmp
     cd ~
     cd -
     ```
     **Output** (on error):
     ```
     cd: No such file or directory
     ```

2. **pwd**
   - **Description**: Prints the absolute path of the current working directory.
   - **Supports Redirection**: Can redirect output to a file using `>` or `>>`.
   - **Example**:
     ```bash
     pwd
     pwd > output.txt
     ```
     **Output** (example):
     ```
     /home/user/project
     ```

3. **echo [arguments]**
   - **Description**: Prints arguments to standard output, with support for arithmetic expressions.
   - **Features**:
     - Prints arguments separated by spaces.
     - Evaluates arithmetic expressions within `$(( ))`, supporting `+`, `-`, `*`, `/`, and parentheses.
     - Supports output redirection with `>` or `>>`.
   - **Example**:
     ```bash
     echo hello world
     echo $((2 + 3 * 4))
     echo hello > output.txt
     ```
     **Output**:
     ```
     hello world
     14
     ```

4. **ls [-a] [-l] [directory ...]**
   - **Description**: Lists directory contents.
   - **Options**:
     - `-a`: Includes hidden files (starting with `.`).
     - `-l`: Long listing format, showing permissions, link count, owner, group, size, modification time, and name.
     - `[directory]`: Lists contents of specified directories (defaults to current directory).
   - **Features**:
     - Directories are displayed in cyan for distinction.
     - Entries are sorted (directories first, then alphabetically).
   - **Example**:
     ```bash
     ls
     ls -la /tmp
     ls dir1 dir2
     ```
     **Output** (for `ls -l` example):
     ```
     drwxr-xr-x 2 user group 4096 Oct 10 12:34 dir1
     -rw-r--r-- 1 user group  123 Oct 10 12:34 file1.txt
     ```

5. **pinfo [pid]**
   - **Description**: Displays process information for the specified PID or the shell's PID if none provided.
   - **Output**: Shows process state, virtual memory size (`VmSize`), and executable path from `/proc/[pid]/status` and `/proc/[pid]/exe`.
   - **Example**:
     ```bash
     pinfo
     pinfo 1234
     ```
     **Output** (example):
     ```
     State: S (sleeping)
     VmSize: 123456 kB
     Executable Path: /home/user/project/ishell
     ```

6. **search <filename>**
   - **Description**: Recursively searches for a file or directory in the current directory and its subdirectories.
   - **Output**: Prints `True` if found, `False` otherwise.
   - **Example**:
     ```bash
     search myfile.txt
     ```
     **Output**:
     ```
     True
     ```

7. **history [n]**
   - **Description**: Displays the last `n` commands from the command history (default is 10, max is 20).
   - **Example**:
     ```bash
     history
     history 5
     ```
     **Output** (example):
     ```
     cd /tmp
     ls -la
     echo hello
     pwd
     pinfo
     ```

8. **exit**
   - **Description**: Terminates the shell and restores terminal settings.
   - **Example**:
     ```bash
     exit
     ```

### External Commands
- **Description**: Commands not recognized as built-in are executed as external programs using `execv`.
- **Path Resolution**: The shell searches for executables in `/bin`, `/usr/bin`, `/usr/local/bin`, and directories in the `PATH` environment variable.
- **Features**:
  - Supports I/O redirection (`<`, `>`, `>>`).
  - Supports piping (`|`).
  - Supports background execution with `&`.
- **Example**:
  ```bash
  cat file.txt
  gcc -o program program.c
  sleep 10 &
  ```

### Input/Output Redirection
- **Operators**:
  - `<`: Redirects input from a file to the command.
  - `>`: Redirects command output to a file, overwriting it.
  - `>>`: Redirects command output to a file, appending to it.
- **Supported Commands**: Works with `pwd`, `echo`, and external commands.
- **Example**:
  ```bash
  echo hello world > output.txt
  cat < input.txt > output.txt
  ls -l >> dirlist.txt
  ```
- **Notes**:
  - Redirection is processed before command execution.
  - Errors (e.g., file not found) are reported to `stderr`.

### Piping
- **Description**: Chains multiple commands, where the output of one command is piped as input to the next using `|`.
- **Supported Commands**: Works with external commands and some built-ins (if redirected output is involved).
- **Example**:
  ```bash
  ls -l | grep txt
  cat file.txt | wc -l
  ```
- **Notes**:
  - Each command in the pipeline is executed in a separate process.
  - Piping is processed after splitting commands by `|` and handling redirections.

### Background Processes
- **Description**: Run commands in the background by appending `&` to the command.
- **Features**:
  - Displays the PID of the background process.
  - Background processes do not block the shell.
- **Example**:
  ```bash
  sleep 10 &
  ```
  **Output**:
  ```
  Process running in background with PID: 1234
  ```

### Signal Handling
- **Ctrl-C (SIGINT)**: Sends `SIGINT` to the foreground process, terminating it.
- **Ctrl-Z (SIGTSTP)**: Sends `SIGTSTP` to the foreground process, stopping it and printing its PID.
- **Child Process Handling**: Reaps terminated child processes using `SIGCHLD` to prevent zombies.
- **Example**:
  ```bash
  sleep 100
  # Press Ctrl-C to terminate
  # Press Ctrl-Z to stop, outputs:
  Process 1234 stopped
  ```

### Command History Navigation
- **Description**: Navigate through previous commands using arrow keys.
- **Features**:
  - **Up Arrow**: Shows previous commands from history.
  - **Down Arrow**: Shows next commands or clears input if at the end.
  - Commands are stored in `.shell_history` in the current directory (max 20 entries).
- **Example**:
  Press `Up Arrow` after running `ls` and `cd /tmp` to recall `cd /tmp`.

### Tab Autocompletion
- **Description**: Press `Tab` to autocomplete filenames in the current directory.
- **Features**:
  - Completes to the full filename if there is a single match.
  - Displays all matching filenames if multiple matches exist.
- **Example**:
  ```bash
  ls fi<Tab>
  ```
  - If `file.txt` is the only match, completes to `ls file.txt`.
  - If multiple files start with `fi` (e.g., `file1.txt`, `file2.txt`), lists them:
    ```
    file1.txt    file2.txt
    ```

### Arithmetic Evaluation in `echo`
- **Description**: Evaluates arithmetic expressions within `$(( ))` in the `echo` command.
- **Supported Operators**: `+`, `-`, `*`, `/`, and parentheses `()`.
- **Example**:
  ```bash
  echo $(( (2 + 3) * 4 ))
  ```
  **Output**:
  ```
  20
  ```
- **Notes**:
  - Division by zero or invalid expressions result in an error and shell termination.
  - Only integer arithmetic is supported.

## Command Syntax Notes
- **Multiple Commands**: Separate commands with `;` to execute sequentially.
  ```bash
  ls; pwd; echo hello
  ```
- **Whitespace**: Leading/trailing whitespace is trimmed from commands and tokens.
- **Quotes**: Arguments in single (`'`) or double (`"`) quotes have quotes removed during tokenization.
  ```bash
  echo "hello world"
  ```
  **Output**:
  ```
  hello world
  ```

## Usage Notes
- **Prompt**: Displays `user@hostname:~/current/directory>` with color-coded elements for clarity.
- **History File**: Stored as `.shell_history` in the directory where `ishell` is run. Requires write permissions.
- **Error Handling**:
  - Invalid commands or file access issues print errors to `stderr`.
  - Arithmetic errors in `echo` terminate the shell with an error message.
- **Limitations**:
  - No support for shell scripting, environment variable substitution, or advanced job control.
  - Autocompletion is limited to filenames in the current directory (no command completion).
  - Piping is supported but may not work seamlessly with all built-in commands.

## Troubleshooting
- **Compilation Errors**:
  - Ensure g++ is installed: `sudo apt install g++` (Ubuntu).
  - Verify C++11 support in your g++ version.
- **Permission Issues**:
  - Ensure executables in `PATH` have execute permissions.
  - Check write permissions for `.shell_history` in the current directory.
- **Terminal Issues**:
  - If the terminal behaves oddly after exiting (e.g., no echo), run:
    ```bash
    reset
    ```
- **Command Not Found**:
  - Ensure the external command exists in `PATH` or specify the full path (e.g., `/bin/ls`).
- **Signal Handling**:
  - Only foreground processes respond to `Ctrl-C`/`Ctrl-Z`. Background processes are not affected.

## Example Session
```bash
$ ./ishell
user@hostname:~/project> ls -la
drwxr-xr-x 2 user group 4096 Oct 10 12:34 .
drwxr-xr-x 5 user group 4096 Oct 10 12:00 ..
-rw-r--r-- 1 user group  123 Oct 10 12:34 ishell.cpp
user@hostname:~/project> echo $((2 + 3 * 4)) > result.txt
user@hostname:~/project> cat result.txt
14
user@hostname:~/project> ls | grep txt
result.txt
user@hostname:~/project> pinfo
State: S (sleeping)
VmSize: 123456 kB
Executable Path: /home/user/project/ishell
user@hostname:~/project> sleep 10 &
Process running in background with PID: 1234
user@hostname:~/project> history 3
ls -la
echo $((2 + 3 * 4)) > result.txt
cat result.txt
user@hostname:~/project> exit
```

## Contributing
Contributions are welcome! To contribute:
1. Fork the repository or create a local copy.
2. Add features, fix bugs, or improve documentation.
3. Submit pull requests or share patches.

