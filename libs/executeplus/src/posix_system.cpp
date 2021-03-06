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
 * \file posix_system.cpp
 * \author Julien KAUFFMANN <julien.kauffmann@freelan.org>
 * \brief Posix system primitives.
 */

#include "posix_system.hpp"

#ifdef UNIX

#include "error.hpp"

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

#include <sstream>

#include <boost/lexical_cast.hpp>
#include <boost/iostreams/device/file_descriptor.hpp>
#include <boost/iostreams/stream.hpp>

namespace executeplus
{
	int execute(const std::vector<std::string>& args, boost::system::error_code& ec, std::ostream* output)
	{
#if FREELAN_DEBUG
		std::cout << "Executing:";

		for (auto&& arg : args)
		{
			std::cout << " " << arg;
		}

		std::cout << std::endl;
#endif

		int fd[2] = {0, 0};

		if (::pipe(fd) < 0)
		{
			ec = boost::system::error_code(errno, boost::system::system_category());

			return -1;
		}

		int output_fd[2] = {0, 0};

		if (output)
		{
			if (::pipe(output_fd) < 0)
			{
				ec = boost::system::error_code(errno, boost::system::system_category());

				return -1;
			}
		}

		const pid_t pid = fork();

		switch (pid)
		{
			case -1:
				{
					// fork() failed.
					if (output)
					{
						::close(output_fd[0]);
						::close(output_fd[1]);
					}

					::close(fd[0]);
					::close(fd[1]);

					ec = boost::system::error_code(errno, boost::system::system_category());

					return -1;
				}
			case 0:
				{
					// Child process
					const int fdlimit = ::sysconf(_SC_OPEN_MAX);

					if (output)
					{
						::dup2(output_fd[1], STDOUT_FILENO);
						::close(output_fd[1]);
					}

					for (int n = 0; n < fdlimit; ++n)
					{
						if ((n != fd[1]) && (!output || (n != STDOUT_FILENO)))
						{
							::close(n);
						}
					}

					fcntl(fd[1], F_SETFD, FD_CLOEXEC);

					// Estimate the required size for the argv buffer.
					// One null-terminated byte per arg.
					size_t buffer_size = args.size();

					for (auto&& arg : args)
					{
						buffer_size += arg.size();
					}

					std::vector<char> buffer(buffer_size, 0x00);
					std::vector<char*> argv(args.size() + 1, nullptr);
					auto offset = buffer.begin();

					for (size_t i = 0; i != args.size(); ++i)
					{
						const auto& arg = args[i];

						argv[i] = &*offset;
						offset = std::copy(arg.begin(), arg.end(), offset);
						*(offset++) = '\0';
					}

					// Execute the file specified
					::execv(argv[0], &argv[0]);

					// Something went wrong. Sending back errno to parent process then exiting.
					if (::write(fd[1], &errno, sizeof(errno))) {}

					_exit(127);

					break;
				}
			default:
				{
					// Parent process
					::close(fd[1]);

					if (output)
					{
						::close(output_fd[1]);
					}

					int child_errno = 0;

					if (output)
					{
						// This will take ownership of the file descriptor.
						boost::iostreams::file_descriptor_source output_src(output_fd[0], boost::iostreams::close_handle);
						boost::iostreams::stream<boost::iostreams::file_descriptor_source> output_is(output_src);

						(*output) << output_is.rdbuf();
					}

					const ssize_t readcnt = ::read(fd[0], &child_errno, sizeof(child_errno));

					::close(fd[0]);

					if (readcnt < 0)
					{
						ec = boost::system::error_code(errno, boost::system::system_category());

						return -1;
					}
					else if (readcnt == sizeof(child_errno))
					{
						ec = boost::system::error_code(child_errno, boost::system::system_category());

						return -1;
					}

					int status = 0;

					if (::waitpid(pid, &status, 0) != pid)
					{
						ec = boost::system::error_code(child_errno, boost::system::system_category());

						return -1;
					}
					else
					{
						if (WIFEXITED(status))
						{
							const int result = WEXITSTATUS(status);

#if FREELAN_DEBUG
							std::cout << "Exit status: " << result << std::endl;
#endif

							return result;
						}
					}

					break;
				}
		}

		return EXIT_FAILURE;
	}

	int execute(const std::vector<std::string>& args, std::ostream* output)
	{
		boost::system::error_code ec;

		const auto result = execute(args, ec, output);

		if (result < 0)
		{
			throw boost::system::system_error(ec);
		}

		return result;
	}

	void checked_execute(const std::vector<std::string>& args, std::ostream* output)
	{
		if (execute(args, output) != 0)
		{
			throw boost::system::system_error(make_error_code(executeplus_error::external_process_failed));
		}
	}
}

#endif
