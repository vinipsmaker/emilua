EMILUA_GPERF_DECLS_BEGIN(includes)
#include <emilua/core.hpp>

#include <sys/capability.h>
#include <sys/mount.h>

#include <linux/securebits.h>
#include <grp.h>

#include <boost/scope_exit.hpp>

#define EMILUA_DETAIL_INT_CONSTANT(X) \
    [](lua_State* L) -> int {         \
        lua_pushinteger(L, X);        \
        return 1;                     \
    }
EMILUA_GPERF_DECLS_END(includes)

namespace emilua {

EMILUA_GPERF_DECLS_BEGIN(sys_bindings)
static void check_last_error(lua_State* L, int last_error)
{
    if (last_error != 0) {
        lua_getfield(L, LUA_GLOBALSINDEX, "errexit");
        if (lua_toboolean(L, -1)) {
            errno = last_error;
            perror("<3>linux_namespaces/init");
            std::exit(1);
        }
    }
};
EMILUA_GPERF_DECLS_END(sys_bindings)

int posix_mt_index(lua_State* L)
{
    auto key = tostringview(L, 2);
    return EMILUA_GPERF_BEGIN(key)
        EMILUA_GPERF_PARAM(int (*action)(lua_State*))
        EMILUA_GPERF_DEFAULT_VALUE([](lua_State* L) -> int {
            return luaL_error(L, "key not found");
        })
        // ### CONSTANTS ###
        EMILUA_GPERF_PAIR("O_CLOEXEC", EMILUA_DETAIL_INT_CONSTANT(O_CLOEXEC))
        // errno
        EMILUA_GPERF_PAIR(
            "EAFNOSUPPORT", EMILUA_DETAIL_INT_CONSTANT(EAFNOSUPPORT))
        EMILUA_GPERF_PAIR("EADDRINUSE", EMILUA_DETAIL_INT_CONSTANT(EADDRINUSE))
        EMILUA_GPERF_PAIR(
            "EADDRNOTAVAIL", EMILUA_DETAIL_INT_CONSTANT(EADDRNOTAVAIL))
        EMILUA_GPERF_PAIR("EISCONN", EMILUA_DETAIL_INT_CONSTANT(EISCONN))
        EMILUA_GPERF_PAIR("E2BIG", EMILUA_DETAIL_INT_CONSTANT(E2BIG))
        EMILUA_GPERF_PAIR("EDOM", EMILUA_DETAIL_INT_CONSTANT(EDOM))
        EMILUA_GPERF_PAIR("EFAULT", EMILUA_DETAIL_INT_CONSTANT(EFAULT))
        EMILUA_GPERF_PAIR("EBADF", EMILUA_DETAIL_INT_CONSTANT(EBADF))
        EMILUA_GPERF_PAIR("EBADMSG", EMILUA_DETAIL_INT_CONSTANT(EBADMSG))
        EMILUA_GPERF_PAIR("EPIPE", EMILUA_DETAIL_INT_CONSTANT(EPIPE))
        EMILUA_GPERF_PAIR(
            "ECONNABORTED", EMILUA_DETAIL_INT_CONSTANT(ECONNABORTED))
        EMILUA_GPERF_PAIR("EALREADY", EMILUA_DETAIL_INT_CONSTANT(EALREADY))
        EMILUA_GPERF_PAIR(
            "ECONNREFUSED", EMILUA_DETAIL_INT_CONSTANT(ECONNREFUSED))
        EMILUA_GPERF_PAIR("ECONNRESET", EMILUA_DETAIL_INT_CONSTANT(ECONNRESET))
        EMILUA_GPERF_PAIR("EXDEV", EMILUA_DETAIL_INT_CONSTANT(EXDEV))
        EMILUA_GPERF_PAIR(
            "EDESTADDRREQ", EMILUA_DETAIL_INT_CONSTANT(EDESTADDRREQ))
        EMILUA_GPERF_PAIR("EBUSY", EMILUA_DETAIL_INT_CONSTANT(EBUSY))
        EMILUA_GPERF_PAIR("ENOTEMPTY", EMILUA_DETAIL_INT_CONSTANT(ENOTEMPTY))
        EMILUA_GPERF_PAIR("ENOEXEC", EMILUA_DETAIL_INT_CONSTANT(ENOEXEC))
        EMILUA_GPERF_PAIR("EEXIST", EMILUA_DETAIL_INT_CONSTANT(EEXIST))
        EMILUA_GPERF_PAIR("EFBIG", EMILUA_DETAIL_INT_CONSTANT(EFBIG))
        EMILUA_GPERF_PAIR(
            "ENAMETOOLONG", EMILUA_DETAIL_INT_CONSTANT(ENAMETOOLONG))
        EMILUA_GPERF_PAIR("ENOSYS", EMILUA_DETAIL_INT_CONSTANT(ENOSYS))
        EMILUA_GPERF_PAIR(
            "EHOSTUNREACH", EMILUA_DETAIL_INT_CONSTANT(EHOSTUNREACH))
        EMILUA_GPERF_PAIR("EIDRM", EMILUA_DETAIL_INT_CONSTANT(EIDRM))
        EMILUA_GPERF_PAIR("EILSEQ", EMILUA_DETAIL_INT_CONSTANT(EILSEQ))
        EMILUA_GPERF_PAIR("ENOTTY", EMILUA_DETAIL_INT_CONSTANT(ENOTTY))
        EMILUA_GPERF_PAIR("EINTR", EMILUA_DETAIL_INT_CONSTANT(EINTR))
        EMILUA_GPERF_PAIR("EINVAL", EMILUA_DETAIL_INT_CONSTANT(EINVAL))
        EMILUA_GPERF_PAIR("ESPIPE", EMILUA_DETAIL_INT_CONSTANT(ESPIPE))
        EMILUA_GPERF_PAIR("EIO", EMILUA_DETAIL_INT_CONSTANT(EIO))
        EMILUA_GPERF_PAIR("EISDIR", EMILUA_DETAIL_INT_CONSTANT(EISDIR))
        EMILUA_GPERF_PAIR("EMSGSIZE", EMILUA_DETAIL_INT_CONSTANT(EMSGSIZE))
        EMILUA_GPERF_PAIR("ENETDOWN", EMILUA_DETAIL_INT_CONSTANT(ENETDOWN))
        EMILUA_GPERF_PAIR("ENETRESET", EMILUA_DETAIL_INT_CONSTANT(ENETRESET))
        EMILUA_GPERF_PAIR(
            "ENETUNREACH", EMILUA_DETAIL_INT_CONSTANT(ENETUNREACH))
        EMILUA_GPERF_PAIR("ENOBUFS", EMILUA_DETAIL_INT_CONSTANT(ENOBUFS))
        EMILUA_GPERF_PAIR("ECHILD", EMILUA_DETAIL_INT_CONSTANT(ECHILD))
        EMILUA_GPERF_PAIR("ENOLINK", EMILUA_DETAIL_INT_CONSTANT(ENOLINK))
        EMILUA_GPERF_PAIR("ENOLCK", EMILUA_DETAIL_INT_CONSTANT(ENOLCK))
        EMILUA_GPERF_PAIR("ENODATA", EMILUA_DETAIL_INT_CONSTANT(ENODATA))
        EMILUA_GPERF_PAIR("ENOMSG", EMILUA_DETAIL_INT_CONSTANT(ENOMSG))
        EMILUA_GPERF_PAIR(
            "ENOPROTOOPT", EMILUA_DETAIL_INT_CONSTANT(ENOPROTOOPT))
        EMILUA_GPERF_PAIR("ENOSPC", EMILUA_DETAIL_INT_CONSTANT(ENOSPC))
        EMILUA_GPERF_PAIR("ENOSR", EMILUA_DETAIL_INT_CONSTANT(ENOSR))
        EMILUA_GPERF_PAIR("ENXIO", EMILUA_DETAIL_INT_CONSTANT(ENXIO))
        EMILUA_GPERF_PAIR("ENODEV", EMILUA_DETAIL_INT_CONSTANT(ENODEV))
        EMILUA_GPERF_PAIR("ENOENT", EMILUA_DETAIL_INT_CONSTANT(ENOENT))
        EMILUA_GPERF_PAIR("ESRCH", EMILUA_DETAIL_INT_CONSTANT(ESRCH))
        EMILUA_GPERF_PAIR("ENOTDIR", EMILUA_DETAIL_INT_CONSTANT(ENOTDIR))
        EMILUA_GPERF_PAIR("ENOTSOCK", EMILUA_DETAIL_INT_CONSTANT(ENOTSOCK))
        EMILUA_GPERF_PAIR("ENOSTR", EMILUA_DETAIL_INT_CONSTANT(ENOSTR))
        EMILUA_GPERF_PAIR("ENOTCONN", EMILUA_DETAIL_INT_CONSTANT(ENOTCONN))
        EMILUA_GPERF_PAIR("ENOMEM", EMILUA_DETAIL_INT_CONSTANT(ENOMEM))
        EMILUA_GPERF_PAIR("ENOTSUP", EMILUA_DETAIL_INT_CONSTANT(ENOTSUP))
        EMILUA_GPERF_PAIR("ECANCELED", EMILUA_DETAIL_INT_CONSTANT(ECANCELED))
        EMILUA_GPERF_PAIR(
            "EINPROGRESS", EMILUA_DETAIL_INT_CONSTANT(EINPROGRESS))
        EMILUA_GPERF_PAIR("EPERM", EMILUA_DETAIL_INT_CONSTANT(EPERM))
        EMILUA_GPERF_PAIR("EOPNOTSUPP", EMILUA_DETAIL_INT_CONSTANT(EOPNOTSUPP))
        EMILUA_GPERF_PAIR(
            "EWOULDBLOCK", EMILUA_DETAIL_INT_CONSTANT(EWOULDBLOCK))
        EMILUA_GPERF_PAIR("EOWNERDEAD", EMILUA_DETAIL_INT_CONSTANT(EOWNERDEAD))
        EMILUA_GPERF_PAIR("EACCES", EMILUA_DETAIL_INT_CONSTANT(EACCES))
        EMILUA_GPERF_PAIR("EPROTO", EMILUA_DETAIL_INT_CONSTANT(EPROTO))
        EMILUA_GPERF_PAIR(
            "EPROTONOSUPPORT", EMILUA_DETAIL_INT_CONSTANT(EPROTONOSUPPORT))
        EMILUA_GPERF_PAIR("EROFS", EMILUA_DETAIL_INT_CONSTANT(EROFS))
        EMILUA_GPERF_PAIR("EDEADLK", EMILUA_DETAIL_INT_CONSTANT(EDEADLK))
        EMILUA_GPERF_PAIR("EAGAIN", EMILUA_DETAIL_INT_CONSTANT(EAGAIN))
        EMILUA_GPERF_PAIR("ERANGE", EMILUA_DETAIL_INT_CONSTANT(ERANGE))
        EMILUA_GPERF_PAIR(
            "ENOTRECOVERABLE", EMILUA_DETAIL_INT_CONSTANT(ENOTRECOVERABLE))
        EMILUA_GPERF_PAIR("ETIME", EMILUA_DETAIL_INT_CONSTANT(ETIME))
        EMILUA_GPERF_PAIR("ETXTBSY", EMILUA_DETAIL_INT_CONSTANT(ETXTBSY))
        EMILUA_GPERF_PAIR("ETIMEDOUT", EMILUA_DETAIL_INT_CONSTANT(ETIMEDOUT))
        EMILUA_GPERF_PAIR("ENFILE", EMILUA_DETAIL_INT_CONSTANT(ENFILE))
        EMILUA_GPERF_PAIR("EMFILE", EMILUA_DETAIL_INT_CONSTANT(EMFILE))
        EMILUA_GPERF_PAIR("EMLINK", EMILUA_DETAIL_INT_CONSTANT(EMLINK))
        EMILUA_GPERF_PAIR("ELOOP", EMILUA_DETAIL_INT_CONSTANT(ELOOP))
        EMILUA_GPERF_PAIR("EOVERFLOW", EMILUA_DETAIL_INT_CONSTANT(EOVERFLOW))
        EMILUA_GPERF_PAIR("EPROTOTYPE", EMILUA_DETAIL_INT_CONSTANT(EPROTOTYPE))
        // file creation flags
        EMILUA_GPERF_PAIR("O_CREAT", EMILUA_DETAIL_INT_CONSTANT(O_CREAT))
        EMILUA_GPERF_PAIR("O_RDONLY", EMILUA_DETAIL_INT_CONSTANT(O_RDONLY))
        EMILUA_GPERF_PAIR("O_WRONLY", EMILUA_DETAIL_INT_CONSTANT(O_WRONLY))
        EMILUA_GPERF_PAIR("O_RDWR", EMILUA_DETAIL_INT_CONSTANT(O_RDWR))
        EMILUA_GPERF_PAIR(
            "O_DIRECTORY", EMILUA_DETAIL_INT_CONSTANT(O_DIRECTORY))
        EMILUA_GPERF_PAIR("O_EXCL", EMILUA_DETAIL_INT_CONSTANT(O_EXCL))
        EMILUA_GPERF_PAIR("O_NOCTTY", EMILUA_DETAIL_INT_CONSTANT(O_NOCTTY))
        EMILUA_GPERF_PAIR("O_NOFOLLOW", EMILUA_DETAIL_INT_CONSTANT(O_NOFOLLOW))
        EMILUA_GPERF_PAIR("O_TMPFILE", EMILUA_DETAIL_INT_CONSTANT(O_TMPFILE))
        EMILUA_GPERF_PAIR("O_TRUNC", EMILUA_DETAIL_INT_CONSTANT(O_TRUNC))
        // file status flags
        EMILUA_GPERF_PAIR("O_APPEND", EMILUA_DETAIL_INT_CONSTANT(O_APPEND))
        EMILUA_GPERF_PAIR("O_ASYNC", EMILUA_DETAIL_INT_CONSTANT(O_ASYNC))
        EMILUA_GPERF_PAIR("O_DIRECT", EMILUA_DETAIL_INT_CONSTANT(O_DIRECT))
        EMILUA_GPERF_PAIR("O_DSYNC", EMILUA_DETAIL_INT_CONSTANT(O_DSYNC))
        EMILUA_GPERF_PAIR(
            "O_LARGEFILE", EMILUA_DETAIL_INT_CONSTANT(O_LARGEFILE))
        EMILUA_GPERF_PAIR("O_NOATIME", EMILUA_DETAIL_INT_CONSTANT(O_NOATIME))
        EMILUA_GPERF_PAIR("O_NONBLOCK", EMILUA_DETAIL_INT_CONSTANT(O_NONBLOCK))
        EMILUA_GPERF_PAIR("O_PATH", EMILUA_DETAIL_INT_CONSTANT(O_PATH))
        EMILUA_GPERF_PAIR("O_SYNC", EMILUA_DETAIL_INT_CONSTANT(O_SYNC))
        // mode_t constants
        EMILUA_GPERF_PAIR("S_IRWXU", EMILUA_DETAIL_INT_CONSTANT(S_IRWXU))
        EMILUA_GPERF_PAIR("S_IRUSR", EMILUA_DETAIL_INT_CONSTANT(S_IRUSR))
        EMILUA_GPERF_PAIR("S_IWUSR", EMILUA_DETAIL_INT_CONSTANT(S_IWUSR))
        EMILUA_GPERF_PAIR("S_IXUSR", EMILUA_DETAIL_INT_CONSTANT(S_IXUSR))
        EMILUA_GPERF_PAIR("S_IRWXG", EMILUA_DETAIL_INT_CONSTANT(S_IRWXG))
        EMILUA_GPERF_PAIR("S_IRGRP", EMILUA_DETAIL_INT_CONSTANT(S_IRGRP))
        EMILUA_GPERF_PAIR("S_IWGRP", EMILUA_DETAIL_INT_CONSTANT(S_IWGRP))
        EMILUA_GPERF_PAIR("S_IXGRP", EMILUA_DETAIL_INT_CONSTANT(S_IXGRP))
        EMILUA_GPERF_PAIR("S_IRWXO", EMILUA_DETAIL_INT_CONSTANT(S_IRWXO))
        EMILUA_GPERF_PAIR("S_IROTH", EMILUA_DETAIL_INT_CONSTANT(S_IROTH))
        EMILUA_GPERF_PAIR("S_IWOTH", EMILUA_DETAIL_INT_CONSTANT(S_IWOTH))
        EMILUA_GPERF_PAIR("S_IXOTH", EMILUA_DETAIL_INT_CONSTANT(S_IXOTH))
        EMILUA_GPERF_PAIR("S_ISUID", EMILUA_DETAIL_INT_CONSTANT(S_ISUID))
        EMILUA_GPERF_PAIR("S_ISGID", EMILUA_DETAIL_INT_CONSTANT(S_ISGID))
        EMILUA_GPERF_PAIR("S_ISVTX", EMILUA_DETAIL_INT_CONSTANT(S_ISVTX))
        // mount() flags
        EMILUA_GPERF_PAIR("MS_REMOUNT", EMILUA_DETAIL_INT_CONSTANT(MS_REMOUNT))
        EMILUA_GPERF_PAIR("MS_BIND", EMILUA_DETAIL_INT_CONSTANT(MS_BIND))
        EMILUA_GPERF_PAIR("MS_SHARED", EMILUA_DETAIL_INT_CONSTANT(MS_SHARED))
        EMILUA_GPERF_PAIR("MS_PRIVATE", EMILUA_DETAIL_INT_CONSTANT(MS_PRIVATE))
        EMILUA_GPERF_PAIR("MS_SLAVE", EMILUA_DETAIL_INT_CONSTANT(MS_SLAVE))
        EMILUA_GPERF_PAIR(
            "MS_UNBINDABLE", EMILUA_DETAIL_INT_CONSTANT(MS_UNBINDABLE))
        EMILUA_GPERF_PAIR("MS_MOVE", EMILUA_DETAIL_INT_CONSTANT(MS_MOVE))
        EMILUA_GPERF_PAIR("MS_DIRSYNC", EMILUA_DETAIL_INT_CONSTANT(MS_DIRSYNC))
        EMILUA_GPERF_PAIR(
            "MS_LAZYTIME", EMILUA_DETAIL_INT_CONSTANT(MS_LAZYTIME))
        EMILUA_GPERF_PAIR(
            "MS_MANDLOCK", EMILUA_DETAIL_INT_CONSTANT(MS_MANDLOCK))
        EMILUA_GPERF_PAIR("MS_NOATIME", EMILUA_DETAIL_INT_CONSTANT(MS_NOATIME))
        EMILUA_GPERF_PAIR("MS_NODEV", EMILUA_DETAIL_INT_CONSTANT(MS_NODEV))
        EMILUA_GPERF_PAIR(
            "MS_NODIRATIME", EMILUA_DETAIL_INT_CONSTANT(MS_NODIRATIME))
        EMILUA_GPERF_PAIR("MS_NOEXEC", EMILUA_DETAIL_INT_CONSTANT(MS_NOEXEC))
        EMILUA_GPERF_PAIR("MS_NOSUID", EMILUA_DETAIL_INT_CONSTANT(MS_NOSUID))
        EMILUA_GPERF_PAIR("MS_RDONLY", EMILUA_DETAIL_INT_CONSTANT(MS_RDONLY))
        EMILUA_GPERF_PAIR("MS_REC", EMILUA_DETAIL_INT_CONSTANT(MS_REC))
        EMILUA_GPERF_PAIR(
            "MS_RELATIME", EMILUA_DETAIL_INT_CONSTANT(MS_RELATIME))
        EMILUA_GPERF_PAIR("MS_SILENT", EMILUA_DETAIL_INT_CONSTANT(MS_SILENT))
        EMILUA_GPERF_PAIR(
            "MS_STRICTATIME", EMILUA_DETAIL_INT_CONSTANT(MS_STRICTATIME))
        EMILUA_GPERF_PAIR(
            "MS_SYNCHRONOUS", EMILUA_DETAIL_INT_CONSTANT(MS_SYNCHRONOUS))
        EMILUA_GPERF_PAIR(
            "MS_NOSYMFOLLOW", EMILUA_DETAIL_INT_CONSTANT(MS_NOSYMFOLLOW))
        // umount2() flags
        EMILUA_GPERF_PAIR("MNT_FORCE", EMILUA_DETAIL_INT_CONSTANT(MNT_FORCE))
        EMILUA_GPERF_PAIR("MNT_DETACH", EMILUA_DETAIL_INT_CONSTANT(MNT_DETACH))
        EMILUA_GPERF_PAIR("MNT_EXPIRE", EMILUA_DETAIL_INT_CONSTANT(MNT_EXPIRE))
        EMILUA_GPERF_PAIR(
            "UMOUNT_NOFOLLOW", EMILUA_DETAIL_INT_CONSTANT(UMOUNT_NOFOLLOW))
        // setns() flags
        EMILUA_GPERF_PAIR(
            "CLONE_NEWCGROUP", EMILUA_DETAIL_INT_CONSTANT(CLONE_NEWCGROUP))
        EMILUA_GPERF_PAIR(
            "CLONE_NEWIPC", EMILUA_DETAIL_INT_CONSTANT(CLONE_NEWIPC))
        EMILUA_GPERF_PAIR(
            "CLONE_NEWNET", EMILUA_DETAIL_INT_CONSTANT(CLONE_NEWNET))
        EMILUA_GPERF_PAIR(
            "CLONE_NEWNS", EMILUA_DETAIL_INT_CONSTANT(CLONE_NEWNS))
        EMILUA_GPERF_PAIR(
            "CLONE_NEWPID", EMILUA_DETAIL_INT_CONSTANT(CLONE_NEWPID))
        EMILUA_GPERF_PAIR(
            "CLONE_NEWTIME", EMILUA_DETAIL_INT_CONSTANT(CLONE_NEWTIME))
        EMILUA_GPERF_PAIR(
            "CLONE_NEWUSER", EMILUA_DETAIL_INT_CONSTANT(CLONE_NEWUSER))
        EMILUA_GPERF_PAIR(
            "CLONE_NEWUTS", EMILUA_DETAIL_INT_CONSTANT(CLONE_NEWUTS))
        // cap_set_secbits() flags
        EMILUA_GPERF_PAIR(
            "SECBIT_NOROOT", EMILUA_DETAIL_INT_CONSTANT(SECBIT_NOROOT))
        EMILUA_GPERF_PAIR(
            "SECBIT_NOROOT_LOCKED",
            EMILUA_DETAIL_INT_CONSTANT(SECBIT_NOROOT_LOCKED))
        EMILUA_GPERF_PAIR(
            "SECBIT_NO_SETUID_FIXUP",
            EMILUA_DETAIL_INT_CONSTANT(SECBIT_NO_SETUID_FIXUP))
        EMILUA_GPERF_PAIR(
            "SECBIT_NO_SETUID_FIXUP_LOCKED",
            EMILUA_DETAIL_INT_CONSTANT(SECBIT_NO_SETUID_FIXUP_LOCKED))
        EMILUA_GPERF_PAIR(
            "SECBIT_KEEP_CAPS", EMILUA_DETAIL_INT_CONSTANT(SECBIT_KEEP_CAPS))
        EMILUA_GPERF_PAIR(
            "SECBIT_KEEP_CAPS_LOCKED",
            EMILUA_DETAIL_INT_CONSTANT(SECBIT_KEEP_CAPS_LOCKED))
        EMILUA_GPERF_PAIR(
            "SECBIT_NO_CAP_AMBIENT_RAISE",
            EMILUA_DETAIL_INT_CONSTANT(SECBIT_NO_CAP_AMBIENT_RAISE))
        EMILUA_GPERF_PAIR(
            "SECBIT_NO_CAP_AMBIENT_RAISE_LOCKED",
            EMILUA_DETAIL_INT_CONSTANT(SECBIT_NO_CAP_AMBIENT_RAISE_LOCKED))
        // ### FUNCTIONS ###
        EMILUA_GPERF_PAIR(
            "read",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, [](lua_State* L) -> int {
                    int fd = luaL_checkinteger(L, 1);
                    int nbyte = luaL_checkinteger(L, 2);
                    void* ud;
                    lua_Alloc a = lua_getallocf(L, &ud);
                    char* buf = static_cast<char*>(a(ud, NULL, 0, nbyte));
                    int res = read(fd, buf, nbyte);
                    int last_error = (res == -1) ? errno : 0;
                    check_last_error(L, last_error);
                    if (last_error == 0) {
                        lua_pushlstring(L, buf, res);
                    } else {
                        lua_pushnil(L);
                    }
                    lua_pushinteger(L, last_error);
                    return 2;
                });
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "write",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, [](lua_State* L) -> int {
                    int fd = luaL_checkinteger(L, 1);
                    std::size_t len;
                    const char* str = lua_tolstring(L, 2, &len);
                    int res = write(fd, str, len);
                    int last_error = (res == -1) ? errno : 0;
                    check_last_error(L, last_error);
                    lua_pushinteger(L, res);
                    lua_pushinteger(L, last_error);
                    return 2;
                });
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "open",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, [](lua_State* L) -> int {
                    lua_settop(L, 3);
                    const char* str = luaL_checkstring(L, 1);
                    int flags = luaL_checkinteger(L, 2);
                    int res;
                    if (lua_isnil(L, 3)) {
                        res = open(str, flags);
                    } else {
                        mode_t mode = luaL_checkinteger(L, 3);
                        res = open(str, flags, mode);
                    }
                    int last_error = (res == -1) ? errno : 0;
                    check_last_error(L, last_error);
                    lua_pushinteger(L, res);
                    lua_pushinteger(L, last_error);
                    return 2;
                });
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "mkdir",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, [](lua_State* L) -> int {
                    const char* path = luaL_checkstring(L, 1);
                    mode_t mode = luaL_checkinteger(L, 2);
                    int res = mkdir(path, mode);
                    int last_error = (res == -1) ? errno : 0;
                    check_last_error(L, last_error);
                    lua_pushinteger(L, res);
                    lua_pushinteger(L, last_error);
                    return 2;
                });
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "chdir",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, [](lua_State* L) -> int {
                    const char* path = luaL_checkstring(L, 1);
                    int res = chdir(path);
                    int last_error = (res == -1) ? errno : 0;
                    check_last_error(L, last_error);
                    lua_pushinteger(L, res);
                    lua_pushinteger(L, last_error);
                    return 2;
                });
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "umask",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, [](lua_State* L) -> int {
                    mode_t res = umask(luaL_checkinteger(L, 1));
                    lua_pushinteger(L, res);
                    return 1;
                });
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "mount",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, [](lua_State* L) -> int {
                    const char* source =
                        lua_isnil(L, 1) ? nullptr : lua_tostring(L, 1);
                    const char* target = luaL_checkstring(L, 2);
                    const char* fstype =
                        lua_isnil(L, 3) ? nullptr : lua_tostring(L, 3);
                    unsigned long flags = luaL_checkinteger(L, 4);
                    const void* data =
                        lua_isnil(L, 5) ? nullptr : lua_tostring(L, 5);
                    int res = mount(source, target, fstype, flags, data);
                    int last_error = (res == -1) ? errno : 0;
                    check_last_error(L, last_error);
                    lua_pushinteger(L, res);
                    lua_pushinteger(L, last_error);
                    return 2;
                });
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "umount",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, [](lua_State* L) -> int {
                    const char* target = luaL_checkstring(L, 1);
                    int res = umount(target);
                    int last_error = (res == -1) ? errno : 0;
                    check_last_error(L, last_error);
                    lua_pushinteger(L, res);
                    lua_pushinteger(L, last_error);
                    return 2;
                });
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "umount2",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, [](lua_State* L) -> int {
                    const char* target = luaL_checkstring(L, 1);
                    int flags = luaL_checkinteger(L, 2);
                    int res = umount2(target, flags);
                    int last_error = (res == -1) ? errno : 0;
                    check_last_error(L, last_error);
                    lua_pushinteger(L, res);
                    lua_pushinteger(L, last_error);
                    return 2;
                });
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "pivot_root",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, [](lua_State* L) -> int {
                    const char* new_root = luaL_checkstring(L, 1);
                    const char* put_old = luaL_checkstring(L, 2);
                    int res = syscall(SYS_pivot_root, new_root, put_old);
                    int last_error = (res == -1) ? errno : 0;
                    check_last_error(L, last_error);
                    lua_pushinteger(L, res);
                    lua_pushinteger(L, last_error);
                    return 2;
                });
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "chroot",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, [](lua_State* L) -> int {
                    const char* path = luaL_checkstring(L, 1);
                    int res = chroot(path);
                    int last_error = (res == -1) ? errno : 0;
                    check_last_error(L, last_error);
                    lua_pushinteger(L, res);
                    lua_pushinteger(L, last_error);
                    return 2;
                });
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "sethostname",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, [](lua_State* L) -> int {
                    std::size_t len;
                    const char* str = lua_tolstring(L, 1, &len);
                    int res = sethostname(str, len);
                    int last_error = (res == -1) ? errno : 0;
                    check_last_error(L, last_error);
                    lua_pushinteger(L, res);
                    lua_pushinteger(L, last_error);
                    return 2;
                });
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "setdomainname",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, [](lua_State* L) -> int {
                    std::size_t len;
                    const char* str = lua_tolstring(L, 1, &len);
                    int res = setdomainname(str, len);
                    int last_error = (res == -1) ? errno : 0;
                    check_last_error(L, last_error);
                    lua_pushinteger(L, res);
                    lua_pushinteger(L, last_error);
                    return 2;
                });
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "setsid",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, [](lua_State* L) -> int {
                    pid_t res = setsid();
                    int last_error = (res == -1) ? errno : 0;
                    check_last_error(L, last_error);
                    lua_pushinteger(L, res);
                    lua_pushinteger(L, last_error);
                    return 2;
                });
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "setpgid",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, [](lua_State* L) -> int {
                    pid_t pid = luaL_checkinteger(L, 1);
                    pid_t pgid = luaL_checkinteger(L, 2);
                    int res = setpgid(pid, pgid);
                    int last_error = (res == -1) ? errno : 0;
                    check_last_error(L, last_error);
                    lua_pushinteger(L, res);
                    lua_pushinteger(L, last_error);
                    return 2;
                });
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "setresuid",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, [](lua_State* L) -> int {
                    uid_t ruid = luaL_checkinteger(L, 1);
                    uid_t euid = luaL_checkinteger(L, 2);
                    uid_t suid = luaL_checkinteger(L, 3);
                    int res = setresuid(ruid, euid, suid);
                    int last_error = (res == -1) ? errno : 0;
                    check_last_error(L, last_error);
                    lua_pushinteger(L, res);
                    lua_pushinteger(L, last_error);
                    return 2;
                });
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "setresgid",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, [](lua_State* L) -> int {
                    gid_t rgid = luaL_checkinteger(L, 1);
                    gid_t egid = luaL_checkinteger(L, 2);
                    gid_t sgid = luaL_checkinteger(L, 3);
                    int res = setresgid(rgid, egid, sgid);
                    int last_error = (res == -1) ? errno : 0;
                    check_last_error(L, last_error);
                    lua_pushinteger(L, res);
                    lua_pushinteger(L, last_error);
                    return 2;
                });
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "setgroups",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, [](lua_State* L) -> int {
                    luaL_checktype(L, 1, LUA_TTABLE);
                    std::vector<gid_t> groups;
                    for (int i = 0 ;; ++i) {
                        lua_rawgeti(L, 1, i + 1);
                        switch (lua_type(L, -1)) {
                        case LUA_TNIL: {
                            int res = setgroups(groups.size(), groups.data());
                            int last_error = (res == -1) ? errno : 0;
                            check_last_error(L, last_error);
                            lua_pushinteger(L, res);
                            lua_pushinteger(L, last_error);
                            return 2;
                        }
                        case LUA_TNUMBER:
                            groups.emplace_back(lua_tointeger(L, -1));
                            lua_pop(L, 1);
                            break;
                        default:
                            errno = EINVAL;
                            perror("<3>linux_namespaces/init/setgroups");
                            std::exit(1);
                        }
                    }
                });
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "cap_set_proc",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, [](lua_State* L) -> int {
                    const char* caps = luaL_checkstring(L, 1);
                    cap_t caps2 = cap_from_text(caps);
                    if (caps2 == NULL) {
                        perror("<3>linux_namespaces/init/cap_set_proc");
                        std::exit(1);
                    }
                    BOOST_SCOPE_EXIT_ALL(&) { cap_free(caps2); };
                    int res = cap_set_proc(caps2);
                    int last_error = (res == -1) ? errno : 0;
                    check_last_error(L, last_error);
                    lua_pushinteger(L, res);
                    lua_pushinteger(L, last_error);
                    return 2;
                });
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "cap_drop_bound",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, [](lua_State* L) -> int {
                    const char* name = luaL_checkstring(L, 1);
                    cap_value_t cap;
                    if (cap_from_name(name, &cap) == -1) {
                        perror("<3>linux_namespaces/init/cap_drop_bound");
                        std::exit(1);
                    }
                    int res = cap_drop_bound(cap);
                    int last_error = (res == -1) ? errno : 0;
                    check_last_error(L, last_error);
                    lua_pushinteger(L, res);
                    lua_pushinteger(L, last_error);
                    return 2;
                });
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "cap_set_ambient",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, [](lua_State* L) -> int {
                    const char* name = luaL_checkstring(L, 1);
                    cap_value_t cap;
                    if (cap_from_name(name, &cap) == -1) {
                        perror("<3>linux_namespaces/init/cap_set_ambient");
                        std::exit(1);
                    }

                    luaL_checktype(L, 2, LUA_TBOOLEAN);
                    cap_flag_value_t value =
                        lua_toboolean(L, 2) ? CAP_SET : CAP_CLEAR;

                    int res = cap_set_ambient(cap, value);
                    int last_error = (res == -1) ? errno : 0;
                    check_last_error(L, last_error);
                    lua_pushinteger(L, res);
                    lua_pushinteger(L, last_error);
                    return 2;
                });
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "cap_reset_ambient",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, [](lua_State* L) -> int {
                    int res = cap_reset_ambient();
                    int last_error = (res == -1) ? errno : 0;
                    check_last_error(L, last_error);
                    lua_pushinteger(L, res);
                    lua_pushinteger(L, last_error);
                    return 2;
                });
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "cap_set_secbits",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, [](lua_State* L) -> int {
                    int res = cap_set_secbits(luaL_checkinteger(L, 1));
                    int last_error = (res == -1) ? errno : 0;
                    check_last_error(L, last_error);
                    lua_pushinteger(L, res);
                    lua_pushinteger(L, last_error);
                    return 2;
                });
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "unshare",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, [](lua_State* L) -> int {
                    int nstype = luaL_checkinteger(L, 1);
                    int res = unshare(nstype);
                    int last_error = (res == -1) ? errno : 0;
                    check_last_error(L, last_error);
                    lua_pushinteger(L, res);
                    lua_pushinteger(L, last_error);
                    return 2;
                });
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "setns",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, [](lua_State* L) -> int {
                    int fd = luaL_checkinteger(L, 1);
                    int nstype = luaL_checkinteger(L, 2);
                    int res = setns(fd, nstype);
                    int last_error = (res == -1) ? errno : 0;
                    check_last_error(L, last_error);
                    lua_pushinteger(L, res);
                    lua_pushinteger(L, last_error);
                    return 2;
                });
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "execve",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, [](lua_State* L) -> int {
                    lua_settop(L, 3);

                    const char* pathname = luaL_checkstring(L, 1);

                    std::vector<std::string> argvbuf;
                    switch (lua_type(L, 2)) {
                    case LUA_TNIL:
                        break;
                    case LUA_TTABLE:
                        for (int i = 1 ;; ++i) {
                            lua_rawgeti(L, 2, i);
                            switch (lua_type(L, -1)) {
                            case LUA_TNIL:
                                goto end_for;
                            case LUA_TSTRING:
                                argvbuf.emplace_back(tostringview(L));
                                lua_pop(L, 1);
                                break;
                            default:
                                errno = EINVAL;
                                perror("<3>linux_namespaces/init/execve");
                                std::exit(1);
                            }
                        }
                    end_for:
                        break;
                    default:
                        errno = EINVAL;
                        perror("<3>linux_namespaces/init/execve");
                        std::exit(1);
                    }

                    std::vector<std::string> envpbuf;
                    switch (lua_type(L, 3)) {
                    case LUA_TNIL:
                        break;
                    case LUA_TTABLE:
                        lua_pushnil(L);
                        while (lua_next(L, 3) != 0) {
                            if (
                                lua_type(L, -2) != LUA_TSTRING ||
                                lua_type(L, -1) != LUA_TSTRING
                            ) {
                                errno = EINVAL;
                                perror("<3>linux_namespaces/init/execve");
                                std::exit(1);
                            }

                            envpbuf.emplace_back(tostringview(L, -2));
                            envpbuf.back() += '=';
                            envpbuf.back() += tostringview(L, -1);
                            lua_pop(L, 1);
                        }
                        break;
                    default:
                        errno = EINVAL;
                        perror("<3>linux_namespaces/init/execve");
                        std::exit(1);
                    }

                    std::vector<char*> argv;
                    argv.reserve(argvbuf.size() + 1);
                    for (auto& a : argvbuf) {
                        argv.emplace_back(a.data());
                    }
                    argv.emplace_back(nullptr);

                    std::vector<char*> envp;
                    envp.reserve(envpbuf.size() + 1);
                    for (auto& e : envpbuf) {
                        envp.emplace_back(e.data());
                    }
                    envp.emplace_back(nullptr);

                    int res = execve(pathname, argv.data(), envp.data());
                    int last_error = errno;
                    check_last_error(L, last_error);
                    lua_pushinteger(L, res);
                    lua_pushinteger(L, last_error);
                    return 2;
                });
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "fexecve",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, [](lua_State* L) -> int {
                    lua_settop(L, 3);

                    int fd = luaL_checkinteger(L, 1);

                    std::vector<std::string> argvbuf;
                    switch (lua_type(L, 2)) {
                    case LUA_TNIL:
                        break;
                    case LUA_TTABLE:
                        for (int i = 1 ;; ++i) {
                            lua_rawgeti(L, 2, i);
                            switch (lua_type(L, -1)) {
                            case LUA_TNIL:
                                goto end_for;
                            case LUA_TSTRING:
                                argvbuf.emplace_back(tostringview(L));
                                lua_pop(L, 1);
                                break;
                            default:
                                errno = EINVAL;
                                perror("<3>linux_namespaces/init/fexecve");
                                std::exit(1);
                            }
                        }
                    end_for:
                        break;
                    default:
                        errno = EINVAL;
                        perror("<3>linux_namespaces/init/fexecve");
                        std::exit(1);
                    }

                    std::vector<std::string> envpbuf;
                    switch (lua_type(L, 3)) {
                    case LUA_TNIL:
                        break;
                    case LUA_TTABLE:
                        lua_pushnil(L);
                        while (lua_next(L, 3) != 0) {
                            if (
                                lua_type(L, -2) != LUA_TSTRING ||
                                lua_type(L, -1) != LUA_TSTRING
                            ) {
                                errno = EINVAL;
                                perror("<3>linux_namespaces/init/fexecve");
                                std::exit(1);
                            }

                            envpbuf.emplace_back(tostringview(L, -2));
                            envpbuf.back() += '=';
                            envpbuf.back() += tostringview(L, -1);
                            lua_pop(L, 1);
                        }
                        break;
                    default:
                        errno = EINVAL;
                        perror("<3>linux_namespaces/init/fexecve");
                        std::exit(1);
                    }

                    std::vector<char*> argv;
                    argv.reserve(argvbuf.size() + 1);
                    for (auto& a : argvbuf) {
                        argv.emplace_back(a.data());
                    }
                    argv.emplace_back(nullptr);

                    std::vector<char*> envp;
                    envp.reserve(envpbuf.size() + 1);
                    for (auto& e : envpbuf) {
                        envp.emplace_back(e.data());
                    }
                    envp.emplace_back(nullptr);

                    int res = fexecve(fd, argv.data(), envp.data());
                    int last_error = errno;
                    check_last_error(L, last_error);
                    lua_pushinteger(L, res);
                    lua_pushinteger(L, last_error);
                    return 2;
                });
                return 1;
            })
    EMILUA_GPERF_END(key)(L);
}

} // namespace emilua
