#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/wait.h>
#include <sys/file.h>


#define LOG_FILE "log.txt"

void printUsage() {
    const char *usage =
        "Usage: fileManager <command> [arguments]\n"
        "Commands:\n"
        "  createDir \"folderName\"                         - Create a new directory\n"
        "  createFile \"fileName\"                          - Create a new file\n"
        "  listDir \"folderName\"                           - List all files in a directory\n"
        "  listFilesByExtension \"folderName\" \".txt\"     - List files with specific extension\n"
        "  readFile \"fileName\"                            - Read a file's content\n"
        "  appendToFile \"fileName\" \"new content\"        - Append content to a file\n"
        "  deleteFile \"fileName\"                          - Delete a file\n"
        "  deleteDir \"folderName\"                         - Delete an empty directory\n"
        "  showLogs                                         - Display operation logs\n";
    
    write(STDOUT_FILENO, usage, strlen(usage));
}

void writeMessage(int fd, const char *prefix, const char *name, const char *suffix) {
    write(fd, prefix, strlen(prefix));
    write(fd, "\"", 1);
    write(fd, name, strlen(name));
    write(fd, "\"", 1);
    write(fd, suffix, strlen(suffix));
}

void logAction(const char *prefix, const char *name, const char *suffix) {
    int fd = open(LOG_FILE, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd == -1) return;
    
    time_t now = time(NULL);
    char timeBuffer[30];
    struct tm *tm_info = localtime(&now);
    strftime(timeBuffer, 30, "[%Y-%m-%d %H:%M:%S] ", tm_info);
    
    flock(fd, LOCK_EX);
    write(fd, timeBuffer, strlen(timeBuffer));
    writeMessage(fd, prefix, name, suffix);
    write(fd, "\n", 1);
    flock(fd, LOCK_UN);
    
    close(fd);
}


void createDir(const char *folderName) {
    if (mkdir(folderName, 0755) == -1) {
        if (errno == EEXIST) {
            writeMessage(STDERR_FILENO, "Error: Directory ", folderName, " already exists.\n");
        } else {
            writeMessage(STDERR_FILENO, "Error: Failed to create ", folderName, " directory.\n");
        }
        return;
    }
    writeMessage(STDOUT_FILENO, "Directory ", folderName, " created successfully.\n");
    logAction("Directory ", folderName, " created successfully.");
}

void createFile(const char *fileName) {
    int fd = open(fileName, O_CREAT | O_WRONLY, 0644);
    if (fd == -1) {
        if (errno == EEXIST) {
            writeMessage(STDERR_FILENO, "Error: File ", fileName, " already exists.\n");
        } else {
            writeMessage(STDERR_FILENO, "Error: Failed to create ", fileName, " file.\n");
        }
        return;
    }
    
    // Zaman damgasını dosyaya yazma
    time_t now = time(NULL);
    char timeBuffer[30];
    struct tm *tm_info = localtime(&now);
    strftime(timeBuffer, 30, "Created at: %Y-%m-%d %H:%M:%S\n", tm_info);
    write(fd, timeBuffer, strlen(timeBuffer));
    close(fd);
    
    writeMessage(STDOUT_FILENO, "File ", fileName, " created successfully.\n");
    logAction("File ", fileName, " created successfully.");
}

void listDir(const char *folderName) {
    pid_t pid = fork();
    if (pid == 0) { // Çocuk süreç
        DIR *dir = opendir(folderName);
        if (dir == NULL) {
            writeMessage(STDERR_FILENO, "Error: Directory ", folderName, " not found.\n");
            exit(1);
        }
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
                write(STDOUT_FILENO, entry->d_name, strlen(entry->d_name));
                write(STDOUT_FILENO, "\n", 1);
            }
        }
        closedir(dir);
        exit(0);
    } else {
        wait(NULL);
    }
}

void listFilesByExtension(const char *folderName, const char *extension) {
    pid_t pid = fork();
    if (pid == 0) {
        DIR *dir = opendir(folderName);
        
        if (dir == NULL) {
            writeMessage(STDERR_FILENO, "Error: Directory ", folderName, " not found.\n");
            exit(1);
        }

        struct dirent *entry;
        int found = 0;
        size_t extLen = strlen(extension);
        while ((entry = readdir(dir)) != NULL) {
            size_t nameLen = strlen(entry->d_name);
            if (nameLen > extLen && strcmp(entry->d_name + nameLen - extLen, extension) == 0) {
                write(STDOUT_FILENO, entry->d_name, nameLen);
                write(STDOUT_FILENO, "\n", 1);
                found = 1;
            }
        }
        if (!found) {
            write(STDOUT_FILENO, "No files with the specified extension found.\n", 43);
        }
        closedir(dir);
        exit(0);
    } else {
        wait(NULL);
    }
}

void readFile(const char *fileName) {
    int fd = open(fileName, O_RDONLY);
    if (fd == -1) {
        writeMessage(STDERR_FILENO, "Error: File ", fileName, " not found.\n");
        return;
    }
    char buffer[256];
    ssize_t bytesRead;
    while ((bytesRead = read(fd, buffer, sizeof(buffer) - 1)) > 0) {
        write(STDOUT_FILENO, buffer, bytesRead);
    }
    close(fd);
}

void appendToFile(const char *fileName, const char *content) {
    int fd = open(fileName, O_WRONLY | O_APPEND);
    if (fd == -1) {
        writeMessage(STDERR_FILENO, "Error: Cannot write to ", fileName, " file.\n");
        return;
    }
    if (flock(fd, LOCK_EX) == -1) {
        writeMessage(STDERR_FILENO, "Error: File ", fileName, " is locked.\n");
        close(fd);
        return;
    }
    write(fd, content, strlen(content));
    write(fd, "\n", 1);
    flock(fd, LOCK_UN);
    close(fd);
    
    writeMessage(STDOUT_FILENO, "Content appended to ", fileName, " successfully.\n");
    logAction("Content appended to ", fileName, " successfully.");
}

void deleteFile(const char *fileName) {
    pid_t pid = fork();
    if (pid == 0) {
        
        if (unlink(fileName) == -1) {
            writeMessage(STDERR_FILENO, "Error: File ", fileName, " not found.\n");
            exit(1);
        }

        writeMessage(STDOUT_FILENO, "File ", fileName, " deleted successfully.\n");
        logAction("File ", fileName, " deleted successfully.");
        exit(0);
    } else {
        wait(NULL);
    }
}

void deleteDir(const char *folderName) {
    pid_t pid = fork();
    if (pid == 0) {

        if (rmdir(folderName) == -1) {
            if (errno == ENOTEMPTY) {
                writeMessage(STDERR_FILENO, "Error: Directory ", folderName, " not empty.\n");
            }
            else {
                writeMessage(STDERR_FILENO, "Error: Directory ", folderName, " not found.\n");
            }
            exit(1);
        }
        
        writeMessage(STDOUT_FILENO, "Directory ", folderName, " deleted successfully.\n");
        logAction("Directory ", folderName, " deleted successfully.");
        exit(0);
    } else {
        wait(NULL);
    }
}

void showLogs() {
    int fd = open(LOG_FILE, O_RDONLY);
    if (fd == -1) {
        write(STDERR_FILENO, "Error: No log file found.\n", 27);
        return;
    }
    char buffer[256];
    ssize_t bytesRead;
    while ((bytesRead = read(fd, buffer, sizeof(buffer) - 1)) > 0) {
        write(STDOUT_FILENO, buffer, bytesRead);
    }
    close(fd);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printUsage();
        return 1;
    }
    
    // if (argc < 3 && strcmp(argv[1], "showLogs") != 0) {
    //     write(STDERR_FILENO, "Usage: <command> <name>\n", 25);
    //     return 1;
    // }
    
    if (strcmp(argv[1], "createDir") == 0) {
        createDir(argv[2]);
    } else if (strcmp(argv[1], "createFile") == 0) {
        createFile(argv[2]);
    } else if (strcmp(argv[1], "listDir") == 0) {
        listDir(argv[2]);
    } else if (strcmp(argv[1], "listFilesByExtension") == 0 && argc == 4) {
        listFilesByExtension(argv[2], argv[3]);
    } else if (strcmp(argv[1], "readFile") == 0) {
        readFile(argv[2]);
    } else if (strcmp(argv[1], "appendToFile") == 0 && argc == 4) {
        appendToFile(argv[2], argv[3]);
    } else if (strcmp(argv[1], "deleteFile") == 0) {
        deleteFile(argv[2]);
    } else if (strcmp(argv[1], "deleteDir") == 0) {
        deleteDir(argv[2]);
    } else if (strcmp(argv[1], "showLogs") == 0) {
        showLogs();
    } else {
        write(STDERR_FILENO, "Invalid command.\n", 18);
    }
    
    return 0;
}
