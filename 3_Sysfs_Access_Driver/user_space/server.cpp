#include <iostream>
#include <fcntl.h>      // open()
#include <unistd.h>     // write(), close()
#include <cstring>      // strlen()
#include <sys/types.h>
#include <sys/stat.h>

int main() {
    const char* filename = "/sys/class/Mithilesh/mydevice/val";
    const char* message = "12";

    // Open file with write-only flag, create if not exists, truncate if exists
    int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        std::perror("open");
        return 1;
    }

    // Write message to file
    ssize_t bytes_written = write(fd, message, std::strlen(message));
    if (bytes_written < 0) {
        std::perror("write");
        close(fd);
        return 1;
    }

    // Close file
    close(fd);

    std::cout << "Successfully wrote to " << filename << std::endl;
    return 0;
}

