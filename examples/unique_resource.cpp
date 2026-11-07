#include <cstdio>

#ifndef _WIN32
#    include <sys/socket.h>
#    include <unistd.h>
#endif

#include "unique_resource.h"

int main()
{
    //! [Using make_unique_resource_checked()]
    auto file = wwa::utils::make_unique_resource_checked(
        std::fopen("potentially_nonexistent_file.txt", "r"), nullptr, std::fclose
    );

    if (file.get() != nullptr) {
        std::puts("The file exists.\n");
    }
    else {
        std::puts("The file does not exist.\n");
    }
    //! [Using make_unique_resource_checked()]

#ifndef _WIN32
    //! [Using unique_resource]
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock != -1) {
        const wwa::utils::unique_resource sock_guard(sock, close);
        // Do something with the socket
        // The socket will be closed when `sock_guard` goes out of scope
    }
    //! [Using unique_resource]
#endif

    return 0;
}
