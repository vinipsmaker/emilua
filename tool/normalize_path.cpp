#include <filesystem>
#include <string>
#include <cstdio>

#include <boost/predef/os/windows.h>

int main()
{
    std::string buffer;
    buffer.resize(8192);

    {
        std::size_t nread;
        std::size_t used = 0;
        do {
            if (used == buffer.size()) buffer.resize(buffer.size() * 2);
            nread = fread(buffer.data() + used, sizeof(char),
                          buffer.size() - used, stdin);
            used += nread;
        } while (nread > 0);
        buffer.resize(used);
    }

#if BOOST_OS_WINDOWS
    {
        std::filesystem::path p{buffer, std::filesystem::path::native_format};
        p.make_preferred();
        buffer = p.u8string();
    }
#endif // BOOST_OS_WINDOWS

    fwrite(buffer.data(), buffer.size(), 1, stdout);
}
