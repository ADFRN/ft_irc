/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: afournie <afournie@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/10/26 16:12:21 by afournie          #+#    #+#             */
/*   Updated: 2026/10/26 16:22:52 by afournie         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef SERVER_HPP
# define SERVER_HPP

#include <iostream>
#include <vector>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <poll.h>
#include <csignal>
#include "Client.hpp"

class Server
{
	private:
		int _port;
		int _serSocketFD;
		static bool _signal;
		std::vector<Client> _clients;
		std::vector<struct pollfd> fds;

	public:
		Server();
		~Server();

		void ServerInit();
		void SerSocket();
		void AcceptNewClient();
		void ReceiveNewData(int fd);

		static void SignalHandler(int signum);

		void CloseFds();
		void ClearClients(int fd);
};

#endif
