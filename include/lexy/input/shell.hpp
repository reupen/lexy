// Copyright (C) 2020 Jonathan Müller <jonathanmueller.dev@gmail.com>
// This file is subject to the license terms in the LICENSE file
// found in the top-level directory of this distribution.

#ifndef LEXY_INPUT_SHELL_HPP_INCLUDED
#define LEXY_INPUT_SHELL_HPP_INCLUDED

#include <cstdio>

#include <lexy/_detail/assert.hpp>
#include <lexy/_detail/buffer_builder.hpp>
#include <lexy/error.hpp>
#include <lexy/input/base.hpp>
#include <lexy/lexeme.hpp>

namespace lexy
{
#if 0
/// Controls how the shell performs I/O.
class Prompt
{
    using encoding = ...;

    /// Called to display the primary prompt.
    void primary_prompt();

    /// Called to display the continuation prompt.
    void continuation_prompt();

    /// Called to display EOF.
    void eof_prompt();

    /// Whether or not the user has closed the input.
    bool is_open() const;

    /// Returns a callback object for reading the next line.
    auto read_line()
    {
        struct callback
        {
            /// Reads at most `size` characters into the `buffer` until and including a newline.
            /// Returns the number of characters read.
            /// If the number of characters read is less than the size,
            /// the entire line has been read or a read error occurs.
            std::size_t operator()(char_type* buffer, std::size_t size); 

            /// Called after the shell has finished reading.
            void done() &&;
        };

        return callback{...};
    }

    /// Writes a message out.
    /// The arguments are passed by the user to indicate kinds of messages.
    auto write_message(Args&&... config_args)
    {
        struct callback
        {
            /// Writes the buffer.
            void operator()(const char_type* buffer, std::size_t size); 

            /// Called to finish writing.
            void done() &&;
        };

        return callback{...};
    }
};
#endif

template <typename Encoding = default_encoding>
struct default_prompt
{
    using encoding  = Encoding;
    using char_type = typename Encoding::char_type;
    static_assert(sizeof(char_type) == sizeof(char), "only support single-byte encodings");

    void primary_prompt() noexcept
    {
        std::fputs("> ", stdout);
    }

    void continuation_prompt() noexcept
    {
        std::fputs(". ", stdout);
    }

    void eof_prompt() noexcept
    {
        // We write an additional newline to prevent output on the same line.
        std::fputs("\n", stdout);
    }

    bool is_open() const noexcept
    {
        return !std::feof(stdin) && !std::ferror(stdin);
    }

    auto read_line()
    {
        struct callback
        {
            std::size_t operator()(char_type* buffer, std::size_t size)
            {
                LEXY_PRECONDITION(size > 1);

                auto memory = reinterpret_cast<char*>(buffer);
                if (auto str = std::fgets(memory, int(size), stdin))
                    return std::strlen(str);
                else
                    return 0;
            }

            void done() && {}
        };

        return callback{};
    }

    auto write_message()
    {
        struct callback
        {
            void operator()(const char_type* buffer, std::size_t size)
            {
                std::fprintf(stdout, "%.*s", int(size), reinterpret_cast<const char*>(buffer));
            }

            void done() &&
            {
                std::putchar('\n');
            }
        };

        return callback{};
    }
};
} // namespace lexy

namespace lexy
{
/// Reads input from an interactive shell.
template <typename Prompt = default_prompt<>>
class shell
{
public:
    using encoding    = typename Prompt::encoding;
    using char_type   = typename encoding::char_type;
    using prompt_type = Prompt;

    shell() = default;
    explicit shell(Prompt prompt) : _prompt(LEXY_MOV(prompt)) {}

    /// Whether or not the shell is still open.
    bool is_open() const noexcept
    {
        return _prompt.is_open();
    }

    // This is both Reader and Input.
    class input
    {
    public:
        using encoding  = typename Prompt::encoding;
        using char_type = typename encoding::char_type;
        using iterator  = typename _detail::buffer_builder<char_type>::stable_iterator;

        auto reader() const&
        {
            return *this;
        }

        bool eof() const
        {
            if (_idx != _shell->_buffer.read_size())
                // We're still having characters in the read buffer.
                return false;
            else if (!_shell->_prompt.is_open())
                // The prompt has been closed by the user.
                return true;
            else
            {
                // We've reached the end of the buffer, but the user might be willing to type
                // another line.
                _shell->_prompt.continuation_prompt();
                auto did_append = _shell->append_next_line();
                if (!did_append)
                    _shell->_prompt.eof_prompt();
                return !did_append;
            }
        }

        auto peek() const
        {
            if (eof())
                return encoding::eof();
            else
                return encoding::to_int_type(_shell->_buffer.read_data()[_idx]);
        }

        void bump() noexcept
        {
            ++_idx;
        }

        auto cur() const noexcept
        {
            return iterator(_shell->_buffer, _idx);
        }

    private:
        explicit input(shell* s) : _shell(s), _idx(0)
        {
            _shell->_buffer.clear();
            _shell->_prompt.primary_prompt();
            if (!_shell->append_next_line())
                _shell->_prompt.eof_prompt();
        }

        shell*      _shell;
        std::size_t _idx;

        friend shell;
    };

    /// Asks the user to enter input.
    /// This will invalidate the previous buffer.
    /// Returns an Input for that input.
    auto prompt_for_input()
    {
        return input(this);
    }

    /// Writes a message out to the shell.
    template <typename... Args>
    auto write_message(Args&&... args)
    {
        using prompt_writer = decltype(_prompt.write_message(LEXY_FWD(args)...));
        class writer
        {
        public:
            writer(const writer&) = delete;
            writer& operator=(const writer&) = delete;

            ~writer() noexcept
            {
                LEXY_MOV(_writer).done();
            }

            writer& operator()(const char_type* str, std::size_t length)
            {
                _writer(str, length);
                return *this;
            }
            writer& operator()(const char_type* str)
            {
                _writer(str, std::strlen(str));
                return *this;
            }
            writer& operator()(char_type c)
            {
                _writer(&c, 1);
                return *this;
            }

            writer& operator()(lexy::lexeme_for<input> lexeme)
            {
                // We know that the iterator is contiguous.
                auto data = &*lexeme.begin();
                _writer(data, lexeme.size());
                return *this;
            }

        private:
            explicit writer(prompt_writer&& writer) : _writer(LEXY_MOV(writer)) {}

            LEXY_EMPTY_MEMBER prompt_writer _writer;

            friend shell;
        };
        return writer{_prompt.write_message(LEXY_FWD(args)...)};
    }

    Prompt& get_prompt() noexcept
    {
        return _prompt;
    }
    const Prompt& get_prompt() const noexcept
    {
        return _prompt;
    }

private:
    // Returns whether or not we've read anything.
    bool append_next_line()
    {
        // Grwo buffer if necessary.
        constexpr auto min_capacity = 128;
        if (_buffer.write_size() < min_capacity)
            _buffer.grow();

        for (auto reader = _prompt.read_line(); true;)
        {
            const auto buffer_size = _buffer.write_size();

            // Read into the entire write area of the buffer from the file,
            // commiting what we've just read.
            const auto read = reader(_buffer.write_data(), buffer_size);
            _buffer.commit(read);

            // Check whether we've read the entire line.
            if (_buffer.write_data()[-1] == '\n')
            {
                LEXY_MOV(reader).done();
                return true;
            }
            else if (read < buffer_size)
            {
                LEXY_ASSERT(!_prompt.is_open(), "read error but prompt still open?!");
                return false;
            }

            // We've filled the entire buffer and need more space.
            // This grow might be unnecessary if we're just so happen to reach the newline with the
            // next character, but checking this requires reading more input.
            _buffer.grow();
        }

        LEXY_ASSERT(false, "unreachable");
    }

    _detail::buffer_builder<char_type> _buffer;
    LEXY_EMPTY_MEMBER Prompt           _prompt;
};

//=== convenience typedefs ===//
template <typename Prompt = default_prompt<>>
using shell_lexeme = lexeme_for<shell<Prompt>>;

template <typename Tag, typename Prompt = default_prompt<>>
using shell_error = error_for<shell<Prompt>, Tag>;

template <typename Production, typename Prompt = default_prompt<>>
using shell_error_context = error_context<Production, shell<Prompt>>;
} // namespace lexy

#endif // LEXY_INPUT_SHELL_HPP_INCLUDED

