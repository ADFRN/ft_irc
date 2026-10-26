/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Client.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: afournie <afournie@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/10/26 16:13:21 by afournie          #+#    #+#             */
/*   Updated: 2026/10/26 16:29:20 by afournie         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CLIENT_HPP
# define  CLIENT_HPP

#include <iostream>

class Client
{
	private:
		int _fd;
		std::string _ipAdd;

	public:
		Client();
		~Client();

		int getFd();
		void setFd(int fd);
		void setIpAdd(std::string ipAdd);
};

#endif
