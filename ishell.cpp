#include <iostream>
#include <sstream>
#include <vector>
#include <list>
#include <string>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <pwd.h>
#include <grp.h>
#include <ctime>
#include <algorithm>
#include <termios.h>
#include <signal.h>
#include <errno.h>
#include <limits.h>
#include <cctype>       // for isdigit(), isspace() 

using namespace std;

// --- Color Codes ---
#define COLOR_BLUE     "\033[34m"  
#define COLOR_RED      "\033[31m"  
#define COLOR_GREEN    "\033[32m"  
#define COLOR_YELLOW   "\033[33m"  
#define COLOR_PURPLE   "\033[35m"  
#define COLOR_CYAN     "\033[36m"  
#define COLOR_RESET    "\033[0m"   

// --- Global Variables ---

// History file aur maximum history size
const string HISTORY_FILE = ".shell_history";
const size_t MAX_HISTORY_SIZE = 20;
list<string> history;              // command history ki list
vector<string> historyVector;      // vector mein history (for easy access)
string prevDirectory;              // last directory remember karne ke liye
volatile pid_t fg_pid = 0;         // current foreground process id
struct termios orig_termios;       

// --- Function Declarations ---
void loadHistory(list<string>& history);
void saveHistory(const list<string>& history);
void addCommandToHistory(list<string>& history, const string &command);
string getUser();
string getSystemName();
string getCurrentDirectory();
void printPrompt();
string readInput();
vector<string> tokenize(const string &str, const char *delim);
void processRedirection(vector<string> &tokens, int &inputFd, int &outputFd);

void handleCd(const vector<string> &tokens);
void handlePwd(const vector<string>& tokens);
void handleEcho(const vector<string> &tokens);
void handleLs(const vector<string> &tokens);
void handlePinfo(const vector<string> &tokens);
void handleSearch(const vector<string> &tokens);
void handleHistory(const vector<string> &tokens);
bool searchRecursive(const char *basePath, const string &target);

void executeExternalCommand(vector<string> tokens, bool background);
void executePipedCommands(vector<string> pipedCommands);
void executeCommand(string command);
void setNonCanonicalMode();
void resetTerminal();
string findExecutablePath(const string &cmd);

// Arithmetic evaluator prototypes
long long parseExpression(const string &s, size_t &i);
long long parseTerm(const string &s, size_t &i);
long long parseFactor(const string &s, size_t &i);
long long evaluateArithmetic(const string &expr);

// --- Signal Handlers ---
void sigintHandler(int sig);
void sigtstpHandler(int sig);
void sigchldHandler(int sig);

// ===================== Signal Handlers Implementation =====================

// Ctrl-C ke liye handler. Yeh foreground process ko terminate karta hai.
void sigintHandler(int sig) {
    (void)sig;
    if (fg_pid != 0) {
        kill(fg_pid, SIGINT);
        fg_pid = 0;
    }
}

// Ctrl-Z ke liye handler. Yeh foreground process ko stop karta hai.
void sigtstpHandler(int sig) {
    (void)sig;
    if (fg_pid != 0) {
        kill(fg_pid, SIGTSTP);
        cout << "\n" << COLOR_YELLOW << "Process " << fg_pid << " stopped" 
            << COLOR_RESET << "\n";
        fg_pid = 0;
    }
}

// Child process termination ko handle karta hai.
void sigchldHandler(int sig) {
    (void)sig;
    int status;
    while (waitpid(-1, &status, WNOHANG) > 0) {}
}

// ===================== Terminal Settings =====================
// Non-canonical mode mein set karta hai for char-by-char input.
void setNonCanonicalMode() {
    tcgetattr(STDIN_FILENO, &orig_termios);
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ICANON | ECHO); 
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
}

// Terminal settings ko wapas original state mein le jane ke liye.
void resetTerminal() {
    tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
}

// ===================== History Functions =====================

void loadHistory(list<string>& history) {
    ifstream infile(HISTORY_FILE);
    if (infile.is_open()) {
        string line;
        while (getline(infile, line)) {
            if (history.size() >= MAX_HISTORY_SIZE)
                history.pop_front();
            history.push_back(line);
            historyVector.push_back(line);
        }
        infile.close();
    }
}

// History file ko update karta hai.
void saveHistory(const list<string>& history) {
    ofstream outfile(HISTORY_FILE, ios::trunc);
    if (outfile.is_open()) {
        for (const auto &cmd : history)
            outfile << cmd << "\n";
        outfile.close();
    }
}

void addCommandToHistory(list<string>& history, const string &command) {
    if (command.empty())
        return;
    if (history.size() >= MAX_HISTORY_SIZE)
        history.pop_front();
    history.push_back(command);
    historyVector.push_back(command);
    if (historyVector.size() > MAX_HISTORY_SIZE)
        historyVector.erase(historyVector.begin());
    saveHistory(history);
}

// ===================== Prompt and Input Functions =====================

string getUser() {
    char *user = getenv("USER");
    return user ? string(user) : "unknown";
}

// System ka hostname return karta hai.
string getSystemName() {
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == 0)
        return string(hostname);
    return "unknown";
}

string getCurrentDirectory() {
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) != nullptr)
        return string(cwd);
    return "";
}

void printPrompt() {
    string user = getUser();
    string sys = getSystemName();
    string cwd = getCurrentDirectory();
    const char *home = getenv("HOME");
    if (home && cwd.find(home) == 0)
        cwd = "~" + cwd.substr(strlen(home));
    cout << COLOR_BLUE << user << COLOR_RED << "@" << COLOR_GREEN << sys
        << COLOR_RESET << ":" << COLOR_PURPLE << cwd << COLOR_YELLOW << "> " 
        << COLOR_CYAN;
    cout.flush();
}

// ===================== readInput() with Autocomplete & History =====================

// Function arrow keys, TAB, Ctrl-D handle karta hai.
string readInput() {
    string input;
    size_t historyIndex = historyVector.size(); 
    printPrompt();
    int c;
    while ((c = getchar()) != EOF) {
        if (c == '\n') {
            cout << COLOR_RESET << "\n";
            break;
        }
        if (c == 27) {                      // Arrow keys ke liye escape seq
            int seq1 = getchar();
            int seq2 = getchar();
            if (seq1 == '[') {
                if (seq2 == 'A') {          // UP arrow 
                    if (!historyVector.empty() && historyIndex > 0) {
                        historyIndex--;
                        input = historyVector[historyIndex];
                        cout << "\r\033[K"; // line clear karo
                        printPrompt();
                        cout << input;
                    }
                } else if (seq2 == 'B') {   // DOWN arrow 
                    if (!historyVector.empty() && historyIndex < historyVector.size() - 1) {
                        historyIndex++;
                        input = historyVector[historyIndex];
                        cout << "\r\033[K";
                        printPrompt();
                        cout << input;
                    } else {
                        // Sabse recent history pe pohonch gaye, to input clear karo
                        historyIndex = historyVector.size();
                        input = "";
                        cout << "\r\033[K";
                        printPrompt();
                    }
                }
            }
            continue;
        } else if (c == '\t') {
            size_t pos = input.find_last_of(" ");
            string currentToken = (pos == string::npos) ? input : input.substr(pos + 1);
            string dirPath, filePrefix;
            size_t slashPos = currentToken.rfind('/');
            if (slashPos != string::npos) {
                // Agar directory part mil jaye
                dirPath = currentToken.substr(0, slashPos + 1);
                filePrefix = currentToken.substr(slashPos + 1);
            } else {
                // Nahi to current directory ka use karo
                dirPath = ".";
                filePrefix = currentToken;
            }
            vector<string> matches;
            DIR *dp = opendir(dirPath.c_str());
            if (dp) {
                struct dirent *entry;
                while ((entry = readdir(dp)) != nullptr) {
                    string fname(entry->d_name);
                    if (fname.find(filePrefix) == 0)
                        matches.push_back(fname);
                }
                closedir(dp);
            }
            if (matches.size() == 1) {
                string completion = matches[0].substr(filePrefix.size());
                input += completion;
                cout << completion;
            } else if (matches.size() > 1) {
                sort(matches.begin(), matches.end());
                cout << "\n";
                for (const auto &m : matches)
                    cout << m << "    ";
                cout << "\n";
                printPrompt();
                cout << input;
            }
            continue;
        } else if (c == 127 || c == 8) {    // Backspace key
            if (!input.empty()) {
                input.pop_back();
                cout << "\b \b";
            }
            continue;
        } else if (c == 4) {                // Ctrl-D: exit if no input
            if (input.empty()) {
                cout << "\n";
                exit(0);
            }
            break;
        } else {
            input.push_back((char)c);
            cout << (char)c;
        }
    }
    return input;
}

// ===================== Tokenization Helper =====================

// Tokenize string based on delimiter
vector<string> tokenize(const string &str, const char *delim) {
    vector<string> tokens;
    char *cstr = new char[str.size() + 1];
    strcpy(cstr, str.c_str());
    char *saveptr;
    char *token = strtok_r(cstr, delim, &saveptr);
    while (token != nullptr) {
        string t(token);
        // token quotes mein hai, to unhe hata do
        if (t.size() >= 2 && ((t.front() == '"' && t.back() == '"') ||
            (t.front() == '\'' && t.back() == '\'')))
            t = t.substr(1, t.size() - 2);
        tokens.push_back(t);
        token = strtok_r(nullptr, delim, &saveptr);
    }
    delete[] cstr;
    return tokens;
}

// ===================== findExecutablePath Helper =====================

string findExecutablePath(const string &cmd) {
    string trimmed = cmd;
    size_t start = trimmed.find_first_not_of(" \t");
    size_t end = trimmed.find_last_not_of(" \t");
    if (start != string::npos && end != string::npos)
        trimmed = trimmed.substr(start, end - start + 1);
    else
        trimmed = "";
    if (trimmed.empty())
        return "";
    if (trimmed.find('/') != string::npos)
        return trimmed;
    vector<string> commonDirs = {"/bin", "/usr/bin", "/usr/local/bin"};
    for (const auto &dir : commonDirs) {
        string fullPath = dir + "/" + trimmed;
        if (access(fullPath.c_str(), X_OK) == 0) {
            if (fullPath.find("Xorg.wrap") != string::npos)
                continue;
            return fullPath;
        }
    }
    char *pathEnv = getenv("PATH");
    if (pathEnv) {
        string pathStr(pathEnv);
        vector<string> paths = tokenize(pathStr, ":");
        for (const auto &dir : paths) {
            string fullPath = dir + "/" + trimmed;
            if (access(fullPath.c_str(), X_OK) == 0) {
                if (fullPath.find("Xorg.wrap") != string::npos)
                    continue;
                return fullPath;
            }
        }
    }
    return "";
}

// ===================== I/O Redirection Helper =====================

// I/O redirection operators (<, >, >>) ko process karta hai
void processRedirection(vector<string> &tokens, int &inputFd, int &outputFd) {
    inputFd = -1;
    outputFd = -1;
    vector<string> newTokens;
    for (size_t i = 0; i < tokens.size(); i++) {
        if (tokens[i] == "<") {
            if (i + 1 < tokens.size()) {
                inputFd = open(tokens[i + 1].c_str(), O_RDONLY);
                if (inputFd < 0)
                    perror("open input");
                i++;
            } else {
                cerr << "No input file specified\n";
            }
        } else if (tokens[i] == ">") {
            if (i + 1 < tokens.size()) {
                outputFd = open(tokens[i + 1].c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (outputFd < 0)
                    perror("open output");
                i++;
            } else {
                cerr << "No output file specified\n";
            }
        } else if (tokens[i] == ">>") {
            if (i + 1 < tokens.size()) {
                outputFd = open(tokens[i + 1].c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
                if (outputFd < 0)
                    perror("open output");
                i++;
            } else {
                cerr << "No output file specified\n";
            }
        } else {
            newTokens.push_back(tokens[i]);
        }
    }
    tokens = newTokens;
}

// ===================== Arithmetic Evaluator =====================

// recursive-descent parser arithmetic expressions evaluate karta hai
long long parseExpression(const string &s, size_t &i) {
    long long result = parseTerm(s, i);
    while (i < s.size()) {
        if (s[i] == '+') {
            i++;
            result += parseTerm(s, i);
        } else if (s[i] == '-') {
            i++;
            result -= parseTerm(s, i);
        } else {
            break;
        }
    }
    return result;
}

long long parseTerm(const string &s, size_t &i) {
    long long result = parseFactor(s, i);
    while (i < s.size()) {
        if (s[i] == '*') {
            i++;
            result *= parseFactor(s, i);
        } else if (s[i] == '/') {
            i++;
            long long divisor = parseFactor(s, i);
            if (divisor == 0) {
                cerr << "Division by zero in arithmetic expression\n";
                exit(EXIT_FAILURE);
            }
            result /= divisor;
        } else {
            break;
        }
    }
    return result;
}

long long parseFactor(const string &s, size_t &i) {
    while (i < s.size() && isspace(s[i]))
        i++;
    long long result = 0;
    if (i < s.size() && s[i] == '(') {
        i++; // '(' ko skip karo
        result = parseExpression(s, i);
        if (i < s.size() && s[i] == ')')
            i++;
        else {
            cerr << "Missing closing parenthesis in arithmetic expression\n";
            exit(EXIT_FAILURE);
        }
    } else {
        bool negative = false;
        if (i < s.size() && s[i] == '-') {
            negative = true;
            i++;
        }
        long long num = 0;
        bool valid = false;
        while (i < s.size() && isdigit(s[i])) {
            valid = true;
            num = num * 10 + (s[i] - '0');
            i++;
        }
        if (!valid) {
            cerr << "Invalid arithmetic expression\n";
            exit(EXIT_FAILURE);
        }
        result = negative ? -num : num;
    }
    return result;
}

long long evaluateArithmetic(const string &expr) {
    size_t i = 0;
    long long result = parseExpression(expr, i);
    return result;
}

// ===================== External Command Execution =====================

void executeExternalCommand(vector<string> tokens, bool background) {
    if (tokens.empty() || tokens[0].empty())
        return;
    int inputFd, outputFd;
    processRedirection(tokens, inputFd, outputFd);
    if (!tokens.empty() && tokens.back() == "&") {
        background = true;
        tokens.pop_back();
    }
    vector<char*> args;
    for (size_t i = 0; i < tokens.size(); i++)
        args.push_back(const_cast<char*>(tokens[i].c_str()));
    args.push_back(nullptr);
    
    string fullPath = findExecutablePath(tokens[0]);
    if (fullPath.empty())
        return;
    args[0] = const_cast<char*>(fullPath.c_str());
    
    pid_t pid = fork();
    if (pid == 0) {
        if (inputFd != -1) {
            if (dup2(inputFd, STDIN_FILENO) == -1) {
                perror("dup2 input");
                exit(EXIT_FAILURE);
            }
            close(inputFd);
        }
        if (outputFd != -1) {
            if (dup2(outputFd, STDOUT_FILENO) == -1) {
                perror("dup2 output");
                exit(EXIT_FAILURE);
            }
            close(outputFd);
        }
        execv(fullPath.c_str(), args.data());
        perror("execv");
        exit(EXIT_FAILURE);
    } else if (pid > 0) {
        if (!background) {
            fg_pid = pid;
            int status;
            waitpid(pid, &status, WUNTRACED);
            fg_pid = 0;
        } else {
            cout << COLOR_YELLOW << "Process running in background with PID: " 
                << pid << COLOR_RESET << "\n";
        }
    } else {
        perror("fork");
    }
    if (inputFd != -1)
        close(inputFd);
    if (outputFd != -1)
        close(outputFd);
}

// ===================== Pipeline Execution =====================

void executePipedCommands(vector<string> pipedCommands) {
    vector<string> filtered;
    for (auto &s : pipedCommands) {
        string t = s;
        size_t start = t.find_first_not_of(" \t");
        size_t end = t.find_last_not_of(" \t");
        if (start != string::npos && end != string::npos) {
            t = t.substr(start, end - start + 1);
            if (!t.empty())
                filtered.push_back(t);
        }
    }
    if (filtered.empty())
        return;
    int n = filtered.size();
    int pipefds[2 * (n - 1)];
    for (int i = 0; i < n - 1; i++) {
        if (pipe(pipefds + i * 2) < 0) {
            perror("pipe");
            exit(EXIT_FAILURE);
        }
    }
    vector<pid_t> pids;
    for (int i = 0; i < n; i++) {
        vector<string> tokens = tokenize(filtered[i], " \t");
        int segInputFd = -1, segOutputFd = -1;
        processRedirection(tokens, segInputFd, segOutputFd);
        if (tokens.empty() || tokens[0].empty())
            continue;
        vector<char*> args;
        for (auto &s : tokens)
            args.push_back(const_cast<char*>(s.c_str()));
        args.push_back(nullptr);
        string fullPath = findExecutablePath(tokens[0]);
        if (fullPath.empty())
            continue;
        args[0] = const_cast<char*>(fullPath.c_str());
        pid_t pid = fork();
        if (pid == 0) {
            if (i != 0 && segInputFd == -1) {
                if (dup2(pipefds[(i - 1) * 2], STDIN_FILENO) == -1) {
                    perror("dup2 pipe input");
                    exit(EXIT_FAILURE);
                }
            }
            if (i != n - 1 && segOutputFd == -1) {
                if (dup2(pipefds[i * 2 + 1], STDOUT_FILENO) == -1) {
                    perror("dup2 pipe output");
                    exit(EXIT_FAILURE);
                }
            }
            if (segInputFd != -1) {
                if (dup2(segInputFd, STDIN_FILENO) == -1) {
                    perror("dup2 seg input");
                    exit(EXIT_FAILURE);
                }
                close(segInputFd);
            }
            if (segOutputFd != -1) {
                if (dup2(segOutputFd, STDOUT_FILENO) == -1) {
                    perror("dup2 seg output");
                    exit(EXIT_FAILURE);
                }
                close(segOutputFd);
            }
            for (int j = 0; j < 2 * (n - 1); j++)
                close(pipefds[j]);
            execv(fullPath.c_str(), args.data());
            perror("execv");
            exit(EXIT_FAILURE);
        } else if (pid < 0) {
            perror("fork");
            exit(EXIT_FAILURE);
        }
        pids.push_back(pid);
        if (segInputFd != -1)
            close(segInputFd);
        if (segOutputFd != -1)
            close(segOutputFd);
    }
    for (int i = 0; i < 2 * (n - 1); i++)
        close(pipefds[i]);
    for (pid_t pid : pids)
        waitpid(pid, nullptr, 0);
}

// ===================== Built-In Command Handlers =====================

void handleCd(const vector<string> &tokens) {
    string target;
    if (tokens.size() == 1)
        target = getenv("HOME");
    else if (tokens.size() == 2) {
        target = tokens[1];
        if (target == "~")
            target = getenv("HOME");
        if (target == "-") {
            if (prevDirectory.empty()) {
                cerr << "No previous directory\n";
                return;
            }
            target = prevDirectory;
        }
    } else {
        cerr << "Invalid arguments for cd\n";
        return;
    }
    char *curr = getcwd(nullptr, 0);
    if (chdir(target.c_str()) != 0)
        perror("cd");
    else
        prevDirectory = string(curr);
    if (curr)
        free(curr);
}

void handlePwd(const vector<string>& tokens) {
    (void)tokens; 
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) != nullptr)
        cout << cwd << "\n";
    else
        perror("pwd");
}

void handleEcho(const vector<string> &tokens) {
    for (size_t i = 1; i < tokens.size(); i++) {
        const string &tok = tokens[i];
        if (tok.size() >= 4 && tok.substr(0, 3) == "$((" && tok.substr(tok.size() - 2) == "))") {
            string expr = tok.substr(3, tok.size() - 5);
            long long result = evaluateArithmetic(expr);
            cout << result;
        } else {
            cout << tokens[i];
        }
        if (i < tokens.size() - 1)
            cout << " ";
    }
    cout << "\n";
}

void handleLs(const vector<string> &tokens) {
    bool flag_a = false, flag_l = false;
    vector<string> dirs;
    for (size_t i = 1; i < tokens.size(); i++) {
        string token = tokens[i];
        if (token[0] == '-') {
            if (token.find('a') != string::npos)
                flag_a = true;
            if (token.find('l') != string::npos)
                flag_l = true;
        } else {
            dirs.push_back(token);
        }
    }
    if (dirs.empty())
        dirs.push_back(".");
    for (auto &dir : dirs) {
        DIR *dp = opendir(dir.c_str());
        if (dp == nullptr) {
            perror(("ls: cannot access " + dir).c_str());
            continue;
        }
        vector<pair<string, bool>> entries;
        struct dirent *entry;
        while ((entry = readdir(dp)) != nullptr) {
            string fname(entry->d_name);
            if (!flag_a && fname[0] == '.')
                continue;
            string fullPath = (dir == "." ? fname : dir + "/" + fname);
            struct stat sb;
            bool isDir = false;
            if (stat(fullPath.c_str(), &sb) == 0 && S_ISDIR(sb.st_mode))
                isDir = true;
            entries.push_back(make_pair(fname, isDir));
        }
        closedir(dp);
        sort(entries.begin(), entries.end(), [](const pair<string, bool> &a, const pair<string, bool> &b) {
            if (a.second != b.second)
                return a.second > b.second;     // directories first
            return a.first < b.first;
        });
        if (flag_l) {
            for (auto &p : entries) {
                string f = p.first;
                string fullPath = (dir == "." ? f : dir + "/" + f);
                struct stat sb;
                if (stat(fullPath.c_str(), &sb) == -1) {
                    perror("stat");
                    continue;
                }
                char perms[11];
                perms[0] = S_ISDIR(sb.st_mode) ? 'd' : '-';
                perms[1] = (sb.st_mode & S_IRUSR) ? 'r' : '-';
                perms[2] = (sb.st_mode & S_IWUSR) ? 'w' : '-';
                perms[3] = (sb.st_mode & S_IXUSR) ? 'x' : '-';
                perms[4] = (sb.st_mode & S_IRGRP) ? 'r' : '-';
                perms[5] = (sb.st_mode & S_IWGRP) ? 'w' : '-';
                perms[6] = (sb.st_mode & S_IXGRP) ? 'x' : '-';
                perms[7] = (sb.st_mode & S_IROTH) ? 'r' : '-';
                perms[8] = (sb.st_mode & S_IWOTH) ? 'w' : '-';
                perms[9] = (sb.st_mode & S_IXOTH) ? 'x' : '-';
                perms[10] = '\0';
                struct passwd *pw = getpwuid(sb.st_uid);
                struct group  *gr = getgrgid(sb.st_gid);
                char timebuf[80];
                struct tm *timeinfo = localtime(&sb.st_mtime);
                strftime(timebuf, sizeof(timebuf), "%b %d %H:%M", timeinfo);
                cout << perms << " " << sb.st_nlink << " " 
                    << (pw ? pw->pw_name : "unknown") << " " 
                    << (gr ? gr->gr_name : "unknown") << " " 
                    << sb.st_size << " " << timebuf << " ";
                if (p.second)
                    cout << COLOR_CYAN << f << COLOR_RESET << "\n";
                else
                    cout << f << "\n";
            }
        } else {
            for (auto &p : entries) {
                if (p.second)
                    cout << COLOR_CYAN << p.first << COLOR_RESET << "\n";
                else
                    cout << p.first << "\n";
            }
        }
    }
}

void handlePinfo(const vector<string> &tokens) {
    pid_t pid = getpid();
    if (tokens.size() == 2)
        pid = stoi(tokens[1]);
    stringstream ss;
    ss << "/proc/" << pid << "/status";
    ifstream statusFile(ss.str());
    if (!statusFile.is_open()) {
        cerr << "Error: Could not open status file for PID " << pid << "\n";
        return;
    }
    string line;
    while (getline(statusFile, line)) {
        if (line.find("State:") != string::npos || line.find("VmSize:") != string::npos)
            cout << line << "\n";
    }
    statusFile.close();
    ss.str("");
    ss << "/proc/" << pid << "/exe";
    char exePath[1024];
    ssize_t len = readlink(ss.str().c_str(), exePath, sizeof(exePath) - 1);
    if (len != -1) {
        exePath[len] = '\0';
        cout << "Executable Path: " << exePath << "\n";
    } else {
        perror("readlink");
    }
}

bool searchRecursive(const char *basePath, const string &target) {
    DIR *dir = opendir(basePath);
    if (!dir)
        return false;
    struct dirent *entry;
    bool found = false;
    while ((entry = readdir(dir)) != nullptr) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        string path = string(basePath) + "/" + entry->d_name;
        if (string(entry->d_name) == target) {
            found = true;
            break;
        }
        struct stat sb;
        if (stat(path.c_str(), &sb) == 0 && S_ISDIR(sb.st_mode)) {
            if (searchRecursive(path.c_str(), target)) {
                found = true;
                break;
            }
        }
    }
    closedir(dir);
    return found;
}

void handleSearch(const vector<string> &tokens) {
    if (tokens.size() != 2) {
        cerr << "Usage: search <filename>\n";
        return;
    }
    bool found = searchRecursive(".", tokens[1]);
    cout << (found ? "True" : "False") << "\n";
}

void handleHistory(const vector<string> &tokens) {
    int num = 10;
    if (tokens.size() == 2)
        num = stoi(tokens[1]);
    int count = 0, total = history.size();
    for (auto it = history.begin(); it != history.end(); ++it) {
        if (total - count <= num)
            cout << *it << "\n";
        count++;
    }
}

// ===================== Command Execution =====================

void executeCommand(string command) {
    // Command ko semicolon se split karo aur alag-alag execute karo
    char *cmdCpy = new char[command.size() + 1];
    strcpy(cmdCpy, command.c_str());
    char *saveptr;
    char *singleCmd = strtok_r(cmdCpy, ";", &saveptr);
    while (singleCmd != nullptr) {
        string cmdStr(singleCmd);
        size_t start = cmdStr.find_first_not_of(" \t");
        if (start != string::npos)
            cmdStr = cmdStr.substr(start);
        size_t end = cmdStr.find_last_not_of(" \t");
        if (end != string::npos)
            cmdStr = cmdStr.substr(0, end + 1);
        if (cmdStr.empty()) {
            singleCmd = strtok_r(nullptr, ";", &saveptr);
            continue;
        }
        if (cmdStr.find("|") != string::npos) {
            vector<string> rawPiped = tokenize(cmdStr, "|");
            vector<string> pipedCommands;
            for (auto &s : rawPiped) {
                size_t sPos = s.find_first_not_of(" \t");
                size_t ePos = s.find_last_not_of(" \t");
                if (sPos != string::npos && ePos != string::npos) {
                    string trimmed = s.substr(sPos, ePos - sPos + 1);
                    if (!trimmed.empty())
                        pipedCommands.push_back(trimmed);
                }
            }
            if (!pipedCommands.empty())
                executePipedCommands(pipedCommands);
        } else {
            vector<string> tokens = tokenize(cmdStr, " \t");
            if (tokens.empty() || tokens[0].empty()) {
                singleCmd = strtok_r(nullptr, ";", &saveptr);
                continue;
            }
            // Built-in commands
            if (tokens[0] == "cd")
                handleCd(tokens);
            else if (tokens[0] == "pwd") {
                int inputFd = -1, outputFd = -1;
                processRedirection(tokens, inputFd, outputFd);
                if (outputFd != -1) {
                    int saved_stdout = dup(STDOUT_FILENO);
                    dup2(outputFd, STDOUT_FILENO);
                    close(outputFd);
                    handlePwd(tokens);
                    dup2(saved_stdout, STDOUT_FILENO);
                    close(saved_stdout);
                } else {
                    handlePwd(tokens);
                }
            }
            else if (tokens[0] == "echo") {
                int saved_stdout = -1;
                int inputFd = -1, outputFd = -1;
                processRedirection(tokens, inputFd, outputFd);
                if (outputFd != -1) {
                    saved_stdout = dup(STDOUT_FILENO);
                    dup2(outputFd, STDOUT_FILENO);
                    close(outputFd);
                }
                handleEcho(tokens);
                if (saved_stdout != -1) {
                    dup2(saved_stdout, STDOUT_FILENO);
                    close(saved_stdout);
                }
            }
            else if (tokens[0] == "ls")
                handleLs(tokens);
            else if (tokens[0] == "pinfo")
                handlePinfo(tokens);
            else if (tokens[0] == "search")
                handleSearch(tokens);
            else if (tokens[0] == "history")
                handleHistory(tokens);
            else if (tokens[0] == "exit") {
                delete[] cmdCpy;
                resetTerminal();
                exit(0);
            }
            else {
                // External commands
                bool background = false;
                if (!tokens.empty() && tokens.back() == "&") {
                    background = true;
                    tokens.pop_back();
                }
                executeExternalCommand(tokens, background);
            }
        }
        singleCmd = strtok_r(nullptr, ";", &saveptr);
    }
    delete[] cmdCpy;
}

// ===================== Main Function =====================

int main(void) {
    char *cwd = getcwd(nullptr, 0);
    if (cwd)
        prevDirectory = string(cwd);
    if (cwd)
        free(cwd);
    
    loadHistory(history);
    signal(SIGINT, sigintHandler);
    signal(SIGTSTP, sigtstpHandler);
    signal(SIGCHLD, sigchldHandler);
    setNonCanonicalMode();
    
    while (true) {
        string input = readInput();
        if (!input.empty())
            addCommandToHistory(history, input);
        executeCommand(input);
    }
    
    resetTerminal();
    return 0;
}
