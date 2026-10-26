/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: afournie <afournie@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/10/26 16:35:34 by afournie          #+#    #+#             */
/*   Updated: 2026/10/26 16:40:45 by afournie         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Client.hpp"
#include "Server.hpp"
#include <cstdlib>

int main(int ac, char **av)
{
	if (ac != 3)
	{
		std::cerr << "Please provide port and password (./ircserv <port> <password>)" << std::endl;
		return (EXIT_FAILURE);
	}
	std::cout << "port: " << av[1] << std::endl;
	std::cout << "password: " << av[2] << std::endl;
	return (EXIT_SUCCESS);
}
