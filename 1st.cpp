#include <iostream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <string.h>
#include <dirent.h>
#include <vector>
#include <sstream>
#include <fstream>
#include <cstdio>
#include <errno.h>

#define BUFFER_SIZE 256

void read_directory(const char *dirname, std::vector<std::pair<std::string, std::string>> &file_contents) {
    DIR *dir;
    struct dirent *ent;

    if ((dir = opendir(dirname)) != NULL) {
        while ((ent = readdir(dir)) != NULL) {
            if (ent->d_type == DT_REG) {
                std::string filename = ent->d_name;
                std::string filepath = std::string(dirname) + "/" + filename;
                std::ifstream file(filepath);
                if (file.is_open()) {
                    std::string content((std::istreambuf_iterator<char>(file)),
                                         std::istreambuf_iterator<char>());
                    file_contents.push_back(std::make_pair(filename, content));
                    file.close();
                } else {
                    std::cerr << "Error opening file: " << filepath << std::endl;
                    exit(EXIT_FAILURE);
                }
            }
        }
        closedir(dir);
    } else {
        perror("Error opening directory");
        exit(EXIT_FAILURE);
    }
}

void update_directory(const char *dirname, int pipe_fd) {
    std::vector<std::pair<std::string, std::string>> received_data;
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    std::string data_received;
    while (true) {
        bytes_read = read(pipe_fd, buffer, BUFFER_SIZE);
        if (bytes_read == -1) {
            perror("read");
            exit(EXIT_FAILURE);
        } else if (bytes_read == 0) {
            break;
        } else {
            buffer[bytes_read] = '\0';
            data_received += buffer;
        }
    }
    close(pipe_fd);
    std::istringstream iss(data_received);
    std::string line;
    while (std::getline(iss, line)) {
        std::string filename = line;
        std::getline(iss, line);
        std::string content = line;
        received_data.push_back(std::make_pair(filename, content));
    }

    for (const auto &data : received_data) {
        std::string filepath = std::string(dirname) + "/" + data.first;
        std::ofstream file(filepath);
        if (file.is_open()) {
            file << data.second;
            file.close();
        } else {
            std::cerr << "Error creating file: " << filepath << std::endl;
            exit(EXIT_FAILURE);
        }
    }
}

int main() {
    // Create pipes
    int pipe_child1_to_child2[2];
    int pipe_child2_to_child1[2];
    if (pipe(pipe_child1_to_child2) == -1 || pipe(pipe_child2_to_child1) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    // Fork and execute children
    pid_t child1_pid, child2_pid;
    child1_pid = fork();
    if (child1_pid == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    } else if (child1_pid == 0) { // Child 1 takes control of Dir 1
        close(pipe_child1_to_child2[0]);
        close(pipe_child2_to_child1[1]);
        std::vector<std::pair<std::string, std::string>> file_contents;
        read_directory("Dir1", file_contents);

        for (const auto &file_content : file_contents) {
            std::string message = file_content.first + "\n" + file_content.second + "\n";
            write(pipe_child1_to_child2[1], message.c_str(), message.length());
        }

        close(pipe_child1_to_child2[1]); 

        update_directory("Dir1", pipe_child2_to_child1[0]);
    
        exit(EXIT_SUCCESS);
    } else { 
        close(pipe_child1_to_child2[1]); 

        child2_pid = fork();

        if (child2_pid == -1) {
            perror("fork");
            exit(EXIT_FAILURE);
        } else if (child2_pid == 0) { // Child2 takes control of Dir2
            close(pipe_child2_to_child1[0]); 
            close(pipe_child1_to_child2[1]);
            std::vector<std::pair<std::string, std::string>> file_contents;
            read_directory("Dir2", file_contents);
            for (const auto &file_content : file_contents) {
                std::string message = file_content.first + "\n" + file_content.second + "\n";
                write(pipe_child2_to_child1[1], message.c_str(), message.length());
            }
            close(pipe_child2_to_child1[1]);
            update_directory("Dir2", pipe_child1_to_child2[0]);
            exit(EXIT_SUCCESS);
        } else {
            close(pipe_child2_to_child1[1]);
            int status1, status2;
            waitpid(child1_pid, &status1, 0);
            waitpid(child2_pid, &status2, 0);
            close(pipe_child1_to_child2[0]);
            close(pipe_child2_to_child1[0]);
        }
    }
    return 0;
}