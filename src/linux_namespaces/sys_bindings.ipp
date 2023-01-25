namespace emilua {

static int posix_mt_index(lua_State* L)
{
    static constexpr auto check_last_error = [](lua_State* L, int last_error) {
        if (last_error != 0) {
            lua_getfield(L, LUA_GLOBALSINDEX, "errexit");
            if (lua_toboolean(L, -1)) {
                errno = last_error;
                perror("<3>linux_namespaces/init");
                std::exit(1);
            }
        }
    };

    auto key = tostringview(L, 2);

    if (false) {
    // ### CONSTANTS ###
#define EMILUA_DETAIL_INT_CONSTANT(X) \
    } else if (key == #X) {           \
        lua_pushinteger(L, X);        \
        return 1;
    EMILUA_DETAIL_INT_CONSTANT(O_CLOEXEC)
    // errno
    EMILUA_DETAIL_INT_CONSTANT(EAFNOSUPPORT)
    EMILUA_DETAIL_INT_CONSTANT(EADDRINUSE)
    EMILUA_DETAIL_INT_CONSTANT(EADDRNOTAVAIL)
    EMILUA_DETAIL_INT_CONSTANT(EISCONN)
    EMILUA_DETAIL_INT_CONSTANT(E2BIG)
    EMILUA_DETAIL_INT_CONSTANT(EDOM)
    EMILUA_DETAIL_INT_CONSTANT(EFAULT)
    EMILUA_DETAIL_INT_CONSTANT(EBADF)
    EMILUA_DETAIL_INT_CONSTANT(EBADMSG)
    EMILUA_DETAIL_INT_CONSTANT(EPIPE)
    EMILUA_DETAIL_INT_CONSTANT(ECONNABORTED)
    EMILUA_DETAIL_INT_CONSTANT(EALREADY)
    EMILUA_DETAIL_INT_CONSTANT(ECONNREFUSED)
    EMILUA_DETAIL_INT_CONSTANT(ECONNRESET)
    EMILUA_DETAIL_INT_CONSTANT(EXDEV)
    EMILUA_DETAIL_INT_CONSTANT(EDESTADDRREQ)
    EMILUA_DETAIL_INT_CONSTANT(EBUSY)
    EMILUA_DETAIL_INT_CONSTANT(ENOTEMPTY)
    EMILUA_DETAIL_INT_CONSTANT(ENOEXEC)
    EMILUA_DETAIL_INT_CONSTANT(EEXIST)
    EMILUA_DETAIL_INT_CONSTANT(EFBIG)
    EMILUA_DETAIL_INT_CONSTANT(ENAMETOOLONG)
    EMILUA_DETAIL_INT_CONSTANT(ENOSYS)
    EMILUA_DETAIL_INT_CONSTANT(EHOSTUNREACH)
    EMILUA_DETAIL_INT_CONSTANT(EIDRM)
    EMILUA_DETAIL_INT_CONSTANT(EILSEQ)
    EMILUA_DETAIL_INT_CONSTANT(ENOTTY)
    EMILUA_DETAIL_INT_CONSTANT(EINTR)
    EMILUA_DETAIL_INT_CONSTANT(EINVAL)
    EMILUA_DETAIL_INT_CONSTANT(ESPIPE)
    EMILUA_DETAIL_INT_CONSTANT(EIO)
    EMILUA_DETAIL_INT_CONSTANT(EISDIR)
    EMILUA_DETAIL_INT_CONSTANT(EMSGSIZE)
    EMILUA_DETAIL_INT_CONSTANT(ENETDOWN)
    EMILUA_DETAIL_INT_CONSTANT(ENETRESET)
    EMILUA_DETAIL_INT_CONSTANT(ENETUNREACH)
    EMILUA_DETAIL_INT_CONSTANT(ENOBUFS)
    EMILUA_DETAIL_INT_CONSTANT(ECHILD)
    EMILUA_DETAIL_INT_CONSTANT(ENOLINK)
    EMILUA_DETAIL_INT_CONSTANT(ENOLCK)
    EMILUA_DETAIL_INT_CONSTANT(ENODATA)
    EMILUA_DETAIL_INT_CONSTANT(ENOMSG)
    EMILUA_DETAIL_INT_CONSTANT(ENOPROTOOPT)
    EMILUA_DETAIL_INT_CONSTANT(ENOSPC)
    EMILUA_DETAIL_INT_CONSTANT(ENOSR)
    EMILUA_DETAIL_INT_CONSTANT(ENXIO)
    EMILUA_DETAIL_INT_CONSTANT(ENODEV)
    EMILUA_DETAIL_INT_CONSTANT(ENOENT)
    EMILUA_DETAIL_INT_CONSTANT(ESRCH)
    EMILUA_DETAIL_INT_CONSTANT(ENOTDIR)
    EMILUA_DETAIL_INT_CONSTANT(ENOTSOCK)
    EMILUA_DETAIL_INT_CONSTANT(ENOSTR)
    EMILUA_DETAIL_INT_CONSTANT(ENOTCONN)
    EMILUA_DETAIL_INT_CONSTANT(ENOMEM)
    EMILUA_DETAIL_INT_CONSTANT(ENOTSUP)
    EMILUA_DETAIL_INT_CONSTANT(ECANCELED)
    EMILUA_DETAIL_INT_CONSTANT(EINPROGRESS)
    EMILUA_DETAIL_INT_CONSTANT(EPERM)
    EMILUA_DETAIL_INT_CONSTANT(EOPNOTSUPP)
    EMILUA_DETAIL_INT_CONSTANT(EWOULDBLOCK)
    EMILUA_DETAIL_INT_CONSTANT(EOWNERDEAD)
    EMILUA_DETAIL_INT_CONSTANT(EACCES)
    EMILUA_DETAIL_INT_CONSTANT(EPROTO)
    EMILUA_DETAIL_INT_CONSTANT(EPROTONOSUPPORT)
    EMILUA_DETAIL_INT_CONSTANT(EROFS)
    EMILUA_DETAIL_INT_CONSTANT(EDEADLK)
    EMILUA_DETAIL_INT_CONSTANT(EAGAIN)
    EMILUA_DETAIL_INT_CONSTANT(ERANGE)
    EMILUA_DETAIL_INT_CONSTANT(ENOTRECOVERABLE)
    EMILUA_DETAIL_INT_CONSTANT(ETIME)
    EMILUA_DETAIL_INT_CONSTANT(ETXTBSY)
    EMILUA_DETAIL_INT_CONSTANT(ETIMEDOUT)
    EMILUA_DETAIL_INT_CONSTANT(ENFILE)
    EMILUA_DETAIL_INT_CONSTANT(EMFILE)
    EMILUA_DETAIL_INT_CONSTANT(EMLINK)
    EMILUA_DETAIL_INT_CONSTANT(ELOOP)
    EMILUA_DETAIL_INT_CONSTANT(EOVERFLOW)
    EMILUA_DETAIL_INT_CONSTANT(EPROTOTYPE)
    // file creation flags
    EMILUA_DETAIL_INT_CONSTANT(O_CREAT)
    EMILUA_DETAIL_INT_CONSTANT(O_RDONLY)
    EMILUA_DETAIL_INT_CONSTANT(O_WRONLY)
    EMILUA_DETAIL_INT_CONSTANT(O_RDWR)
    EMILUA_DETAIL_INT_CONSTANT(O_DIRECTORY)
    EMILUA_DETAIL_INT_CONSTANT(O_EXCL)
    EMILUA_DETAIL_INT_CONSTANT(O_NOCTTY)
    EMILUA_DETAIL_INT_CONSTANT(O_NOFOLLOW)
    EMILUA_DETAIL_INT_CONSTANT(O_TMPFILE)
    EMILUA_DETAIL_INT_CONSTANT(O_TRUNC)
    // file status flags
    EMILUA_DETAIL_INT_CONSTANT(O_APPEND)
    EMILUA_DETAIL_INT_CONSTANT(O_ASYNC)
    EMILUA_DETAIL_INT_CONSTANT(O_DIRECT)
    EMILUA_DETAIL_INT_CONSTANT(O_DSYNC)
    EMILUA_DETAIL_INT_CONSTANT(O_LARGEFILE)
    EMILUA_DETAIL_INT_CONSTANT(O_NOATIME)
    EMILUA_DETAIL_INT_CONSTANT(O_NONBLOCK)
    EMILUA_DETAIL_INT_CONSTANT(O_PATH)
    EMILUA_DETAIL_INT_CONSTANT(O_SYNC)
    // mode_t constants
    EMILUA_DETAIL_INT_CONSTANT(S_IRWXU)
    EMILUA_DETAIL_INT_CONSTANT(S_IRUSR)
    EMILUA_DETAIL_INT_CONSTANT(S_IWUSR)
    EMILUA_DETAIL_INT_CONSTANT(S_IXUSR)
    EMILUA_DETAIL_INT_CONSTANT(S_IRWXG)
    EMILUA_DETAIL_INT_CONSTANT(S_IRGRP)
    EMILUA_DETAIL_INT_CONSTANT(S_IWGRP)
    EMILUA_DETAIL_INT_CONSTANT(S_IXGRP)
    EMILUA_DETAIL_INT_CONSTANT(S_IRWXO)
    EMILUA_DETAIL_INT_CONSTANT(S_IROTH)
    EMILUA_DETAIL_INT_CONSTANT(S_IWOTH)
    EMILUA_DETAIL_INT_CONSTANT(S_IXOTH)
    EMILUA_DETAIL_INT_CONSTANT(S_ISUID)
    EMILUA_DETAIL_INT_CONSTANT(S_ISGID)
    EMILUA_DETAIL_INT_CONSTANT(S_ISVTX)
    // mount() flags
    EMILUA_DETAIL_INT_CONSTANT(MS_REMOUNT)
    EMILUA_DETAIL_INT_CONSTANT(MS_BIND)
    EMILUA_DETAIL_INT_CONSTANT(MS_SHARED)
    EMILUA_DETAIL_INT_CONSTANT(MS_PRIVATE)
    EMILUA_DETAIL_INT_CONSTANT(MS_SLAVE)
    EMILUA_DETAIL_INT_CONSTANT(MS_UNBINDABLE)
    EMILUA_DETAIL_INT_CONSTANT(MS_MOVE)
    EMILUA_DETAIL_INT_CONSTANT(MS_DIRSYNC)
    EMILUA_DETAIL_INT_CONSTANT(MS_LAZYTIME)
    EMILUA_DETAIL_INT_CONSTANT(MS_MANDLOCK)
    EMILUA_DETAIL_INT_CONSTANT(MS_NOATIME)
    EMILUA_DETAIL_INT_CONSTANT(MS_NODEV)
    EMILUA_DETAIL_INT_CONSTANT(MS_NODIRATIME)
    EMILUA_DETAIL_INT_CONSTANT(MS_NOEXEC)
    EMILUA_DETAIL_INT_CONSTANT(MS_NOSUID)
    EMILUA_DETAIL_INT_CONSTANT(MS_RDONLY)
    EMILUA_DETAIL_INT_CONSTANT(MS_REC)
    EMILUA_DETAIL_INT_CONSTANT(MS_RELATIME)
    EMILUA_DETAIL_INT_CONSTANT(MS_SILENT)
    EMILUA_DETAIL_INT_CONSTANT(MS_STRICTATIME)
    EMILUA_DETAIL_INT_CONSTANT(MS_SYNCHRONOUS)
    EMILUA_DETAIL_INT_CONSTANT(MS_NOSYMFOLLOW)
    // umount2() flags
    EMILUA_DETAIL_INT_CONSTANT(MNT_FORCE)
    EMILUA_DETAIL_INT_CONSTANT(MNT_DETACH)
    EMILUA_DETAIL_INT_CONSTANT(MNT_EXPIRE)
    EMILUA_DETAIL_INT_CONSTANT(UMOUNT_NOFOLLOW)
    // setns() flags
    EMILUA_DETAIL_INT_CONSTANT(CLONE_NEWCGROUP)
    EMILUA_DETAIL_INT_CONSTANT(CLONE_NEWIPC)
    EMILUA_DETAIL_INT_CONSTANT(CLONE_NEWNET)
    EMILUA_DETAIL_INT_CONSTANT(CLONE_NEWNS)
    EMILUA_DETAIL_INT_CONSTANT(CLONE_NEWPID)
    EMILUA_DETAIL_INT_CONSTANT(CLONE_NEWTIME)
    EMILUA_DETAIL_INT_CONSTANT(CLONE_NEWUSER)
    EMILUA_DETAIL_INT_CONSTANT(CLONE_NEWUTS)
#undef EMILUA_DETAIL_INT_CONSTANT
    // ### FUNCTIONS ###
    } else if (key == "read") {
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
    } else if (key == "write") {
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
    } else if (key == "open") {
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
    } else if (key == "mkdir") {
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
    } else if (key == "chdir") {
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
    } else if (key == "umask") {
        lua_pushcfunction(L, [](lua_State* L) -> int {
            mode_t res = umask(luaL_checkinteger(L, 1));
            lua_pushinteger(L, res);
            return 1;
        });
        return 1;
    } else if (key == "mount") {
        lua_pushcfunction(L, [](lua_State* L) -> int {
            const char* source = lua_isnil(L, 1) ? nullptr : lua_tostring(L, 1);
            const char* target = luaL_checkstring(L, 2);
            const char* fstype = lua_isnil(L, 3) ? nullptr : lua_tostring(L, 3);
            unsigned long flags = luaL_checkinteger(L, 4);
            const void* data = lua_isnil(L, 5) ? nullptr : lua_tostring(L, 5);
            int res = mount(source, target, fstype, flags, data);
            int last_error = (res == -1) ? errno : 0;
            check_last_error(L, last_error);
            lua_pushinteger(L, res);
            lua_pushinteger(L, last_error);
            return 2;
        });
        return 1;
    } else if (key == "umount") {
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
    } else if (key == "umount2") {
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
    } else if (key == "pivot_root") {
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
    } else if (key == "chroot") {
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
    } else if (key == "sethostname") {
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
    } else if (key == "setdomainname") {
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
    } else if (key == "setresuid") {
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
    } else if (key == "setresgid") {
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
    } else if (key == "setgroups") {
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
    } else if (key == "cap_set_proc") {
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
    } else if (key == "cap_drop_bound") {
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
    } else if (key == "cap_set_ambient") {
        lua_pushcfunction(L, [](lua_State* L) -> int {
            const char* name = luaL_checkstring(L, 1);
            cap_value_t cap;
            if (cap_from_name(name, &cap) == -1) {
                perror("<3>linux_namespaces/init/cap_set_ambient");
                std::exit(1);
            }

            luaL_checktype(L, 2, LUA_TBOOLEAN);
            cap_flag_value_t value = lua_toboolean(L, 2) ? CAP_SET : CAP_CLEAR;

            int res = cap_set_ambient(cap, value);
            int last_error = (res == -1) ? errno : 0;
            check_last_error(L, last_error);
            lua_pushinteger(L, res);
            lua_pushinteger(L, last_error);
            return 2;
        });
        return 1;
    } else if (key == "cap_reset_ambient") {
        lua_pushcfunction(L, [](lua_State* L) -> int {
            int res = cap_reset_ambient();
            int last_error = (res == -1) ? errno : 0;
            check_last_error(L, last_error);
            lua_pushinteger(L, res);
            lua_pushinteger(L, last_error);
            return 2;
        });
        return 1;
    } else if (key == "cap_set_secbits") {
        lua_pushcfunction(L, [](lua_State* L) -> int {
            int res = cap_set_secbits(luaL_checkinteger(L, 1));
            int last_error = (res == -1) ? errno : 0;
            check_last_error(L, last_error);
            lua_pushinteger(L, res);
            lua_pushinteger(L, last_error);
            return 2;
        });
        return 1;
    } else if (key == "setns") {
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
    } else if (key == "execve") {
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
    } else if (key == "fexecve") {
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
    } else {
        return luaL_error(L, "key not found");
    }
}

} // namespace emilua
