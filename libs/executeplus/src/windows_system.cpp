/*
 * libexecuteplus - A portable execution library.
 * Copyright (C) 2010-2011 Julien KAUFFMANN <julien.kauffmann@freelan.org>
 *
 * This file is part of libexecuteplus.
 *
 * libexecuteplus is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 3 of
 * the License, or (at your option) any later version.
 *
 * libexecuteplus is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program. If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * In addition, as a special exception, the copyright holders give
 * permission to link the code of portions of this program with the
 * OpenSSL library under certain conditions as described in each
 * individual source file, and distribute linked combinations
 * including the two.
 * You must obey the GNU General Public License in all respects
 * for all of the code used other than OpenSSL.  If you modify
 * file(s) with this exception, you may extend this exception to your
 * version of the file(s), but you are not obligated to do so.  If you
 * do not wish to do so, delete this exception statement from your
 * version.  If you delete this exception statement from all source
 * files in the program, then also delete it here.
 *
 * If you intend to use libexecuteplus in a commercial software, please
 * contact me : we may arrange this for a small fee or no fee at all,
 * depending on the nature of your project.
 */

/**
 * \file windows_system.cpp
 * \author Julien KAUFFMANN <julien.kauffmann@freelan.org>
 * \brief Windows system primitives.
 */

#include "windows_system.hpp"

#ifdef WINDOWS

#include "error.hpp"

#include <iostream>
#include <sstream>
#include <locale>
#include <codecvt>
#include <algorithm>

#include <boost/lexical_cast.hpp>

#include <shlobj.h>
#include <shellapi.h>

namespace executeplus
{
	namespace
	{
		class handle_closer
		{
			public:
				handle_closer(HANDLE handle) : m_handle(handle) {}
				~handle_closer() { ::CloseHandle(m_handle); }

			private:
				HANDLE m_handle;
		};

		void output(const char* str)
		{
			std::cout << str << std::endl;
		}

		void output(const wchar_t* str)
		{
			std::wcout << str << std::endl;
		}

		bool has_escapable_characters(const std::string& str)
		{
			return (str.find_first_of(" \t\n\v\"") != std::string::npos);
		}

		bool has_escapable_characters(const std::wstring& str)
		{
			return (str.find_first_of(L" \t\n\v\"") != std::string::npos);
		}

		template <typename CharType>
		struct argument_helper;

		template <>
		struct argument_helper<char>
		{
			static const char ESCAPE_CHARACTER = '\\';
			static const char QUOTE_CHARACTER = '"';
		};

		template <>
		struct argument_helper<wchar_t>
		{
			static const wchar_t ESCAPE_CHARACTER = L'\\';
			static const wchar_t QUOTE_CHARACTER = L'"';
		};

		template <typename CharType>
		std::basic_string<CharType> escape_argument(const std::basic_string<CharType>& arg)
		{
			std::basic_string<CharType> result(1, argument_helper<CharType>::QUOTE_CHARACTER);

			for (auto it = arg.begin();; ++it)
			{
				unsigned int escapes_count = 0;

				while ((it != arg.end()) && (*it == argument_helper<CharType>::ESCAPE_CHARACTER))
				{
					++it;
					++escapes_count;
				}

				if (it == arg.end())
				{
					result.append(escapes_count * 2, argument_helper<CharType>::ESCAPE_CHARACTER);
					break;
				}
				else if (*it == argument_helper<CharType>::QUOTE_CHARACTER)
				{
					result.append(escapes_count * 2 + 1, argument_helper<CharType>::ESCAPE_CHARACTER);
					result.push_back(*it);
				}
				else
				{
					result.append(escapes_count, argument_helper<CharType>::ESCAPE_CHARACTER);
					result.push_back(*it);
				}
			}

			result.push_back(argument_helper<CharType>::QUOTE_CHARACTER);

			return result;
		}

		template <typename CharType>
		std::basic_string<CharType> escape_argument_if_needed(const std::basic_string<CharType>& arg)
		{
			if (!arg.empty() && !has_escapable_characters(arg))
			{
				return arg;
			}
			else
			{
				return escape_argument(arg);
			}
		}

		DWORD do_create_process(const char* application, char* command_line, STARTUPINFOA& si, PROCESS_INFORMATION& pi)
		{
			return ::CreateProcessA(application, command_line, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
		}

		DWORD do_create_process(const wchar_t* application, wchar_t* command_line, STARTUPINFOW& si, PROCESS_INFORMATION& pi)
		{
			return ::CreateProcessW(application, command_line, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
		}

		template <typename CharType>
		struct startupinfo;

		template <>
		struct startupinfo<char>
		{
			typedef STARTUPINFOA type;
		};

		template <>
		struct startupinfo<wchar_t>
		{
			typedef STARTUPINFOW type;
		};

		template <typename CharType>
		DWORD create_process(const CharType* application, CharType* command_line)
		{
			DWORD exit_status;

			typename startupinfo<CharType>::type si;
			si.cb = sizeof(si);
			si.lpReserved = NULL;
			si.lpDesktop = NULL;
			si.lpTitle = NULL;
			si.dwX = 0;
			si.dwY = 0;
			si.dwXSize = 0;
			si.dwYSize = 0;
			si.dwXCountChars = 0;
			si.dwYCountChars = 0;
			si.dwFillAttribute = 0;
			si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES; // Remove STARTF_USESTDHANDLES to show stdout
			si.wShowWindow = SW_HIDE;
			si.cbReserved2 = 0;
			si.lpReserved2 = NULL;
			si.hStdInput = INVALID_HANDLE_VALUE;
#if FREELAN_DEBUG
			si.hStdOutput = ::GetStdHandle(STD_OUTPUT_HANDLE);
#else
			si.hStdOutput = INVALID_HANDLE_VALUE;
#endif
			si.hStdError = INVALID_HANDLE_VALUE;

			PROCESS_INFORMATION pi;

#if FREELAN_DEBUG
			output(command_line);
#endif

			if (!do_create_process(application, command_line, si, pi))
			{
				throw boost::system::system_error(::GetLastError(), boost::system::system_category());
			}

			handle_closer thread_closer(pi.hThread);
			handle_closer process_closer(pi.hProcess);

			DWORD wait_result = ::WaitForSingleObject(pi.hProcess, INFINITE);

			switch (wait_result)
			{
				case WAIT_OBJECT_0:
					{
						DWORD exit_code = 0;

						if (::GetExitCodeProcess(pi.hProcess, &exit_code))
						{
							exit_status = static_cast<int>(exit_code);
						}
						else
						{
							throw boost::system::system_error(::GetLastError(), boost::system::system_category());
						}

						break;
					}
				default:
					{
						throw boost::system::system_error(::GetLastError(), boost::system::system_category());
					}
			}

			return exit_status;
		}

		template <typename CharType>
		int do_execute(const std::vector<std::basic_string<CharType>>& args, boost::system::error_code& ec)
		{
			if (args.empty())
			{
				ec = make_error_code(executeplus_error::external_process_failed);

				return EXIT_FAILURE;
			}

			const std::basic_string<CharType> application = args.front();
			std::basic_ostringstream<CharType> command_line_buffer;

			for (auto it = args.begin(); it != args.end(); ++it)
			{
				if (it != args.begin())
				{
					command_line_buffer << " ";
				}

				command_line_buffer << escape_argument_if_needed(*it);
			}

			std::basic_string<CharType> command_line = command_line_buffer.str();

			return create_process(application.c_str(), &command_line[0]);
		}

		template <typename CharType>
		int do_execute(const std::vector<std::basic_string<CharType>>& args)
		{
			boost::system::error_code ec;

			const auto result = execute(args, ec);

			if (result < 0)
			{
				throw boost::system::system_error(ec);
			}

			return result;
		}

		template <typename CharType>
		void do_checked_execute(const std::vector<std::basic_string<CharType>>& args)
		{
			if (do_execute(args) != 0)
			{
				throw boost::system::system_error(make_error_code(executeplus_error::external_process_failed));
			}
		}
	}

#ifdef UNICODE
	int execute(const std::vector<std::wstring>& args, boost::system::error_code& ec)
#else
	int execute(const std::vector<std::string>& args, boost::system::error_code& ec)
#endif
	{
		return do_execute(args, ec);
	}

#ifdef UNICODE
	int execute(const std::vector<std::wstring>& args)
#else
	int execute(const std::vector<std::string>& args)
#endif
	{
		return do_execute(args);
	}

#ifdef UNICODE
	void checked_execute(const std::vector<std::wstring>& args)
#else
	void checked_execute(const std::vector<std::string>& args)
#endif
	{
		do_checked_execute(args);
	}
}

#endif
