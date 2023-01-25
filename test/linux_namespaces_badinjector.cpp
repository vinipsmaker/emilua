/* Copyright (c) 2023 Vinícius dos Santos Oliveira

   Distributed under the Boost Software License, Version 1.0. (See accompanying
   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt) */

#include <emilua/linux_namespaces.hpp>
#include <emilua/plugin.hpp>
#include <emilua/actor.hpp>

#include <boost/archive/iterators/base64_from_binary.hpp>
#include <boost/archive/iterators/transform_width.hpp>

#include <random>

namespace emilua {

class linux_namespaces_badinjector_plugin : plugin
{
public:
    void init_appctx(app_context&) noexcept override;

    std::error_code init_lua_module(
        emilua::vm_context& /*vm_ctx*/, lua_State* L) override
    {
        lua_newtable(L);

        lua_pushinteger(
            L, EMILUA_CONFIG_LINUX_NAMESPACES_MESSAGE_MAX_MEMBERS_NUMBER);
        lua_setfield(L, -2, "CONFIG_MESSAGE_MAX_MEMBERS_NUMBER");

        lua_pushcfunction(L, to_base64);
        lua_setfield(L, -2, "to_base64");

        lua_pushcfunction(L, padding_exists);
        lua_setfield(L, -2, "padding_exists");

        lua_pushcfunction(L, send_too_small);
        lua_setfield(L, -2, "send_too_small");

        lua_pushcfunction(L, send_too_big);
        lua_setfield(L, -2, "send_too_big");

        lua_pushcfunction(L, send_too_many_fds);
        lua_setfield(L, -2, "send_too_many_fds");

        lua_pushcfunction(L, send_invalid_root);
        lua_setfield(L, -2, "send_invalid_root");

        lua_pushcfunction(L, send_overflow_root_str);
        lua_setfield(L, -2, "send_overflow_root_str");

        lua_pushcfunction(L, send_missing_root_fd);
        lua_setfield(L, -2, "send_missing_root_fd");

        lua_pushcfunction(L, send_missing_root_actorfd);
        lua_setfield(L, -2, "send_missing_root_actorfd");

        lua_pushcfunction(L, send_overflow_dict);
        lua_setfield(L, -2, "send_overflow_dict");

        lua_pushcfunction(L, send_overflow_dict2);
        lua_setfield(L, -2, "send_overflow_dict2");

        lua_pushcfunction(L, send_invalid_leaf);
        lua_setfield(L, -2, "send_invalid_leaf");

        lua_pushcfunction(L, send_overflow_empty_key_str);
        lua_setfield(L, -2, "send_overflow_empty_key_str");

        lua_pushcfunction(L, fuzzer_seed);
        lua_setfield(L, -2, "fuzzer_seed");

        lua_pushcfunction(L, fuzzer_send_good);
        lua_setfield(L, -2, "fuzzer_send_good");

        lua_pushcfunction(L, fuzzer_send_bad);
        lua_setfield(L, -2, "fuzzer_send_bad");

        return {};
    }

private:
    struct file_descriptor {};
    struct actor_address {};
    using leaf_t = std::variant<
        bool, double, std::string, file_descriptor, actor_address
    >;

    static constexpr int CONFIG_MESSAGE_MAX_MEMBERS_NUMBER =
        EMILUA_CONFIG_LINUX_NAMESPACES_MESSAGE_MAX_MEMBERS_NUMBER;

    static int to_base64(lua_State* L);

    static int padding_exists(lua_State* L);
    static int send_too_small(lua_State* L);
    static int send_too_big(lua_State* L);
    static int send_too_many_fds(lua_State* L);
    static int send_invalid_root(lua_State* L);
    static int send_overflow_root_str(lua_State* L);
    static int send_missing_root_fd(lua_State* L);
    static int send_missing_root_actorfd(lua_State* L);
    static int send_overflow_dict(lua_State* L);
    static int send_overflow_dict2(lua_State* L);
    static int send_invalid_leaf(lua_State* L);
    static int send_overflow_empty_key_str(lua_State* L);

    static std::string generate_good_string();
    static leaf_t generate_good_leaf();
    static std::map<std::string, leaf_t> generate_good_dict();
    static std::tuple<std::size_t, int>
    generate_good_message(linux_container_message& message, int descriptors[]);

    static int fuzzer_seed(lua_State* L);
    static int fuzzer_send_good(lua_State* L);
    static int fuzzer_send_bad(lua_State* L);

    static std::mt19937 prng;
    static std::uniform_int_distribution<std::uint8_t> leaf_t_dist;
    static std::uniform_int_distribution<std::uint8_t> bool_dist;
    static std::uniform_int_distribution<std::uint64_t> double_dist;
    static std::uniform_int_distribution<char> char_dist;
    static std::uniform_int_distribution<std::uint8_t> str_size_dist;
    static std::uniform_int_distribution<std::uint8_t> dict_size_dist;
    static std::uniform_int_distribution<std::uint64_t>
    unknown_snan_mantissa_dist;

    static int pipefds[2];
};

std::mt19937 linux_namespaces_badinjector_plugin::prng;

std::uniform_int_distribution<std::uint8_t>
linux_namespaces_badinjector_plugin::leaf_t_dist;

std::uniform_int_distribution<std::uint8_t>
linux_namespaces_badinjector_plugin::bool_dist;

std::uniform_int_distribution<std::uint64_t>
linux_namespaces_badinjector_plugin::double_dist;

std::uniform_int_distribution<char>
linux_namespaces_badinjector_plugin::char_dist;

std::uniform_int_distribution<std::uint8_t>
linux_namespaces_badinjector_plugin::str_size_dist;

std::uniform_int_distribution<std::uint8_t>
linux_namespaces_badinjector_plugin::dict_size_dist;

std::uniform_int_distribution<std::uint64_t>
linux_namespaces_badinjector_plugin::unknown_snan_mantissa_dist;

int linux_namespaces_badinjector_plugin::pipefds[2];

inline void set_as_blocking(int fd)
{
    int flags = fcntl(fd, F_GETFL);
    if (flags == -1) {
        perror("F_GETFL");
        std::exit(1);
    }
    flags &= ~O_NONBLOCK;
    if (fcntl(fd, F_SETFL, flags) == -1) {
        perror("F_SETFL");
        std::exit(1);
    }
}

void linux_namespaces_badinjector_plugin::init_appctx(app_context&) noexcept
{
    leaf_t_dist.param(decltype(leaf_t_dist)::param_type{1, 5});
    bool_dist.param(decltype(bool_dist)::param_type{0, 1});
    double_dist.param(decltype(double_dist)::param_type{0, UINT64_MAX});
    char_dist.param(decltype(char_dist)::param_type{CHAR_MIN, CHAR_MAX});
    str_size_dist.param(decltype(str_size_dist)::param_type{0, UINT8_MAX});
    dict_size_dist.param(decltype(dict_size_dist)::param_type{
        1, CONFIG_MESSAGE_MAX_MEMBERS_NUMBER});
    unknown_snan_mantissa_dist.param(
        decltype(unknown_snan_mantissa_dist)::param_type{
            7, MANTISSA_MASK ^ QNAN_BIT});

    if (pipe(pipefds) == -1) {
        perror("badinjector_plugin/init_appctx/pipe");
        std::abort();
    }
}

int linux_namespaces_badinjector_plugin::to_base64(lua_State* L)
{
    using boost::archive::iterators::base64_from_binary;
    using boost::archive::iterators::transform_width;
    using base64_text = base64_from_binary<transform_width<const char*, 6, 8>>;

    auto str = tostringview(L, 1);

    std::string ret{
        base64_text{str.data()},
        base64_text{str.data() + str.size()}};
    ret.append((3 - str.size() % 3) % 3, '=');

    push(L, ret);
    return 1;
}

int linux_namespaces_badinjector_plugin::padding_exists(lua_State* L)
{
    linux_container_message message;
    std::memset(&message, 0xFF, sizeof(message));
    for (int i = 0 ; i != CONFIG_MESSAGE_MAX_MEMBERS_NUMBER ; ++i) {
        message.members[i].as_int = 0;
    }
    for (int i = 0 ; i != sizeof(message.strbuf) ; ++i) {
        message.strbuf[i] = 0;
    }
    unsigned char* p = reinterpret_cast<unsigned char*>(&message);
    unsigned char res = 0;
    for (std::size_t i = 0 ; i != sizeof(message) ; ++i) {
        res |= p[i];
    }
    lua_pushboolean(L, (res != 0) ? 1 : 0);
    return 1;
}

int linux_namespaces_badinjector_plugin::send_too_small(lua_State* L)
{
    auto channel = static_cast<linux_container_address*>(lua_touserdata(L, 1));
    auto channelfd = channel->dest.native_handle();
    linux_container_message message;

    set_as_blocking(channelfd);

    auto nwritten = send(channelfd, &message, sizeof(message.members[0]),
                         MSG_NOSIGNAL);
    if (nwritten == -1) {
        push(L, std::error_code{errno, std::system_category()});
        return lua_error(L);
    }

    return 0;
}

int linux_namespaces_badinjector_plugin::send_too_big(lua_State* L)
{
    auto channel = static_cast<linux_container_address*>(lua_touserdata(L, 1));
    auto channelfd = channel->dest.native_handle();
    linux_container_message messages[2];

    set_as_blocking(channelfd);

    auto nwritten = send(channelfd, &messages, sizeof(messages), MSG_NOSIGNAL);
    if (nwritten == -1) {
        push(L, std::error_code{errno, std::system_category()});
        return lua_error(L);
    }

    return 0;
}

int linux_namespaces_badinjector_plugin::send_too_many_fds(lua_State* L)
{
    auto channel = static_cast<linux_container_address*>(lua_touserdata(L, 1));
    auto channelfd = channel->dest.native_handle();
    linux_container_message message;

    set_as_blocking(channelfd);

    message.members[0].as_int = EXPONENT_MASK | linux_container_message::nil;
    message.members[1].as_double = 0;

    struct msghdr msg;
    std::memset(&msg, 0, sizeof(msg));

    struct iovec iov;
    iov.iov_base = &message;
    iov.iov_len = sizeof(message.members[0]) * 2;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    static constexpr int nfds = 3 + CONFIG_MESSAGE_MAX_MEMBERS_NUMBER;
    static int fds[nfds];

    for (int i = 0 ; i != nfds ; ++i) {
        fds[i] = pipefds[0];
    }

    union
    {
        struct cmsghdr align;
        char buf[CMSG_SPACE(sizeof(int) * nfds)];
    } cmsgu;
    msg.msg_control = cmsgu.buf;
    msg.msg_controllen = sizeof(cmsgu.buf);

    struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(fds));
    std::memcpy(CMSG_DATA(cmsg), &fds, sizeof(fds));

    auto nwritten = sendmsg(channelfd, &msg, MSG_NOSIGNAL);
    if (nwritten == -1) {
        push(L, std::error_code{errno, std::system_category()});
        return lua_error(L);
    }

    return 0;
}

int linux_namespaces_badinjector_plugin::send_invalid_root(lua_State* L)
{
    auto channel = static_cast<linux_container_address*>(lua_touserdata(L, 1));
    auto channelfd = channel->dest.native_handle();
    linux_container_message message;

    set_as_blocking(channelfd);

    message.members[0].as_int = EXPONENT_MASK | linux_container_message::nil;
    message.members[1].as_int = EXPONENT_MASK | (MANTISSA_MASK ^ QNAN_BIT);

    auto nwritten = send(channelfd, &message, sizeof(message.members[0]) * 2,
                         MSG_NOSIGNAL);
    if (nwritten == -1) {
        push(L, std::error_code{errno, std::system_category()});
        return lua_error(L);
    }

    return 0;
}

int linux_namespaces_badinjector_plugin::send_overflow_root_str(lua_State* L)
{
    auto channel = static_cast<linux_container_address*>(lua_touserdata(L, 1));
    auto channelfd = channel->dest.native_handle();
    linux_container_message message;

    set_as_blocking(channelfd);

    message.members[0].as_int = EXPONENT_MASK | linux_container_message::nil;
    message.members[1].as_int = EXPONENT_MASK | linux_container_message::string;

    auto nwritten = send(channelfd, &message, sizeof(message.members[0]) * 2,
                         MSG_NOSIGNAL);
    if (nwritten == -1) {
        push(L, std::error_code{errno, std::system_category()});
        return lua_error(L);
    }

    return 0;
}

int
linux_namespaces_badinjector_plugin::send_overflow_empty_key_str(lua_State* L)
{
    auto channel = static_cast<linux_container_address*>(lua_touserdata(L, 1));
    auto channelfd = channel->dest.native_handle();
    linux_container_message message;

    set_as_blocking(channelfd);

    message.members[0].as_int = EXPONENT_MASK
        | linux_container_message::boolean_false;
    message.members[1].as_int = EXPONENT_MASK | linux_container_message::nil;

    auto nwritten = send(channelfd, &message, sizeof(message.members),
                         MSG_NOSIGNAL);
    if (nwritten == -1) {
        push(L, std::error_code{errno, std::system_category()});
        return lua_error(L);
    }

    return 0;
}

int linux_namespaces_badinjector_plugin::send_missing_root_fd(lua_State* L)
{
    auto channel = static_cast<linux_container_address*>(lua_touserdata(L, 1));
    auto channelfd = channel->dest.native_handle();
    linux_container_message message;

    set_as_blocking(channelfd);

    message.members[0].as_int = EXPONENT_MASK | linux_container_message::nil;
    message.members[1].as_int =
        EXPONENT_MASK | linux_container_message::file_descriptor;

    auto nwritten = send(channelfd, &message, sizeof(message.members[0]) * 2,
                         MSG_NOSIGNAL);
    if (nwritten == -1) {
        push(L, std::error_code{errno, std::system_category()});
        return lua_error(L);
    }

    return 0;
}

int linux_namespaces_badinjector_plugin::send_missing_root_actorfd(lua_State* L)
{
    auto channel = static_cast<linux_container_address*>(lua_touserdata(L, 1));
    auto channelfd = channel->dest.native_handle();
    linux_container_message message;

    set_as_blocking(channelfd);

    message.members[0].as_int = EXPONENT_MASK | linux_container_message::nil;
    message.members[1].as_int =
        EXPONENT_MASK | linux_container_message::actor_address;

    auto nwritten = send(channelfd, &message, sizeof(message.members[0]) * 2,
                         MSG_NOSIGNAL);
    if (nwritten == -1) {
        push(L, std::error_code{errno, std::system_category()});
        return lua_error(L);
    }

    return 0;
}

int linux_namespaces_badinjector_plugin::send_overflow_dict(lua_State* L)
{
    auto channel = static_cast<linux_container_address*>(lua_touserdata(L, 1));
    auto channelfd = channel->dest.native_handle();
    linux_container_message message;

    set_as_blocking(channelfd);

    message.members[0].as_double = 0;
    message.members[1].as_int = EXPONENT_MASK | linux_container_message::nil;

    auto nwritten = send(channelfd, &message, sizeof(message.members[0]) * 2,
                         MSG_NOSIGNAL);
    if (nwritten == -1) {
        push(L, std::error_code{errno, std::system_category()});
        return lua_error(L);
    }

    return 0;
}

int linux_namespaces_badinjector_plugin::send_overflow_dict2(lua_State* L)
{
    auto channel = static_cast<linux_container_address*>(lua_touserdata(L, 1));
    auto channelfd = channel->dest.native_handle();
    linux_container_message message;

    set_as_blocking(channelfd);

    for (int i = 0 ; i != CONFIG_MESSAGE_MAX_MEMBERS_NUMBER ; ++i) {
        message.members[i].as_double = 0;
    }

    auto nwritten = send(channelfd, &message, sizeof(message.members),
                         MSG_NOSIGNAL);
    if (nwritten == -1) {
        push(L, std::error_code{errno, std::system_category()});
        return lua_error(L);
    }

    return 0;
}

int linux_namespaces_badinjector_plugin::send_invalid_leaf(lua_State* L)
{
    auto channel = static_cast<linux_container_address*>(lua_touserdata(L, 1));
    auto channelfd = channel->dest.native_handle();
    linux_container_message message;

    set_as_blocking(channelfd);

    message.members[0].as_double = 0;
    message.members[1].as_int = EXPONENT_MASK | (MANTISSA_MASK ^ QNAN_BIT);
    message.members[2].as_int = EXPONENT_MASK | linux_container_message::nil;

    message.strbuf[0] = 1;
    message.strbuf[1] = 'a';

    message.strbuf[2] = 1;
    message.strbuf[3] = 'b';

    auto nwritten = send(channelfd, &message, sizeof(message.members) + 4,
                         MSG_NOSIGNAL);
    if (nwritten == -1) {
        push(L, std::error_code{errno, std::system_category()});
        return lua_error(L);
    }

    return 0;
}

std::string linux_namespaces_badinjector_plugin::generate_good_string()
{
    std::string ret;
    int str_size = str_size_dist(prng);
    ret.reserve(str_size);
    for (int i = 0 ; i != str_size ; ++i) {
        ret.push_back(char_dist(prng));
    }
    return ret;
}

linux_namespaces_badinjector_plugin::leaf_t
linux_namespaces_badinjector_plugin::generate_good_leaf()
{
    switch (leaf_t_dist(prng)) {
    case 1:
        return bool_dist(prng) == 1 ? true : false;
    case 2: {
        union {
            double as_double;
            std::uint64_t as_int;
        } buf;
        buf.as_int = double_dist(prng);
        if (is_snan(buf.as_int)) {
            return std::numeric_limits<double>::quiet_NaN();
        }
        return buf.as_double;
    }
    case 3:
        return generate_good_string();
    case 4:
        return file_descriptor{};
    case 5:
        return actor_address{};
    default:
        assert(false);
        throw;
    }
}

std::map<
    std::string,
    linux_namespaces_badinjector_plugin::leaf_t
> linux_namespaces_badinjector_plugin::generate_good_dict()
{
    std::map<std::string, leaf_t> ret;
    int dict_size = dict_size_dist(prng);
    for (int i = 0 ; i != dict_size ; ++i) {
        auto key = generate_good_string();
        ret[key] = generate_good_leaf();
    }
    return ret;
}

int linux_namespaces_badinjector_plugin::fuzzer_seed(lua_State* L)
{
    if (lua_type(L, 1) == LUA_TNUMBER) {
        prng.seed(lua_tointeger(L, 1));
        return 0;
    }

    std::random_device rd;
    decltype(prng)::result_type seed = rd();
    prng.seed(seed);
    lua_pushinteger(L, seed);
    return 1;
}

std::tuple<std::size_t, int>
linux_namespaces_badinjector_plugin::generate_good_message(
    linux_container_message& message, int descriptors[])
{
    std::size_t message_size;
    int descriptors_size;

    if (bool_dist(prng) == 1) {
        auto leaf = generate_good_leaf();
        message.members[0].as_int =
            EXPONENT_MASK | linux_container_message::nil;
        message_size = sizeof(message.members[0]) * 2;
        descriptors_size = 0;
        std::visit(hana::overload(
            [&](bool b) {
                message.members[1].as_int = EXPONENT_MASK;
                message.members[1].as_int |=
                    b ?
                    linux_container_message::boolean_true :
                    linux_container_message::boolean_false;
            },
            [&](double d) {
                message.members[1].as_double = d;
            },
            [&](std::string& s) {
                message.members[1].as_int =
                    EXPONENT_MASK | linux_container_message::string;
                message.strbuf[0] = s.size();
                std::memcpy(message.strbuf + 1, s.data(), s.size());
                message_size = sizeof(message.members) + s.size() + 1;
            },
            [&](file_descriptor) {
                message.members[1].as_int =
                    EXPONENT_MASK | linux_container_message::file_descriptor;
                descriptors_size = 1;
            },
            [&](actor_address) {
                message.members[1].as_int =
                    EXPONENT_MASK | linux_container_message::actor_address;
                descriptors_size = 1;
            }
        ), leaf);
    } else {
        auto dict = generate_good_dict();
        message_size = sizeof(message.members);
        descriptors_size = 0;
        int i = 0;
        for (auto& [key, leaf] : dict) {
            auto strbufidx = message_size - sizeof(message.members);
            message_size += key.size() + 1;
            message.strbuf[strbufidx++] = key.size();
            std::memcpy(message.strbuf + strbufidx, key.data(), key.size());
            strbufidx += key.size();
            std::visit(hana::overload(
                [&](bool b) {
                    message.members[i].as_int = EXPONENT_MASK;
                    message.members[i].as_int |=
                        b ?
                        linux_container_message::boolean_true :
                        linux_container_message::boolean_false;
                },
                [&](double d) {
                    message.members[i].as_double = d;
                },
                [&](std::string& s) {
                    message.members[i].as_int =
                        EXPONENT_MASK | linux_container_message::string;
                    message.strbuf[strbufidx++] = s.size();
                    std::memcpy(message.strbuf + strbufidx, s.data(), s.size());
                    message_size += s.size() + 1;
                },
                [&](file_descriptor) {
                    message.members[i].as_int =
                        EXPONENT_MASK |
                        linux_container_message::file_descriptor;
                    ++descriptors_size;
                },
                [&](actor_address) {
                    message.members[i].as_int =
                        EXPONENT_MASK | linux_container_message::actor_address;
                    ++descriptors_size;
                }
            ), leaf);
            ++i;
        }
        if (i != CONFIG_MESSAGE_MAX_MEMBERS_NUMBER) {
            message.members[i].as_int =
                EXPONENT_MASK | linux_container_message::nil;
        }
    }

    for (int i = 0 ; i != descriptors_size ; ++i) {
        descriptors[i] = pipefds[1];
    }

    return {message_size, descriptors_size};
}

int linux_namespaces_badinjector_plugin::fuzzer_send_good(lua_State* L)
{
    linux_container_message message;
    std::size_t message_size;
    int descriptors[CONFIG_MESSAGE_MAX_MEMBERS_NUMBER];
    int descriptors_size;

    std::tie(message_size, descriptors_size) =
        generate_good_message(message, descriptors);

    if (lua_isnoneornil(L, 1)) {
        return 0;
    }

    // ### from here onwards we do *NOT* mutate the PRNG state ###

    auto channel = static_cast<linux_container_address*>(lua_touserdata(L, 1));
    auto channelfd = channel->dest.native_handle();

    set_as_blocking(channelfd);

    struct msghdr msg;
    std::memset(&msg, 0, sizeof(msg));

    struct iovec iov;
    iov.iov_base = &message;
    iov.iov_len = message_size;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    union
    {
        struct cmsghdr align;
        char buf[CMSG_SPACE(sizeof(int) * CONFIG_MESSAGE_MAX_MEMBERS_NUMBER)];
    } cmsgu;
    msg.msg_control = cmsgu.buf;
    msg.msg_controllen = CMSG_SPACE(sizeof(int) * descriptors_size);

    struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(int) * descriptors_size);
    std::memcpy(CMSG_DATA(cmsg), &descriptors, sizeof(int) * descriptors_size);

    auto nwritten = sendmsg(channelfd, &msg, MSG_NOSIGNAL);
    if (nwritten == -1) {
        push(L, std::error_code{errno, std::system_category()});
        return lua_error(L);
    }

    return 0;
}

int linux_namespaces_badinjector_plugin::fuzzer_send_bad(lua_State* L)
{
    linux_container_message message;
    std::size_t message_size;
    int descriptors[CONFIG_MESSAGE_MAX_MEMBERS_NUMBER];
    int descriptors_size;

    std::tie(message_size, descriptors_size) =
        generate_good_message(message, descriptors);

    int nilpos = -1;

    for (int i = 0 ; i != CONFIG_MESSAGE_MAX_MEMBERS_NUMBER ; ++i) {
        if (
            message.members[i].as_int ==
            (EXPONENT_MASK | linux_container_message::nil)
        ) {
            nilpos = i;
            break;
        }
    }

    for (bool mutated = false ; !mutated ;) {
        if (bool_dist(prng) == 1) {
            std::uint64_t invalid_value = EXPONENT_MASK;
            if (bool_dist(prng) == 1) {
                invalid_value |= DOUBLE_SIGN_BIT;
            }
            invalid_value |= unknown_snan_mantissa_dist(prng);
            std::uint8_t i;
            if (nilpos == 0) {
                i = bool_dist(prng);
            } else {
                std::uint8_t upper_bound;
                if (nilpos == -1) {
                    upper_bound = CONFIG_MESSAGE_MAX_MEMBERS_NUMBER - 1;
                } else {
                    upper_bound = nilpos;
                }
                std::uniform_int_distribution<std::uint8_t> dist{
                    0, upper_bound};
                i = dist(prng);
            }
            message.members[i].as_int = invalid_value;
            mutated = true;
        }

        if (bool_dist(prng) == 1) {
            std::uniform_int_distribution<std::uint8_t> dist(
                0, message_size - 1);
            message_size = dist(prng);
            mutated = true;
        }

        if (descriptors_size > 0 && bool_dist(prng) == 1) {
            std::uniform_int_distribution<std::uint8_t> dist(
                0, descriptors_size - 1);
            descriptors_size = dist(prng);
            mutated = true;
        }
    }

    if (lua_isnoneornil(L, 1)) {
        return 0;
    }

    // ### from here onwards we do *NOT* mutate the PRNG state ###

    auto channel = static_cast<linux_container_address*>(lua_touserdata(L, 1));
    auto channelfd = channel->dest.native_handle();

    set_as_blocking(channelfd);

    struct msghdr msg;
    std::memset(&msg, 0, sizeof(msg));

    struct iovec iov;
    iov.iov_base = &message;
    iov.iov_len = message_size;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    union
    {
        struct cmsghdr align;
        char buf[CMSG_SPACE(sizeof(int) * CONFIG_MESSAGE_MAX_MEMBERS_NUMBER)];
    } cmsgu;
    msg.msg_control = cmsgu.buf;
    msg.msg_controllen = CMSG_SPACE(sizeof(int) * descriptors_size);

    struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(int) * descriptors_size);
    std::memcpy(CMSG_DATA(cmsg), &descriptors, sizeof(int) * descriptors_size);

    auto nwritten = sendmsg(channelfd, &msg, MSG_NOSIGNAL);
    if (nwritten == -1) {
        push(L, std::error_code{errno, std::system_category()});
        return lua_error(L);
    }

    return 0;
}

} // namespace emilua

extern "C" BOOST_SYMBOL_EXPORT
emilua::linux_namespaces_badinjector_plugin EMILUA_PLUGIN_SYMBOL;
emilua::linux_namespaces_badinjector_plugin EMILUA_PLUGIN_SYMBOL;
